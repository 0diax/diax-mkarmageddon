/**
 * Mortal Kombat: Armageddon - Midway "process" coroutines on host fibers
 *
 * MKA runs most of its game logic as cooperative processes, the arcade-era
 * Midway model: a process runs until it sleeps, possibly deep inside nested
 * calls, and the next resume continues it right there. The originals do it by
 * swapping esp (0x001C5010..0x001C568A). Recompiled code cannot: every guest
 * call is a host C call, so the frames a sleeping process leaves behind live on
 * the host stack, and every switch ends in `jmp [saved address]` -- an indirect
 * jump to a return address, which no dispatch table can resolve.
 *
 * So each process that can sleep runs on a host fiber. Its host frames stay on
 * the fiber's stack while it sleeps; its guest stack is handled exactly as the
 * original does. The scheduler's `call [vtbl+0x10]` (resume) and the process's
 * `call [vtbl+0x14]` (sleep) both look like ordinary calls that return.
 *
 * Process object (guest):
 *   +0x00 vtable          +0x04 alive        +0x0C float wake time
 *   +0x10..+0x1C saved ebp, ebx, esi, edi    +0x2C callback
 *   +0x30 function        +0x34 resume address
 *   +0x40 stack/buffer top                   +0x44 saved stack pointer
 *
 * Three kinds, by vtable (0x0055DB5C / 84 / AC / D4):
 *   A  no stack of its own (resume 0x1C50A0, jump 0x1C5180; sleep is a trap)
 *   B  runs on the scheduler's stack, copied out to a 0x200 buffer on sleep
 *      (resume 0x1C51C0, sleep 0x1C5340, jump 0x1C5430)
 *   C  own 16 KB stack (resume 0x1C5470, sleep 0x1C55A0, jump 0x1C5650, and
 *      0x1C5610/0x1C5630 to borrow the scheduler's stack and give it back)
 * plus 0x1C5010, which a process calls to end itself (118 call sites).
 *
 * "jump" replaces the running process's function and unwinds to the loop that
 * calls it, as if the old function had returned a float; here that is a
 * longjmp to the loop.
 */

#include <windows.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include <xbox/xboxrecomp.h>

typedef void (*recomp_func_t)(void);
extern recomp_func_t recomp_lookup(uint32_t xbox_va);
extern recomp_func_t recomp_lookup_manual(uint32_t xbox_va);

extern RECOMP_TLS uint32_t g_eax, g_ecx, g_edx, g_esp;
extern RECOMP_TLS uint32_t g_ebx, g_esi, g_edi, g_ebp, g_seh_ebp;
extern RECOMP_TLS double g_fp_stack[8];
extern RECOMP_TLS int g_fp_top;
extern ptrdiff_t g_xbox_mem_offset;

#define G32(va) (*(volatile uint32_t *)((uintptr_t)g_xbox_mem_offset + (uint32_t)(va)))
#define GF(va)  (*(volatile float *)((uintptr_t)g_xbox_mem_offset + (uint32_t)(va)))

/* Guest globals of the process system. */
#define CUR_PROC     0x007350C0u  /* the running process */
#define SLEEP_TIME   0x007350C4u  /* what sleep stores as the wake time */
#define PROC_B4      0x007350B4u
#define SCHED_RET    0x005B8B40u  /* scheduler: return address of resume */
#define SCHED_ESP    0x005B8B50u  /* scheduler: esp after that pop */
#define SCHED_EBP    0x005B8B60u
#define MAIN_SAVE    0x005B8B54u  /* kind C: process esp while on the main stack */
#define JUMP_ARG     0x005B8B38u  /* jump: the float the old function "returns" */
#define RET_SCRATCH  0x005B8B3Cu
#define SENTINEL     0x004D7C38u  /* wake time meaning "call the function again" */

/* Return addresses the original loops push when calling the function. */
#define RET_A 0x001C50BAu
#define RET_B 0x001C5291u
#define RET_C 0x001C54C2u

