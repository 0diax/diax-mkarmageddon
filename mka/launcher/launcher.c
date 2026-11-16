/* Settings launcher for the recompiled Mortal Kombat: Armageddon.
 *
 * Installs the game from the player's own disc (an ISO, or the default.xbe of one already extracted: copied,
 * translated and compiled here by scripts\install.ps1), shows the options run.cmd used to hard-code
 * (resolution, window, HD textures, intro movies, players, controls), writes them to video.ini and
 * launcher.ini, sets the runtime's environment and starts build\Release\mka_recomp.exe the way run.cmd does.
 * Settings the window does not show are left in video.ini as they were. It runs from the game folder (mka\)
 * or from the package's top folder, beside it.
 *
 * The window is one surface drawn here (GDI+ shapes, GDI text), dark, with the pages in a sidebar: Home (the
 * disc and the installation), Video, Game and Controls (every key and mouse button, and the controller's
 * buttons). It has no child controls: the items of a page are laid out in s_items, and the mouse, the keyboard
 * (Tab, arrows, Enter, Space) and the painting all go through that list. */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static const unsigned k_heights[] = { 480, 720, 1080, 1440, 2160 };
/* The launcher's language: English, or Brazilian Portuguese (launcher.ini language=pt-BR). Every text is a pair. */
static int s_lang;
#define TR(en, pt) (s_lang ? (pt) : (en))

static const wchar_t *const k_height_names[5] = { L"480p", L"720p", L"1080p", L"1440p", L"4K" };
/* Window heights; the width follows the aspect. */
static const unsigned k_win_h[] = { 720, 960, 1080, 1440 };
/* video.ini's aspect: "original" is the game's 4:3; a wider one also widens the game's camera (recomp_manual.c). */
static const char *const k_aspect[] = { "original", "16:9", "21:9" };
static const unsigned k_aspect_n[] = { 4, 16, 21 }, k_aspect_d[] = { 3, 9, 9 };
static const wchar_t *const k_aspect_names[] = { L"4:3", L"16:9", L"21:9" };
static const char *const k_aa[] = { "off", "fxaa", "ssaa" };
static const wchar_t *const k_aa_names[2][3] = { { L"Off", L"FXAA", L"SSAA" }, { L"Desligado", L"FXAA", L"SSAA" } };
/* RECOMP_MOUSE_STEP for each menu speed: the mouse counts that make one step. */
static const int k_mouse_step[] = { 240, 150, 90 };

/* Where the HD packs are downloaded from: their authors' pages. */
static const wchar_t *const k_ps2_pack_url = L"https://github.com/ckiscxrsed/Mortal-Kombat-Armageddon-HD-Textures/releases";
static const wchar_t *const k_remake_url =
    L"https://github.com/UstanakChaos/Mortal-Kombat-Armageddon-PCSX2-Remaster-Remake-Arts-and-Menus/releases";

/* RECOMP_PAD_MAP's names (xinput_device.c), and what they are on either kind of controller */
#define PAD_BUTTONS 12
static const char *const k_pad[PAD_BUTTONS] = { "A", "B", "X", "Y", "LB", "RB", "LT", "RT", "START", "BACK", "L3", "R3" };
static const wchar_t *const k_pad_names[2][PAD_BUTTONS] = {
    { L"A  /  Cross", L"B  /  Circle", L"X  /  Square", L"Y  /  Triangle", L"LB  /  L1", L"RB  /  R1",
      L"LT  /  L2", L"RT  /  R2", L"Start  /  Options", L"Back  /  Share", L"L3 (left stick)", L"R3 (right stick)" },
    { L"A  /  Cruz", L"B  /  Círculo", L"X  /  Quadrado", L"Y  /  Triângulo", L"LB  /  L1", L"RB  /  R1",
      L"LT  /  L2", L"RT  /  R2", L"Start  /  Options", L"Back  /  Share", L"L3 (analógico esq.)", L"R3 (analógico dir.)" }
};
/* what each does in a fight (the game's own layout) */
static const wchar_t *const k_pad_actions[2][PAD_BUTTONS] = {
    { L"Kick", L"Throw", L"Punch", L"Uppercut", L"Grab", L"Weapon", L"Fighting style", L"Block", L"Pause", L"Back",
      L"Left stick click", L"Right stick click" },
    { L"Chute", L"Arremesso", L"Soco", L"Gancho", L"Agarrão", L"Arma", L"Estilo de luta", L"Defesa", L"Pausa", L"Voltar",
      L"Clique do analógico esquerdo", L"Clique do analógico direito" }
};
static int s_map[PAD_BUTTONS];             /* game button -> the physical one doing it */

/* ---- the keyboard and the mouse: launcher.ini key_<BUTTON>, the game's RECOMP_KEY_MAP (pc_input.c) ---------- */

/* What a key of the game's can be: a scancode (0x100 set for an extended key), a mouse button or the wheel, with
 * the runtime's names. A key without a name is written as its scancode ("#73"). */
#define SRC_MOUSE 0x300
#define SRC_WHEEL_UP 0x310
#define SRC_WHEEL_DOWN 0x311
static const struct { const char *name; unsigned short id; } k_src[] = {
    {"A",0x1E},{"B",0x30},{"C",0x2E},{"D",0x20},{"E",0x12},{"F",0x21},{"G",0x22},{"H",0x23},{"I",0x17},{"J",0x24},
    {"K",0x25},{"L",0x26},{"M",0x32},{"N",0x31},{"O",0x18},{"P",0x19},{"Q",0x10},{"R",0x13},{"S",0x1F},{"T",0x14},
    {"U",0x16},{"V",0x2F},{"W",0x11},{"X",0x2D},{"Y",0x15},{"Z",0x2C},
    {"1",0x02},{"2",0x03},{"3",0x04},{"4",0x05},{"5",0x06},{"6",0x07},{"7",0x08},{"8",0x09},{"9",0x0A},{"0",0x0B},
    {"F1",0x3B},{"F2",0x3C},{"F3",0x3D},{"F4",0x3E},{"F5",0x3F},{"F6",0x40},{"F7",0x41},{"F8",0x42},{"F9",0x43},
    {"F10",0x44},{"F12",0x58},
    {"Esc",0x01},{"Space",0x39},{"Enter",0x1C},{"Tab",0x0F},{"Backspace",0x0E},{"CapsLock",0x3A},
    {"Up",0x148},{"Down",0x150},{"Left",0x14B},{"Right",0x14D},
    {"Delete",0x153},{"Home",0x147},{"End",0x14F},{"PageUp",0x149},{"PageDown",0x151},
    {"LShift",0x2A},{"RShift",0x36},{"LCtrl",0x1D},{"RCtrl",0x11D},{"LAlt",0x38},{"RAlt",0x138},
    {"Numpad0",0x52},{"Numpad1",0x4F},{"Numpad2",0x50},{"Numpad3",0x51},{"Numpad4",0x4B},{"Numpad5",0x4C},
    {"Numpad6",0x4D},{"Numpad7",0x47},{"Numpad8",0x48},{"Numpad9",0x49},{"NumpadEnter",0x11C},{"NumpadPlus",0x4E},
    {"NumpadMinus",0x4A},{"NumpadMultiply",0x37},{"NumpadDivide",0x135},{"NumpadDecimal",0x53},
    {"Minus",0x0C},{"Equals",0x0D},{"LBracket",0x1A},{"RBracket",0x1B},{"Semicolon",0x27},{"Quote",0x28},
    {"Grave",0x29},{"Comma",0x33},{"Period",0x34},{"Slash",0x35},{"Backslash",0x2B},
    {"Mouse1",SRC_MOUSE},{"Mouse2",SRC_MOUSE + 1},{"Mouse3",SRC_MOUSE + 2},{"Mouse4",SRC_MOUSE + 3},
    {"Mouse5",SRC_MOUSE + 4},{"WheelUp",SRC_WHEEL_UP},{"WheelDown",SRC_WHEEL_DOWN},
};
#define SRC_NAMES ((int)(sizeof(k_src) / sizeof(k_src[0])))

/* The game's buttons as the keyboard page lists them, RECOMP_KEY_MAP's names, and each one's three keys. */
enum { KA_UP, KA_DOWN, KA_LEFT, KA_RIGHT, KA_X, KA_Y, KA_A, KA_B, KA_RT, KA_LT, KA_LB, KA_RB, KA_START, KA_BACK,
       KA_L3, KA_R3, KA_COUNT };
#define KA_SLOTS 3
static const char *const k_ka[KA_COUNT] = { "UP", "DOWN", "LEFT", "RIGHT", "X", "Y", "A", "B", "RT", "LT", "LB", "RB",
                                            "START", "BACK", "L3", "R3" };
static const wchar_t *const k_ka_names[2][KA_COUNT] = {
    { L"Up", L"Down", L"Left", L"Right", L"Punch", L"Uppercut", L"Kick", L"Throw", L"Block", L"Fighting style", L"Grab",
      L"Weapon", L"Pause", L"Back", L"Left stick click", L"Right stick click" },
    { L"Cima", L"Baixo", L"Esquerda", L"Direita", L"Soco", L"Gancho", L"Chute", L"Arremesso", L"Defesa", L"Estilo de luta",
      L"Agarrão", L"Arma", L"Pausa", L"Voltar", L"Clique do analógico esquerdo", L"Clique do analógico direito" }
};
/* the controller button each one is, and what else it does */
static const wchar_t *const k_ka_sub[2][KA_COUNT] = {
    { L"Jump · moves up in the menus", L"Crouch · moves down in the menus", L"D-pad and left stick", L"D-pad and left stick",
      L"X", L"Y", L"A · confirms in the menus", L"B · goes back in the menus", L"RT · accelerates in Motor Kombat",
      L"LT", L"LB", L"RB", L"Start", L"Back", L"L3", L"R3" },
    { L"Pulo · sobe nos menus", L"Agachar · desce nos menus", L"Direcional e analógico esquerdo",
      L"Direcional e analógico esquerdo", L"X", L"Y", L"A · confirma nos menus", L"B · volta nos menus",
      L"RT · acelera no Motor Kombat", L"LT", L"LB", L"RB", L"Start", L"Back", L"L3", L"R3" }
};
static const unsigned short k_ka_default[KA_COUNT][KA_SLOTS] = {
    { 0x11, 0x148, SRC_WHEEL_UP }, { 0x1F, 0x150, SRC_WHEEL_DOWN }, { 0x1E, 0x14B, 0 }, { 0x20, 0x14D, 0 },
    { 0x24, 0, 0 }, { 0x16, 0, 0 }, { 0x25, 0x1C, SRC_MOUSE }, { 0x17, 0x0E, SRC_MOUSE + 1 },
    { 0x26, 0x39, 0 }, { 0x18, 0, 0 }, { 0x23, 0, 0 }, { 0x31, 0, 0 }, { 0x01, 0, 0 }, { 0x0F, 0, 0 },
    { 0, 0, 0 }, { 0, 0, 0 }
};
static unsigned short s_keys[KA_COUNT][KA_SLOTS];

/* The settings, as the pages show and change them. */
static struct {
    int res, aspect, mode, win, aa, vsync;              /* video.ini */
    int hd, intros, unlock, players, p1;                /* launcher.ini */
    int mouse_menus, mouse_speed;
} cfg;

static wchar_t s_dir[MAX_PATH], s_pack[MAX_PATH], s_remake[MAX_PATH], s_disc[MAX_PATH], s_version[32];

static int exists(const wchar_t *path) { return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES; }

/* ---- video.ini: key=value lines, kept in order; the ones the launcher owns are replaced --------------------- */

#define MAX_LINES 128
static char s_lines[MAX_LINES][400];
static int s_nlines;

static void path_in_dir(wchar_t *out, const wchar_t *name)
{
    swprintf(out, MAX_PATH, L"%s\\%s", s_dir, name);
}

static void ini_load(const wchar_t *name)
{
    wchar_t path[MAX_PATH];
    FILE *f;
    path_in_dir(path, name);
    s_nlines = 0;
    if (_wfopen_s(&f, path, L"r") || !f) return;
    while (s_nlines < MAX_LINES && fgets(s_lines[s_nlines], sizeof(s_lines[0]), f)) {
        s_lines[s_nlines][strcspn(s_lines[s_nlines], "\r\n")] = 0;
        s_nlines++;
    }
    fclose(f);
}

static const char *ini_get(const char *key, const char *fallback)
{
    size_t n = strlen(key);
    int i;
    for (i = 0; i < s_nlines; i++)
        if (!_strnicmp(s_lines[i], key, n) && s_lines[i][n] == '=') return s_lines[i] + n + 1;
    return fallback;
}

static void ini_set(const char *key, const char *value)
{
    size_t n = strlen(key);
    int i;
    for (i = 0; i < s_nlines; i++) {
        if (!_strnicmp(s_lines[i], key, n) && s_lines[i][n] == '=') {
            snprintf(s_lines[i], sizeof(s_lines[0]), "%s=%s", key, value);
            return;
        }
    }
    if (s_nlines < MAX_LINES) snprintf(s_lines[s_nlines++], sizeof(s_lines[0]), "%s=%s", key, value);
}

static void ini_save(const wchar_t *name)
{
    wchar_t path[MAX_PATH];
    FILE *f;
    int i;
    path_in_dir(path, name);
    if (_wfopen_s(&f, path, L"w") || !f) return;
    for (i = 0; i < s_nlines; i++) fprintf(f, "%s\n", s_lines[i]);
    fclose(f);
}

/* ---- the HD packs: hd-textures\ beside the game folder, extracted as they were downloaded ------------------- */

/* The shallowest folder under dir (three levels at most) whose name ends with suffix, into out ("" if none):
 * a pack keeps working wherever its download put it ("SLUS-21410", "MK Armageddon Remake\SLUS-21543"...).
 * Folders put aside ("_...") and a pack's optional sets are not looked into. */
static int find_pack(const wchar_t *dir, const wchar_t *suffix, int depth, wchar_t *out)
{
    wchar_t pattern[MAX_PATH], (*subs)[MAX_PATH] = NULL;
    WIN32_FIND_DATAW fd;
    HANDLE h;
    size_t n = wcslen(suffix);
    int count = 0, i, found = 0;
    *out = 0;
    swprintf(pattern, MAX_PATH, L"%s\\*", dir);
    if ((h = FindFirstFileW(pattern, &fd)) == INVALID_HANDLE_VALUE) return 0;
    subs = (wchar_t (*)[MAX_PATH])calloc(256, sizeof(*subs));
    do {
        size_t len = wcslen(fd.cFileName);
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || fd.cFileName[0] == L'.' || fd.cFileName[0] == L'_' ||
            !_wcsnicmp(fd.cFileName, L"Optional", 8))
            continue;
        if (!found && len >= n && !_wcsicmp(fd.cFileName + len - n, suffix)) {
            swprintf(out, MAX_PATH, L"%s\\%s", dir, fd.cFileName);
            found = 1;
        }
        if (subs && count < 256) wcscpy_s(subs[count++], MAX_PATH, fd.cFileName);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    for (i = 0; !found && depth < 2 && subs && i < count; i++) {
        wchar_t sub[MAX_PATH];
        swprintf(sub, MAX_PATH, L"%s\\%s", dir, subs[i]);
        found = find_pack(sub, suffix, depth + 1, out);
    }
    free(subs);
    return found;
}

static void find_packs(void)
{
    wchar_t hd[MAX_PATH];
    swprintf(hd, MAX_PATH, L"%s\\..\\hd-textures", s_dir);
    find_pack(hd, L"SLUS-21410", 0, s_pack);        /* the PS2 version's pack (ckiscxrsed) */
    find_pack(hd, L"SLUS-21543", 0, s_remake);      /* the remake of the arts and menus (UstanakChaos) */
}

static unsigned win_width(int aspect, int win)
{
    return (k_win_h[win] * k_aspect_n[aspect] / k_aspect_d[aspect] + 1u) & ~1u;
}

