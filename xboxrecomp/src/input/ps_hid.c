/* PlayStation controllers read straight from HID (ps_hid.h).
 *
 * The reports, as the controllers send them (the layouts SDL and the Linux hid-playstation driver read too):
 *
 *   DualShock 4 over USB, and either controller's short Bluetooth report (id 0x01 before the full one is asked for):
 *     [1] LX [2] LY [3] RX [4] RY [5] hat (low 4 bits) square 0x10 cross 0x20 circle 0x40 triangle 0x80
 *     [6] L1 0x01 R1 0x02 L2 0x04 R2 0x08 share 0x10 options 0x20 L3 0x40 R3 0x80 [7] PS 0x01 [8] L2 [9] R2
 *   DualShock 4 over Bluetooth, full report 0x11: the same two bytes further on.
 *   DualSense over USB (id 0x01, 64 bytes), and over Bluetooth full report 0x31 one byte further on:
 *     [1] LX [2] LY [3] RX [4] RY [5] L2 [6] R2 [7] counter [8] hat and face buttons as above
 *     [9] L1 R1 L2 R2 create options L3 R3 as above [10] PS 0x01
 *
 * Sticks are 0..255 with 0 at the top; the hat is 0 up, clockwise to 7, 8 centred. */

#include "ps_hid.h"

#if defined(_WIN32)
#include <windows.h>
#include <setupapi.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma comment(lib, "hid.lib")
#pragma comment(lib, "setupapi.lib")

#define PS_MAX 4

typedef struct PsPad {
    volatile LONG live;                /* a thread is reading it */
    wchar_t path[512];
    HANDLE file;
    HANDLE out;                        /* a second handle for rumble: a synchronous one waits on the blocked read */
    int dualsense, bluetooth;
    BYTE rumble[2];                    /* the last sent: strong (left), weak (right) */
    XBOX_INPUT_STATE state;
    DWORD packet;
    unsigned order;                    /* when it was found: the earliest is the first pad */
} PsPad;

static PsPad s_pads[PS_MAX];
static SRWLOCK s_lock = SRWLOCK_INIT;
static unsigned s_found;

static int ps_known_pid(USHORT pid, int *dualsense)
{
    switch (pid) {
    case 0x05C4: case 0x09CC: case 0x0BA0: *dualsense = 0; return 1;   /* DualShock 4 v1, v2, wireless adapter */
    case 0x0CE6: case 0x0DF2: *dualsense = 1; return 1;                /* DualSense, DualSense Edge */
    default: return 0;
    }
}

static SHORT ps_axis(BYTE v, int flip)
{
    int x = (int)v * 257 - 32768;                                  /* 0 -> -32768, 255 -> 32767 */
    if (flip) x = -x - 1;                                          /* PlayStation Y grows downwards */
    return (SHORT)x;
}

/* One report into the Xbox pad's terms. `sticks` is where LX is, `buttons` the hat-and-face byte, `trig` where L2 is
 * (or -1 for the triggers' bits only). */