enum { KIND_A, KIND_B, KIND_C };

typedef struct ProcCtx {
    uint32_t proc;
    int kind;
    LPVOID fiber;            /* B and C */
    LPVOID return_fiber;     /* whoever resumed it last */
    int done;
    int in_run;              /* loop_jb belongs to a live run_process frame */
    jmp_buf loop_jb;         /* jump: back to the function loop */
    jmp_buf die_jb;          /* A only: 0x1C5010 back to resume */
    uint32_t resume_addr;    /* [proc+0x34] when it slept, to spot redirects */
    struct ProcCtx *prev;    /* t_running before this run began */
    struct ProcCtx *next;
} ProcCtx;

static __declspec(thread) ProcCtx *t_running;

/* Runtime: guest longjmp across fibers (xbox_memory_layout.c). */
extern void recomp_fiber_note(void);
extern void recomp_fiber_resumed(void);
extern void recomp_set_fiber_abandon_hook(void (*hook)(void *fiber));

#define CTX_BUCKETS 256
static ProcCtx *s_ctx[CTX_BUCKETS];
static unsigned s_live_fibers, s_peak_fibers;

static unsigned ctx_hash(uint32_t proc) { return (proc >> 4) & (CTX_BUCKETS - 1); }

static ProcCtx *ctx_find(uint32_t proc)
{
    ProcCtx *c;
    for (c = s_ctx[ctx_hash(proc)]; c; c = c->next)
        if (c->proc == proc)
            return c;
    return NULL;
}

static void ctx_remove(ProcCtx *ctx)
{
    ProcCtx **pp = &s_ctx[ctx_hash(ctx->proc)];
    while (*pp && *pp != ctx)
        pp = &(*pp)->next;
    if (*pp)
        *pp = ctx->next;
    if (ctx->fiber) {
        DeleteFiber(ctx->fiber);
        s_live_fibers--;
    }
    HeapFree(GetProcessHeap(), 0, ctx);
}

static ProcCtx *ctx_by_fiber(LPVOID fiber)
{
    unsigned b;
    ProcCtx *c;
    for (b = 0; b < CTX_BUCKETS; b++)
        for (c = s_ctx[b]; c; c = c->next)
            if (c->fiber == fiber)
                return c;
    return NULL;
}

/* A guest longjmp left this fiber for another one (the main loop restarting,
 * say). Its frames are dead, and so are those of every fiber that resumed it
 * on the way down: mark them finished so the resumes that are still waiting
 * delete them instead of switching back in. */
static void fiber_abandoned(void *fiber)
{
    ProcCtx *c = ctx_by_fiber(fiber);
    while (c && !c->done) {
        c->done = 1;
        c = ctx_by_fiber(c->return_fiber);
    }
}

/* The running process, as far as this stack can still see it. A guest
 * longjmp can leave t_running naming a kind-A run whose frame (and ProcCtx,
 * which lives in it) is gone, or a fiber run that was abandoned; drop those. */
static ProcCtx *current_ctx(void)
{
    ULONG_PTR lo = 0, hi = 0;
    uintptr_t sp = (uintptr_t)_AddressOfReturnAddress();
    LPVOID fiber = IsThreadAFiber() ? GetCurrentFiber() : NULL;

    GetCurrentThreadStackLimits(&lo, &hi);
    while (t_running) {
        ProcCtx *c = t_running;
        if (c->kind == KIND_A ? ((uintptr_t)c > sp && (uintptr_t)c < hi)
                              : (c->fiber == fiber && !c->done))
            break;
        t_running = c->prev;
    }
    return t_running;
}