/* How far the installation got: 0 nothing, 1 an ISO chosen, 2 the disc copied (not compiled), 3 ready to play. */
static int install_state(void)
{
    wchar_t xbe[MAX_PATH], built[MAX_PATH];
    path_in_dir(xbe, L"game\\default.xbe");
    path_in_dir(built, L"build\\Release\\mka_recomp.exe");
    return exists(built) ? 3 : exists(xbe) ? 2 : *s_disc ? 1 : 0;
}

/* ---- the controller's buttons: launcher.ini pad_map, the game's RECOMP_PAD_MAP ("GAME=PAD,...") ------------- */

static int pad_index(const char *s, size_t n)
{
    int i;
    for (i = 0; i < PAD_BUTTONS; i++)
        if (strlen(k_pad[i]) == n && !_strnicmp(k_pad[i], s, n)) return i;
    return -1;
}

/* Needs launcher.ini loaded. */
static void map_load(void)
{
    const char *p = ini_get("pad_map", "");
    int i;
    for (i = 0; i < PAD_BUTTONS; i++) s_map[i] = i;
    while (*p) {
        const char *eq = strchr(p, '='), *end = strchr(p, ',');
        if (!end) end = p + strlen(p);
        if (eq && eq < end) {
            const int game = pad_index(p, (size_t)(eq - p)), pad = pad_index(eq + 1, (size_t)(end - eq - 1));
            if (game >= 0 && pad >= 0) s_map[game] = pad;
        }
        p = *end ? end + 1 : end;
    }
}

/* Only the buttons moved: "" when none is. */
static void map_string(char *out, size_t size)
{
    int i;
    *out = 0;
    for (i = 0; i < PAD_BUTTONS; i++) {
        if (s_map[i] == i) continue;
        snprintf(out + strlen(out), size - strlen(out), "%s%s=%s", *out ? "," : "", k_pad[i], k_pad[s_map[i]]);
    }
}

/* ---- the keys ------------------------------------------------------------------------------------------------ */

static void keys_default(void) { memcpy(s_keys, k_ka_default, sizeof(s_keys)); }

static int key_is_default(int a) { return !memcmp(s_keys[a], k_ka_default[a], sizeof(s_keys[a])); }


/* Whether a key is one the game itself gives this button. */
static int key_in_default(int a, unsigned id)
{
    int s;
    for (s = 0; s < KA_SLOTS; s++) if (k_ka_default[a][s] == id) return 1;
    return 0;
}

/* A button's keys with no gap between them: the ones after an emptied place move up. */
static void keys_pack(int a)
{
    int s, n = 0;
    for (s = 0; s < KA_SLOTS; s++) if (s_keys[a][s]) s_keys[a][n++] = s_keys[a][s];
    while (n < KA_SLOTS) s_keys[a][n++] = 0;
}

static void key_clear(int a, int s)
{
    s_keys[a][s] = 0;
    keys_pack(a);
}

/* F11 (fullscreen) and Insert are the game window's own; the Windows keys are the system's. */
static int src_reserved(unsigned id) { return id == 0x57 || id == 0x152 || id == 0x15B || id == 0x15C; }

/* One button's keys as launcher.ini and RECOMP_KEY_MAP write them: "K/Enter/Mouse1", "None". */
static void key_string(int a, char *out, size_t size)
{
    int s, i;
    *out = 0;
    for (s = 0; s < KA_SLOTS; s++) {
        const unsigned id = s_keys[a][s];
        if (!id) continue;
        for (i = 0; i < SRC_NAMES && k_src[i].id != id; i++) {}
        if (i < SRC_NAMES) snprintf(out + strlen(out), size - strlen(out), "%s%s", *out ? "/" : "", k_src[i].name);
        else snprintf(out + strlen(out), size - strlen(out), "%s#%X", *out ? "/" : "", id);
    }
    if (!*out) snprintf(out, size, "None");
}

static void key_parse(int a, const char *value)
{
    int n = 0, i;
    memset(s_keys[a], 0, sizeof(s_keys[a]));
    while (*value && n < KA_SLOTS) {
        const char *end = strchr(value, '/');
        size_t len;
        unsigned id = 0;
        if (!end) end = value + strlen(value);
        len = (size_t)(end - value);
        if (*value == '#') id = (unsigned)strtoul(value + 1, NULL, 16);
        else
            for (i = 0; i < SRC_NAMES; i++)
                if (strlen(k_src[i].name) == len && !_strnicmp(k_src[i].name, value, len)) id = k_src[i].id;
        if (id && id < 0x320 && !src_reserved(id)) s_keys[a][n++] = (unsigned short)id;
        value = *end ? end + 1 : end;
    }
}

/* Needs launcher.ini loaded: a button without a line (or with an empty one) keeps the game's own keys. */
static void keys_load(void)
{
    char name[32];
    int a;
    keys_default();
    for (a = 0; a < KA_COUNT; a++) {
        const char *v;
        snprintf(name, sizeof(name), "key_%s", k_ka[a]);
        v = ini_get(name, "");
        if (*v) key_parse(a, v);
    }
}

/* RECOMP_KEY_MAP: only the buttons whose keys were changed; "" when none was. */
static void keymap_string(char *out, size_t size)
{
    char keys[96];
    int a;
    *out = 0;
    for (a = 0; a < KA_COUNT; a++) {
        if (key_is_default(a)) continue;
        key_string(a, keys, sizeof(keys));
        snprintf(out + strlen(out), size - strlen(out), "%s%s=%s", *out ? "," : "", k_ka[a], keys);
    }
}

/* What a person reads for a key: its name in their keyboard layout, or the mouse button. */
static void src_label(unsigned id, wchar_t *out, int cap)
{
    static const wchar_t *const mouse[2][5] = {
        { L"Left click", L"Right click", L"Middle click", L"Mouse 4", L"Mouse 5" },
        { L"Clique esquerdo", L"Clique direito", L"Clique do meio", L"Mouse 4", L"Mouse 5" } };
    int i;
    *out = 0;
    if (!id) return;
    if (id >= SRC_MOUSE && id < SRC_MOUSE + 5) { swprintf(out, cap, L"%s", mouse[s_lang][id - SRC_MOUSE]); return; }
    if (id == SRC_WHEEL_UP) { swprintf(out, cap, L"%s", TR(L"Wheel up", L"Roda para cima")); return; }
    if (id == SRC_WHEEL_DOWN) { swprintf(out, cap, L"%s", TR(L"Wheel down", L"Roda para baixo")); return; }
    switch (id) {
    case 0x148: swprintf(out, cap, L"↑"); return;
    case 0x150: swprintf(out, cap, L"↓"); return;
    case 0x14B: swprintf(out, cap, L"←"); return;
    case 0x14D: swprintf(out, cap, L"→"); return;
    case 0x01: swprintf(out, cap, L"Esc"); return;
    }
    if (GetKeyNameTextW((LONG)(((id & 0xFF) << 16) | ((id & 0x100) ? 1u << 24 : 0)), out, cap) > 0) return;
    for (i = 0; i < SRC_NAMES; i++)
        if (k_src[i].id == id) { swprintf(out, cap, L"%hs", k_src[i].name); return; }
    swprintf(out, cap, L"#%X", id);
}

/* ---- the settings: video.ini and launcher.ini ---------------------------------------------------------------- */

static void load_settings(void)
{
    unsigned h, wh;
    int i;
    ini_load(L"video.ini");
    h = (unsigned)atoi(ini_get("internal_height", "1440"));
    wh = (unsigned)atoi(ini_get("window_height", "960"));
    cfg.res = 3; cfg.win = 1; cfg.aa = 1; cfg.aspect = 0;
    for (i = 0; i < 5; i++) if (k_heights[i] == h) cfg.res = i;
    for (i = 0; i < 4; i++) if (k_win_h[i] == wh) cfg.win = i;
    for (i = 0; i < 3; i++) if (!_stricmp(ini_get("aspect", "original"), k_aspect[i])) cfg.aspect = i;
    for (i = 0; i < 3; i++) if (!_stricmp(ini_get("aa", "fxaa"), k_aa[i])) cfg.aa = i;
    cfg.mode = !_stricmp(ini_get("window_mode", "windowed"), "borderless");
    cfg.vsync = atoi(ini_get("vsync", "1")) != 0;

    ini_load(L"launcher.ini");
    MultiByteToWideChar(CP_UTF8, 0, ini_get("disc", ""), -1, s_disc, MAX_PATH);
    cfg.hd = *s_pack && atoi(ini_get("hd_textures", "1"));
    cfg.intros = atoi(ini_get("skip_intros", "1")) != 0;
    cfg.unlock = atoi(ini_get("unlock_all", "0")) != 0;
    cfg.players = atoi(ini_get("players", "2")) == 1 ? 0 : 1;
    cfg.p1 = atoi(ini_get("p1_keyboard_only", "0")) != 0;
    cfg.mouse_menus = atoi(ini_get("mouse_menus", "1")) != 0;
    cfg.mouse_speed = atoi(ini_get("mouse_speed", "1"));
    if (cfg.mouse_speed < 0 || cfg.mouse_speed > 2) cfg.mouse_speed = 1;
    map_load();
    keys_load();
}

/* The pages' settings into video.ini and launcher.ini. */
static void save_settings(void)
{
    char v[32], text[256], name[32];
    int a;

    ini_load(L"video.ini");
    snprintf(v, sizeof(v), "%u", k_heights[cfg.res]); ini_set("internal_height", v);
    ini_set("window_mode", cfg.mode ? "borderless" : "windowed");
    ini_set("aspect", k_aspect[cfg.aspect]);
    snprintf(v, sizeof(v), "%u", win_width(cfg.aspect, cfg.win)); ini_set("window_width", v);
    snprintf(v, sizeof(v), "%u", k_win_h[cfg.win]); ini_set("window_height", v);
    ini_set("aa", k_aa[cfg.aa]);
    ini_set("vsync", cfg.vsync ? "1" : "0");
    ini_save(L"video.ini");

    ini_load(L"launcher.ini");
    ini_set("language", s_lang ? "pt-BR" : "en");
    if (*s_pack) ini_set("hd_textures", cfg.hd ? "1" : "0");      /* without a pack the choice is kept as it was */
    ini_set("skip_intros", cfg.intros ? "1" : "0");
    ini_set("players", cfg.players ? "2" : "1");
    ini_set("p1_keyboard_only", cfg.p1 ? "1" : "0");
    ini_set("unlock_all", cfg.unlock ? "1" : "0");
    ini_set("mouse_menus", cfg.mouse_menus ? "1" : "0");
    snprintf(v, sizeof(v), "%d", cfg.mouse_speed); ini_set("mouse_speed", v);
    map_string(text, sizeof(text));
    ini_set("pad_map", text);
    for (a = 0; a < KA_COUNT; a++) {
        snprintf(name, sizeof(name), "key_%s", k_ka[a]);
        if (key_is_default(a)) *text = 0;
        else key_string(a, text, sizeof(text));
        if (*text || *ini_get(name, "")) ini_set(name, text);
    }
    ini_save(L"launcher.ini");
}

/* The unlocked session's saves: a fresh copy of the player's (UDATA and TDATA), which the game may write to as it
 * likes. The partition images beside them are not copied: the runtime makes them, empty, where they are missing. */
static int copy_saves(const wchar_t *from, const wchar_t *to)
{
    static const wchar_t *const subs[] = { L"UDATA", L"TDATA" };
    wchar_t src[MAX_PATH + 2], dst[MAX_PATH + 2] = { 0 };
    SHFILEOPSTRUCTW op = { 0 };
    int i;
    swprintf(dst, MAX_PATH, L"%s", to);                 /* double-null ended: the array is zeroed */
    op.wFunc = FO_DELETE;
    op.pFrom = dst;
    op.fFlags = FOF_NO_UI;
    if (exists(to) && SHFileOperationW(&op)) return 0;
    CreateDirectoryW(to, NULL);
    for (i = 0; i < 2; i++) {
        memset(src, 0, sizeof(src));
        swprintf(src, MAX_PATH, L"%s\\%s", from, subs[i]);
        if (!exists(src)) continue;                     /* no saves yet is a fresh start, not an error */
        memset(&op, 0, sizeof(op));
        op.wFunc = FO_COPY;
        op.pFrom = src;
        op.pTo = dst;
        op.fFlags = FOF_NO_UI;
        SHFileOperationW(&op);
    }
    return 1;
}

/* ---- the disc: the player's own ISO, installed by scripts\install.ps1 ---------------------------------------- */

static void save_disc(void)
{
    char d[3 * MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, s_disc, -1, d, sizeof(d), NULL, NULL);
    ini_load(L"launcher.ini");
    ini_set("disc", d);
    ini_save(L"launcher.ini");
}

static void choose_disc(HWND w)
{
    wchar_t file[MAX_PATH] = L"", *slash;
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner = w;
    ofn.lpstrFilter = TR(L"Xbox disc (*.iso, *.xiso, default.xbe)\0*.iso;*.xiso;default.xbe\0All files\0*.*\0",
                         L"Disco do Xbox (*.iso, *.xiso, default.xbe)\0*.iso;*.xiso;default.xbe\0Todos os arquivos\0*.*\0");
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = TR(L"Your Mortal Kombat: Armageddon ISO (original Xbox, NTSC-U)",
                        L"A sua ISO do Mortal Kombat: Armageddon (Xbox original, NTSC-U)");
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (!GetOpenFileNameW(&ofn)) return;
    /* the default.xbe of an extracted disc stands for its folder */
    if ((slash = wcsrchr(file, L'\\')) != NULL && !_wcsicmp(slash + 1, L"default.xbe")) *slash = 0;
    wcscpy_s(s_disc, MAX_PATH, file);
    save_disc();
}

/* scripts\install.ps1 in its own window; the launcher closes, since the build replaces it, and the script opens
 * it again at the end. */
static void install_game(HWND w)
{
    wchar_t script[MAX_PATH], xbe[MAX_PATH], cmd[4 * MAX_PATH];
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    swprintf(script, MAX_PATH, L"%s\\..\\scripts\\install.ps1", s_dir);
    path_in_dir(xbe, L"game\\default.xbe");
    if (!exists(script)) {
        MessageBoxW(w, TR(L"scripts\\install.ps1 is missing beside the mka folder.", L"Falta scripts\\install.ps1 ao lado da pasta mka."), L"Mortal Kombat: Armageddon", MB_ICONERROR);
        return;
    }
    if (!*s_disc && !exists(xbe)) {
        MessageBoxW(w, TR(L"Choose your ISO first.", L"Escolha a sua ISO primeiro."), L"Mortal Kombat: Armageddon", MB_ICONINFORMATION);
        return;
    }
    save_settings();
    if (*s_disc)
        swprintf(cmd, 4 * MAX_PATH, L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"%s\" -Lang %s -Disc \"%s\"",
                 script, TR(L"en", L"pt-BR"), s_disc);
    else
        swprintf(cmd, 4 * MAX_PATH, L"powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"%s\" -Lang %s", script,
                 TR(L"en", L"pt-BR"));
    if (!CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, s_dir, &si, &pi)) {
        MessageBoxW(w, TR(L"PowerShell could not be opened.", L"Não foi possível abrir o PowerShell."), L"Mortal Kombat: Armageddon", MB_ICONERROR);
        return;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    DestroyWindow(w);
}

static void open_hd_folder(void)
{
    wchar_t dir[MAX_PATH];
    swprintf(dir, MAX_PATH, L"%s\\..\\hd-textures", s_dir);
    CreateDirectoryW(dir, NULL);
    ShellExecuteW(NULL, L"open", dir, NULL, NULL, SW_SHOWNORMAL);
}

/* ---- Steam: a non-Steam game shortcut ------------------------------------------------------------------------ */

/* Steam keeps them per account in userdata\<id>\config\shortcuts.vdf, a binary key-value file: 0x00 opens a map,
 * 0x01 a string, 0x02 a 32-bit number, 0x08 ends the map. The shortcut's id is the crc32 of the quoted exe and
 * the name, top bit set. Steam reads the file at start and writes it back from memory, so it has to be closed. */

