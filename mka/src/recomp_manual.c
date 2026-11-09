/**
 * Manual function overrides and ICALL diagnostics
 *
 * This file provides:
 *   - recomp_lookup_manual()  : intercept specific Xbox VAs with hand-written code
 *   - recomp_icall_fail_log() : log when an indirect call target can't be resolved
 *   - ICALL trace ring buffer  : globals used by the RECOMP_ICALL macro
 *
 * The recomp pipeline generates an auto-dispatch table (recomp_lookup) that
 * resolves most function addresses. recomp_lookup_manual() is called FIRST,
 * giving you a chance to override any function with a custom implementation.
 *
 * Common reasons to add manual overrides:
 *   - Trace a function to understand call flow (wrap the generated version)
 *   - Fix a function the lifter translated incorrectly
 *   - Stub out a function that crashes (return early, set eax to a safe value)
 *   - Redirect a function to a native implementation (e.g., skip CRT init)
 *   - Intercept D3D/audio calls for custom rendering or sound
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/* ── ICALL trace ring buffer ───────────────────────────────── */

/*
 * These globals are written by the RECOMP_ICALL macro (defined in
 * recomp_types.h) every time an indirect call is dispatched. When a
 * crash occurs, the VEH handler or recomp_icall_fail_log() can dump
 * the last 16 call targets to help you trace what happened.
 *
 * The runtime owns them: xbox_kernel defines all three in
 * src/kernel/xbox_memory_layout.c, and recomp_types.h declares them extern.
 * Declare, do not define -- a definition here as well is a duplicate symbol,
 * and a project copied from this template failed to link on all three:
 *
 *   xbox_memory_layout.obj : error LNK2005: g_icall_count already defined
 *                            in recomp_manual.obj
 */
extern volatile uint32_t g_icall_trace[16];
extern volatile uint32_t g_icall_trace_idx;
extern volatile uint64_t g_icall_count;

typedef void (*recomp_func_t)(void);

/* ── Register state (defined in xbox_memory_layout.c) ──────── */

/* Thread-local in the runtime, so the declaration has to say so too: a plain
 * extern would name the image's TLS template, not this thread's copy. */
#include <xbox/xboxrecomp.h>

extern RECOMP_TLS uint32_t g_eax, g_esp, g_ecx, g_esi, g_edi;
extern ptrdiff_t g_xbox_mem_offset;

#define GUEST32(va) (*(uint32_t *)((uintptr_t)g_xbox_mem_offset + (va)))
#define GUEST8(va)  (*(uint8_t *)((uintptr_t)g_xbox_mem_offset + (va)))

/* ── XNET tcpxsum (0x004A3E18) ─────────────────────────────── */

/*
 * ULONG __stdcall tcpxsum(ULONG cksum, PUCHAR buf, ULONG len), the XNET
 * stack's ones'-complement checksum. The original enters an unrolled adc loop
 * through a jump table at 0x004A3F14 (Duff's device), landing mid-function at
 * addresses like 0x004A3EBA that are not functions, so the lifted body fails
 * its first indirect tail jump. Same result in plain C: the sum of the buffer
 * as little-endian 16-bit words (an odd last byte as a low byte), folded to
 * 16 bits, plus cksum's low half with end-around carry. The original's
 * byte-swap for odd addresses only undoes its own aligned reads.
 */
void sub_004A3E18(void)
{
    uint32_t cksum = GUEST32(g_esp + 4);
    uint32_t buf   = GUEST32(g_esp + 8);
    uint32_t len   = GUEST32(g_esp + 12);
    uint64_t sum = 0;
    uint32_t i;

    for (i = 0; i + 1 < len; i += 2)
        sum += (uint32_t)GUEST8(buf + i) | ((uint32_t)GUEST8(buf + i + 1) << 8);
    if (len & 1)
        sum += GUEST8(buf + len - 1);
    while (sum >> 16)
        sum = (sum & 0xFFFF) + (sum >> 16);

    sum += cksum & 0xFFFF;
    sum = (sum & 0xFFFF) + (sum >> 16);

    g_eax = (uint32_t)sum;
    g_esp += 16;  /* ret 12 */
}

/* ── XPP URB wait (0x004D2EAE) ─────────────────────────────── */

/*
 * XAPI's synchronous USB request: KeWaitForSingleObject(event) with a 50 ms
 * timeout, and on STATUS_TIMEOUT cancel the URB (0x004D1CEA) and wait again,
 * forever, for the cancel. The emulated controller runs on a host timer, so a
 * control transfer can outlast 50 ms, and the cancel then never completed:
 * MKA hung in XInputOpen at boot about one start in six. The emulated pad
 * always answers, so wait for the transfer itself, without the timeout.
 * stdcall (event), ret 4; ecx/edx carry the cancel's arguments, unused now.
 */
extern recomp_func_t recomp_lookup(uint32_t xbox_va);
extern recomp_func_t recomp_lookup_kernel(uint32_t xbox_va);

void sub_004D2EAE(void)
{
    uint32_t ev = GUEST32(g_esp + 4);
    uint32_t thunk = GUEST32(0x004D7820);   /* KeWaitForSingleObject */
    recomp_func_t fn = recomp_lookup_kernel(thunk);

    if (!fn)
        fn = recomp_lookup(thunk);
    g_esp -= 24;
    GUEST32(g_esp)      = 0x004D2EF4;       /* return address */
    GUEST32(g_esp + 4)  = ev;               /* Object */
    GUEST32(g_esp + 8)  = 0;                /* WaitReason: Executive */
    GUEST32(g_esp + 12) = 0;                /* WaitMode: KernelMode */
    GUEST32(g_esp + 16) = 0;                /* Alertable: FALSE */
    GUEST32(g_esp + 20) = 0;                /* Timeout: NULL, infinite */
    if (fn)
        fn();                               /* stdcall: pops 20 + return */
    else
        g_esp += 24;
    g_esp += 8;                             /* ret 4 */
}

