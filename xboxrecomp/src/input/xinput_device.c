/**
 * Xbox Input Compatibility Layer
 *
 * Translates the Xbox controller API to a host gamepad backend.
 * Handles the structural differences between the Xbox gamepad
 * (analog buttons as bytes, separate trigger channels) and the host
 * (digital face buttons, trigger axes).
 *
 *   _WIN32 -> Windows XInput
 *   POSIX  -> SDL2 GameController
 */

#include "xinput_xbox.h"
#include <string.h>

/* ======================================================================== */
#if defined(_WIN32)
/* ====================  XInput backend  ================================== */
/* ======================================================================== */

#include <xinput.h>
#include <stdlib.h>
#include "ps_hid.h"
#pragma comment(lib, "xinput.lib")

static BOOL  g_controller_connected[XBOX_MAX_CONTROLLERS] = { FALSE };
static DWORD g_last_packet[XBOX_MAX_CONTROLLERS] = { 0 };
static ULONGLONG g_retry_at[XBOX_MAX_CONTROLLERS];

void xbox_InputInit(void)
{
    for (DWORD i = 0; i < XBOX_MAX_CONTROLLERS; i++) {
        XINPUT_STATE state;
        DWORD result = XInputGetState(i, &state);
        g_controller_connected[i] = (result == ERROR_SUCCESS);
    }
}

/* RECOMP_PAD_MAP: which physical button does what, as "GAME=PAD" pairs separated by commas, both named
 * A B X Y LB RB LT RT START BACK L3 R3 (on a PlayStation pad cross circle square triangle L1 R1 L2 R2 options
 * share/create L3 R3, in the same places). "A=X,X=A" swaps A and X; a button not named keeps its place. The same map
 * serves every controller, XInput or PlayStation. */
enum { PM_A, PM_B, PM_X, PM_Y, PM_LB, PM_RB, PM_LT, PM_RT, PM_START, PM_BACK, PM_L3, PM_R3, PM_COUNT };
static const char *const s_pm_names[PM_COUNT] = { "A", "B", "X", "Y", "LB", "RB", "LT", "RT", "START", "BACK", "L3",
                                                  "R3" };
static int s_pad_map[PM_COUNT] = { -1 };

static int pad_map_name(const char *s, size_t n)
{
    int i;
    for (i = 0; i < PM_COUNT; i++)
        if (strlen(s_pm_names[i]) == n && !_strnicmp(s_pm_names[i], s, n)) return i;
    return -1;
}

static void pad_map_init(void)
{
    const char *e = getenv("RECOMP_PAD_MAP"), *p;
    int i;
    for (i = 0; i < PM_COUNT; i++) s_pad_map[i] = i;
    for (p = e; p && *p; ) {
        const char *eq = strchr(p, '='), *end = strchr(p, ',');
        if (!end) end = p + strlen(p);
        if (eq && eq < end) {
            const int game = pad_map_name(p, (size_t)(eq - p)), pad = pad_map_name(eq + 1, (size_t)(end - eq - 1));
            if (game >= 0 && pad >= 0) s_pad_map[game] = pad;
        }
        p = *end ? end + 1 : end;
    }
}

static void pad_remap(XBOX_INPUT_STATE *s)
{
    static const WORD digital[PM_COUNT] = { 0, 0, 0, 0, 0, 0, 0, 0, XBOX_GAMEPAD_START, XBOX_GAMEPAD_BACK,
                                            XBOX_GAMEPAD_LEFT_THUMB, XBOX_GAMEPAD_RIGHT_THUMB };
    static const int analog[PM_COUNT] = { XBOX_BUTTON_A, XBOX_BUTTON_B, XBOX_BUTTON_X, XBOX_BUTTON_Y,
                                          XBOX_BUTTON_BLACK, XBOX_BUTTON_WHITE, XBOX_BUTTON_LTRIGGER,
                                          XBOX_BUTTON_RTRIGGER, -1, -1, -1, -1 };
    BYTE in[PM_COUNT];
    int i, same = 1;
    if (s_pad_map[0] < 0) pad_map_init();
    for (i = 0; i < PM_COUNT; i++) same &= s_pad_map[i] == i;
    if (same) return;
    for (i = 0; i < PM_COUNT; i++)
        in[i] = analog[i] >= 0 ? s->Gamepad.bAnalogButtons[analog[i]] : (s->Gamepad.wButtons & digital[i]) ? 255 : 0;
    s->Gamepad.wButtons &= 0x000F;      /* the D-pad stays */
    for (i = 0; i < PM_COUNT; i++) {
        const BYTE v = in[s_pad_map[i]];
        if (analog[i] >= 0) s->Gamepad.bAnalogButtons[analog[i]] = v;
        else if (v >= XBOX_ANALOG_BUTTON_THRESHOLD) s->Gamepad.wButtons |= digital[i];
    }
}

/* A port with no XInput controller takes a PlayStation one (ps_hid.c): the first such port the first, and so on.
 * XInput is asked again about an empty port once a second -- asking costs time when nothing is there. */