typedef struct { char *p; size_t n, cap; } Buf;

static void buf_put(Buf *b, const void *data, size_t n)
{
    if (b->n + n > b->cap) {
        size_t cap = b->cap ? b->cap * 2 : 4096;
        char *grown;
        while (cap < b->n + n) cap *= 2;
        if (!(grown = (char *)realloc(b->p, cap))) return;
        b->p = grown;
        b->cap = cap;
    }
    memcpy(b->p + b->n, data, n);
    b->n += n;
}
static void vdf_str(Buf *b, const char *k, const char *v)
{
    buf_put(b, "\1", 1); buf_put(b, k, strlen(k) + 1); buf_put(b, v, strlen(v) + 1);
}
static void vdf_int(Buf *b, const char *k, uint32_t v)
{
    unsigned char le[4] = { (unsigned char)v, (unsigned char)(v >> 8), (unsigned char)(v >> 16), (unsigned char)(v >> 24) };
    buf_put(b, "\2", 1); buf_put(b, k, strlen(k) + 1); buf_put(b, le, 4);
}

static uint32_t crc32_of(const char *s)
{
    uint32_t c = 0xFFFFFFFFu;
    int k;
    for (; *s; s++) {
        c ^= (unsigned char)*s;
        for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
    }
    return ~c;
}

/* past a key (a 0-ended string) at *p; 0 when the file ends first */
static int vdf_key(const char *f, size_t n, size_t *p, const char **key)
{
    const char *z = memchr(f + *p, 0, n - *p);
    if (!z) return 0;
    if (key) *key = f + *p;
    *p = (size_t)(z - f) + 1;
    return 1;
}
/* past one value of type t (its key already read) */
static int vdf_skip(const char *f, size_t n, size_t *p, int t)
{
    switch (t) {
    case 0:
        while (*p < n) {
            const int u = (unsigned char)f[(*p)++];
            if (u == 8) return 1;
            if (!vdf_key(f, n, p, NULL) || !vdf_skip(f, n, p, u)) return 0;
        }
        return 0;
    case 1: return vdf_key(f, n, p, NULL);
    case 2: case 3: *p += 4; return *p <= n;
    case 7: *p += 8; return *p <= n;
    default: return 0;
    }
}

/* One account's shortcuts.vdf with this launcher in it; 1 when it was there already or was added. */
static int steam_add_to(const wchar_t *vdf, const char *q_exe, const char *q_dir, const char *exe, const char *name)
{
    static const char head[] = "\0shortcuts\0";
    Buf f = { 0 }, e = { 0 };
    FILE *in, *out;
    size_t p, end = 0;
    int next = 0, have = 0, ok = 0;
    char index[16];
    if (!_wfopen_s(&in, vdf, L"rb") && in) {
        char chunk[65536];
        size_t got;
        while ((got = fread(chunk, 1, sizeof(chunk), in)) > 0) buf_put(&f, chunk, got);
        fclose(in);
    }
    if (f.n < 2) { f.n = 0; buf_put(&f, head, sizeof(head) - 1); buf_put(&f, "\x08\x08", 2); }
    if (f.n < sizeof(head) - 1 || memcmp(f.p, head, sizeof(head) - 1)) { free(f.p); return 0; }
    /* the entries ("0", "1", ...): the next number, and whether this exe is one of them */
    for (p = sizeof(head) - 1; p < f.n;) {
        const int t = (unsigned char)f.p[p++];
        const char *key;
        if (t == 8) { end = p - 1; ok = 1; break; }
        if (t != 0 || !vdf_key(f.p, f.n, &p, &key)) break;
        if (atoi(key) + 1 > next) next = atoi(key) + 1;
        for (;;) {
            int u;
            size_t at;
            if (p >= f.n) break;
            if ((u = (unsigned char)f.p[p++]) == 8) break;
            if (!vdf_key(f.p, f.n, &p, &key)) { p = f.n; break; }
            at = p;
            if (!vdf_skip(f.p, f.n, &p, u)) { p = f.n; break; }
            if (u == 1 && !_stricmp(key, "exe") && !_stricmp(f.p + at, q_exe)) have = 1;
        }
    }
    if (!ok) { free(f.p); return 0; }
    if (!have) {
        char both[3 * MAX_PATH];
        snprintf(both, sizeof(both), "%s%s", q_exe, name);
        snprintf(index, sizeof(index), "%d", next);
        buf_put(&e, "\0", 1); buf_put(&e, index, strlen(index) + 1);
        vdf_int(&e, "appid", crc32_of(both) | 0x80000000u);
        vdf_str(&e, "AppName", name);
        vdf_str(&e, "Exe", q_exe);
        vdf_str(&e, "StartDir", q_dir);
        vdf_str(&e, "icon", exe);
        vdf_str(&e, "ShortcutPath", "");
        vdf_str(&e, "LaunchOptions", "");
        vdf_int(&e, "IsHidden", 0);
        vdf_int(&e, "AllowDesktopConfig", 1);
        vdf_int(&e, "AllowOverlay", 1);
        vdf_int(&e, "OpenVR", 0);
        vdf_int(&e, "Devkit", 0);
        vdf_str(&e, "DevkitGameID", "");
        vdf_int(&e, "DevkitOverrideAppID", 0);
        vdf_int(&e, "LastPlayTime", 0);
        vdf_str(&e, "FlatpakAppID", "");
        buf_put(&e, "\0tags\0\x08\x08", 8);
        {
            wchar_t backup[MAX_PATH];
            swprintf(backup, MAX_PATH, L"%s.antes-do-mka", vdf);
            if (exists(vdf)) CopyFileW(vdf, backup, FALSE);
        }
        if (_wfopen_s(&out, vdf, L"wb") || !out) { free(f.p); free(e.p); return 0; }
        ok = fwrite(f.p, 1, end, out) == end && fwrite(e.p, 1, e.n, out) == e.n &&
             fwrite(f.p + end, 1, f.n - end, out) == f.n - end;
        if (fclose(out)) ok = 0;
    }
    free(f.p);
    free(e.p);
    return ok;
}

static void add_to_steam(HWND w)
{
    wchar_t steam[MAX_PATH], exe[MAX_PATH], pattern[MAX_PATH];
    char q_exe[3 * MAX_PATH], q_dir[3 * MAX_PATH], u_exe[3 * MAX_PATH], u_dir[3 * MAX_PATH];
    DWORD size = sizeof(steam), pid = 0, psize = sizeof(pid);
    WIN32_FIND_DATAW fd;
    HANDLE find;
    int users = 0, added = 0;
    wchar_t *slash;
    if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam", L"SteamPath", RRF_RT_REG_SZ, NULL, steam, &size)) {
        MessageBoxW(w, TR(L"Steam was not found on this computer.", L"A Steam não foi encontrada neste computador."),
                    TR(L"Add to Steam", L"Adicionar à Steam"), MB_ICONWARNING);
        return;
    }
    for (slash = steam; *slash; slash++) if (*slash == L'/') *slash = L'\\';
    if (!RegGetValueW(HKEY_CURRENT_USER, L"Software\\Valve\\Steam\\ActiveProcess", L"pid", RRF_RT_REG_DWORD, NULL, &pid,
                      &psize) && pid) {
        HANDLE p = OpenProcess(SYNCHRONIZE, FALSE, pid);
        if (p) {
            const int alive = WaitForSingleObject(p, 0) == WAIT_TIMEOUT;
            CloseHandle(p);
            if (alive) {
                MessageBoxW(w, TR(L"Close Steam (Steam > Exit) and try again: it rewrites its list of shortcuts when it closes.",
                                  L"Feche a Steam (Steam > Sair) e tente de novo: ela reescreve a lista de atalhos ao fechar."),
                            TR(L"Add to Steam", L"Adicionar à Steam"), MB_ICONINFORMATION);
                return;
            }
        }
    }
    GetModuleFileNameW(NULL, exe, MAX_PATH);
    WideCharToMultiByte(CP_UTF8, 0, exe, -1, u_exe, sizeof(u_exe), NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, s_dir, -1, u_dir, sizeof(u_dir), NULL, NULL);
    snprintf(q_exe, sizeof(q_exe), "\"%s\"", u_exe);
    snprintf(q_dir, sizeof(q_dir), "\"%s\"", u_dir);
    swprintf(pattern, MAX_PATH, L"%s\\userdata\\*", steam);
    if ((find = FindFirstFileW(pattern, &fd)) != INVALID_HANDLE_VALUE) {
        do {
            wchar_t vdf[MAX_PATH], config[MAX_PATH];
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || !iswdigit(fd.cFileName[0]) || !wcscmp(fd.cFileName, L"0"))
                continue;
            swprintf(config, MAX_PATH, L"%s\\userdata\\%s\\config", steam, fd.cFileName);
            if (!exists(config)) continue;
            users++;
            swprintf(vdf, MAX_PATH, L"%s\\shortcuts.vdf", config);
            added += steam_add_to(vdf, q_exe, q_dir, u_exe, "Mortal Kombat: Armageddon");
        } while (FindNextFileW(find, &fd));
        FindClose(find);
    }
    MessageBoxW(w, !users ? TR(L"Steam has no account on this computer.", L"A Steam não tem nenhuma conta neste computador.")
                   : added ? TR(L"Done: open Steam and the game is in your library.",
                                L"Pronto: abra a Steam e o jogo estará na biblioteca.")
                           : TR(L"The Steam shortcut could not be written (shortcuts.vdf).",
                                L"Não foi possível gravar o atalho na Steam (shortcuts.vdf)."),
                TR(L"Add to Steam", L"Adicionar à Steam"), added ? MB_ICONINFORMATION : MB_ICONWARNING);
}

/* ---- starting the game --------------------------------------------------------------------------------------- */

static void env(const wchar_t *name, const wchar_t *value) { SetEnvironmentVariableW(name, value); }

static void save_and_play(HWND w)
{
    wchar_t path[MAX_PATH], game[MAX_PATH], out[MAX_PATH], err[MAX_PATH], cmd[MAX_PATH + 4];
    char pad_map[256], key_map[1024];
    wchar_t wmap[256], wkeys[1024], step[16], packs[2 * MAX_PATH + 2];
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    HANDLE ho, he;

    save_settings();
    map_string(pad_map, sizeof(pad_map));
    keymap_string(key_map, sizeof(key_map));

    path_in_dir(game, L"build\\Release\\mka_recomp.exe");
    if (!exists(game)) {
        MessageBoxW(w, TR(L"The game is not installed yet: on the Home page, choose your ISO and click Install and Compile.",
                          L"O jogo ainda não foi instalado: na página Início, escolha a sua ISO e clique em Instalar e compilar."),
                    L"Mortal Kombat: Armageddon", MB_ICONINFORMATION);
        return;
    }

    /* The same environment run.cmd sets. */
    env(L"RECOMP_AC97_READY", L"1");
    env(L"RECOMP_APU_DSP_ACK", L"0x83218810");
    env(L"RECOMP_APU_DSP_THREADS", L"2");
    env(L"RECOMP_NV2A_KELVIN", L"1");
    env(L"RECOMP_VBLANK", L"1");
    env(L"RECOMP_VBLANK_KEEP_GRID", L"1");
    env(L"RECOMP_USB", L"1");
    env(L"RECOMP_USB_PADS", cfg.players ? L"2" : L"1");
    /* Player one on a controller slot nothing occupies leaves it to the keyboard; controller 0 then drives
     * player two. */
    env(L"RECOMP_PAD1_XINPUT", cfg.p1 ? L"3" : NULL);
    env(L"RECOMP_PAD2_XINPUT", cfg.p1 ? L"0" : NULL);
    env(L"RECOMP_KEY_LAYOUT", L"pad");
    /* The player's keys and mouse buttons (only the changed ones), and the mouse walking the menus. */
    MultiByteToWideChar(CP_UTF8, 0, key_map, -1, wkeys, 1024);
    env(L"RECOMP_KEY_MAP", *key_map ? wkeys : NULL);
    swprintf(step, 16, L"%d", cfg.mouse_menus ? k_mouse_step[cfg.mouse_speed] : 0);
    env(L"RECOMP_MOUSE_STEP", cfg.mouse_menus && cfg.mouse_speed == 1 ? NULL : step);
    env(L"RECOMP_FB_WINDOW", L"1");
    env(L"RECOMP_WINDOW_TITLE", L"Mortal Kombat: Armageddon");
    env(L"RECOMP_HIDE_FILES", cfg.intros ? L"moviexbox\\LOGON.SFD;moviexbox\\OPENINGN.SFD" : NULL);
    path_in_dir(path, L"video.ini"); env(L"RECOMP_VIDEO_INI", path);
    env(L"RECOMP_KELVIN_GPU", L"1");
    path_in_dir(path, L"kgpu-cache"); CreateDirectoryW(path, NULL); env(L"RECOMP_KGPU_CACHE", path);
    env(L"RECOMP_KELVIN_THREADS", L"12");
    /* The video settings page, in the game's Options > VIDEO ([RB] / [V]), in the launcher's language, MKA's rows only. */
    env(L"RECOMP_NATIVE_MENUS", L"0");
    env(L"RECOMP_LANGUAGE", TR(L"en", L"pt-BR"));
    env(L"RECOMP_VIDEO_MENU_HIDE", L"dof,ao_method,ao_quality,reflections,rt,fov,blur,crosshair,hitmarkers");
    /* The remake of the arts and menus goes first; the full pack recognises the textures and fills in the rest.
     * The packs' optional sets are left out (the remake's own readme calls its Bonus Test incomplete). */
    if (*s_remake) swprintf(packs, 2 * MAX_PATH + 2, L"%s;%s", s_remake, s_pack);
    else swprintf(packs, 2 * MAX_PATH + 2, L"%s", s_pack);
    env(L"RECOMP_TEXTURE_PACK", cfg.hd && *s_pack ? packs : NULL);
    env(L"RECOMP_TEXTURE_SKIP", L"Optional*;_*;Bonus Test");
    MultiByteToWideChar(CP_UTF8, 0, pad_map, -1, wmap, 256);
    env(L"RECOMP_PAD_MAP", *pad_map ? wmap : NULL);
    path_in_dir(path, L"save"); CreateDirectoryW(path, NULL);
    /* Everything unlocked plays on a copy of the saves, made afresh each time: the player's profile never sees it. */
    if (cfg.unlock) {
        wchar_t copy[MAX_PATH];
        path_in_dir(copy, L"save_desbloqueado");
        if (!copy_saves(path, copy)) {
            MessageBoxW(w, TR(L"The copy of the saves for the unlocked session could not be made.",
                              L"Não foi possível preparar a cópia dos saves para a sessão desbloqueada."),
                        L"Mortal Kombat: Armageddon", MB_ICONERROR);
            return;
        }
        env(L"RECOMP_SAVE_DIR", L"save_desbloqueado");
        env(L"RECOMP_UNLOCK_ALL", L"1");
    } else {
        env(L"RECOMP_SAVE_DIR", NULL);
        env(L"RECOMP_UNLOCK_ALL", NULL);
    }

    path_in_dir(out, L"stdout.txt");
    path_in_dir(err, L"stderr.txt");
    ho = CreateFileW(out, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, 0, NULL);
    he = CreateFileW(err, GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, 0, NULL);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = ho;
    si.hStdError = he;
    swprintf(cmd, MAX_PATH + 4, L"\"%s\"", game);
    if (!CreateProcessW(game, cmd, NULL, NULL, TRUE, 0, NULL, s_dir, &si, &pi)) {
        MessageBoxW(w, TR(L"The game could not be started.", L"Não foi possível iniciar o jogo."), L"Mortal Kombat: Armageddon", MB_ICONERROR);
    } else {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        DestroyWindow(w);
    }
    if (ho != INVALID_HANDLE_VALUE) CloseHandle(ho);
    if (he != INVALID_HANDLE_VALUE) CloseHandle(he);
}

/* ================================================================================================================
 * The window
 * ================================================================================================================ */

/* ---- GDI+ (its flat C interface, declared here: its headers are C++) ----------------------------------------- */