static void ps_decode(const BYTE *r, int sticks, int buttons, int trig, XBOX_INPUT_STATE *s)
{
    static const WORD hat[9] = {
        XBOX_GAMEPAD_DPAD_UP, XBOX_GAMEPAD_DPAD_UP | XBOX_GAMEPAD_DPAD_RIGHT, XBOX_GAMEPAD_DPAD_RIGHT,
        XBOX_GAMEPAD_DPAD_DOWN | XBOX_GAMEPAD_DPAD_RIGHT, XBOX_GAMEPAD_DPAD_DOWN,
        XBOX_GAMEPAD_DPAD_DOWN | XBOX_GAMEPAD_DPAD_LEFT, XBOX_GAMEPAD_DPAD_LEFT,
        XBOX_GAMEPAD_DPAD_UP | XBOX_GAMEPAD_DPAD_LEFT, 0
    };
    const BYTE face = r[buttons], sh = r[buttons + 1];
    BYTE *a = s->Gamepad.bAnalogButtons;
    memset(&s->Gamepad, 0, sizeof(s->Gamepad));
    s->Gamepad.wButtons = hat[(face & 0x0F) <= 8 ? face & 0x0F : 8];
    if (sh & 0x10) s->Gamepad.wButtons |= XBOX_GAMEPAD_BACK;
    if (sh & 0x20) s->Gamepad.wButtons |= XBOX_GAMEPAD_START;
    if (sh & 0x40) s->Gamepad.wButtons |= XBOX_GAMEPAD_LEFT_THUMB;
    if (sh & 0x80) s->Gamepad.wButtons |= XBOX_GAMEPAD_RIGHT_THUMB;
    a[XBOX_BUTTON_A] = (face & 0x20) ? 255 : 0;        /* cross */
    a[XBOX_BUTTON_B] = (face & 0x40) ? 255 : 0;        /* circle */
    a[XBOX_BUTTON_X] = (face & 0x10) ? 255 : 0;        /* square */
    a[XBOX_BUTTON_Y] = (face & 0x80) ? 255 : 0;        /* triangle */
    a[XBOX_BUTTON_BLACK] = (sh & 0x01) ? 255 : 0;      /* L1, as XInput's left shoulder */
    a[XBOX_BUTTON_WHITE] = (sh & 0x02) ? 255 : 0;      /* R1 */
    a[XBOX_BUTTON_LTRIGGER] = trig >= 0 ? r[trig] : (sh & 0x04) ? 255 : 0;
    a[XBOX_BUTTON_RTRIGGER] = trig >= 0 ? r[trig + 1] : (sh & 0x08) ? 255 : 0;
    s->Gamepad.sThumbLX = ps_axis(r[sticks], 0);
    s->Gamepad.sThumbLY = ps_axis(r[sticks + 1], 1);
    s->Gamepad.sThumbRX = ps_axis(r[sticks + 2], 0);
    s->Gamepad.sThumbRY = ps_axis(r[sticks + 3], 1);
}

typedef struct PsOpen { PsPad *pad; int dualsense; DWORD length; } PsOpen;

static DWORD WINAPI ps_reader(LPVOID arg)
{
    PsOpen o = *(PsOpen *)arg;
    BYTE buf[1024];
    DWORD got;
    free(arg);
    for (;;) {
        XBOX_INPUT_STATE s;
        int ok = 1;
        if (!ReadFile(o.pad->file, buf, o.length, &got, NULL) || !got) break;
        memset(&s, 0, sizeof(s));
        if (buf[0] == 0x01 && o.dualsense && got >= 64) ps_decode(buf, 1, 8, 5, &s);        /* DualSense USB */
        else if (buf[0] == 0x31 && o.dualsense && got >= 12) ps_decode(buf + 1, 1, 8, 5, &s); /* DualSense BT */
        else if (buf[0] == 0x11 && !o.dualsense && got >= 12) ps_decode(buf + 2, 1, 5, 8, &s); /* DS4 BT */
        else if (buf[0] == 0x01 && got >= 10) ps_decode(buf, 1, 5, 8, &s);                     /* DS4 USB, BT short */
        else ok = 0;
        if (!ok) continue;
        AcquireSRWLockExclusive(&s_lock);
        s.dwPacketNumber = ++o.pad->packet;
        o.pad->state = s;
        ReleaseSRWLockExclusive(&s_lock);
    }
    AcquireSRWLockExclusive(&s_lock);
    CloseHandle(o.pad->file);
    o.pad->file = NULL;
    if (o.pad->out) CloseHandle(o.pad->out);
    o.pad->out = NULL;
    o.pad->path[0] = 0;
    InterlockedExchange(&o.pad->live, 0);
    ReleaseSRWLockExclusive(&s_lock);
    fprintf(stderr, "[PAD] PlayStation controller disconnected\n");
    return 0;
}