static DWORD ps_fallback(DWORD dwPort, XBOX_INPUT_STATE *pState)
{
    DWORD k, before = 0, r;
    for (k = 0; k < dwPort; k++) before += g_controller_connected[k] ? 1 : 0;
    r = ps_hid_get_state(dwPort - before, pState);
    if (r == ERROR_SUCCESS) pad_remap(pState);
    return r;
}

DWORD xbox_InputGetState(DWORD dwPort, XBOX_INPUT_STATE *pState)
{
    XINPUT_STATE xi_state;
    DWORD result;

    if (dwPort >= XBOX_MAX_CONTROLLERS || !pState)
        return ERROR_DEVICE_NOT_CONNECTED;

    if (!g_controller_connected[dwPort] && GetTickCount64() < g_retry_at[dwPort])
        return ps_fallback(dwPort, pState);
    result = XInputGetState(dwPort, &xi_state);
    if (result != ERROR_SUCCESS) {
        g_controller_connected[dwPort] = FALSE;
        g_retry_at[dwPort] = GetTickCount64() + 1000;
        return ps_fallback(dwPort, pState);
    }

    g_controller_connected[dwPort] = TRUE;
    g_last_packet[dwPort] = xi_state.dwPacketNumber;

    memset(pState, 0, sizeof(XBOX_INPUT_STATE));
    pState->dwPacketNumber = xi_state.dwPacketNumber;
    pState->Gamepad.wButtons = xi_state.Gamepad.wButtons & 0x00FF;

    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_A] =
        (xi_state.Gamepad.wButtons & XINPUT_GAMEPAD_A) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_B] =
        (xi_state.Gamepad.wButtons & XINPUT_GAMEPAD_B) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_X] =
        (xi_state.Gamepad.wButtons & XINPUT_GAMEPAD_X) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_Y] =
        (xi_state.Gamepad.wButtons & XINPUT_GAMEPAD_Y) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_BLACK] =
        (xi_state.Gamepad.wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_WHITE] =
        (xi_state.Gamepad.wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_LTRIGGER] = xi_state.Gamepad.bLeftTrigger;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_RTRIGGER] = xi_state.Gamepad.bRightTrigger;

    pState->Gamepad.sThumbLX = xi_state.Gamepad.sThumbLX;
    pState->Gamepad.sThumbLY = xi_state.Gamepad.sThumbLY;
    pState->Gamepad.sThumbRX = xi_state.Gamepad.sThumbRX;
    pState->Gamepad.sThumbRY = xi_state.Gamepad.sThumbRY;

    pad_remap(pState);
    return ERROR_SUCCESS;
}

DWORD xbox_InputSetState(DWORD dwPort, const XBOX_VIBRATION *pVibration)
{
    XINPUT_VIBRATION xi_vib;

    if (dwPort >= XBOX_MAX_CONTROLLERS || !pVibration)
        return ERROR_DEVICE_NOT_CONNECTED;

    xi_vib.wLeftMotorSpeed = pVibration->wLeftMotorSpeed;
    xi_vib.wRightMotorSpeed = pVibration->wRightMotorSpeed;
    if (!g_controller_connected[dwPort]) {
        /* the PlayStation controller standing in for this port (ps_fallback) */
        DWORD k, before = 0;
        for (k = 0; k < dwPort; k++) before += g_controller_connected[k] ? 1 : 0;
        ps_hid_set_rumble(dwPort - before, xi_vib.wLeftMotorSpeed, xi_vib.wRightMotorSpeed);
        return ERROR_SUCCESS;
    }
    return XInputSetState(dwPort, &xi_vib);
}

BOOL xbox_InputIsConnected(DWORD dwPort)
{
    if (dwPort >= XBOX_MAX_CONTROLLERS) return FALSE;
    return g_controller_connected[dwPort];
}

DWORD xbox_InputGetCapabilities(DWORD dwPort, DWORD dwFlags, XBOX_INPUT_CAPABILITIES *pCaps)
{
    XINPUT_CAPABILITIES xi_caps;
    DWORD result;

    if (dwPort >= XBOX_MAX_CONTROLLERS || !pCaps)
        return ERROR_DEVICE_NOT_CONNECTED;

    result = XInputGetCapabilities(dwPort, dwFlags, &xi_caps);
    if (result != ERROR_SUCCESS) return result;

    memset(pCaps, 0, sizeof(XBOX_INPUT_CAPABILITIES));
    pCaps->Type = xi_caps.Type;
    pCaps->SubType = xi_caps.SubType;
    pCaps->Flags = xi_caps.Flags;
    return ERROR_SUCCESS;
}

/* ======================================================================== */
#else /* !_WIN32 */
/* ====================  SDL2 GameController backend  ===================== */
/* ======================================================================== */

#include <SDL.h>

static SDL_GameController *g_pads[XBOX_MAX_CONTROLLERS];
static BOOL  g_controller_connected[XBOX_MAX_CONTROLLERS];
static DWORD g_packet[XBOX_MAX_CONTROLLERS];