/* ── CRT memmove/memcpy (0x0039C6E0) ───────────────────────── */

/*
 * void * __cdecl memmove(void *dst, const void *src, size_t n), the MSVC CRT
 * one (memcpy shares the body). Its byte-tail and backward-copy paths jump
 * through tables at 0x0039C8CC and 0x0039C978 the lifter could not bound, so
 * an overlapping or unaligned copy ended in an unresolved indirect jump.
 * Guest memory is one contiguous host mapping, so the host memmove does it.
 */
void sub_0039C6E0(void)
{
    uint32_t dst = GUEST32(g_esp + 4);
    uint32_t src = GUEST32(g_esp + 8);
    uint32_t n   = GUEST32(g_esp + 12);

    if (n)
        memmove((void *)((uintptr_t)g_xbox_mem_offset + dst),
                (const void *)((uintptr_t)g_xbox_mem_offset + src), n);
    g_eax = dst;
    g_esp += 4;  /* ret */
}

/* ── Manual function overrides ─────────────────────────────── */

/*
 * Return a function pointer to override the given Xbox VA, or NULL
 * to fall through to the auto-generated dispatch table.
 *
 * This is called on every indirect call (RECOMP_ICALL) and every
 * direct call through the dispatch table, so keep it fast. A chain
 * of if-statements on uint32_t compiles to a simple comparison
 * sequence; for large override tables, consider a sorted array
 * with binary search.
 *
 * Examples of common override patterns:
 *
 *   // Trace wrapper: log entry/exit around the generated function
 *   extern void sub_00012345(void);
 *   static void traced_sub_00012345(void) {
 *       fprintf(stderr, "[TRACE] sub_00012345 entered, eax=0x%08X\n", g_eax);
 *       sub_00012345();
 *       fprintf(stderr, "[TRACE] sub_00012345 returned, eax=0x%08X\n", g_eax);
 *   }
 *
 *   // Stub: skip a function entirely (return 0 in eax)
 *   static void stub_00067890(void) {
 *       g_eax = 0;
 *   }
 *
 *   // Fix: replace a broken lifted function with correct C
 *   static void fixed_sub_000ABCDE(void) {
 *       // Read arguments from stack/registers per calling convention
 *       uint32_t arg1 = g_ecx;
 *       uint32_t arg2 = MEM32(g_esp + 4);
 *       // ... correct implementation ...
 *       g_eax = result;
 *   }
 */
recomp_func_t recomp_lookup_manual(uint32_t xbox_va)
{
    /*
     * TODO: Add your overrides here. Examples:
     *
     * if (xbox_va == 0x00012345) return traced_sub_00012345;
     * if (xbox_va == 0x00067890) return stub_00067890;
     * if (xbox_va == 0x000ABCDE) return fixed_sub_000ABCDE;
     */

    (void)xbox_va;
    return (recomp_func_t)0;
}

/* ── ICALL failure logging ─────────────────────────────────── */

/*
 * Called when RECOMP_ICALL cannot resolve a target address.
 * This usually means one of:
 *   - A vtable dispatch to an address not in the dispatch table
 *   - A function pointer loaded from uninitialized or corrupt memory
 *   - A kernel thunk address that the bridge doesn't handle
 *
 * During early bring-up you will see many of these. Most are harmless
 * (the ICALL macro pops the dummy return address and continues).
 * Focus on the ones that cause crashes or incorrect behavior.
 */
void recomp_icall_fail_log(uint32_t va)
{
    /* The caller pushed its return address just before the call: the call site, and the guest stack above it. */
    {
        int k;
        fprintf(stderr, "[ICALL] Failed to resolve VA 0x%08X (total calls: %llu), called from 0x%08X; ecx %08X esi %08X"
                        " edi %08X; stack:", va, (unsigned long long)g_icall_count, GUEST32(g_esp), g_ecx, g_esi, g_edi);
        for (k = 1; k < 24; k++) fprintf(stderr, " %08X", GUEST32(g_esp + 4 * k));
        fprintf(stderr, "\n");
    }

    /* Dump last 16 call targets from the ring buffer */
    fprintf(stderr, "  Recent ICALL targets:\n");
    for (int i = 0; i < 16; i++) {
        int idx = (g_icall_trace_idx - 16 + i) & 15;
        if (g_icall_trace[idx])
            fprintf(stderr, "    [%2d] 0x%08X\n", i, g_icall_trace[idx]);
    }
    fflush(stderr);
}

/* An indirect call whose target is not code: a null or wild function pointer.
 *
 * Skipping these is right -- calling a data address is worse -- but skipping
 * them *silently* is not. They almost always arrive inside a loop, so the
 * symptom is a hang with no output rather than a diagnosable null vtable call.
 *
 * Rate-limited per address: a spin can produce millions of these, and the
 * useful information is which addresses occur, not how often.
 */
void recomp_icall_not_code_log(uint32_t va)
{
    enum { SLOTS = 16 };
    static uint32_t seen[SLOTS];
    static uint64_t hits[SLOTS];
    static int count;
    int i;

    for (i = 0; i < count; i++)
        if (seen[i] == va)
            break;
    if (i == count) {
        if (count == SLOTS)
            return;
        seen[count] = va;
        hits[count] = 0;
        count++;
    }
    hits[i]++;
    /* Report at 1, 10, 100, 1000 ... rather than once. A single line says a
     * wild pointer was skipped; the progression says it is being skipped in a
     * loop, which is the difference between a curiosity and the reason the
     * title is hung. */
    {
        uint64_t n = hits[i];
        while (n >= 10 && n % 10 == 0)
            n /= 10;
        if (n != 1)
            return;
    }
    fprintf(stderr, "[ICALL] target 0x%08X is not code -- skipped %llu time(s) "
                    "(null or wild function pointer, at call #%llu)\n",
            va, (unsigned long long)hits[i],
            (unsigned long long)g_icall_count);
    fflush(stderr);
}