/* A longjmp that only restores registers.
 *
 * MSVC's x64 longjmp unwinds frame by frame with RtlUnwind when the buffer's
 * Frame is set, and on a fiber that walk hit a frame it could not unwind and
 * raised STATUS_BAD_STACK (0xC0000028) the first time a process jumped. Every
 * frame skipped here is lifted guest code or this file -- no destructors, no
 * __finally -- so there is nothing for the unwind to run, and a zero Frame
 * makes longjmp a plain register restore. */
static void checked_longjmp(jmp_buf jb, const char *what, ProcCtx *ctx)
{
    ULONG_PTR lo = 0, hi = 0;
    uint64_t target = ((_JUMP_BUFFER *)jb)->Rsp;
    GetCurrentThreadStackLimits(&lo, &hi);
    if (target < lo || target >= hi)
        fprintf(stderr, "[MKPROC] %s of 0x%08X (kind %d): target rsp %llX outside stack %llX..%llX"
                " (fiber %p, process fiber %p)\n", what, ctx->proc, ctx->kind,
                (unsigned long long)target, (unsigned long long)lo, (unsigned long long)hi,
                GetCurrentFiber(), ctx->fiber);
    ((_JUMP_BUFFER *)jb)->Frame = 0;
    longjmp(jb, 1);
}

/* ── guest helpers ─────────────────────────────────────────── */

static void guest_call(uint32_t va, uint32_t ret)
{
    recomp_func_t fn = recomp_lookup_manual(va);
    if (!fn)
        fn = recomp_lookup(va);
    if (!fn) {
        fprintf(stderr, "[MKPROC] no code for guest function 0x%08X\n", va);
        return;
    }
    g_esp -= 4;
    G32(g_esp) = ret;
    fn();
}

static double fp_pop_value(void)
{
    double v = g_fp_stack[g_fp_top & 7];
    g_fp_top = (g_fp_top + 1) & 7;
    return v;
}

static void restore_scheduler_stack(void)
{
    g_esp = G32(SCHED_ESP);
    g_ebp = G32(SCHED_EBP);
    g_seh_ebp = g_ebp;
}

/* The process's callback, as each original routine calls it (ecx = proc). */
static void call_callback(void)
{
    uint32_t proc = G32(CUR_PROC);
    g_eax = g_ecx = g_edx = proc;
    guest_call(G32(proc + 0x2C), 0x001C5024u);
}

/* vtable +0x0C: the process system's own kill, cdecl (proc). */
static void kill_current(void)
{
    uint32_t proc;
    G32(PROC_B4) = 0;
    proc = G32(CUR_PROC);
    if (proc && G32(proc + 4)) {
        g_ecx = g_edx = proc;
        g_eax = G32(proc);
        g_esp -= 4;
        G32(g_esp) = proc;
        guest_call(G32(G32(proc) + 0x0C), 0x001C5085u);
        g_esp += 4;
    }
}

/* What every resume does once the function loop ends (0x1C50DD, 0x1C52C5,
 * 0x1C54F6): the callback, and a kill when the wake time says it is done. */
static void after_loop(int kind)
{
    uint32_t proc = G32(CUR_PROC);
    float t;

    if (G32(proc + 0x2C)) {
        call_callback();
        if (G32(G32(CUR_PROC) + 4) == 0)
            return;
    }
    t = GF(G32(CUR_PROC) + 0x0C);
    if (!(t < GF(SENTINEL)))
        return;
    if (kind == KIND_C)
        restore_scheduler_stack();
    kill_current();
}

/* The loop calling [proc+0x30] until its returned wake time is not the
 * sentinel, then after_loop. A jump lands back here with the new function
 * already stored -- including one made from after_loop's callback or kill,
 * which is why after_loop runs inside this frame: in the original that jump
 * re-enters the loop the same way, and a jmp_buf from a returned frame was
 * STATUS_BAD_STACK. */