typedef struct { UINT32 version; void *debug_callback; BOOL no_background_thread, no_external_codecs; } GdipStartup;
typedef struct { float x, y, w, h; } GdipRectF;
int WINAPI GdiplusStartup(ULONG_PTR *token, const GdipStartup *input, void *output);
int WINAPI GdipCreateFromHDC(HDC dc, void **graphics);
int WINAPI GdipDeleteGraphics(void *graphics);
int WINAPI GdipSetSmoothingMode(void *graphics, int mode);
int WINAPI GdipSetPixelOffsetMode(void *graphics, int mode);
int WINAPI GdipSetClipRectI(void *graphics, int x, int y, int w, int h, int combine);
int WINAPI GdipResetClip(void *graphics);
int WINAPI GdipFlush(void *graphics, int intention);
int WINAPI GdipCreatePath(int fill_mode, void **path);
int WINAPI GdipDeletePath(void *path);
int WINAPI GdipAddPathArc(void *path, float x, float y, float w, float h, float start, float sweep);
int WINAPI GdipAddPathRectangle(void *path, float x, float y, float w, float h);
int WINAPI GdipClosePathFigure(void *path);
int WINAPI GdipCreateSolidFill(DWORD argb, void **brush);
int WINAPI GdipCreateLineBrushFromRect(const GdipRectF *rect, DWORD from, DWORD to, int mode, int wrap, void **brush);
int WINAPI GdipDeleteBrush(void *brush);
int WINAPI GdipFillPath(void *graphics, void *brush, void *path);
int WINAPI GdipFillEllipse(void *graphics, void *brush, float x, float y, float w, float h);
int WINAPI GdipCreatePen1(DWORD argb, float width, int unit, void **pen);
int WINAPI GdipDeletePen(void *pen);
int WINAPI GdipDrawPath(void *graphics, void *pen, void *path);
int WINAPI GdipDrawEllipse(void *graphics, void *pen, float x, float y, float w, float h);

/* ---- the look -------------------------------------------------------------------------------------------------- */

/* Colours, 0xRRGGBB: near-black surfaces, an ember accent, gold for what asks for attention. */
enum {
    C_BG = 0x0B0B0E, C_SIDE = 0x0F0F13, C_CARD = 0x15151A, C_LINE = 0x25252D, C_ROW = 0x1B1B21,
    C_CTL = 0x1F1F26, C_CTL_HOT = 0x2A2A34, C_CTL_LINE = 0x34343F,
    C_TEXT = 0xF2F2F5, C_MUTED = 0xA6A6B2, C_FAINT = 0x70707C,
    C_ACCENT = 0xE8502A, C_ACCENT_HI = 0xF06A2C, C_ACCENT_LO = 0xCF421B, C_ACCENT_DEEP = 0xB03010,
    C_SELECTED = 0x3A1D14, C_GOLD = 0xF5B642, C_OK = 0x4CC38A, C_FOCUS = 0xFFC46B
};
#define OPAQUE_ARGB(c) (0xFF000000u | (unsigned)(c))
static COLORREF cr(unsigned c) { return RGB((c >> 16) & 255, (c >> 8) & 255, c & 255); }

enum { F_BODY, F_BOLD, F_SMALL, F_CAPS, F_TITLE, F_HERO, F_BRAND, F_PLAY, F_ICON, F_ICON_S, F_COUNT };
/* Segoe MDL2 Assets, the system's icon font */
#define IC_HOME L"\xE80F"
#define IC_VIDEO L"\xE7F4"
#define IC_GAME L"\xE7FC"
#define IC_KEYS L"\xE765"
#define IC_PLAY L"\xE768"
#define IC_FOLDER L"\xED25"
#define IC_OPEN L"\xE8A7"
#define IC_BUILD L"\xE896"
#define IC_CHECK L"\xE73E"
#define IC_DOWN L"\xE70D"
#define IC_RESET L"\xE72C"
#define IC_ADD L"\xE710"

static HWND s_wnd;
static int s_dpi = 96;
static HFONT s_fonts[F_COUNT];
static HICON s_icon;
static HDC s_mdc, s_bdc;                    /* for measuring text; the back buffer everything is drawn into */
static HBITMAP s_bbmp;
static int s_bw, s_bh;
static void *s_g;                           /* GDI+ on the back buffer, while painting */
static RECT s_clip;                         /* text is cut to this while the scrolled items are drawn */
static int s_clipping;

static int S(int px) { return MulDiv(px, s_dpi, 96); }

/* ---- the items of a page --------------------------------------------------------------------------------------- */

enum { K_CARD, K_TEXT, K_DIVIDER, K_STEP,
       K_NAV, K_BUTTON, K_TOGGLE, K_SEG, K_SELECT, K_BIND };      /* from K_NAV on: what the pointer and keys reach */
enum { A_NONE, A_LANG, A_TAB, A_RELAYOUT, A_CHOOSE, A_INSTALL, A_STEAM, A_HD_FOLDER, A_HD_PS2, A_HD_REMAKE, A_PLAY,
       A_KEYS_RESET, A_PAD_RESET };
enum { B_SECONDARY, B_PRIMARY, B_PLAY };

typedef struct Item {
    int kind, id, arg, style;
    RECT r;                                 /* fixed: in the window; else in the page, which scrolls */
    int fixed, disabled;
    const wchar_t *text, *sub;
    int font, color, flags;                 /* K_TEXT */
    int *value;
    const wchar_t *const *opts;             /* K_SEG, K_SELECT */
    int nopts, segw[6];
} Item;
#define MAX_ITEMS 320
static Item s_items[MAX_ITEMS];
static int s_n, s_fixed;

static int s_page, s_tab;                   /* the page shown; Controls' tab (keyboard and mouse / controller) */
static RECT s_view;                         /* where the page is shown, scrolled by s_scroll */
static int s_scroll, s_content_h;
static int s_hot = -1, s_hot_part = -1, s_press = -1, s_focus = -1, s_focus_ring;
static int s_thumb_drag, s_thumb_grab;
/* the list open under a K_SELECT, and the key being captured for a K_BIND */
static int s_pop = -1, s_pop_hot;
static int s_cap = -1, s_cap_hot;
static DWORD s_cap_until;
static wchar_t s_cap_msg[160], s_toast[240];
#define CAPTURE_MS 10000

static struct { int x, y, w; } L;           /* where the next row of the page goes */
static int s_card, s_first_row;

/* Texts made for one layout (window sizes, labels with a name in them). */
static wchar_t s_pool[8192];
static size_t s_pool_n;
static const wchar_t *s_win_opts[4];

static const wchar_t *pf(const wchar_t *fmt, ...)
{
    wchar_t *out = s_pool + s_pool_n;
    const size_t room = sizeof(s_pool) / sizeof(s_pool[0]) - s_pool_n;
    va_list ap;
    int n;
    if (room < 2) return L"";
    va_start(ap, fmt);
    n = _vsnwprintf_s(out, room, _TRUNCATE, fmt, ap);
    va_end(ap);
    s_pool_n += (n < 0 ? wcslen(out) : (size_t)n) + 1;
    return out;
}

static int tw(int font, const wchar_t *s)
{
    SIZE z = { 0, 0 };
    SelectObject(s_mdc, s_fonts[font]);
    SetTextCharacterExtra(s_mdc, font == F_CAPS ? S(1) : 0);
    GetTextExtentPoint32W(s_mdc, s, (int)wcslen(s), &z);
    return z.cx;
}

static int wrap_h(int font, const wchar_t *s, int w)
{
    RECT r = { 0, 0, w, 0 };
    SelectObject(s_mdc, s_fonts[font]);
    SetTextCharacterExtra(s_mdc, 0);
    DrawTextW(s_mdc, s, -1, &r, DT_CALCRECT | DT_WORDBREAK | DT_NOPREFIX);
    return r.bottom;
}

static Item *item(int kind, int x, int y, int w, int h)
{
    static Item spare;
    Item *it = s_n < MAX_ITEMS ? &s_items[s_n++] : &spare;
    memset(it, 0, sizeof(*it));
    it->kind = kind;
    it->fixed = s_fixed;
    SetRect(&it->r, x, y, x + w, y + h);
    return it;
}

static Item *text_item(int x, int y, int w, int h, const wchar_t *s, int font, int color, int flags)
{
    Item *it = item(K_TEXT, x, y, w, h);
    it->text = s; it->font = font; it->color = color; it->flags = flags;
    return it;
}

static void card_begin(const wchar_t *title)
{
    s_card = s_n;
    item(K_CARD, L.x, L.y, L.w, 0);
    L.y += S(16);
    if (title) {
        text_item(L.x + S(20), L.y, L.w - S(40), S(16), title, F_CAPS, C_MUTED, DT_SINGLELINE);
        L.y += S(20);
    }
    s_first_row = 1;
}

static void card_end(void)
{
    L.y += S(8);
    if (s_card < MAX_ITEMS) s_items[s_card].r.bottom = L.y;
    L.y += S(16);
}

/* A row of a card: a name and what it means at the left, a control of cw x ch at the right (under them when the two
 * do not fit side by side). The control's place comes back; *whole gets the row's, for a control that is the row. */
static RECT row(const wchar_t *label, const wchar_t *sub, int cw, int ch, RECT *whole)
{
    const int pad = S(20), vp = S(12), inner = L.w - 2 * pad;
    int stacked, lw, sh, th, h, ty;
    RECT r;
    if (cw > inner) cw = inner;
    stacked = inner - cw - S(20) < S(200);
    lw = stacked ? inner : inner - cw - S(20);
    sh = sub ? wrap_h(F_SMALL, sub, lw) : 0;
    th = S(20) + (sub ? S(2) + sh : 0);
    if (!s_first_row) item(K_DIVIDER, L.x + pad, L.y, inner, 1);
    s_first_row = 0;
    if (stacked) {
        h = vp + th + S(10) + ch + vp;
        ty = L.y + vp;
        SetRect(&r, L.x + pad, ty + th + S(10), L.x + pad + cw, ty + th + S(10) + ch);
    } else {
        h = 2 * vp + (th > ch ? th : ch);
        ty = L.y + (h - th) / 2;
        SetRect(&r, L.x + L.w - pad - cw, L.y + (h - ch) / 2, L.x + L.w - pad, L.y + (h - ch) / 2 + ch);
    }
    if (whole) SetRect(whole, L.x + S(8), L.y + S(4), L.x + L.w - S(8), L.y + h - S(4));
    else {
        text_item(L.x + pad, ty, lw, S(20), label, F_BODY, C_TEXT, DT_SINGLELINE | DT_END_ELLIPSIS);
        if (sub) text_item(L.x + pad, ty + S(22), lw, sh, sub, F_SMALL, C_MUTED, DT_WORDBREAK);
    }
    L.y += h;
    return r;
}

/* The widths of a segmented choice's parts, each as wide as its text; the whole width comes back. */
static int seg_measure(Item *it)
{
    int i, total = 0;
    for (i = 0; i < it->nopts && i < 6; i++) {
        it->segw[i] = tw(F_BOLD, it->opts[i]) + S(28);      /* the chosen one is in bold */
        if (it->segw[i] < S(56)) it->segw[i] = S(56);
        total += it->segw[i];
    }
    return total;
}

static Item *seg_at(RECT r, int id, int *value, const wchar_t *const *opts, int n, int equal)
{
    Item *it = item(K_SEG, r.left, r.top, r.right - r.left, r.bottom - r.top);
    int i, total;
    it->id = id; it->value = value; it->opts = opts; it->nopts = n;
    total = seg_measure(it);
    /* shared out evenly, or squeezed where the row was narrower than the texts */
    for (i = 0; i < n; i++)
        it->segw[i] = equal || total > r.right - r.left ? equal ? (r.right - r.left) / n
                                                                 : it->segw[i] * (r.right - r.left) / total
                                                         : it->segw[i];
    return it;
}

static Item *seg(const wchar_t *label, const wchar_t *sub, int id, int *value, const wchar_t *const *opts, int n)
{
    Item probe = { 0 };
    RECT r;
    probe.opts = opts; probe.nopts = n;
    r = row(label, sub, seg_measure(&probe), S(34), NULL);
    return seg_at(r, id, value, opts, n, 0);
}

/* The whole row is the switch: its name is as good a place to click as the switch itself. */
static Item *toggle(const wchar_t *label, const wchar_t *sub, int id, int *value)
{
    RECT whole;
    Item *it;
    row(label, sub, S(44), S(24), &whole);
    it = item(K_TOGGLE, whole.left, whole.top, whole.right - whole.left, whole.bottom - whole.top);
    it->id = id; it->value = value; it->text = label; it->sub = sub;
    return it;
}

static int button_w(const wchar_t *text, const wchar_t *icon)
{
    return tw(F_BOLD, text) + S(32) + (icon ? S(24) : 0);
}

static Item *button_at(int x, int y, int w, int h, const wchar_t *text, const wchar_t *icon, int id, int style)
{
    Item *it = item(K_BUTTON, x, y, w, h);
    it->text = text; it->sub = icon; it->id = id; it->style = style;
    return it;
}

typedef struct { const wchar_t *text, *icon; int id, style; } Btn;

/* Buttons side by side from the card's left, on a new line where they do not fit. */
static void buttons(const Btn *b, int n)
{
    const int pad = S(20), h = S(36);
    int i, x = L.x + pad;
    for (i = 0; i < n; i++) {
        const int w = button_w(b[i].text, b[i].icon);
        if (x > L.x + pad && x + w > L.x + L.w - pad) { x = L.x + pad; L.y += h + S(8); }
        button_at(x, L.y, w, h, b[i].text, b[i].icon, b[i].id, b[i].style);
        x += w + S(8);
    }
    L.y += h + S(12);
}

/* ---- the pages ------------------------------------------------------------------------------------------------- */

static void page_home(void)
{
    static const wchar_t *const hero[2][4] = {
        { L"Choose Your ISO to Begin", L"ISO Chosen — Install to Continue", L"Disc Copied — Compile to Finish",
          L"Ready to Play" },
        { L"Escolha a sua ISO para começar", L"ISO escolhida — instale para continuar", L"Disco copiado — falta compilar",
          L"Pronto para jogar" } };
    const int st = install_state(), pad = S(20);
    const wchar_t *steps[3];
    int i, x;
    Item *it;

    /* how far the installation got, in large type, and its three steps */
    s_card = s_n;
    item(K_CARD, L.x, L.y, L.w, 0)->style = 1;
    L.y += S(20);
    text_item(L.x + pad, L.y, L.w - 2 * pad, S(16), TR(L"INSTALLATION", L"INSTALAÇÃO"), F_CAPS, C_GOLD, DT_SINGLELINE);
    L.y += S(22);
    text_item(L.x + pad, L.y, L.w - 2 * pad, S(34), hero[s_lang][st], F_HERO, C_TEXT, DT_SINGLELINE | DT_END_ELLIPSIS);
    L.y += S(48);
    steps[0] = TR(L"Choose ISO", L"Escolher a ISO");
    steps[1] = TR(L"Install and Compile", L"Instalar e compilar");
    steps[2] = TR(L"Play", L"Jogar");
    for (i = 0, x = L.x + pad; i < 3; i++) {
        const int w = S(32) + tw(F_BODY, steps[i]);
        if (i) { text_item(x, L.y, S(28), S(24), L"—", F_BODY, C_FAINT, DT_SINGLELINE | DT_CENTER | DT_VCENTER); x += S(28); }
        it = item(K_STEP, x, L.y, w, S(24));
        it->text = steps[i]; it->id = i + 1;
        it->arg = i == 0 ? (st >= 1 ? 2 : 1) : i == 1 ? (st >= 3 ? 2 : st >= 1 ? 1 : 0) : (st >= 3 ? 1 : 0);
        x += w;
    }
    L.y += S(24) + S(12);
    card_end();

    card_begin(TR(L"YOUR DISC", L"O SEU DISCO"));
    L.y += S(6);
    text_item(L.x + pad, L.y, L.w - 2 * pad, S(20), *s_disc ? s_disc : TR(L"No ISO chosen.", L"Nenhuma ISO escolhida."),
              F_BODY, *s_disc ? C_TEXT : C_MUTED, DT_SINGLELINE | DT_PATH_ELLIPSIS);
    L.y += S(24);
    {
        const wchar_t *note = TR(L"Your own disc of the original Xbox release (NTSC-U): an ISO, or the default.xbe of a disc "
                                 L"already extracted. The game is copied, translated and compiled on this PC, which takes "
                                 L"several minutes.",
                                 L"O seu próprio disco da versão do Xbox original (NTSC-U): uma ISO, ou o default.xbe de um "
                                 L"disco já extraído. O jogo é copiado, traduzido e compilado neste PC, o que leva vários "
                                 L"minutos.");
        const int h = wrap_h(F_SMALL, note, L.w - 2 * pad);
        text_item(L.x + pad, L.y, L.w - 2 * pad, h, note, F_SMALL, C_MUTED, DT_WORDBREAK);
        L.y += h + S(16);
    }
    {
        Btn b[2] = { { TR(L"Choose ISO…", L"Escolher a ISO…"), IC_FOLDER, A_CHOOSE, B_SECONDARY },
                     { TR(L"Install and Compile", L"Instalar e compilar"), IC_BUILD, A_INSTALL, B_SECONDARY } };
        if (st == 0) b[0].style = B_PRIMARY;
        if (st == 1 || st == 2) b[1].style = B_PRIMARY;
        buttons(b, 2);
    }
    card_end();

    card_begin(L"STEAM");
    {
        const wchar_t *text = TR(L"Add to Steam", L"Adicionar à Steam");
        RECT r = row(TR(L"Non-Steam Game Shortcut", L"Atalho de jogo não-Steam"),
                     TR(L"Puts this launcher in your Steam library. Close Steam first: it rewrites its list of "
                        L"shortcuts when it closes.",
                        L"Coloca este launcher na sua biblioteca da Steam. Feche a Steam antes: ela reescreve a lista "
                        L"de atalhos ao fechar."),
                     button_w(text, IC_ADD), S(36), NULL);
        button_at(r.left, r.top, r.right - r.left, S(36), text, IC_ADD, A_STEAM, B_SECONDARY);
    }
    card_end();
}