/* ── The completion list at 0x008353FC, one thread at a time ─────────────── */

/*
 * Six functions share a doubly linked list whose head is at 0x008353FC: three
 * insert (0x2AE320, 0x2AE800, 0x2BF2C0, from the main thread), 0x2CD630 drains
 * it from another guest thread through an engine object's callback, and
 * 0x8D390 / 0x2BEF60 reset it. None of them locks: on the Xbox's single core
 * the few instructions of an insert are rarely interrupted. Here the threads
 * run truly in parallel, and every few minutes of fighting the drain walked a
 * half-linked node (fault in sub_002CD580, esi = 0x7501E283). One recursive
 * lock around all six restores the exclusion the single core gave.
 */
static CRITICAL_SECTION s_list_cs;
static INIT_ONCE s_list_once = INIT_ONCE_STATIC_INIT;
static BOOL CALLBACK list_cs_init(PINIT_ONCE o, PVOID p, PVOID *c)
{
    (void)o; (void)p; (void)c;
    InitializeCriticalSectionAndSpinCount(&s_list_cs, 4000);
    return TRUE;
}

extern void sub_0008D390_gen(void);
void sub_0008D390(void)
{
    InitOnceExecuteOnce(&s_list_once, list_cs_init, NULL, NULL);
    EnterCriticalSection(&s_list_cs);
    sub_0008D390_gen();
    LeaveCriticalSection(&s_list_cs);
}

extern void sub_002AE320_gen(void);
void sub_002AE320(void)
{
    InitOnceExecuteOnce(&s_list_once, list_cs_init, NULL, NULL);
    EnterCriticalSection(&s_list_cs);
    sub_002AE320_gen();
    LeaveCriticalSection(&s_list_cs);
}

extern void sub_002AE800_gen(void);
void sub_002AE800(void)
{
    InitOnceExecuteOnce(&s_list_once, list_cs_init, NULL, NULL);
    EnterCriticalSection(&s_list_cs);
    sub_002AE800_gen();
    LeaveCriticalSection(&s_list_cs);
}

extern void sub_002BEF60_gen(void);
void sub_002BEF60(void)
{
    InitOnceExecuteOnce(&s_list_once, list_cs_init, NULL, NULL);
    EnterCriticalSection(&s_list_cs);
    sub_002BEF60_gen();
    LeaveCriticalSection(&s_list_cs);
}

extern void sub_002BF2C0_gen(void);
void sub_002BF2C0(void)
{
    InitOnceExecuteOnce(&s_list_once, list_cs_init, NULL, NULL);
    EnterCriticalSection(&s_list_cs);
    sub_002BF2C0_gen();
    LeaveCriticalSection(&s_list_cs);
}

extern void sub_002CD630_gen(void);
void sub_002CD630(void)
{
    InitOnceExecuteOnce(&s_list_once, list_cs_init, NULL, NULL);
    EnterCriticalSection(&s_list_cs);
    sub_002CD630_gen();
    LeaveCriticalSection(&s_list_cs);
}

/* ── Widescreen camera (video setup 0x002695F0) ─────────────── */

/*
 * MKA has its own widescreen mode: with the dashboard's widescreen flag the
 * video setup writes the camera aspect 0x54CA88 = 16/9 and the horizontal
 * FOV 0x54CA8C = 88.18 degrees (72 at 4:3: the same vertical view, more of it
 * at the sides). It also switches the back buffer to 720x480, which the GPU
 * renderer does not upscale -- only 640x480 is drawn at the player's
 * resolution. So the flags stay 4:3 and, when video.ini asks for a wider
 * aspect, the camera gets the game's own widescreen values for it, while the
 * renderer draws the 640x480 buffer at that aspect.
 */
unsigned pc_video_aspect(void);
void pc_video_aspect_ratio(unsigned aspect, unsigned *num, unsigned *den);

static int unlock_all_on(void);
static DWORD WINAPI cs_pointer_thread(LPVOID unused);
static int mm_pointer_tick(ULONGLONG now);
void kgpu_set_hud_fit(float scale);     /* the runtime's GPU backend (kelvin_gpu.h) */
void kgpu_set_hud_keep(int keep);
static float s_hud_fit = 1.0f;          /* how much narrower the 2D is drawn (1: stretched as the game had it) */

/* RECOMP_MEMDUMP="ms,ms,..." (from the first video setup) with RECOMP_MEMDUMP_DIR: the title's memory below 64 MB
 * written to <dir>\mem_<ms>.bin at those times, to find a variable by what changes between two of them. */
static DWORD WINAPI memdump_thread(LPVOID unused)
{
    const char *times = getenv("RECOMP_MEMDUMP"), *dir = getenv("RECOMP_MEMDUMP_DIR"), *p = times;
    const ULONGLONG t0 = GetTickCount64();
    (void)unused;
    while (p && *p) {
        const unsigned ms = (unsigned)strtoul(p, (char **)&p, 10);
        char path[512];
        FILE *f;
        while (GetTickCount64() - t0 < ms) Sleep(5);
        /* "ms:address=value" writes a dword instead (RECOMP_MEMDUMP_DIR then unused) */
        if (*p == ':') {
            const uint32_t addr = (uint32_t)strtoul(p + 1, (char **)&p, 16);
            const uint32_t value = *p == '=' ? (uint32_t)strtol(p + 1, (char **)&p, 0) : 0;
            GUEST32(addr) = value;
            fprintf(stderr, "[MKA] memory %08X set to %d\n", addr, (int)value);
            while (*p == ',') p++;
            continue;
        }
        snprintf(path, sizeof(path), "%s\\mem_%u.bin", dir, ms);
        if ((f = fopen(path, "wb")) != NULL) {
            fwrite(&GUEST8(0), 1, 0x04000000, f);
            fclose(f);
            fprintf(stderr, "[MKA] memory written to %s\n", path);
        }
        while (*p == ',') p++;
    }
    return 0;
}