/* Open up to XBOX_MAX_CONTROLLERS attached game controllers. */
static void open_controllers(void)
{
    int slot = 0;
    for (int i = 0; i < SDL_NumJoysticks() && slot < XBOX_MAX_CONTROLLERS; i++) {
        if (!SDL_IsGameController(i))
            continue;
        if (!g_pads[slot]) {
            g_pads[slot] = SDL_GameControllerOpen(i);
            g_controller_connected[slot] = (g_pads[slot] != NULL);
        }
        slot++;
    }
}

void xbox_InputInit(void)
{
    if (!SDL_WasInit(SDL_INIT_GAMECONTROLLER))
        SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    open_controllers();
}

DWORD xbox_InputGetState(DWORD dwPort, XBOX_INPUT_STATE *pState)
{
    if (dwPort >= XBOX_MAX_CONTROLLERS || !pState)
        return ERROR_DEVICE_NOT_CONNECTED;

    SDL_GameController *c = g_pads[dwPort];
    if (!c || !SDL_GameControllerGetAttached(c)) {
        g_controller_connected[dwPort] = FALSE;
        return ERROR_DEVICE_NOT_CONNECTED;
    }

    SDL_GameControllerUpdate();
    g_controller_connected[dwPort] = TRUE;

    memset(pState, 0, sizeof(XBOX_INPUT_STATE));
    pState->dwPacketNumber = ++g_packet[dwPort];

    WORD btn = 0;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_UP))    btn |= XBOX_GAMEPAD_DPAD_UP;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  btn |= XBOX_GAMEPAD_DPAD_DOWN;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  btn |= XBOX_GAMEPAD_DPAD_LEFT;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) btn |= XBOX_GAMEPAD_DPAD_RIGHT;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_START))      btn |= XBOX_GAMEPAD_START;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_BACK))       btn |= XBOX_GAMEPAD_BACK;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_LEFTSTICK))  btn |= XBOX_GAMEPAD_LEFT_THUMB;
    if (SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_RIGHTSTICK)) btn |= XBOX_GAMEPAD_RIGHT_THUMB;
    pState->Gamepad.wButtons = btn;

    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_A] =
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_A) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_B] =
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_B) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_X] =
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_X) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_Y] =
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_Y) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_BLACK] =
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) ? 255 : 0;
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_WHITE] =
        SDL_GameControllerGetButton(c, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) ? 255 : 0;

    /* SDL trigger axes are 0..32767 -> Xbox analog button 0..255 */
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_LTRIGGER] =
        (BYTE)(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERLEFT) >> 7);
    pState->Gamepad.bAnalogButtons[XBOX_BUTTON_RTRIGGER] =
        (BYTE)(SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) >> 7);

    /* SDL Y axis points down; the Xbox Y axis points up -- invert.
     * Use (-1 - v) so v = -32768 does not overflow SHORT. */
    pState->Gamepad.sThumbLX = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX);
    pState->Gamepad.sThumbLY =
        (SHORT)(-1 - SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY));
    pState->Gamepad.sThumbRX = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTX);
    pState->Gamepad.sThumbRY =
        (SHORT)(-1 - SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_RIGHTY));

    return ERROR_SUCCESS;
}

DWORD xbox_InputSetState(DWORD dwPort, const XBOX_VIBRATION *pVibration)
{
    if (dwPort >= XBOX_MAX_CONTROLLERS || !pVibration)
        return ERROR_DEVICE_NOT_CONNECTED;

    SDL_GameController *c = g_pads[dwPort];
    if (!c) return ERROR_DEVICE_NOT_CONNECTED;

    /* SDL rumble needs a duration; refresh for ~1s on each call (the game
     * polls vibration continuously). */
    SDL_GameControllerRumble(c, pVibration->wLeftMotorSpeed,
                             pVibration->wRightMotorSpeed, 1000);
    return ERROR_SUCCESS;
}

BOOL xbox_InputIsConnected(DWORD dwPort)
{
    if (dwPort >= XBOX_MAX_CONTROLLERS) return FALSE;
    return g_controller_connected[dwPort];
}

DWORD xbox_InputGetCapabilities(DWORD dwPort, DWORD dwFlags, XBOX_INPUT_CAPABILITIES *pCaps)
{
    (void)dwFlags;
    if (dwPort >= XBOX_MAX_CONTROLLERS || !pCaps)
        return ERROR_DEVICE_NOT_CONNECTED;
    if (!g_pads[dwPort])
        return ERROR_DEVICE_NOT_CONNECTED;

    memset(pCaps, 0, sizeof(XBOX_INPUT_CAPABILITIES));
    pCaps->Type    = 1;   /* XINPUT_DEVTYPE_GAMEPAD */
    pCaps->SubType = 1;   /* XINPUT_DEVSUBTYPE_GAMEPAD */
    pCaps->Flags   = 0;
    return ERROR_SUCCESS;
}

#endif /* _WIN32 */