static void run_process(ProcCtx *ctx)
{
    for (;;) {
        double t;
        uint32_t proc;

        ctx->in_run = 1;
        if (setjmp(ctx->loop_jb) == 0) {
            proc = G32(CUR_PROC);
            g_eax = proc;
            guest_call(G32(proc + 0x30),
                       ctx->kind == KIND_A ? RET_A
                       : ctx->kind == KIND_B ? RET_B : RET_C);
            t = fp_pop_value();
        } else {
            /* jump: the original unwinds to the slot below the loop's call
             * and returns, so esp ends at that stack's top. */
            proc = G32(CUR_PROC);
            g_esp = ctx->kind == KIND_C ? G32(proc + 0x40) : G32(SCHED_ESP);
            g_ebp = g_seh_ebp = G32(SCHED_EBP);
            t = (double)GF(JUMP_ARG);
        }

        proc = G32(CUR_PROC);
        GF(proc + 0x0C) = (float)t;
        if (ctx->kind != KIND_A)
            G32(proc + 0x44) = G32(proc + 0x40);
        if (GF(proc + 0x0C) == GF(SENTINEL))
            continue;
        after_loop(ctx->kind);
        ctx->in_run = 0;
        return;
    }
}

/* ── fibers ────────────────────────────────────────────────── */

static void finish_fiber(ProcCtx *ctx)
{
    ctx->done = 1;
    restore_scheduler_stack();
    SwitchToFiber(ctx->return_fiber);
    /* Never resumed: resume deletes a finished fiber. */
    fprintf(stderr, "[MKPROC] finished process 0x%08X was resumed\n", ctx->proc);
    for (;;)
        SwitchToFiber(ctx->return_fiber);
}

static VOID CALLBACK process_fiber_main(LPVOID param)
{
    ProcCtx *ctx = (ProcCtx *)param;

    /* A fresh start: B calls on the scheduler's stack, C on its own. */
    g_esp = ctx->kind == KIND_C ? G32(ctx->proc + 0x44) : G32(SCHED_ESP);
    run_process(ctx);
    finish_fiber(ctx);
}

static LPVOID scheduler_fiber(void)
{
    if (!IsThreadAFiber()) {
        LPVOID f = ConvertThreadToFiberEx(NULL, FIBER_FLAG_FLOAT_SWITCH);
        recomp_fiber_note();  /* its setjmps can now be reached from a process */
        recomp_set_fiber_abandon_hook(fiber_abandoned);
        return f;
    }
    return GetCurrentFiber();
}

/* Enter the scheduler side of a resume: pop the return address the way the
 * originals do and record the scheduler's stack. */
static void resume_prologue(void)
{
    G32(SCHED_RET) = G32(g_esp);
    g_esp += 4;
    G32(SCHED_ESP) = g_esp;
    G32(SCHED_EBP) = g_ebp;
}