/* MKA_CAMERA_TRACE: the camera table (0x5B74B8, 8 of 0xE4) every two seconds, view window and offset. */
static DWORD WINAPI camera_trace_thread(LPVOID unused)
{
    (void)unused;
    for (;;) {
        int i;
        Sleep(2000);
        for (i = 0; i < 8; i++) {
            const uint32_t cam = GUEST32(0x5B74B8 + i * 0xE4);
            float w[2], o[2];
            if (cam < 0x10000 || cam >= 0x04000000)
                continue;
            memcpy(w, &GUEST32(cam + 0x68), 8);
            memcpy(o, &GUEST32(cam + 0x78), 8);
            fprintf(stderr, "[MKA] camera table %d: %08X window %.4f %.4f offset %.4f %.4f\n", i, cam, w[0], w[1], o[0], o[1]);
        }
    }
}

extern void sub_002695F0_gen(void);
void sub_002695F0(void)
{
    unsigned num, den;
    unlock_all_on();                    /* at boot, so its keeper is there before any profile */
    {
        static int dumping;
        if (!dumping++ && getenv("RECOMP_MEMDUMP"))
            CloseHandle(CreateThread(NULL, 0, memdump_thread, NULL, 0, NULL));
        if (dumping == 1 && getenv("MKA_CAMERA_TRACE"))
            CloseHandle(CreateThread(NULL, 0, camera_trace_thread, NULL, 0, NULL));
    }
    {
        static int pointing;
        if (!pointing++)
            CloseHandle(CreateThread(NULL, 0, cs_pointer_thread, NULL, 0, NULL));
    }
    sub_002695F0_gen();
    pc_video_aspect_ratio(pc_video_aspect(), &num, &den);
    if (num && den && num * 3u != den * 4u && GUEST32(0x54CA88) == 0x3FAAAAABu) {
        const float pi = 3.14159265f, aspect = (float)num / (float)den;
        float hfov = 2.0f * atanf(tanf(36.0f * pi / 180.0f) * aspect * 0.75f) * 180.0f / pi;
        memcpy(&GUEST32(0x54CA88), &aspect, 4);
        memcpy(&GUEST32(0x54CA8C), &hfov, 4);
        fprintf(stderr, "[MKA] widescreen camera: aspect %u:%u, horizontal FOV %.2f\n", num, den, hfov);
        /* The 2D stays stretched across the screen, as the player prefers it. MKA_HUD=fit (experimental) draws the HUD,
         * menus and loading screens at their own proportions instead (kgpu_set_hud_fit); the fighter select keeps
         * its stretched layout either way. */
        {
            const char *e = getenv("MKA_HUD");
            if (e && !_stricmp(e, "fit")) {
                s_hud_fit = (4.0f / 3.0f) / aspect;
                kgpu_set_hud_fit(s_hud_fit);
                fprintf(stderr, "[MKA] 2D at its own proportions (x%.3f)\n", s_hud_fit);
            }
        }
    }
}

/* ── Character select in widescreen ────────────────────────── */

/*
 * The fighter's 3D model stands at a place in the scene, the name and the
 * portrait grid are 2D. With the wider camera the 2D stretches to the screen's
 * edges and the model, shown with more of the scene at the sides, ends up
 * nearer the middle than its name. The shell camera (the first in the table at
 * 0x5B74B8, a RenderWare camera) gets a view offset (RwCameraSetViewOffset's
 * field, +0x78) for as long as the character select is up, which puts the
 * model back where the 4:3 screen had it, unstretched. Measured: an offset of
 * k view windows moves it by 0.625 k in screen units; at 4:3 it stood at
 * -0.736, so at aspect a it is short by 0.736 (1 - (4/3) / a).
 * MKA_CS_OFFSET overrides k.
 */
static uint32_t s_cs_camera;
static float s_cs_offset;
static volatile ULONGLONG s_cs_shifted;     /* when the offset went on */

/* A camera carrying the select's offset that is not the select's own: the fight's cameras are made as copies of
 * the shell's (RenderWare's clone takes the view offset along) before the select is left. Put straight. */
static void cs_camera_clear(uint32_t cam)
{
    const float zero = 0.0f;
    if (s_cs_offset != 0.0f && cam >= 0x10000 && cam < 0x04000000 &&
        GUEST32(cam + 0x78) == *(uint32_t *)&s_cs_offset) {
        static int said;
        memcpy(&GUEST32(cam + 0x78), &zero, 4);
        if (said++ < 8) fprintf(stderr, "[MKA] camera %08X: character-select offset taken off\n", cam);
    }
}

static void cs_camera_shift(int on)
{
    uint32_t cam = GUEST32(0x5B74B8);
    float off = 0.0f, win;
    if (!on) {
        int i;
        if (s_cs_camera) cs_camera_clear(s_cs_camera);
        for (i = 0; i < 8; i++) cs_camera_clear(GUEST32(0x5B74B8 + i * 0xE4));
        s_cs_camera = 0;
        return;
    }
    if (cam < 0x10000 || cam >= 0x04000000)
        return;
    {
        const char *e = getenv("MKA_CS_OFFSET");
        unsigned num, den;
        float k = 0.0f;
        pc_video_aspect_ratio(pc_video_aspect(), &num, &den);
        if (e && *e)
            k = (float)atof(e);
        else if (num && den && num * 3u > den * 4u)
            k = -0.736f * (1.0f - (4.0f / 3.0f) * (float)den / (float)num) / 0.625f;
        if (k == 0.0f)
            return;
        memcpy(&win, &GUEST32(cam + 0x68), 4);
        off = k * win;
    }
    memcpy(&GUEST32(cam + 0x78), &off, 4);
    s_cs_camera = cam;
    s_cs_offset = off;
    s_cs_shifted = GetTickCount64();
}