static void page_video(void)
{
    static const wchar_t *modes[2];
    int i;
    modes[0] = TR(L"Window", L"Janela");
    modes[1] = TR(L"Borderless Fullscreen", L"Tela cheia sem bordas");
    for (i = 0; i < 4; i++) s_win_opts[i] = pf(L"%u × %u", win_width(cfg.aspect, i), k_win_h[i]);

    card_begin(TR(L"DISPLAY", L"TELA"));
    seg(TR(L"Display Mode", L"Modo de exibição"), TR(L"F11 switches during the game.", L"F11 alterna durante o jogo."),
        A_NONE, &cfg.mode, modes, 2);
    seg(TR(L"Aspect Ratio", L"Proporção da tela"),
        TR(L"4:3 is the original picture. The wider ones also widen the game's camera.",
           L"4:3 é a imagem original. As mais largas também ampliam a câmera do jogo."),
        A_RELAYOUT, &cfg.aspect, k_aspect_names, 3);
    seg(TR(L"Window Size", L"Tamanho da janela"), TR(L"Used in window mode.", L"Usado no modo janela."), A_NONE, &cfg.win,
        s_win_opts, 4);
    card_end();

    card_begin(TR(L"QUALITY", L"QUALIDADE"));
    seg(TR(L"Internal Resolution", L"Resolução interna"),
        TR(L"1440p is recommended; 480p is the original Xbox's. Lower it if the game runs slowly.",
           L"1440p é a recomendada; 480p é a do Xbox original. Diminua se o jogo ficar lento."),
        A_NONE, &cfg.res, k_height_names, 5);
    seg(L"Anti-Aliasing", TR(L"FXAA is light. SSAA is sharper and much heavier.",
                             L"FXAA é leve. SSAA é mais nítido e bem mais pesado."),
        A_NONE, &cfg.aa, k_aa_names[s_lang], 3);
    toggle(L"V-Sync", TR(L"No tearing in the picture.", L"Sem cortes na imagem."), A_NONE, &cfg.vsync);
    card_end();

    card_begin(TR(L"HD TEXTURES", L"TEXTURAS HD"));
    toggle(TR(L"Use HD Textures", L"Usar texturas HD"),
           !*s_pack ? TR(L"No pack found in the hd-textures folder. Get one with the buttons below, extract it there "
                         L"and open the launcher again.",
                         L"Nenhum pacote encontrado na pasta hd-textures. Baixe um pelos botões abaixo, extraia nela e "
                         L"abra o launcher de novo.")
           : *s_remake ? TR(L"Found: the remake of the arts and menus, and the PS2 version's pack.",
                            L"Encontrados: o remake de artes e menus e o pacote da versão de PS2.")
                       : TR(L"Found: the PS2 version's pack.", L"Encontrado: o pacote da versão de PS2."),
           A_NONE, &cfg.hd)->disabled = !*s_pack;
    L.y += S(4);
    {
        Btn b[3] = { { TR(L"Open Folder", L"Abrir a pasta"), IC_FOLDER, A_HD_FOLDER, B_SECONDARY },
                     { TR(L"Get HD Pack (PS2)", L"Baixar pacote HD (PS2)"), IC_OPEN, A_HD_PS2, B_SECONDARY },
                     { TR(L"Get HD Remake", L"Baixar remake HD"), IC_OPEN, A_HD_REMAKE, B_SECONDARY } };
        buttons(b, 3);
    }
    card_end();
}

static void page_game(void)
{
    static const wchar_t *players[2], *p1[2];
    players[0] = TR(L"1 Player", L"1 jogador");
    players[1] = TR(L"2 Players (Local)", L"2 jogadores (local)");
    p1[0] = TR(L"Keyboard + Controller 1", L"Teclado + controle 1");
    p1[1] = TR(L"Keyboard Only", L"Só teclado");

    card_begin(TR(L"PLAYERS", L"JOGADORES"));
    seg(TR(L"Players", L"Jogadores"),
        TR(L"The second controller joins by pressing Start.", L"O segundo controle entra apertando Start."),
        A_NONE, &cfg.players, players, 2);
    seg(TR(L"Player 1 Uses", L"Jogador 1 usa"),
        TR(L"With Keyboard Only, controller 1 becomes player 2: two can play with a single controller.",
           L"Com Só teclado, o controle 1 vira o jogador 2: dá para jogar em dois com um controle só."),
        A_NONE, &cfg.p1, p1, 2);
    card_end();

    card_begin(TR(L"SESSION", L"SESSÃO"));
    toggle(TR(L"Skip Intro Movies", L"Pular vídeos de abertura"),
           TR(L"Goes straight to the title screen.", L"Vai direto para a tela de título."), A_NONE, &cfg.intros);
    toggle(TR(L"Unlock Everything", L"Desbloquear tudo"),
           TR(L"A test session: it plays on a copy of your saves, and nothing of it is saved to your profile.",
              L"Sessão de teste: joga em uma cópia dos seus saves, e nada dela é salvo no seu perfil."),
           A_NONE, &cfg.unlock);
    card_end();
}

static void bind_rows(int from, int to)
{
    const int w = S(124), h = S(34), gap = S(8);
    int a, s;
    for (a = from; a <= to; a++) {
        RECT r = row(k_ka_names[s_lang][a], k_ka_sub[s_lang][a], 3 * w + 2 * gap, h, NULL);
        const int cw = (r.right - r.left - 2 * gap) / 3;
        for (s = 0; s < KA_SLOTS; s++) {
            Item *it = item(K_BIND, r.left + s * (cw + gap), r.top, cw, h);
            it->arg = a * KA_SLOTS + s;
        }
    }
}

static void page_controls(void)
{
    static const wchar_t *tabs[2], *speeds[3];
    RECT r;
    int i;
    tabs[0] = TR(L"Keyboard & Mouse", L"Teclado e mouse");
    tabs[1] = TR(L"Controller", L"Controle");
    speeds[0] = TR(L"Slow", L"Lento"); speeds[1] = L"Normal"; speeds[2] = TR(L"Fast", L"Rápido");

    SetRect(&r, L.x, L.y, L.x + S(360), L.y + S(36));
    seg_at(r, A_TAB, &s_tab, tabs, 2, 1);
    L.y += S(36) + S(16);

    if (s_tab == 0) {
        const wchar_t *note = TR(L"Click a key to change it, then press the new key, click a mouse button or turn the wheel. "
                                 L"Right-click (or Delete) clears it. Each action takes up to three, and a key does one "
                                 L"thing only: binding it again moves it. F11 stays the fullscreen switch.",
                                 L"Clique em uma tecla para trocá-la e aperte a nova tecla, clique com um botão do mouse ou "
                                 L"gire a roda. Clique direito (ou Delete) limpa. Cada ação aceita até três, e uma tecla faz "
                                 L"uma coisa só: usá-la de novo a move. F11 continua alternando a tela cheia.");
        const int h = wrap_h(F_SMALL, note, L.w - S(8));
        text_item(L.x + S(4), L.y, L.w - S(8), h, note, F_SMALL, C_MUTED, DT_WORDBREAK);
        L.y += h + S(16);

        card_begin(TR(L"MOVEMENT", L"MOVIMENTO"));
        bind_rows(KA_UP, KA_RIGHT);
        card_end();
        card_begin(TR(L"ATTACKS", L"ATAQUES"));
        bind_rows(KA_X, KA_B);
        card_end();
        card_begin(TR(L"ACTIONS", L"AÇÕES"));
        bind_rows(KA_RT, KA_RB);
        card_end();
        card_begin(TR(L"MENUS & SYSTEM", L"MENUS E SISTEMA"));
        bind_rows(KA_START, KA_R3);
        card_end();

        card_begin(L"MOUSE");
        toggle(TR(L"Mouse Movement Walks the Menus", L"Mover o mouse anda pelos menus"),
               TR(L"Moving the mouse steps through the game's menus, like the D-pad.",
                  L"Mover o mouse percorre os menus do jogo, como o direcional."),
               A_RELAYOUT, &cfg.mouse_menus);
        seg(TR(L"Menu Speed", L"Velocidade nos menus"),
            TR(L"How far the mouse moves for one step.", L"Quanto o mouse precisa andar para um passo."),
            A_NONE, &cfg.mouse_speed, speeds, 3)->disabled = !cfg.mouse_menus;
        card_end();

        {
            Btn b = { TR(L"Restore Default Keys", L"Restaurar teclas padrão"), IC_RESET, A_KEYS_RESET, B_SECONDARY };
            L.x -= S(20);
            buttons(&b, 1);
            L.x += S(20);
        }
    } else {
        const wchar_t *note = TR(L"For each button the game uses, choose the button of your controller that does it. It works "
                                 L"for Xbox and PlayStation controllers (DualShock 4 and DualSense), for both players.",
                                 L"Para cada botão que o jogo usa, escolha o botão do seu controle que faz a função. Vale para "
                                 L"controles de Xbox e de PlayStation (DualShock 4 e DualSense), para os dois jogadores.");
        const int h = wrap_h(F_SMALL, note, L.w - S(8));
        text_item(L.x + S(4), L.y, L.w - S(8), h, note, F_SMALL, C_MUTED, DT_WORDBREAK);
        L.y += h + S(16);

        card_begin(TR(L"BUTTONS", L"BOTÕES"));
        for (i = 0; i < PAD_BUTTONS; i++) {
            RECT c = row(k_pad_actions[s_lang][i], pf(TR(L"Game button: %s", L"Botão do jogo: %s"), k_pad_names[s_lang][i]),
                         S(230), S(34), NULL);
            Item *it = item(K_SELECT, c.left, c.top, c.right - c.left, S(34));
            it->value = &s_map[i]; it->opts = k_pad_names[s_lang]; it->nopts = PAD_BUTTONS;
        }
        card_end();
        {
            Btn b = { TR(L"Restore Default Buttons", L"Restaurar botões padrão"), IC_RESET, A_PAD_RESET, B_SECONDARY };
            L.x -= S(20);
            buttons(&b, 1);
            L.x += S(20);
        }
    }
}

static int focusable(int i)
{
    return i >= 0 && i < s_n && s_items[i].kind >= K_NAV && !s_items[i].disabled;
}

static void clamp_scroll(void)
{
    const int max = s_content_h - (s_view.bottom - s_view.top);
    if (s_scroll > max) s_scroll = max;
    if (s_scroll < 0) s_scroll = 0;
}

/* Every item of the window, for the page shown: the sidebar, the page's title, the page, the Play button. */
static void layout(void)
{
    static const wchar_t *const langs[2] = { L"English", L"Português" };
    static const wchar_t *const icons[4] = { IC_HOME, IC_VIDEO, IC_GAME, IC_KEYS };
    const wchar_t *names[4], *subs[4];
    const int side = S(232), foot = S(84), head = S(104);
    RECT c, r;
    int i, y;

    names[0] = TR(L"Home", L"Início"); names[1] = TR(L"Video", L"Vídeo");
    names[2] = TR(L"Game", L"Jogo"); names[3] = TR(L"Controls", L"Controles");
    subs[0] = TR(L"Install the game from your own disc, then play.", L"Instale o jogo a partir do seu próprio disco e jogue.");
    subs[1] = TR(L"Resolution, window and HD textures.", L"Resolução, janela e texturas HD.");
    subs[2] = TR(L"Players and how a session starts.", L"Jogadores e como a sessão começa.");
    subs[3] = TR(L"Every key, mouse button and controller button.", L"Todas as teclas, botões do mouse e do controle.");

    GetClientRect(s_wnd, &c);
    s_n = 0;
    s_pool_n = 0;
    s_fixed = 1;
    for (i = 0, y = S(104); i < 4; i++, y += S(46)) {
        Item *it = item(K_NAV, S(12), y, side - S(24), S(42));
        it->arg = i; it->text = names[i]; it->sub = icons[i];
    }
    text_item(S(20), c.bottom - S(112), side - S(40), S(16), L"LANGUAGE / IDIOMA", F_CAPS, C_FAINT, DT_SINGLELINE);
    SetRect(&r, S(16), c.bottom - S(88), side - S(16), c.bottom - S(54));
    seg_at(r, A_LANG, &s_lang, langs, 2, 1);
    text_item(S(20), c.bottom - S(38), side - S(40), S(16), pf(TR(L"Version %s", L"Versão %s"), s_version), F_SMALL, C_FAINT,
              DT_SINGLELINE);

    text_item(side + S(32), S(26), c.right - side - S(64), S(38), names[s_page], F_TITLE, C_TEXT, DT_SINGLELINE);
    text_item(side + S(32), S(66), c.right - side - S(64), S(20), subs[s_page], F_BODY, C_MUTED,
              DT_SINGLELINE | DT_END_ELLIPSIS);

    s_fixed = 0;
    SetRect(&s_view, side, head, c.right, c.bottom - foot);
    L.x = side + S(32); L.w = c.right - side - S(64); L.y = S(2);
    switch (s_page) {
    case 0: page_home(); break;
    case 1: page_video(); break;
    case 2: page_game(); break;
    default: page_controls(); break;
    }
    s_content_h = L.y + S(8);
    clamp_scroll();

    s_fixed = 1;
    button_at(c.right - S(32) - S(184), c.bottom - foot + (foot - S(52)) / 2, S(184), S(52), TR(L"PLAY", L"JOGAR"), IC_PLAY,
              A_PLAY, B_PLAY);
    if (!focusable(s_focus)) s_focus = -1;
    s_hot = s_press = -1;
    InvalidateRect(s_wnd, NULL, FALSE);
}

/* ---- drawing --------------------------------------------------------------------------------------------------- */