static void resume_fiber(int kind)
{
    uint32_t proc = G32(CUR_PROC);
    int fresh = !(G32(proc + 0x40) > G32(proc + 0x44));
    ProcCtx *prev = current_ctx();
    ProcCtx *ctx = ctx_find(proc);
    LPVOID me = scheduler_fiber();

    if (fresh) {
        if (ctx)
            ctx_remove(ctx);  /* a reused process object: drop the old run */
        ctx = (ProcCtx *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*ctx));
        ctx->proc = proc;
        ctx->kind = kind;
        /* 16 MB reserved, committed as it grows: a guest frame lifts to
         * several C frames much larger than it, and the game's recursive walks
         * (0x33B1BB..0x34003C, ~130 guest bytes a level) overflowed 1 MB. */
        ctx->fiber = CreateFiberEx(64 * 1024, 16 * 1024 * 1024, FIBER_FLAG_FLOAT_SWITCH,
                                   process_fiber_main, ctx);
        if (!ctx->fiber) {
            fprintf(stderr, "[MKPROC] CreateFiberEx failed (%lu)\n", GetLastError());
            HeapFree(GetProcessHeap(), 0, ctx);
            return;
        }
        if (++s_live_fibers > s_peak_fibers && (s_peak_fibers = s_live_fibers) % 64 == 0)
            fprintf(stderr, "[MKPROC] %u process fibers alive\n", s_live_fibers);
        ctx->next = s_ctx[ctx_hash(proc)];
        s_ctx[ctx_hash(proc)] = ctx;
    } else {
        if (!ctx || !ctx->fiber || ctx->done) {
            fprintf(stderr, "[MKPROC] resume of 0x%08X with saved frames but no fiber\n", proc);
            return;
        }
        if (G32(proc + 0x34) != ctx->resume_addr)
            fprintf(stderr, "[MKPROC] 0x%08X resume address changed 0x%08X -> 0x%08X (ignored)\n",
                    proc, ctx->resume_addr, G32(proc + 0x34));
        if (kind == KIND_B) {
            /* Copy the saved frames back below the scheduler's esp (0x1C51E9). */
            uint32_t from = G32(proc + 0x40), low = G32(proc + 0x44);
            uint32_t to = G32(SCHED_ESP);
            g_esp = to - (from - low);
            while (from > low) {
                from -= 4;
                to -= 4;
                G32(to) = G32(from);
            }
        } else {
            g_esp = G32(proc + 0x44);
        }
    }

    ctx->return_fiber = me;
    ctx->prev = prev;
    t_running = ctx;
    SwitchToFiber(ctx->fiber);
    t_running = prev;

    if (ctx->done)
        ctx_remove(ctx);
    /* A process may have longjmp'd to a setjmp on this fiber (the main loop
     * restarting); its fiber is marked done above and this lands there. */
    recomp_fiber_resumed();
}

/* ── sleep ─────────────────────────────────────────────────── */

static void process_sleep(int kind)
{
    ProcCtx *ctx = current_ctx();
    uint32_t proc = G32(CUR_PROC);

    if (!ctx || !ctx->fiber || ctx->proc != proc) {
        fprintf(stderr, "[MKPROC] sleep of 0x%08X outside its fiber (running %08X)\n",
                proc, ctx ? ctx->proc : 0);
        g_esp += 4;
        return;
    }

    G32(proc + 0x10) = g_ebp;
    G32(proc + 0x14) = g_ebx;
    G32(proc + 0x18) = g_esi;
    G32(proc + 0x1C) = g_edi;
    G32(proc + 0x0C) = G32(SLEEP_TIME);
    G32(RET_SCRATCH) = G32(g_esp);
    g_esp += 4;
    G32(proc + 0x34) = G32(RET_SCRATCH);
    ctx->resume_addr = G32(RET_SCRATCH);

    if (kind == KIND_B) {
        /* Copy the frames between esp and the scheduler's esp into the buffer
         * that ends at +0x40 (0x1C5379). */
        uint32_t from = G32(SCHED_ESP), to = G32(proc + 0x40);
        while (from > g_esp) {
            from -= 4;
            to -= 4;
            G32(to) = G32(from);
        }
        G32(proc + 0x44) = to;
    } else {
        G32(proc + 0x44) = g_esp;
    }

    if (G32(G32(CUR_PROC) + 0x2C))
        call_callback();

    restore_scheduler_stack();
    SwitchToFiber(ctx->return_fiber);
    recomp_fiber_resumed();

    /* Resumed: the resume put esp back; the registers come from the object. */
    proc = ctx->proc;
    g_ebp = g_seh_ebp = G32(proc + 0x10);
    g_ebx = G32(proc + 0x14);
    g_esi = G32(proc + 0x18);
    g_edi = G32(proc + 0x1C);
}

/* ── jump ──────────────────────────────────────────────────── */