/* Every camera's view set up (0x5A400: camera, viewport, field of view, aspect) passes here: any but the select's
 * own loses an offset it got from it. */
extern void sub_0005A400_gen(void);
void sub_0005A400(void)
{
    const uint32_t cam = GUEST32(g_esp + 4);
    sub_0005A400_gen();
    if (cam != s_cs_camera)
        cs_camera_clear(cam);
    if (getenv("MKA_CAMERA_TRACE") && cam >= 0x10000 && cam < 0x04000000) {
        static uint32_t told[16][3];
        static int n;
        const uint32_t ox = GUEST32(cam + 0x78), oy = GUEST32(cam + 0x7C), wx = GUEST32(cam + 0x68);
        int i;
        for (i = 0; i < n && !(told[i][0] == cam && told[i][1] == ox && told[i][2] == wx); i++) {}
        if (i == n && n < 16) {
            float fx, fy, fw;
            memcpy(&fx, &ox, 4); memcpy(&fy, &oy, 4); memcpy(&fw, &wx, 4);
            told[n][0] = cam; told[n][1] = ox; told[n][2] = wx; n++;
            fprintf(stderr, "[MKA] camera %08X from %08X: view window %.4f, offset %.4f %.4f\n",
                    cam, GUEST32(g_esp), fw, fx, fy);
        }
    }
}

/* ── The mouse pointer on the character select ───────────────────── */

/*
 * The fighter grid's cursor is an object of its own (vtable 0x55DD00) that 0x1DE410 makes the current one at
 * [0x5BB3FC], with its handle (+4) at [0x5BB400]: the cell's index at +0x10, its row +0x14, its column +0x18 and
 * the fighter +0x1C. On the 640x480 screen the upper block is 10 columns of 54.1 from x 50 by 4 rows of 52.25
 * from y 31, the lower one 6 columns of 54 from x 158 (under the upper columns 2..7) by 4 rows of 53 from y 240.
 * The free pointer puts the cursor at once on the cell under it: quietly on its neighbour, then one D-pad step of
 * the game's into it, so the fighter, the sound and the model loading stay the game's. A locked cell is skipped
 * by that step; then the cursor goes back the same way to the cell it came from, and waits there for the
 * pointer to go to another cell. The lower block's columns are counted as the upper one's (2..7).
 * MKA_CS_POINTER=0 turns it off.
 */
int pc_input_pointer(int *x, int *y, uint64_t *moved_ms);
void pc_input_pointer_claim(int ms);
void pc_input_pointer_step(int dx, int dy);

static volatile int s_cs_select;                    /* the character select is up (not the ladder after it) */
static volatile ULONGLONG s_cs_since;               /* when it came up */

static int cs_cursor(int *row, int *col)
{
    const uint32_t p = GUEST32(0x5BB3FC);
    if (p < 0x10000 || p >= 0x04000000 - 0x20 || GUEST32(p) != 0x55DD00 || GUEST32(p + 4) != GUEST32(0x5BB400))
        return 0;
    *row = (int)GUEST32(p + 0x14);
    *col = (int)GUEST32(p + 0x18);
    return *row >= 0 && *row < 8 && *col >= 0 && *col < 10;
}

static int cs_cell_exists(int row, int col)
{
    return row >= 0 && row < 8 && (row < 4 ? col >= 0 && col < 10 : col >= 2 && col < 8);
}

/* The cursor set on a cell without the game knowing: row, column and the index (10 a row above, 6 below). The
 * fighter (+0x1C) stays the one it had, so only a step from here makes the game take the new cell up. */
static void cs_cursor_put(int row, int col)
{
    const uint32_t p = GUEST32(0x5BB3FC);
    GUEST32(p + 0x14) = (uint32_t)row;
    GUEST32(p + 0x18) = (uint32_t)col;
    GUEST32(p + 0x10) = (uint32_t)(row < 4 ? row * 10 + col : 40 + (row - 4) * 6 + (col - 2));
}

/* The cell under the 640x480 point (x, y), its column counted as the upper block's. */
static int cs_cell_at(double x, double y, int *row, int *col)
{
    if (y >= 31.0 && y < 240.0 && x >= 50.0 && x < 591.0) {
        *row = (int)((y - 31.0) / 52.25);
        *col = (int)((x - 50.0) / 54.1);
    } else if (y >= 240.0 && y < 452.0 && x >= 158.0 && x < 482.0) {
        *row = 4 + (int)((y - 240.0) / 53.0);
        *col = 2 + (int)((x - 158.0) / 54.0);
    } else {
        return 0;
    }
    if (*row > 7) *row = 7;
    if (*col > 9) *col = 9;
    return 1;
}

/*
 * The shell camera is the fight's camera too, and the screens after the select (the ladder's, the one loading
 * the fight) come through 0x237050 from the select's call site as well, so the offset went on again there and
 * stayed for the whole fight: the fighters stood left of the middle. It belongs to the fighter grid, so it comes
 * off once the grid has gone for 300 ms (or never came, 3 s after the offset went on).
 */
static void cs_offset_watch(ULONGLONG now)
{
    static ULONGLONG grid_at;
    static int keep = -1;
    int row, col;
    const ULONGLONG since = s_cs_shifted;
    const int grid = cs_cursor(&row, &col);
    /* the fighter grid's portraits and its lines come by different ways: the select's 2D stays as the game drew it */
    if (grid != keep) { keep = grid; kgpu_set_hud_keep(grid); }
    if (grid) {
        grid_at = now;
        return;
    }
    if (!s_cs_camera || !since || now - since < 300)
        return;
    if (grid_at > since ? now - grid_at > 300 : now - since > 3000) {
        const uint32_t cam = s_cs_camera;
        s_cs_shifted = 0;
        cs_camera_clear(cam);
    }
}