static void ps_scan(void)
{
    GUID hid;
    HDEVINFO set;
    SP_DEVICE_INTERFACE_DATA ifd;
    DWORD i;
    HidD_GetHidGuid(&hid);
    set = SetupDiGetClassDevsW(&hid, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return;
    ifd.cbSize = sizeof(ifd);
    for (i = 0; SetupDiEnumDeviceInterfaces(set, NULL, &hid, i, &ifd); i++) {
        BYTE detail_buf[sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W) + 1024];
        SP_DEVICE_INTERFACE_DETAIL_DATA_W *detail = (SP_DEVICE_INTERFACE_DETAIL_DATA_W *)detail_buf;
        HIDD_ATTRIBUTES attr;
        PHIDP_PREPARSED_DATA pre;
        HIDP_CAPS caps;
        HANDLE f;
        int dualsense, k, slot = -1, known = 0;
        detail->cbSize = sizeof(*detail);
        if (!SetupDiGetDeviceInterfaceDetailW(set, &ifd, detail, sizeof(detail_buf), NULL, NULL)) continue;
        AcquireSRWLockShared(&s_lock);
        for (k = 0; k < PS_MAX; k++) {
            if (s_pads[k].live && !_wcsicmp(s_pads[k].path, detail->DevicePath)) known = 1;
            if (!s_pads[k].live && slot < 0) slot = k;
        }
        ReleaseSRWLockShared(&s_lock);
        if (known || slot < 0) continue;
        f = CreateFileW(detail->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, 0, NULL);
        if (f == INVALID_HANDLE_VALUE)   /* reading is all this needs, should another program hold write access */
            f = CreateFileW(detail->DevicePath, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                            OPEN_EXISTING, 0, NULL);
        if (f == INVALID_HANDLE_VALUE) continue;
        attr.Size = sizeof(attr);
        if (!HidD_GetAttributes(f, &attr) || attr.VendorID != 0x054C || !ps_known_pid(attr.ProductID, &dualsense) ||
            !HidD_GetPreparsedData(f, &pre)) {
            CloseHandle(f);
            continue;
        }
        if (HidP_GetCaps(pre, &caps) != HIDP_STATUS_SUCCESS || caps.UsagePage != 0x01 ||
            (caps.Usage != 0x05 && caps.Usage != 0x04) || caps.InputReportByteLength > 1024) {
            static int said;
            if (said++ < 4)
                fprintf(stderr, "[PAD] Sony device %04X skipped: usage %04X:%04X, report %u bytes\n", attr.ProductID,
                        caps.UsagePage, caps.Usage, caps.InputReportByteLength);
            HidD_FreePreparsedData(pre);
            CloseHandle(f);
            continue;
        }
        HidD_FreePreparsedData(pre);
        {
            PsOpen *o = (PsOpen *)malloc(sizeof(PsOpen));
            if (!o) { CloseHandle(f); continue; }
            AcquireSRWLockExclusive(&s_lock);
            memset(&s_pads[slot].state, 0, sizeof(s_pads[slot].state));
            wcsncpy(s_pads[slot].path, detail->DevicePath, 511);
            s_pads[slot].path[511] = 0;
            s_pads[slot].file = f;
            s_pads[slot].out = CreateFileW(detail->DevicePath, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                           OPEN_EXISTING, 0, NULL);
            if (s_pads[slot].out == INVALID_HANDLE_VALUE) s_pads[slot].out = NULL;
            s_pads[slot].dualsense = dualsense;
            /* the output report's size says the link: 32 (DualShock 4) or 48 (DualSense) bytes over USB */
            s_pads[slot].bluetooth = caps.OutputReportByteLength != (dualsense ? 48 : 32);
            s_pads[slot].rumble[0] = s_pads[slot].rumble[1] = 0;
            s_pads[slot].order = ++s_found;
            s_pads[slot].packet = 0;
            InterlockedExchange(&s_pads[slot].live, 1);
            ReleaseSRWLockExclusive(&s_lock);
            o->pad = &s_pads[slot];
            o->dualsense = dualsense;
            o->length = caps.InputReportByteLength;
            CloseHandle(CreateThread(NULL, 0, ps_reader, o, 0, NULL));
            fprintf(stderr, "[PAD] %s connected (USB/Bluetooth HID, product %04X)\n",
                    dualsense ? "DualSense" : "DualShock 4", attr.ProductID);
        }
    }
    SetupDiDestroyDeviceInfoList(set);
}

/* Bluetooth output reports end in a CRC-32 of 0xA2 (the HID transaction byte) and the report. */
static DWORD ps_crc32(const BYTE *p, size_t n)
{
    DWORD c = 0xFFFFFFFFu;
    BYTE head = 0xA2;
    size_t i;
    int k;
    for (i = 0; i <= n; i++) {
        c ^= i ? p[i - 1] : head;
        for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1u)));
    }
    return ~c;
}