static void *round_path(float x, float y, float w, float h, float r)
{
    void *p = NULL;
    GdipCreatePath(0, &p);
    if (r * 2 > h) r = h / 2;
    if (r * 2 > w) r = w / 2;
    if (r < 0.5f) GdipAddPathRectangle(p, x, y, w, h);
    else {
        const float d = r * 2;
        GdipAddPathArc(p, x, y, d, d, 180, 90);
        GdipAddPathArc(p, x + w - d, y, d, d, 270, 90);
        GdipAddPathArc(p, x + w - d, y + h - d, d, d, 0, 90);
        GdipAddPathArc(p, x, y + h - d, d, d, 90, 90);
        GdipClosePathFigure(p);
    }
    return p;
}

static void fill_round(RECT r, int radius, unsigned argb)
{
    void *p = round_path((float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top), (float)radius);
    void *b = NULL;
    GdipCreateSolidFill(argb, &b);
    GdipFillPath(s_g, b, p);
    GdipDeleteBrush(b);
    GdipDeletePath(p);
}

/* from one colour to another, left to right or top to bottom */
static void grad_round(RECT r, int radius, unsigned from, unsigned to, int vertical)
{
    const GdipRectF f = { (float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top) };
    void *p = round_path(f.x, f.y, f.w, f.h, (float)radius), *b = NULL;
    GdipCreateLineBrushFromRect(&f, from, to, vertical, 3, &b);
    if (b) { GdipFillPath(s_g, b, p); GdipDeleteBrush(b); }
    GdipDeletePath(p);
}

/* a line of `width` just inside r */
static void stroke_round(RECT r, int radius, unsigned argb, int width)
{
    const float half = width / 2.0f;
    void *p = round_path(r.left + half, r.top + half, r.right - r.left - (float)width, r.bottom - r.top - (float)width,
                         radius - half);
    void *pen = NULL;
    GdipCreatePen1(argb, (float)width, 2, &pen);
    GdipDrawPath(s_g, pen, p);
    GdipDeletePen(pen);
    GdipDeletePath(p);
}

static void fill_rect(RECT r, unsigned argb) { fill_round(r, 0, argb); }

static void dot(int cx, int cy, int radius, unsigned argb)
{
    void *b = NULL;
    GdipCreateSolidFill(argb, &b);
    GdipFillEllipse(s_g, b, (float)(cx - radius), (float)(cy - radius), radius * 2.0f, radius * 2.0f);
    GdipDeleteBrush(b);
}

static void ring(int cx, int cy, int radius, unsigned argb, int width)
{
    void *pen = NULL;
    GdipCreatePen1(argb, (float)width, 2, &pen);
    GdipDrawEllipse(s_g, pen, cx - radius + width / 2.0f, cy - radius + width / 2.0f, radius * 2.0f - width,
                    radius * 2.0f - width);
    GdipDeletePen(pen);
}

static void dtext(const wchar_t *s, int font, int color, RECT r, UINT flags)
{
    int saved = 0;
    if (!s || !*s) return;
    GdipFlush(s_g, 1);
    if (s_clipping) { saved = SaveDC(s_bdc); IntersectClipRect(s_bdc, s_clip.left, s_clip.top, s_clip.right, s_clip.bottom); }
    SelectObject(s_bdc, s_fonts[font]);
    SetTextColor(s_bdc, cr((unsigned)color));
    SetBkMode(s_bdc, TRANSPARENT);
    SetTextCharacterExtra(s_bdc, font == F_CAPS ? S(1) : 0);
    DrawTextW(s_bdc, s, -1, &r, flags | DT_NOPREFIX);
    if (saved) RestoreDC(s_bdc, saved);
}

static RECT grown(RECT r, int d) { InflateRect(&r, d, d); return r; }

/* Where an item is in the window now. */
static RECT item_rect(const Item *it)
{
    RECT r = it->r;
    if (!it->fixed) OffsetRect(&r, 0, s_view.top - s_scroll);
    return r;
}

static void draw_button(const Item *it, RECT r, int hot, int down)
{
    const int play = it->style == B_PLAY, font = play ? F_PLAY : F_BOLD;
    const int iw = it->sub ? S(play ? 30 : 24) : 0, w = tw(font, it->text) + iw;
    RECT t = r;
    if (it->style == B_SECONDARY) {
        fill_round(r, S(8), OPAQUE_ARGB(down ? C_CTL : hot ? C_CTL_HOT : C_CTL));
        stroke_round(r, S(8), OPAQUE_ARGB(hot ? C_FAINT : C_CTL_LINE), 1);
    } else {
        const unsigned top = play ? (hot ? 0xFF7C3C : C_ACCENT_HI) : (hot ? C_ACCENT : C_ACCENT_LO);
        const unsigned bottom = play ? (hot ? C_ACCENT : C_ACCENT_LO) : (hot ? C_ACCENT_LO : C_ACCENT_DEEP);
        grad_round(r, S(play ? 12 : 8), OPAQUE_ARGB(down ? bottom : top), OPAQUE_ARGB(bottom), 1);
        stroke_round(r, S(play ? 12 : 8), 0x38FFFFFF, 1);
    }
    t.left = r.left + (r.right - r.left - w) / 2;
    if (it->sub) {
        RECT ic = t;
        ic.right = ic.left + iw - S(8);
        dtext(it->sub, play ? F_ICON : F_ICON_S, it->style == B_SECONDARY ? C_MUTED : 0xFFFFFF, ic,
              DT_SINGLELINE | DT_VCENTER | DT_CENTER);
        t.left += iw;
    }
    dtext(it->text, font, it->style == B_SECONDARY ? C_TEXT : 0xFFFFFF, t, DT_SINGLELINE | DT_VCENTER);
}