static DWORD WINAPI cs_pointer_thread(LPVOID unused)
{
    const char *trace = getenv("MKA_CS_POINTER_TRACE"), *off = getenv("MKA_CS_POINTER");
    const int pointing = !(off && *off == '0');
    ULONGLONG seen = 0, step_at = 0;
    int want_row = -1, want_col = -1, from_row = 0, from_col = 0, near_row = 0, near_col = 0, step_dx = 0;
    int pending = 0, tries = 0, returning = 0;
    int last_row = -1, last_col = -1, under_row = -1, under_col = -1, said = 0;
    (void)unused;
    for (;;) {
        int x, y, row, col;
        ULONGLONG moved, now;
        Sleep(10);
        now = GetTickCount64();
        cs_offset_watch(now);
        if (pointing && !s_cs_select && mm_pointer_tick(now))
            continue;
        if (!pointing || !s_cs_select || !cs_cursor(&row, &col) || !pc_input_pointer(&x, &y, &moved)) {
            want_row = under_row = -1;
            pending = 0;
            continue;
        }
        pc_input_pointer_claim(250);
        if (trace && (row != last_row || col != last_col) && said++ < 60)
            fprintf(stderr, "[MKA] select cursor: row %d column %d (cell %d) at %u ms\n", row, col,
                    (int)GUEST32(GUEST32(0x5BB3FC) + 0x10), (unsigned)(now % 1000000));
        last_row = row;
        last_col = col;
        if (moved != seen && moved > s_cs_since + 300) {
            int r, c;
            seen = moved;
            /* the pointer over the picture, in the grid's 640 x 480 (the select's 2D is left stretched) */
            if (!cs_cell_at(x * 640.0 / 65536.0, y * 480.0 / 65536.0, &r, &c))
                r = c = -1;
            if (r != under_row || c != under_col) {
                under_row = r;
                under_col = c;
            } else {
                r = -1;                     /* still the same cell: one try for it, not one per twitch */
            }
            if (r >= 0) {
                if (trace) fprintf(stderr, "[MKA] select pointer: row %d column %d\n", r, c);
                want_row = r;
                want_col = c;
                pending = 0;
                tries = 0;
                returning = 0;
            }
        }
        if (want_row < 0)
            continue;
        if (pending) {
            if (row == near_row && col == near_col) {
                if (now - step_at < 250)
                    continue;
                if (++tries < 3) {          /* the step was not taken (a model still loading): again */
                    pc_input_pointer_step(step_dx, 0);
                    step_at = now;
                    continue;
                }
                cs_cursor_put(from_row, from_col);   /* nothing moved: as it was, fighter and all */
                pending = 0;
                want_row = -1;
                continue;
            }
            pending = 0;
            if (row != want_row || col != want_col) {
                /* it skipped a locked cell: back to the cell it came from, once */
                if (returning || (row == from_row && col == from_col)) {
                    want_row = -1;
                } else {
                    want_row = from_row;
                    want_col = from_col;
                    returning = 1;
                }
                continue;
            }
        }
        if (row == want_row && col == want_col) {
            want_row = -1;
            continue;
        }
        if (now - step_at < 60)
            continue;
        /* the neighbour it steps in from: the cell to the left, or to the right where there is none */
        near_row = want_row;
        near_col = cs_cell_exists(want_row, want_col - 1) ? want_col - 1 : want_col + 1;
        step_dx = near_col < want_col ? 1 : -1;
        from_row = row;
        from_col = col;
        cs_cursor_put(near_row, near_col);
        pc_input_pointer_step(step_dx, 0);
        step_at = now;
        tries = 0;
        pending = 1;
    }
}

/* ── The mouse pointer on the main menu ──────────────────────────── */

/*
 * The menus are scripts: each entry is an object (vtable 0x51552C, "OBJ " at +4) whose +0x18 is its script's
 * address, the same in every run, and the main menu's script keeps the selected entry as the first local of its
 * fiber's frame, right after the frame's header (0x5BB378, 0, the next frame, 0x1FF0). That frame is on the heap,
 * so it is looked for (16 MB from 0x01000000, while the pointer moves and it is not known) and then only checked.
 * On the 640x480 screen the entries are rows of 27 from y 126.5, x 40..260; the pointer over one walks the
 * selection there with the game's own D-pad steps, one at a time.
 */
static const uint32_t k_mm_entries[9] = {
    0x80064B44, 0x800650A4, 0x800656EC, 0x80065CEC,     /* KOMBAT, KREATE A FIGHTER, KONQUEST, MOTOR KOMBAT */
    0x80066298, 0x80066930, 0x80066F30, 0x80067490,     /* XBOX LIVE, THE KRYPT, EXTRAS, PROFILES */
    0x800679F0,                                         /* OPTIONS */
};
static uint32_t s_mm_local;                         /* where the selected entry is kept, once found */

static int mm_entry(uint32_t node)
{
    int i;
    if (node < 0x10000 || node >= 0x04000000 - 0x20 || GUEST32(node) != 0x51552C || GUEST32(node + 4) != 0x4F424A20)
        return -1;
    for (i = 0; i < 9; i++)
        if (GUEST32(node + 0x18) == k_mm_entries[i]) return i;
    return -1;
}