static void process_jump(void)
{
    ProcCtx *ctx = current_ctx();
    uint32_t proc = G32(CUR_PROC);

    G32(proc + 0x30) = G32(g_esp + 4);
    G32(JUMP_ARG) = G32(g_esp + 8);
    if (!ctx || ctx->proc != proc || !ctx->in_run) {
        fprintf(stderr, "[MKPROC] jump of 0x%08X outside its run (running %08X%s)\n",
                proc, ctx ? ctx->proc : 0, ctx && !ctx->in_run ? ", loop over" : "");
        g_esp += 4;
        return;
    }
    checked_longjmp(ctx->loop_jb, "jump", ctx);
}

/* ── the guest routines ────────────────────────────────────── */

/* Kind A resume: no stack of its own, so no fiber; 0x1C5010 and jump both
 * unwind with longjmp. */
void sub_001C50A0(void)
{
    ProcCtx ctx;
    ProcCtx *prev = current_ctx();

    resume_prologue();
    memset(&ctx, 0, sizeof(ctx));
    ctx.proc = G32(CUR_PROC);
    ctx.kind = KIND_A;
    ctx.prev = prev;
    t_running = &ctx;
    if (setjmp(ctx.die_jb) == 0)
        run_process(&ctx);
    t_running = prev;
    g_esp = G32(SCHED_ESP);
}

void sub_001C51C0(void) { resume_prologue(); resume_fiber(KIND_B); }
void sub_001C5470(void) { resume_prologue(); resume_fiber(KIND_C); }

void sub_001C5340(void) { process_sleep(KIND_B); }
void sub_001C55A0(void) { process_sleep(KIND_C); }

void sub_001C5180(void) { process_jump(); }
void sub_001C5430(void) { process_jump(); }
void sub_001C5650(void) { process_jump(); }

/* Kind C: continue on the scheduler's stack, then come back. */
void sub_001C5610(void)
{
    G32(RET_SCRATCH) = G32(g_esp);
    g_esp += 4;
    G32(MAIN_SAVE) = g_esp;
    g_esp = G32(SCHED_ESP);
}

void sub_001C5630(void)
{
    G32(RET_SCRATCH) = G32(g_esp);
    g_esp = G32(MAIN_SAVE);  /* the esp saved by 0x1C5610, already popped */
    G32(MAIN_SAVE) = 0;
}

/* A process ends itself: callback, then the kill, then back to the
 * scheduler -- never returning to the caller. */
void sub_001C5010(void)
{
    ProcCtx *ctx = current_ctx();
    uint32_t proc = G32(CUR_PROC);

    if (G32(proc + 0x2C)) {
        call_callback();
        if (G32(G32(CUR_PROC) + 4) == 0)
            goto out;
    }
    restore_scheduler_stack();
    kill_current();
out:
    if (!ctx) {
        fprintf(stderr, "[MKPROC] 0x1C5010 (end process 0x%08X) outside any run\n", proc);
        g_esp = G32(SCHED_ESP);
        return;
    }
    if (ctx->kind == KIND_A)
        checked_longjmp(ctx->die_jb, "end-process", ctx);
    finish_fiber(ctx);
}

/* For the crash handler: the process the current fiber is running, if any. */
uint32_t mka_process_current(void)
{
    return t_running ? t_running->proc : 0;
}

/* For the crash handler: the running process's guest stack, to tell a guest
 * stack overflow (frames written below the process's own stack) apart. */
void mka_process_describe(FILE *out)
{
    ProcCtx *c = t_running;
    uint32_t proc, vt;

    if (!c) {
        fprintf(out, "  process: none (scheduler side)\n");
        return;
    }
    proc = c->proc;
    vt = G32(proc);
    fprintf(out, "  process 0x%08X kind %c vtable 0x%08X: stack top 0x%08X saved sp 0x%08X"
            " size 0x%X main-stack save 0x%08X, guest esp 0x%08X\n",
            proc, "ABC"[c->kind], vt, G32(proc + 0x40), G32(proc + 0x44),
            G32(vt + 0x24), G32(MAIN_SAVE), g_esp);
}