static void draw_item(int i)
{
    const Item *it = &s_items[i];
    const int hot = s_hot == i && !it->disabled, down = s_press == i && hot;
    RECT r = item_rect(it), t;
    int k, x;
    wchar_t label[64];

    switch (it->kind) {
    case K_CARD:
        if (it->style) {
            grad_round(r, S(12), OPAQUE_ARGB(0x3B170C), OPAQUE_ARGB(C_CARD), 0);
            stroke_round(r, S(12), OPAQUE_ARGB(0x4A2416), 1);
        } else {
            fill_round(r, S(12), OPAQUE_ARGB(C_CARD));
            stroke_round(r, S(12), OPAQUE_ARGB(C_LINE), 1);
        }
        break;
    case K_TEXT:
        dtext(it->text, it->font, it->color, r, (UINT)it->flags);
        break;
    case K_DIVIDER:
        fill_rect(r, OPAQUE_ARGB(C_ROW));
        break;
    case K_STEP: {
        const int cx = r.left + S(11), cy = (r.top + r.bottom) / 2;
        t = r; t.right = r.left + S(22);
        if (it->arg == 2) {
            dot(cx, cy, S(11), OPAQUE_ARGB(C_OK));
            dtext(IC_CHECK, F_ICON_S, 0x06140D, t, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
        } else {
            ring(cx, cy, S(11), OPAQUE_ARGB(it->arg ? C_GOLD : C_FAINT), S(2) > 1 ? S(2) * 3 / 4 : 1);
            swprintf(label, 64, L"%d", it->id);
            dtext(label, F_SMALL, it->arg ? C_GOLD : C_FAINT, t, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
        }
        t = r; t.left += S(30);
        dtext(it->text, F_BODY, it->arg ? C_TEXT : C_MUTED, t, DT_SINGLELINE | DT_VCENTER);
        break;
    }
    case K_NAV: {
        const int on = it->arg == s_page;
        if (on || hot) fill_round(r, S(8), OPAQUE_ARGB(on ? C_CTL : C_ROW));
        if (on) {
            t = r; t.right = t.left + S(3); t.top += S(11); t.bottom -= S(11);
            fill_round(t, S(2), OPAQUE_ARGB(C_ACCENT));
        }
        t = r; t.left += S(16); t.right = t.left + S(20);
        dtext(it->sub, F_ICON, on ? C_ACCENT_HI : hot ? C_TEXT : C_MUTED, t, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
        t = r; t.left += S(48);
        dtext(it->text, on ? F_BOLD : F_BODY, on || hot ? C_TEXT : C_MUTED, t, DT_SINGLELINE | DT_VCENTER);
        break;
    }
    case K_BUTTON:
        draw_button(it, r, hot, down);
        break;
    case K_TOGGLE: {
        const int on = *it->value && !it->disabled, tw_ = S(40), th_ = S(22);
        const int lw = r.right - r.left - S(24) - S(64);
        int sh = it->sub ? wrap_h(F_SMALL, it->sub, lw) : 0, th = S(20) + (it->sub ? S(2) + sh : 0);
        int ty = r.top + (r.bottom - r.top - th) / 2;
        RECT sw;
        if (hot) fill_round(r, S(8), OPAQUE_ARGB(C_ROW));
        SetRect(&t, r.left + S(12), ty, r.left + S(12) + lw, ty + S(20));
        dtext(it->text, F_BODY, it->disabled ? C_FAINT : C_TEXT, t, DT_SINGLELINE | DT_END_ELLIPSIS);
        if (it->sub) {
            SetRect(&t, r.left + S(12), ty + S(22), r.left + S(12) + lw, ty + S(22) + sh);
            dtext(it->sub, F_SMALL, C_MUTED, t, DT_WORDBREAK);
        }
        SetRect(&sw, r.right - S(12) - tw_, (r.top + r.bottom - th_) / 2, r.right - S(12), (r.top + r.bottom - th_) / 2 + th_);
        fill_round(sw, th_ / 2, OPAQUE_ARGB(on ? C_ACCENT_LO : C_CTL_HOT));
        if (!on) stroke_round(sw, th_ / 2, OPAQUE_ARGB(it->disabled ? C_CTL_LINE : C_FAINT), 1);
        dot(on ? sw.right - th_ / 2 : sw.left + th_ / 2, (sw.top + sw.bottom) / 2, th_ / 2 - S(4),
            OPAQUE_ARGB(on ? 0xFFFFFF : it->disabled ? C_FAINT : C_MUTED));
        break;
    }
    case K_SEG:
        fill_round(r, S(9), OPAQUE_ARGB(C_CTL));
        stroke_round(r, S(9), OPAQUE_ARGB(C_CTL_LINE), 1);
        for (k = 0, x = r.left; k < it->nopts; k++) {
            const int on = *it->value == k, over = hot && s_hot_part == k;
            SetRect(&t, x, r.top, k == it->nopts - 1 ? r.right : x + it->segw[k], r.bottom);
            x += it->segw[k];
            if (on && !it->disabled) {
                fill_round(grown(t, -S(3)), S(6), OPAQUE_ARGB(C_SELECTED));
                stroke_round(grown(t, -S(3)), S(6), OPAQUE_ARGB(C_ACCENT), 1);
            } else if (on) {
                fill_round(grown(t, -S(3)), S(6), OPAQUE_ARGB(C_CTL_HOT));
            } else if (over) {
                fill_round(grown(t, -S(3)), S(6), OPAQUE_ARGB(C_CTL_HOT));
            }
            dtext(it->opts[k], on ? F_BOLD : F_BODY, it->disabled ? C_FAINT : on || over ? C_TEXT : C_MUTED, t,
                  DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_END_ELLIPSIS);
        }
        break;
    case K_SELECT: {
        const int changed = it->value >= s_map && it->value < s_map + PAD_BUTTONS && *it->value != (int)(it->value - s_map);
        fill_round(r, S(8), OPAQUE_ARGB(hot || s_pop == i ? C_CTL_HOT : C_CTL));
        stroke_round(r, S(8), OPAQUE_ARGB(changed ? C_ACCENT : hot || s_pop == i ? C_FAINT : C_CTL_LINE), 1);
        t = r; t.left += S(14); t.right -= S(34);
        dtext(it->opts[*it->value], F_BODY, C_TEXT, t, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
        t = r; t.left = r.right - S(34);
        dtext(IC_DOWN, F_ICON_S, C_MUTED, t, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
        break;
    }
    case K_BIND: {
        const int a = it->arg / KA_SLOTS, s = it->arg % KA_SLOTS;
        const unsigned id = s_keys[a][s];
        fill_round(r, S(8), OPAQUE_ARGB(hot ? C_CTL_HOT : id ? C_CTL : C_CARD));
        stroke_round(r, S(8), OPAQUE_ARGB(id && !key_in_default(a, id) ? C_ACCENT : hot ? C_FAINT : C_CTL_LINE), 1);
        src_label(id, label, 64);
        t = grown(r, -S(6));
        if (id) dtext(label, F_BOLD, C_TEXT, t, DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_END_ELLIPSIS);
        else dtext(hot ? IC_ADD : L"—", hot ? F_ICON_S : F_BODY, C_FAINT, t, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
        break;
    }
    }
    if (s_focus == i && s_focus_ring && s_cap < 0)
        stroke_round(grown(r, S(3)), S(it->kind == K_BUTTON && it->style == B_PLAY ? 15 : 11), OPAQUE_ARGB(C_FOCUS), S(2));
}

/* The list open under a K_SELECT: below it, or as far up as it takes to fit in the window. */
static RECT pop_rect(void)
{
    const Item *it = &s_items[s_pop];
    RECT r = item_rect(it), c, p;
    const int h = it->nopts * S(32) + S(12);
    GetClientRect(s_wnd, &c);
    SetRect(&p, r.left, r.bottom + S(4), r.right, r.bottom + S(4) + h);
    if (p.bottom > c.bottom - S(8)) OffsetRect(&p, 0, c.bottom - S(8) - p.bottom);
    if (p.top < S(8)) OffsetRect(&p, 0, S(8) - p.top);
    return p;
}

static void draw_pop(void)
{
    const Item *it = &s_items[s_pop];
    RECT p = pop_rect(), t;
    int k;
    fill_round(grown(p, S(1)), S(11), 0x90000000);
    fill_round(p, S(10), OPAQUE_ARGB(C_CTL));
    stroke_round(p, S(10), OPAQUE_ARGB(C_FAINT), 1);
    for (k = 0; k < it->nopts; k++) {
        SetRect(&t, p.left + S(6), p.top + S(6) + k * S(32), p.right - S(6), p.top + S(6) + (k + 1) * S(32));
        if (k == s_pop_hot) fill_round(t, S(6), OPAQUE_ARGB(C_CTL_HOT));
        if (k == *it->value) {
            RECT ic = t;
            ic.left = t.right - S(30);
            dtext(IC_CHECK, F_ICON_S, C_ACCENT_HI, ic, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
        }
        t.left += S(10); t.right -= S(30);
        dtext(it->opts[k], k == *it->value ? F_BOLD : F_BODY, C_TEXT, t, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    }
}

/* The box a key is captured in, and its two buttons. */
static void cap_rects(RECT *card, RECT *unbind, RECT *cancel)
{
    RECT c;
    const int w = S(500), h = S(264);
    GetClientRect(s_wnd, &c);
    SetRect(card, (c.right - w) / 2, (c.bottom - h) / 2, (c.right - w) / 2 + w, (c.bottom - h) / 2 + h);
    SetRect(cancel, card->right - S(24) - S(110), card->bottom - S(24) - S(36), card->right - S(24), card->bottom - S(24));
    SetRect(unbind, card->left + S(24), cancel->top, card->left + S(24) + S(130), cancel->bottom);
}

static void draw_capture(void)
{
    const Item *it = &s_items[s_cap];
    const int a = it->arg / KA_SLOTS, s = it->arg % KA_SLOTS;
    const DWORD now = GetTickCount();
    const int left = (int)(s_cap_until - now) > 0 ? (int)(s_cap_until - now) : 0;
    Item b = { 0 };
    RECT c, card, unbind, cancel, t;
    wchar_t now_is[64], line[160];

    GetClientRect(s_wnd, &c);
    fill_rect(c, 0xB8000000);
    cap_rects(&card, &unbind, &cancel);
    fill_round(card, S(14), OPAQUE_ARGB(0x1A1A20));
    stroke_round(card, S(14), OPAQUE_ARGB(C_ACCENT), 1);
    SetRect(&t, card.left + S(24), card.top + S(24), card.right - S(24), card.top + S(40));
    swprintf(line, 160, TR(L"KEY %d OF %d", L"TECLA %d DE %d"), s + 1, KA_SLOTS);
    dtext(line, F_CAPS, C_GOLD, t, DT_SINGLELINE);
    SetRect(&t, card.left + S(24), card.top + S(46), card.right - S(24), card.top + S(80));
    dtext(k_ka_names[s_lang][a], F_HERO, C_TEXT, t, DT_SINGLELINE | DT_END_ELLIPSIS);
    SetRect(&t, card.left + S(24), card.top + S(90), card.right - S(24), card.top + S(132));
    dtext(TR(L"Press a key. Or, with the pointer inside this box, click a mouse button or turn the wheel.",
             L"Aperte uma tecla. Ou, com o ponteiro dentro desta caixa, clique com um botão do mouse ou gire a roda."),
          F_BODY, C_TEXT, t, DT_WORDBREAK);
    SetRect(&t, card.left + S(24), card.top + S(138), card.right - S(24), card.top + S(158));
    if (*s_cap_msg) dtext(s_cap_msg, F_SMALL, C_GOLD, t, DT_SINGLELINE | DT_END_ELLIPSIS);
    else {
        src_label(s_keys[a][s], now_is, 64);
        swprintf(line, 160, TR(L"Now: %s", L"Agora: %s"), *now_is ? now_is : TR(L"nothing", L"nada"));
        dtext(line, F_SMALL, C_MUTED, t, DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    /* the time left before it closes by itself, so that the keyboard alone can leave it */
    SetRect(&t, card.left + S(24), card.top + S(172), card.right - S(24), card.top + S(172) + S(3));
    fill_round(t, S(2), OPAQUE_ARGB(C_CTL_HOT));
    t.right = t.left + MulDiv(t.right - t.left, left, CAPTURE_MS);
    fill_round(t, S(2), OPAQUE_ARGB(C_ACCENT));

    b.kind = K_BUTTON; b.style = B_SECONDARY;
    b.text = TR(L"Clear Key", L"Limpar tecla");
    draw_button(&b, unbind, s_cap_hot == 1, 0);
    b.text = TR(L"Cancel", L"Cancelar");
    draw_button(&b, cancel, s_cap_hot == 2, 0);
    SetRect(&t, unbind.right + S(12), unbind.top, cancel.left - S(12), unbind.bottom);
    swprintf(line, 160, TR(L"Closes in %d s", L"Fecha em %d s"), (left + 999) / 1000);
    dtext(line, F_SMALL, C_FAINT, t, DT_SINGLELINE | DT_VCENTER | DT_CENTER);
}

/* The scrolled page's thumb, when the page is taller than its place. */
static int thumb_rect(RECT *track, RECT *thumb)
{
    const int view = s_view.bottom - s_view.top;
    int h, y;
    if (s_content_h <= view) return 0;
    SetRect(track, s_view.right - S(14), s_view.top + S(4), s_view.right - S(8), s_view.bottom - S(4));
    h = MulDiv(track->bottom - track->top, view, s_content_h);
    if (h < S(32)) h = S(32);
    y = track->top + MulDiv(track->bottom - track->top - h, s_scroll, s_content_h - view);
    SetRect(thumb, track->left, y, track->right, y + h);
    return 1;
}

static void paint(HDC dc)
{
    static const wchar_t *const status[2][4] = {
        { L"Not installed", L"Not installed", L"Not compiled yet", L"Installed: ready to play" },
        { L"Não instalado", L"Não instalado", L"Falta compilar", L"Instalado: pronto para jogar" } };
    const int side = S(232), foot = S(84), st = install_state();
    RECT c, r, track, thumb;
    int i;

    GetClientRect(s_wnd, &c);
    if (c.right != s_bw || c.bottom != s_bh || !s_bbmp) {
        BITMAPINFO bi = { 0 };
        void *bits;
        if (s_bbmp) DeleteObject(s_bbmp);
        bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
        bi.bmiHeader.biWidth = c.right > 0 ? c.right : 1;
        bi.bmiHeader.biHeight = -(c.bottom > 0 ? c.bottom : 1);
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        s_bbmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
        SelectObject(s_bdc, s_bbmp);
        s_bw = c.right; s_bh = c.bottom;
    }
    s_g = NULL;
    if (GdipCreateFromHDC(s_bdc, &s_g) || !s_g) return;
    GdipSetSmoothingMode(s_g, 4);
    GdipSetPixelOffsetMode(s_g, 4);

    /* the surfaces: the page, the sidebar with an ember glow behind the name, the bar the Play button is on */
    fill_rect(c, OPAQUE_ARGB(C_BG));
    SetRect(&r, 0, 0, side, c.bottom);
    fill_rect(r, OPAQUE_ARGB(C_SIDE));
    SetRect(&r, 0, 0, side, S(220));
    grad_round(r, 0, 0x30E8502A, 0x00E8502A, 1);
    SetRect(&r, side - 1, 0, side, c.bottom);
    fill_rect(r, OPAQUE_ARGB(C_LINE));
    SetRect(&r, side, c.bottom - foot, c.right, c.bottom);
    fill_rect(r, OPAQUE_ARGB(C_SIDE));
    r.bottom = r.top + 1;
    fill_rect(r, OPAQUE_ARGB(C_LINE));

    /* the name */
    GdipFlush(s_g, 1);
    if (s_icon) DrawIconEx(s_bdc, S(20), S(26), s_icon, S(44), S(44), 0, NULL, DI_NORMAL);
    SetRect(&r, S(76), S(27), side - S(8), S(45));
    dtext(L"MORTAL KOMBAT", F_CAPS, C_GOLD, r, DT_SINGLELINE);
    SetRect(&r, S(75), S(44), side - S(4), S(72));
    dtext(L"ARMAGEDDON", F_BRAND, C_TEXT, r, DT_SINGLELINE);

    /* the installation and, when it is ready, what the game will start with */
    dot(side + S(40), c.bottom - foot + S(32), S(5), OPAQUE_ARGB(st == 3 ? C_OK : C_GOLD));
    SetRect(&r, side + S(56), c.bottom - foot + S(21), c.right - S(240), c.bottom - foot + S(43));
    dtext(status[s_lang][st], F_BOLD, C_TEXT, r, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    SetRect(&r, side + S(56), c.bottom - foot + S(44), c.right - S(240), c.bottom - foot + S(64));
    if (*s_toast) dtext(s_toast, F_SMALL, C_GOLD, r, DT_SINGLELINE | DT_END_ELLIPSIS);
    else if (st == 3) {
        wchar_t line[160];
        swprintf(line, 160, L"%s  ·  %s  ·  %s  ·  %s", k_height_names[cfg.res], k_aspect_names[cfg.aspect],
                 cfg.mode ? TR(L"Fullscreen", L"Tela cheia") : TR(L"Window", L"Janela"),
                 cfg.players ? TR(L"2 players", L"2 jogadores") : TR(L"1 player", L"1 jogador"));
        dtext(line, F_SMALL, C_MUTED, r, DT_SINGLELINE | DT_END_ELLIPSIS);
    } else
        dtext(TR(L"On the Home page, choose your ISO and install.", L"Na página Início, escolha a sua ISO e instale."),
              F_SMALL, C_MUTED, r, DT_SINGLELINE | DT_END_ELLIPSIS);

    /* the page, cut to its place; then what stays put */
    GdipSetClipRectI(s_g, s_view.left, s_view.top, s_view.right - s_view.left, s_view.bottom - s_view.top, 0);
    s_clip = s_view;
    s_clipping = 1;
    for (i = 0; i < s_n; i++) {
        if (s_items[i].fixed) continue;
        r = item_rect(&s_items[i]);
        if (r.bottom + S(8) >= s_view.top && r.top - S(8) <= s_view.bottom) draw_item(i);
    }
    s_clipping = 0;
    GdipResetClip(s_g);
    for (i = 0; i < s_n; i++)
        if (s_items[i].fixed) draw_item(i);
    if (thumb_rect(&track, &thumb)) fill_round(thumb, S(3), OPAQUE_ARGB(s_thumb_drag ? C_FAINT : C_CTL_LINE));

    if (s_pop >= 0) draw_pop();
    if (s_cap >= 0) draw_capture();

    GdipFlush(s_g, 1);
    GdipDeleteGraphics(s_g);
    s_g = NULL;
    BitBlt(dc, 0, 0, c.right, c.bottom, s_bdc, 0, 0, SRCCOPY);
}

/* ---- the pointer and the keys ---------------------------------------------------------------------------------- */

static void repaint(void) { InvalidateRect(s_wnd, NULL, FALSE); }

static void toast(const wchar_t *text)
{
    swprintf(s_toast, sizeof(s_toast) / sizeof(s_toast[0]), L"%s", text);
    SetTimer(s_wnd, 2, 6000, NULL);
    repaint();
}

/* The item under a point, and which part of a segmented choice. */
static int hit(int x, int y, int *part)
{
    const POINT p = { x, y };
    int i, k, sx;
    *part = -1;
    for (i = s_n - 1; i >= 0; i--) {
        const Item *it = &s_items[i];
        RECT r = item_rect(it);
        if (!focusable(i) || !PtInRect(&r, p) || (!it->fixed && !PtInRect(&s_view, p))) continue;
        if (it->kind == K_SEG)
            for (k = 0, sx = r.left; k < it->nopts; sx += it->segw[k], k++)
                if (x >= sx && (x < sx + it->segw[k] || k == it->nopts - 1)) { *part = k; break; }
        return i;
    }
    return -1;
}

/* The focused item brought inside the page's place, clear of the bars above and below it. */
static void reveal(int i)
{
    const Item *it = &s_items[i];
    const int view = s_view.bottom - s_view.top;
    if (i < 0 || it->fixed) return;
    if (it->r.top - S(24) < s_scroll) s_scroll = it->r.top - S(24);
    if (it->r.bottom + S(24) > s_scroll + view) s_scroll = it->r.bottom + S(24) - view;
    clamp_scroll();
}

static void focus_step(int dir)
{
    int i = s_focus, n;
    for (n = 0; n < s_n; n++) {
        i = i < 0 ? (dir > 0 ? 0 : s_n - 1) : (i + dir + s_n) % s_n;
        if (focusable(i)) break;
    }
    if (n == s_n) return;
    s_focus = i;
    s_focus_ring = 1;
    reveal(i);
    repaint();
}

static void capture_close(void)
{
    s_cap = -1;
    KillTimer(s_wnd, 1);
    repaint();
}

static void capture_open(int i)
{
    s_cap = i;
    s_cap_hot = 0;
    *s_cap_msg = 0;
    s_cap_until = GetTickCount() + CAPTURE_MS;
    SetTimer(s_wnd, 1, 50, NULL);
    repaint();
}

/* A key for the slot being captured. One key does one thing: where else it was, it leaves. */
static void capture_take(unsigned id)
{
    const int a = s_items[s_cap].arg / KA_SLOTS, s = s_items[s_cap].arg % KA_SLOTS;
    wchar_t name[64], text[240];
    int b, t, from = -1;
    if (src_reserved(id) || !id || id >= 0x320) {
        swprintf(s_cap_msg, 160, L"%s", id == 0x57 ? TR(L"F11 stays the fullscreen switch. Press another key.",
                                                         L"F11 continua alternando a tela cheia. Aperte outra tecla.")
                                                   : TR(L"That key cannot be used. Press another one.",
                                                        L"Essa tecla não pode ser usada. Aperte outra."));
        s_cap_until = GetTickCount() + CAPTURE_MS;
        repaint();
        return;
    }
    for (b = 0; b < KA_COUNT; b++)
        for (t = 0; t < KA_SLOTS; t++)
            if (s_keys[b][t] == id && (b != a || t != s)) { s_keys[b][t] = 0; if (b != a) from = b; }
    s_keys[a][s] = (unsigned short)id;
    for (b = 0; b < KA_COUNT; b++) keys_pack(b);
    capture_close();
    if (from >= 0) {
        src_label(id, name, 64);
        swprintf(text, 240, TR(L"%s now does %s; it no longer does %s.", L"%s agora faz %s; não faz mais %s."), name,
                 k_ka_names[s_lang][a], k_ka_names[s_lang][from]);
        toast(text);
    }
}

static void do_action(int id)
{
    HWND w = s_wnd;
    int i;
    switch (id) {
    case A_CHOOSE: choose_disc(w); layout(); break;
    case A_INSTALL: install_game(w); break;
    case A_STEAM: add_to_steam(w); break;
    case A_HD_FOLDER: open_hd_folder(); break;
    case A_HD_PS2: ShellExecuteW(NULL, L"open", k_ps2_pack_url, NULL, NULL, SW_SHOWNORMAL); break;
    case A_HD_REMAKE: ShellExecuteW(NULL, L"open", k_remake_url, NULL, NULL, SW_SHOWNORMAL); break;
    case A_PLAY: save_and_play(w); break;
    case A_KEYS_RESET:
        if (MessageBoxW(w, TR(L"Put every key and mouse button back to the game's defaults?",
                              L"Voltar todas as teclas e botões do mouse para o padrão do jogo?"),
                        TR(L"Restore Default Keys", L"Restaurar teclas padrão"), MB_YESNO | MB_ICONQUESTION) == IDYES) {
            keys_default();
            layout();
        }
        break;
    case A_PAD_RESET:
        if (MessageBoxW(w, TR(L"Put every controller button back to the game's layout?",
                              L"Voltar todos os botões do controle para o padrão do jogo?"),
                        TR(L"Restore Default Buttons", L"Restaurar botões padrão"), MB_YESNO | MB_ICONQUESTION) == IDYES) {
            for (i = 0; i < PAD_BUTTONS; i++) s_map[i] = i;
            layout();
        }
        break;
    }
}

/* A value changed through its item. */
static void changed(int id)
{
    if (id == A_LANG) {
        *s_toast = 0;
        save_settings();                        /* the installation reads the language from launcher.ini */
    }
    if (id == A_TAB) s_scroll = 0;
    if (id == A_LANG || id == A_TAB || id == A_RELAYOUT) layout();
    repaint();
}

static void activate(int i, int part)
{
    Item *it = &s_items[i];
    if (!focusable(i)) return;
    switch (it->kind) {
    case K_NAV:
        if (s_page != it->arg) { s_page = it->arg; s_scroll = 0; layout(); }
        break;
    case K_BUTTON: do_action(it->id); break;
    case K_TOGGLE: *it->value = !*it->value; changed(it->id); break;
    case K_SEG:
        if (part < 0) part = (*it->value + 1) % it->nopts;
        if (*it->value != part) { *it->value = part; changed(it->id); }
        break;
    case K_SELECT: s_pop = i; s_pop_hot = *it->value; repaint(); break;
    case K_BIND: capture_open(i); break;
    }
}

static void on_mouse_move(int x, int y)
{
    const POINT p = { x, y };
    int part = -1, h;
    TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, s_wnd, 0 };
    TrackMouseEvent(&tme);
    if (s_thumb_drag) {
        RECT track, thumb;
        if (thumb_rect(&track, &thumb)) {
            const int room = (track.bottom - track.top) - (thumb.bottom - thumb.top);
            s_scroll = room > 0 ? MulDiv(y - s_thumb_grab - track.top, s_content_h - (s_view.bottom - s_view.top), room) : 0;
            clamp_scroll();
            repaint();
        }
        return;
    }
    if (s_cap >= 0) {
        RECT card, unbind, cancel;
        cap_rects(&card, &unbind, &cancel);
        h = PtInRect(&unbind, p) ? 1 : PtInRect(&cancel, p) ? 2 : 0;
        if (h != s_cap_hot) { s_cap_hot = h; repaint(); }
        return;
    }
    if (s_pop >= 0) {
        RECT r = pop_rect();
        h = PtInRect(&r, p) ? (y - r.top - S(6)) / S(32) : s_pop_hot;
        if (h >= 0 && h < s_items[s_pop].nopts && h != s_pop_hot) { s_pop_hot = h; repaint(); }
        return;
    }
    h = hit(x, y, &part);
    if (h != s_hot || part != s_hot_part) { s_hot = h; s_hot_part = part; repaint(); }
}

static void on_mouse_down(int button, int x, int y)
{
    const POINT p = { x, y };
    RECT track, thumb;
    int part;
    s_focus_ring = 0;
    if (s_cap >= 0) {
        RECT card, unbind, cancel;
        cap_rects(&card, &unbind, &cancel);
        if (button == 0 && PtInRect(&unbind, p)) {
            key_clear(s_items[s_cap].arg / KA_SLOTS, s_items[s_cap].arg % KA_SLOTS);
            capture_close();
        } else if ((button == 0 && PtInRect(&cancel, p)) || !PtInRect(&card, p)) capture_close();
        else capture_take(SRC_MOUSE + (unsigned)button);
        return;
    }
    if (s_pop >= 0) {
        RECT r = pop_rect();
        if (button == 0 && PtInRect(&r, p)) {
            const int k = (y - r.top - S(6)) / S(32);
            if (k >= 0 && k < s_items[s_pop].nopts) *s_items[s_pop].value = k;
        }
        s_pop = -1;
        repaint();
        return;
    }
    if (button == 1) {                       /* a right click empties a key */
        const int i = hit(x, y, &part);
        if (i >= 0 && s_items[i].kind == K_BIND) {
            key_clear(s_items[i].arg / KA_SLOTS, s_items[i].arg % KA_SLOTS);
            repaint();
        }
        return;
    }
    if (button != 0) return;
    if (thumb_rect(&track, &thumb) && x >= track.left - S(6) && y >= track.top && y < track.bottom) {
        /* the thumb is held where it was taken; a click on the track brings its middle there first */
        const int on_thumb = PtInRect(&thumb, p);
        s_thumb_grab = on_thumb ? y - thumb.top : (thumb.bottom - thumb.top) / 2;
        s_thumb_drag = 1;
        SetCapture(s_wnd);
        if (!on_thumb) on_mouse_move(x, y);
        return;
    }
    s_press = hit(x, y, &part);
    s_hot = s_press; s_hot_part = part;
    if (s_press >= 0) { s_focus = s_press; SetCapture(s_wnd); }
    repaint();
}

static void on_mouse_up(int x, int y)
{
    int part, i = s_press;
    if (GetCapture() == s_wnd) ReleaseCapture();
    if (s_thumb_drag) { s_thumb_drag = 0; repaint(); return; }
    s_press = -1;
    if (i >= 0 && hit(x, y, &part) == i) activate(i, part);
    repaint();
}

static void on_wheel(int delta, int x, int y)
{
    const POINT p = { x, y };
    if (s_cap >= 0) {
        RECT card, unbind, cancel;
        cap_rects(&card, &unbind, &cancel);
        if (PtInRect(&card, p)) capture_take(delta > 0 ? SRC_WHEEL_UP : SRC_WHEEL_DOWN);
        return;
    }
    if (s_pop >= 0) return;
    s_scroll -= MulDiv(delta, S(72), WHEEL_DELTA);
    clamp_scroll();
    on_mouse_move(x, y);
    repaint();
}

static void on_key(WPARAM vk, LPARAM lp)
{
    Item *it = focusable(s_focus) ? &s_items[s_focus] : NULL;
    if (s_cap >= 0) {
        if (!(lp & (1 << 30)))                  /* not the repeat of the key that opened it */
            capture_take((unsigned)((lp >> 16) & 0xFF) | ((lp >> 24) & 1 ? 0x100u : 0u));
        return;
    }
    if (s_pop >= 0) {
        const int n = s_items[s_pop].nopts;
        if (vk == VK_UP) s_pop_hot = (s_pop_hot + n - 1) % n;
        else if (vk == VK_DOWN) s_pop_hot = (s_pop_hot + 1) % n;
        else if (vk == VK_HOME) s_pop_hot = 0;
        else if (vk == VK_END) s_pop_hot = n - 1;
        else if (vk == VK_RETURN || vk == VK_SPACE) { *s_items[s_pop].value = s_pop_hot; s_pop = -1; }
        else if (vk == VK_ESCAPE || vk == VK_TAB) s_pop = -1;
        repaint();
        return;
    }
    switch (vk) {
    case VK_TAB: focus_step(GetKeyState(VK_SHIFT) < 0 ? -1 : 1); break;
    case VK_DOWN: focus_step(1); break;
    case VK_UP: focus_step(-1); break;
    case VK_LEFT: case VK_RIGHT:
        if (it && it->kind == K_SEG) {
            const int v = *it->value + (vk == VK_RIGHT ? 1 : -1);
            s_focus_ring = 1;
            if (v >= 0 && v < it->nopts) { *it->value = v; changed(it->id); }
            repaint();
        } else if (it && it->kind == K_BIND) focus_step(vk == VK_RIGHT ? 1 : -1);
        break;
    case VK_RETURN: case VK_SPACE:
        if (lp & (1 << 30)) break;
        if (it) { s_focus_ring = 1; activate(s_focus, -1); }
        else if (vk == VK_RETURN) do_action(A_PLAY);
        break;
    case VK_DELETE: case VK_BACK:
        if (it && it->kind == K_BIND) { key_clear(it->arg / KA_SLOTS, it->arg % KA_SLOTS); repaint(); }
        break;
    case VK_PRIOR: case VK_NEXT:
        s_scroll += (vk == VK_NEXT ? 1 : -1) * (s_view.bottom - s_view.top - S(48));
        clamp_scroll();
        repaint();
        break;
    }
}

/* ---- the window ------------------------------------------------------------------------------------------------ */

static HFONT make_font(const wchar_t *face, int px, int weight)
{
    LOGFONTW lf = { 0 };
    lf.lfHeight = -S(px);
    lf.lfWeight = weight;
    lf.lfQuality = CLEARTYPE_QUALITY;
    lf.lfCharSet = DEFAULT_CHARSET;
    wcscpy_s(lf.lfFaceName, LF_FACESIZE, face);
    return CreateFontIndirectW(&lf);
}

static int CALLBACK font_found(const LOGFONTW *lf, const TEXTMETRICW *tm, DWORD type, LPARAM found)
{
    (void)lf; (void)tm; (void)type;
    *(int *)found = 1;
    return 0;
}

/* The fonts and the icon, at the window's scale: Segoe UI for reading, Bahnschrift (Windows 10's) for the names. */
static void make_fonts(void)
{
    LOGFONTW probe = { 0 };
    const wchar_t *display = L"Segoe UI";
    int i, found = 0;
    for (i = 0; i < F_COUNT; i++) if (s_fonts[i]) DeleteObject(s_fonts[i]);
    probe.lfCharSet = DEFAULT_CHARSET;
    wcscpy_s(probe.lfFaceName, LF_FACESIZE, L"Bahnschrift");
    EnumFontFamiliesExW(s_mdc, &probe, font_found, (LPARAM)&found, 0);
    if (found) display = L"Bahnschrift";
    s_fonts[F_BODY] = make_font(L"Segoe UI", 14, FW_NORMAL);
    s_fonts[F_BOLD] = make_font(L"Segoe UI", 14, FW_SEMIBOLD);
    s_fonts[F_SMALL] = make_font(L"Segoe UI", 12, FW_NORMAL);
    s_fonts[F_CAPS] = make_font(L"Segoe UI", 11, FW_BOLD);
    s_fonts[F_TITLE] = make_font(display, 32, FW_SEMIBOLD);
    s_fonts[F_HERO] = make_font(display, 26, FW_SEMIBOLD);
    s_fonts[F_BRAND] = make_font(display, 22, FW_BOLD);
    s_fonts[F_PLAY] = make_font(display, 22, FW_BOLD);
    s_fonts[F_ICON] = make_font(L"Segoe MDL2 Assets", 17, FW_NORMAL);
    s_fonts[F_ICON_S] = make_font(L"Segoe MDL2 Assets", 13, FW_NORMAL);
    if (s_icon) DestroyIcon(s_icon);
    s_icon = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1), IMAGE_ICON, S(44), S(44), LR_DEFAULTCOLOR);
}

static int window_dpi(HWND w)
{
    typedef UINT (WINAPI *DpiForWindow)(HWND);
    DpiForWindow get = (DpiForWindow)(void *)GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow");
    HDC dc;
    int dpi;
    if (get && (dpi = (int)get(w)) > 0) return dpi;
    dc = GetDC(NULL);
    dpi = GetDeviceCaps(dc, LOGPIXELSY);
    ReleaseDC(NULL, dc);
    return dpi > 0 ? dpi : 96;
}

/* The window at its size for this screen's scale, no larger than the screen's work area; centred, or kept where
 * `at` says. */
static void fit_window(HWND w, const RECT *at)
{
    MONITORINFO mi = { sizeof(mi) };
    RECT wr, cr_;
    int fw, fh, ww, wh, x, y;
    GetMonitorInfoW(MonitorFromWindow(w, MONITOR_DEFAULTTONEAREST), &mi);
    GetWindowRect(w, &wr);
    GetClientRect(w, &cr_);
    fw = (wr.right - wr.left) - cr_.right;
    fh = (wr.bottom - wr.top) - cr_.bottom;
    ww = S(1040) + fw;
    wh = S(680) + fh;
    if (ww > mi.rcWork.right - mi.rcWork.left) ww = mi.rcWork.right - mi.rcWork.left;
    if (wh > mi.rcWork.bottom - mi.rcWork.top) wh = mi.rcWork.bottom - mi.rcWork.top;
    x = at ? at->left : mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - ww) / 2;
    y = at ? at->top : mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - wh) / 2;
    SetWindowPos(w, NULL, x, y, ww, wh, SWP_NOZORDER | SWP_NOACTIVATE);
}

/* A dark title bar in the window's own colour, where the system has one (Windows 10 2004 and later, 11). */
static void dark_frame(HWND w)
{
    typedef HRESULT (WINAPI *SetAttribute)(HWND, DWORD, LPCVOID, DWORD);
    HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
    SetAttribute set = dwm ? (SetAttribute)(void *)GetProcAddress(dwm, "DwmSetWindowAttribute") : NULL;
    const BOOL on = TRUE;
    const COLORREF caption = cr(C_BG), border = cr(C_LINE);
    if (!set) return;
    if (set(w, 20, &on, sizeof(on)) != S_OK) set(w, 19, &on, sizeof(on));
    set(w, 35, &caption, sizeof(caption));
    set(w, 34, &border, sizeof(border));
}

static LRESULT CALLBACK wndproc(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    const int x = (int)(short)LOWORD(lp), y = (int)(short)HIWORD(lp);
    switch (m) {
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(w, &ps);
        if (s_wnd) paint(dc);
        EndPaint(w, &ps);
        return 0;
    }
    case WM_SIZE:
        if (s_wnd && wp != SIZE_MINIMIZED) layout();
        return 0;
    case WM_DPICHANGED:
        s_dpi = HIWORD(wp);
        make_fonts();
        fit_window(w, (const RECT *)lp);
        layout();
        return 0;
    case WM_MOUSEMOVE: on_mouse_move(x, y); return 0;
    case WM_MOUSELEAVE:
        if (s_hot >= 0 || s_cap_hot) { s_hot = -1; s_cap_hot = 0; repaint(); }
        return 0;
    case WM_LBUTTONDOWN: case WM_LBUTTONDBLCLK: on_mouse_down(0, x, y); return 0;
    case WM_RBUTTONDOWN: on_mouse_down(1, x, y); return 0;
    case WM_MBUTTONDOWN: on_mouse_down(2, x, y); return 0;
    case WM_XBUTTONDOWN: on_mouse_down(HIWORD(wp) == XBUTTON1 ? 3 : 4, x, y); return TRUE;
    case WM_LBUTTONUP: on_mouse_up(x, y); return 0;
    case WM_CAPTURECHANGED:
        if (s_thumb_drag || s_press >= 0) { s_thumb_drag = 0; s_press = -1; repaint(); }
        return 0;
    case WM_MOUSEWHEEL: {
        POINT p = { x, y };
        ScreenToClient(w, &p);
        on_wheel((int)(short)HIWORD(wp), p.x, p.y);
        return 0;
    }
    case WM_SETCURSOR:
        if (LOWORD(lp) == HTCLIENT) {
            SetCursor(LoadCursorW(NULL, s_cap < 0 && s_pop < 0 && s_hot >= 0 ? IDC_HAND : IDC_ARROW));
            return TRUE;
        }
        break;
    case WM_KEYDOWN:
        on_key(wp, lp);
        return 0;
    case WM_SYSKEYDOWN:                         /* Alt and F10 are keys like the others while one is captured */
        if (s_cap >= 0 && wp != VK_F4) { on_key(wp, lp); return 0; }
        break;
    case WM_SYSCHAR:
        if (s_cap >= 0) return 0;
        break;
    case WM_SYSCOMMAND:
        if (s_cap >= 0 && (wp & 0xFFF0) == SC_KEYMENU) return 0;
        break;
    case WM_KILLFOCUS:
        if (s_cap >= 0) capture_close();
        if (s_pop >= 0) { s_pop = -1; repaint(); }
        return 0;
    case WM_TIMER:
        if (wp == 1 && s_cap >= 0) {
            if ((int)(s_cap_until - GetTickCount()) <= 0) capture_close();
            else repaint();
        } else if (wp == 2) {
            KillTimer(w, 2);
            *s_toast = 0;
            repaint();
        }
        return 0;
    case WM_CLOSE:
        save_settings();                        /* what was changed is kept, played or not */
        DestroyWindow(w);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(w, m, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR args, int show)
{
    typedef BOOL (WINAPI *SetDpiContext)(HANDLE);
    const GdipStartup gdip = { 1, NULL, FALSE, FALSE };
    const DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    SetDpiContext set_dpi = (SetDpiContext)(void *)GetProcAddress(GetModuleHandleW(L"user32.dll"),
                                                                 "SetProcessDpiAwarenessContext");
    WNDCLASSW wc = { 0 };
    ULONG_PTR token = 0;
    wchar_t *slash, probe[MAX_PATH];
    FILE *f;
    HWND w;
    MSG msg;
    (void)prev; (void)args;

    if (set_dpi) set_dpi((HANDLE)(INT_PTR)-4);          /* per monitor, version 2: sharp at any scale */
    GetModuleFileNameW(NULL, s_dir, MAX_PATH);
    slash = wcsrchr(s_dir, L'\\');
    if (slash) *slash = 0;
    /* in the package's top folder the game folder is mka\ beside it */
    swprintf(probe, MAX_PATH, L"%s\\mka\\CMakeLists.txt", s_dir);
    if (exists(probe)) wcscat_s(s_dir, MAX_PATH, L"\\mka");
    find_packs();
    /* English unless the player chose Portuguese (launcher.ini language=pt-BR) */
    ini_load(L"launcher.ini");
    s_lang = !_stricmp(ini_get("language", "en"), "pt-BR");
    load_settings();
    wcscpy_s(s_version, 32, L"?");
    swprintf(probe, MAX_PATH, L"%s\\..\\VERSION", s_dir);
    if (!_wfopen_s(&f, probe, L"r") && f) {
        char v[32] = "";
        if (fgets(v, sizeof(v), f)) { v[strcspn(v, "\r\n ")] = 0; if (*v) swprintf(s_version, 32, L"%hs", v); }
        fclose(f);
    }

    if (GdiplusStartup(&token, &gdip, NULL)) return 1;
    s_mdc = CreateCompatibleDC(NULL);
    s_bdc = CreateCompatibleDC(NULL);

    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.lpszClassName = L"MkaLauncher";
    RegisterClassW(&wc);
    w = CreateWindowW(wc.lpszClassName, L"Mortal Kombat: Armageddon — Launcher", style, CW_USEDEFAULT, CW_USEDEFAULT, 640, 480,
                      NULL, NULL, inst, NULL);
    if (!w) return 1;
    s_dpi = window_dpi(w);
    make_fonts();
    dark_frame(w);
    fit_window(w, NULL);
    s_wnd = w;
    layout();
    ShowWindow(w, show);
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}