static int mm_selected(int search)
{
    uint32_t a;
    if (s_mm_local) {
        const int i = mm_entry(GUEST32(s_mm_local));
        if (i >= 0 && GUEST32(s_mm_local - 0x10) == 0x5BB378) return i;
        s_mm_local = 0;
    }
    if (!search)
        return -1;
    if (getenv("MKA_MM_TRACE")) fprintf(stderr, "[MKA] main menu: looking for its frame\n");
    for (a = 0x01000010; a < 0x02000000; a += 4) {
        if (GUEST32(a - 4) != 0x1FF0 || GUEST32(a - 0x10) != 0x5BB378)
            continue;
        if (mm_entry(GUEST32(a)) >= 0) {
            s_mm_local = a;
            if (getenv("MKA_MM_TRACE")) fprintf(stderr, "[MKA] main menu: selection kept at %08X\n", a);
            return mm_entry(GUEST32(a));
        }
    }
    return -1;
}

/* One look at the main menu (from the pointer thread): whether it is up and the pointer is its own there. */
static int mm_pointer_tick(ULONGLONG now)
{
    static ULONGLONG seen, step_at, search_at, moved_at;
    static int want = -1, under = -1, from = -1, tries, steps;
    int x, y, sel;
    uint64_t moved;
    if (!pc_input_pointer(&x, &y, &moved)) { want = under = -1; return 0; }
    if (getenv("MKA_MM_TRACE") && moved != seen) {
        static ULONGLONG said;
        if (now - said > 1000) { said = now; fprintf(stderr, "[MKA] main menu: pointer moved (%d, %d)\n", x, y); }
    }
    if (moved != seen) moved_at = now;
    /* look for the frame only while the pointer is being used, and at most twice a second */
    sel = mm_selected(now - moved_at < 1000 && now - search_at >= 500 && (search_at = now, 1));
    if (sel < 0) { want = under = -1; return 0; }
    pc_input_pointer_claim(250);                /* the selection follows the pointer, not its steps */
    if (moved != seen) {
        const double fx = x * 640.0 / 65536.0, fy = y * 480.0 / 65536.0;
        int r = -1;
        seen = moved;
        if (fx >= 40.0 && fx < 260.0 && fy >= 126.5 && fy < 126.5 + 9 * 27.0) r = (int)((fy - 126.5) / 27.0);
        if (getenv("MKA_MM_TRACE")) fprintf(stderr, "[MKA] main menu: pointer %.0f,%.0f row %d (on %d)\n", fx, fy, r, sel);
        if (r != under) {
            under = r;
            if (r >= 0) { want = r; from = -1; tries = steps = 0; }
        }
    }
    if (want < 0)
        return 1;
    if (sel == want) {
        want = -1;
        return 1;
    }
    /* one step at a time: the next once the game has taken the last (or, 400 ms on, the same one again) */
    if (from == sel && now - step_at < 400)
        return 1;
    if (now - step_at < 120)
        return 1;
    if (from == sel ? ++tries > 3 : ++steps > 16) {    /* the steps are not taken, or it never settles */
        want = -1;
        return 1;
    }
    if (from != sel) tries = 0;
    from = sel;
    pc_input_pointer_step(0, want > sel ? 1 : -1);
    step_at = now;
    return 1;
}

/* ── Video settings from the game's Options page ───────────────────── */

/*
 * The Options menu's VIDEO page (contrast, gamma, brightness; made by 0x1A0360) runs 0x1A04C0 every
 * frame it is up. Telling the runtime so offers its video settings page there
 * ("[V] / [RB] CONFIGURAÇÕES DE VÍDEO" at the bottom of the screen): internal
 * resolution, window, anti-aliasing, aspect, V-Sync... saved to video.ini.
 */
void pc_video_menu_page_seen(void);
extern void sub_001A04C0_gen(void);
void sub_001A04C0(void)
{
    pc_video_menu_page_seen();
    sub_001A04C0_gen();
}

/* ── Everything unlocked (RECOMP_UNLOCK_ALL=1) ───────────────── */

/*
 * The Krypt's 280 items are bits in each player's profile (player one's at
 * [0x5B8908], two's at [0x5B890C]; the bits at +0x4C), and buying one
 * (0x193EA0) sets its bit and hands what it unlocks -- kind 1 a fighter, 2 an
 * arena or a picture, 3 a costume -- to 0x1FC820 with the profile's other half
 * eight bytes before it, as the Konquest rewards do (0x97910). The kinds come
 * from the item table, which the game loads only for the Krypt but which is
 * the first 280 x 32 bytes of art/kryptdata_en.bin (kind at +0x14, which at
 * +0x18). With RECOMP_UNLOCK_ALL every item is bought, in memory, for both
 * players, whenever the character select or the Krypt comes up with a profile
 * still missing some. The launcher runs that mode on a copy of the saves, so
 * nothing of it reaches the player's own profile.
 */
extern void sub_001FC820(void);

/* The masks again and again: a profile loaded from the saves (when one signs in, when a mode starts) brings its own. */
static void unlock_all_masks(void)
{
    int p;
    /* The fighters the character select offers (0x208D90): one 64-bit mask a side at 0x7363C8 and 0x7363D0,
     * which the profiles' Konquest progress fills -- Blaze, Daegon, Taven and Meat come only from there. */
    memset(&GUEST8(0x7363C8), 0xFF, 16);
    for (p = 0; p < 2; p++) {
        const uint32_t profile = GUEST32(p ? 0x5B890C : 0x5B8908);
        if (profile < 0x10000 || profile >= 0x04000000)
            continue;
        memset(&GUEST8(profile + 0x4C), 0xFF, 280 / 8);
        memset(&GUEST8(profile - 8 + 0x100), 0xFF, 0x17);
    }
}

static DWORD WINAPI unlock_all_keeper(LPVOID unused)
{
    (void)unused;
    for (;;) {
        unlock_all_masks();
        Sleep(200);
    }
}

static int unlock_all_on(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("RECOMP_UNLOCK_ALL");
        on = e && *e == '1';
        if (on) {
            fprintf(stderr, "[MKA] everything unlocked for this session (RECOMP_UNLOCK_ALL)\n");
            CloseHandle(CreateThread(NULL, 0, unlock_all_keeper, NULL, 0, NULL));
        }
    }
    return on;
}