/* The motors, as SDL drives them: DualShock 4 report 0x05 (USB) / 0x11 (Bluetooth), DualSense 0x02 / 0x31, the
 * strong (low-frequency, left) motor and the weak one. */
static void ps_send_rumble(PsPad *p, BYTE strong, BYTE weak)
{
    BYTE r[78];
    DWORD n = 0, wrote;
    memset(r, 0, sizeof(r));
    if (!p->dualsense && !p->bluetooth) {
        r[0] = 0x05; r[1] = 0x01; r[4] = weak; r[5] = strong; n = 32;
    } else if (!p->dualsense) {
        r[0] = 0x11; r[1] = 0xC4; r[3] = 0x01; r[6] = weak; r[7] = strong; n = 78;
    } else if (!p->bluetooth) {
        r[0] = 0x02; r[1] = 0x03; r[3] = weak; r[4] = strong; n = 48;
    } else {
        r[0] = 0x31; r[1] = 0x02; r[2] = 0x03; r[4] = weak; r[5] = strong; n = 78;
    }
    if (n == 78) {
        const DWORD crc = ps_crc32(r, 74);
        memcpy(r + 74, &crc, 4);
    }
    if (p->out) WriteFile(p->out, r, n, &wrote, NULL);
}

void ps_hid_set_rumble(unsigned index, WORD left, WORD right)
{
    unsigned k;
    PsPad *pad = NULL;
    BYTE strong = (BYTE)(left >> 8), weak = (BYTE)(right >> 8);
    AcquireSRWLockExclusive(&s_lock);
    /* the index-th live pad by when it was found, as ps_hid_get_state counts */
    for (k = 0; k < PS_MAX; k++) {
        unsigned rank = 0, j;
        if (!s_pads[k].live) continue;
        for (j = 0; j < PS_MAX; j++) rank += s_pads[j].live && s_pads[j].order < s_pads[k].order;
        if (rank == index) pad = &s_pads[k];
    }
    if (pad && (pad->rumble[0] != strong || pad->rumble[1] != weak)) {
        pad->rumble[0] = strong;
        pad->rumble[1] = weak;
        ps_send_rumble(pad, strong, weak);
    }
    ReleaseSRWLockExclusive(&s_lock);
}

/* Looks for new controllers every two seconds: they may be plugged in or paired while the game runs. */
static DWORD WINAPI ps_watch(LPVOID unused)
{
    (void)unused;
    for (;;) {
        ps_scan();
        Sleep(2000);
    }
}

DWORD ps_hid_get_state(unsigned index, XBOX_INPUT_STATE *state)
{
    static INIT_ONCE once = INIT_ONCE_STATIC_INIT;
    BOOL pending;
    unsigned order[PS_MAX], n = 0, k, j;
    int at[PS_MAX];
    DWORD r = ERROR_DEVICE_NOT_CONNECTED;
    if (InitOnceBeginInitialize(&once, 0, &pending, NULL) && pending) {
        const char *e = getenv("RECOMP_PS_PADS");
        if (!(e && *e == '0')) {
            ps_scan();
            fprintf(stderr, "[PAD] looking for PlayStation controllers: %u found\n", s_found);
            CloseHandle(CreateThread(NULL, 0, ps_watch, NULL, 0, NULL));
        }
        InitOnceComplete(&once, 0, NULL);
    }
    AcquireSRWLockShared(&s_lock);
    for (k = 0; k < PS_MAX; k++) {
        if (!s_pads[k].live) continue;
        for (j = n; j > 0 && order[j - 1] > s_pads[k].order; j--) { order[j] = order[j - 1]; at[j] = at[j - 1]; }
        order[j] = s_pads[k].order;
        at[j] = (int)k;
        n++;
    }
    if (index < n) {
        *state = s_pads[at[index]].state;
        r = ERROR_SUCCESS;
    }
    ReleaseSRWLockShared(&s_lock);
    return r;
}

#else

DWORD ps_hid_get_state(unsigned index, XBOX_INPUT_STATE *state)
{
    (void)index; (void)state;
    return ERROR_DEVICE_NOT_CONNECTED;
}

void ps_hid_set_rumble(unsigned index, WORD left, WORD right)
{
    (void)index; (void)left; (void)right;
}

#endif