static void unlock_all_apply(void)
{
    static uint32_t items[280][2];      /* kind, which */
    static int loaded = -1;
    int p;
    if (!unlock_all_on())
        return;
    if (loaded < 0) {
        FILE *f = fopen("game/art/kryptdata_en.bin", "rb");
        uint8_t e[32];
        int i;
        loaded = 0;
        for (i = 0; f && i < 280 && fread(e, 32, 1, f) == 1; i++) {
            memcpy(&items[i][0], e + 0x14, 4);
            memcpy(&items[i][1], e + 0x18, 4);
        }
        if (f) fclose(f);
        loaded = i == 280;
        if (!loaded) fprintf(stderr, "[MKA] unlock: game/art/kryptdata_en.bin could not be read\n");
    }
    for (p = 0; p < 2; p++) {
        const uint32_t profile = GUEST32(p ? 0x5B890C : 0x5B8908);
        uint32_t id, full = 1;
        if (profile < 0x10000 || profile >= 0x04000000)
            continue;
        for (id = 0; id < 280 / 8; id++) full &= GUEST8(profile + 0x4C + id) == 0xFF;
        for (id = 0; id < 0x20; id++) full &= GUEST8(profile - 8 + 0x100 + id) == (id >= 0x17 && id < 0x18 ? 0 : 0xFF);
        if (full)
            continue;
        memset(&GUEST8(profile + 0x4C), 0xFF, 280 / 8);
        /* What 0x1FC820 keeps, as 64-bit masks in the profile's other half: fighters (kind 1) at +0x100, kind 2 at
         * +0x108, kind 3 (56 of them) at +0x110, kind 4 at +0x118 -- all of them, Konquest's fighters too. */
        memset(&GUEST8(profile - 8 + 0x100), 0xFF, 0x20);
        GUEST8(profile - 8 + 0x117) = 0;
        if (loaded) {
            const uint32_t eax = g_eax, ecx = g_ecx;
            for (id = 0; id < 280; id++) {
                if (!items[id][0] || items[id][0] >= 0x14)
                    continue;
                g_esp -= 4; GUEST32(g_esp) = items[id][1];
                g_esp -= 4; GUEST32(g_esp) = items[id][0];
                g_esp -= 4; GUEST32(g_esp) = profile - 8;
                g_esp -= 4; GUEST32(g_esp) = 0;     /* the return address, popped by the callee */
                sub_001FC820();
                g_esp += 12;                        /* cdecl: the caller takes the arguments back */
            }
            g_eax = eax; g_ecx = ecx;
        }
        fprintf(stderr, "[MKA] all 280 Krypt items unlocked for player %d (profile %08X)\n", p + 1, profile);
    }
}

extern void sub_00193210_gen(void);
void sub_00193210(void)
{
    unlock_all_apply();
    sub_00193210_gen();
}

/* ── Character-select preview targets (0x5BE384, 0x5BE38C) ─────────────────── */

/*
 * The character select makes two render targets at 0x5BE384 / 0x5BE38C
 * (0x237050) and the screens after it draw with them (0x238D70) without
 * looking whether they were made. They were not once the contiguous memory
 * ran out (see xbox_ContiguousAlloc), and the null object took the game down
 * in sub_002CD580. Said in the log should it happen again; the select's camera
 * shift (above) starts and ends with these.
 */
extern void sub_00237050_gen(void);
void sub_00237050(void)
{
    const uint32_t from = GUEST32(g_esp);
    unlock_all_apply();
    sub_00237050_gen();
    if (!GUEST32(0x5BE384))
        fprintf(stderr, "[MKA] character-select render targets from %08X could not be made\n", from);
    /* the select's, or another screen's (the ladder's) that comes after it */
    cs_camera_shift(from == 0x001F3D02);
    s_cs_since = GetTickCount64();
    s_cs_select = from == 0x001F3D02;
}

extern void sub_002377B0_gen(void);
void sub_002377B0(void)
{
    s_cs_select = 0;
    cs_camera_shift(0);
    sub_002377B0_gen();
}

extern void sub_00238D70_gen(void);
void sub_00238D70(void)
{
    if (!GUEST32(0x5BE384))
        fprintf(stderr, "[MKA] drawn from %08X with no character-select render target\n", GUEST32(g_esp));
    sub_00238D70_gen();
}

/* Texture creation failing, and the pools behind it, said in the log. */
extern void sub_002B9E50_gen(void);
void sub_002B9E50(void)
{
    const uint32_t from = GUEST32(g_esp), a1 = GUEST32(g_esp + 4), a2 = GUEST32(g_esp + 8), a3 = GUEST32(g_esp + 12),
                   a4 = GUEST32(g_esp + 16);
    sub_002B9E50_gen();
    if (!g_eax)
        fprintf(stderr, "[MKA] texture object %u x %u (%08X, %08X) from %08X failed; create callback %08X, pool alloc %08X\n",
                a1, a2, a3, a4, from, GUEST32(0x835398), GUEST32(0x835484));
}

extern void sub_002BF950_gen(void);
void sub_002BF950(void)
{
    const uint32_t from = GUEST32(g_esp), a1 = GUEST32(g_esp + 4), a2 = GUEST32(g_esp + 8);
    sub_002BF950_gen();
    if (!g_eax) fprintf(stderr, "[MKA] pool alloc (2BF950) %08X %08X from %08X failed\n", a1, a2, from);
}

extern void sub_002C4020_gen(void);
void sub_002C4020(void)
{
    const uint32_t from = GUEST32(g_esp), a1 = GUEST32(g_esp + 4), a2 = GUEST32(g_esp + 8);
    sub_002C4020_gen();
    if (!g_eax) fprintf(stderr, "[MKA] pool alloc (2C4020) %08X %08X from %08X failed\n", a1, a2, from);
}
