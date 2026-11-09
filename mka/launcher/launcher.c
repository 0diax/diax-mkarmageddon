/* Settings launcher for the recompiled Mortal Kombat: Armageddon.
 *
 * Installs the game from the player's own disc (an ISO, or the default.xbe of one already extracted: copied,
 * translated and compiled here by scripts\install.ps1), shows the options run.cmd used to hard-code
 * (resolution, window, HD textures, intro movies, players, controls), writes them to video.ini and
 * launcher.ini, sets the runtime's environment and starts build\Release\mka_recomp.exe the way run.cmd does.
 * Settings the dialog does not show are left in video.ini as they were. It runs from the game folder (mka\)
 * or from the package's top folder, beside it. */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

enum {
    ID_LANG = 90, ID_DISC = 100, ID_CHOOSE, ID_INSTALL, ID_RES, ID_ASPECT, ID_MODE, ID_WIN, ID_AA, ID_VSYNC, ID_HD, ID_HD_FOLDER,
    ID_HD_PS2, ID_HD_REMAKE, ID_INTROS, ID_UNLOCK, ID_PLAYERS, ID_P1, ID_PADMAP, ID_STEAM, ID_PLAY, ID_CLOSE
};
/* the controls window's: one list a game button, then its buttons */
enum { ID_MAP0 = 300, ID_MAP_OK = 340, ID_MAP_CANCEL, ID_MAP_RESET };

static const unsigned k_heights[] = { 480, 720, 1080, 1440, 2160 };
/* The launcher's language: English, or Brazilian Portuguese (launcher.ini language=pt-BR). Every text is a pair. */
static int s_lang;
#define TR(en, pt) (s_lang ? (pt) : (en))

static const wchar_t *const k_height_names[2][5] = {
    { L"480p (original Xbox)", L"720p", L"1080p", L"1440p (recommended)", L"2160p (4K)" },
    { L"480p (original do Xbox)", L"720p", L"1080p", L"1440p (recomendado)", L"2160p (4K)" } };
/* Window heights; the width follows the aspect. */
static const unsigned k_win_h[] = { 720, 960, 1080, 1440 };
/* video.ini's aspect: "original" is the game's 4:3; a wider one also widens the game's camera (recomp_manual.c). */
static const char *const k_aspect[] = { "original", "16:9", "21:9" };
static const unsigned k_aspect_n[] = { 4, 16, 21 }, k_aspect_d[] = { 3, 9, 9 };
static const wchar_t *const k_aspect_names[] = { L"4:3 (original)", L"16:9 (widescreen)", L"21:9 (ultrawide)" };
static const char *const k_aa[] = { "off", "fxaa", "ssaa" };
static const wchar_t *const k_aa_names[2][3] = { { L"Off", L"FXAA (light)", L"SSAA (heavy)" },
                                                { L"Desligado", L"FXAA (leve)", L"SSAA (pesado)" } };

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
    { L"kick", L"throw", L"punch", L"uppercut", L"grab", L"weapon", L"style", L"block", L"pause", L"back", L"", L"" },
    { L"chute", L"arremesso", L"soco", L"gancho", L"agarrão", L"arma", L"estilo", L"defesa", L"pausa", L"voltar",
      L"", L"" }
};
static int s_map[PAD_BUTTONS];             /* game button -> the physical one doing it */
static HWND s_map_wnd;

static wchar_t s_dir[MAX_PATH], s_pack[MAX_PATH], s_remake[MAX_PATH], s_disc[MAX_PATH];
static HWND s_ctl[ID_CLOSE + 1];
static HFONT s_font;

static int exists(const wchar_t *path) { return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES; }

/* ---- video.ini: key=value lines, kept in order; the ones the dialog owns are replaced ---------------------- */

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

/* ---- the dialog ---------------------------------------------------------------------------------------------- */

static HWND add(HWND parent, const wchar_t *cls, const wchar_t *text, DWORD style, int x, int y, int w, int h, int id)
{
    HWND c = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, parent,
                             (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)s_font, TRUE);
    if (id > 0 && id <= ID_CLOSE) s_ctl[id] = c;
    return c;
}

static HWND combo(HWND parent, const wchar_t *label, int y, int id, const wchar_t *const *items, int n, int sel)
{
    HWND c;
    int i;
    add(parent, L"STATIC", label, 0, 20, y + 4, 170, 20, 0);
    c = add(parent, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL, 200, y, 290, 300, id);
    for (i = 0; i < n; i++) SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)items[i]);
    SendMessageW(c, CB_SETCURSEL, sel, 0);
    return c;
}

static int sel(int id) { return (int)SendMessageW(s_ctl[id], CB_GETCURSEL, 0, 0); }
static int checked(int id) { return SendMessageW(s_ctl[id], BM_GETCHECK, 0, 0) == BST_CHECKED; }

static unsigned win_width(int aspect, int win)
{
    if (aspect < 0) aspect = 0;
    return (k_win_h[win] * k_aspect_n[aspect] / k_aspect_d[aspect] + 1u) & ~1u;
}

/* The window sizes for the chosen aspect, keeping the chosen height. */
static void fill_win(int win)
{
    wchar_t text[32];
    int i, aspect = sel(ID_ASPECT);
    SendMessageW(s_ctl[ID_WIN], CB_RESETCONTENT, 0, 0);
    for (i = 0; i < 4; i++) {
        swprintf(text, 32, L"%u x %u", win_width(aspect, i), k_win_h[i]);
        SendMessageW(s_ctl[ID_WIN], CB_ADDSTRING, 0, (LPARAM)text);
    }
    SendMessageW(s_ctl[ID_WIN], CB_SETCURSEL, win, 0);
}

/* The disc row: which disc, and how far the installation got. */
static void show_disc(void)
{
    wchar_t text[2 * MAX_PATH], xbe[MAX_PATH], built[MAX_PATH];
    path_in_dir(xbe, L"game\\default.xbe");
    path_in_dir(built, L"build\\Release\\mka_recomp.exe");
    swprintf(text, 2 * MAX_PATH, L"%s\n%s", *s_disc ? s_disc : TR(L"No ISO chosen.", L"Nenhuma ISO escolhida."),
             exists(built) ? TR(L"Installed: ready to play.", L"Instalado: pronto para jogar.")
             : exists(xbe) ? TR(L"Disc copied: not compiled yet.", L"Disco copiado: falta compilar.")
                           : TR(L"Choose your ISO, then install.", L"Escolha a sua ISO e depois instale."));
    SetWindowTextW(s_ctl[ID_DISC], text);
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

static LRESULT CALLBACK map_wndproc(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    int i;
    switch (m) {
    case WM_COMMAND:
        if (LOWORD(wp) == ID_MAP_RESET) {
            for (i = 0; i < PAD_BUTTONS; i++) SendDlgItemMessageW(w, ID_MAP0 + i, CB_SETCURSEL, i, 0);
        } else if (LOWORD(wp) == ID_MAP_OK || LOWORD(wp) == ID_MAP_CANCEL) {
            if (LOWORD(wp) == ID_MAP_OK)
                for (i = 0; i < PAD_BUTTONS; i++) {
                    const int v = (int)SendDlgItemMessageW(w, ID_MAP0 + i, CB_GETCURSEL, 0, 0);
                    s_map[i] = v >= 0 ? v : i;
                }
            DestroyWindow(w);
        }
        return 0;
    case WM_CTLCOLORSTATIC:
        SetBkMode((HDC)wp, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    case WM_DESTROY:
        EnableWindow(GetWindow(w, GW_OWNER), TRUE);
        SetForegroundWindow(GetWindow(w, GW_OWNER));
        s_map_wnd = NULL;
        return 0;
    }
    return DefWindowProcW(w, m, wp, lp);
}

/* One row a button of the game's (the Xbox pad's, as the game knows them): which button of yours does it. The
 * same for every controller, Xbox or PlayStation; the keyboard keeps its own keys. */
static void map_open(HWND owner)
{
    static int registered;
    RECT r = { 0, 0, 430, 64 + PAD_BUTTONS * 30 + 50 };
    HWND w, c;
    int i, j, y = 12;
    if (s_map_wnd) { SetForegroundWindow(s_map_wnd); return; }
    if (!registered) {
        WNDCLASSW wc = { 0 };
        wc.lpfnWndProc = map_wndproc;
        wc.hInstance = GetModuleHandleW(NULL);
        wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
        wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
        wc.lpszClassName = L"MkaPadMap";
        RegisterClassW(&wc);
        registered = 1;
    }
    AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE);
    w = CreateWindowW(L"MkaPadMap", TR(L"Controller buttons", L"Botões do controle"), WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                      CW_USEDEFAULT, CW_USEDEFAULT, r.right - r.left, r.bottom - r.top, owner, NULL,
                      GetModuleHandleW(NULL), NULL);
    s_map_wnd = w;
    c = CreateWindowExW(0, L"STATIC",
                        TR(L"For each button the game uses, choose the button of your controller that does it.\n"
                           L"It works for Xbox and PlayStation controllers (DualShock 4 and DualSense).",
                           L"Para cada botão que o jogo usa, escolha o botão do seu controle que faz a função.\n"
                           L"Vale para controles de Xbox e de PlayStation (DualShock 4 e DualSense)."),
                        WS_CHILD | WS_VISIBLE, 12, y, 410, 40, w, NULL, GetModuleHandleW(NULL), NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)s_font, TRUE);
    y += 50;
    for (i = 0; i < PAD_BUTTONS; i++, y += 30) {
        wchar_t label[64];
        swprintf(label, 64, *k_pad_actions[s_lang][i] ? L"%s (%s)" : L"%s%s", k_pad_names[s_lang][i], k_pad_actions[s_lang][i]);
        c = CreateWindowExW(0, L"STATIC", label, WS_CHILD | WS_VISIBLE, 12, y + 4, 200, 20, w, NULL,
                            GetModuleHandleW(NULL), NULL);
        SendMessageW(c, WM_SETFONT, (WPARAM)s_font, TRUE);
        c = CreateWindowExW(0, L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL,
                            220, y, 198, 300, w, (HMENU)(INT_PTR)(ID_MAP0 + i), GetModuleHandleW(NULL), NULL);
        SendMessageW(c, WM_SETFONT, (WPARAM)s_font, TRUE);
        for (j = 0; j < PAD_BUTTONS; j++) SendMessageW(c, CB_ADDSTRING, 0, (LPARAM)k_pad_names[s_lang][j]);
        SendMessageW(c, CB_SETCURSEL, s_map[i], 0);
    }
    y += 10;
    {
        const struct { const wchar_t *text; int id, x; } b[] = {
            { TR(L"Default", L"Padrão"), ID_MAP_RESET, 12 }, { L"OK", ID_MAP_OK, 226 },
            { TR(L"Cancel", L"Cancelar"), ID_MAP_CANCEL, 326 }
        };
        for (i = 0; i < 3; i++) {
            c = CreateWindowExW(0, L"BUTTON", b[i].text, WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                                (b[i].id == ID_MAP_OK ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON),
                                b[i].x, y, 92, 28, w, (HMENU)(INT_PTR)b[i].id, GetModuleHandleW(NULL), NULL);
            SendMessageW(c, WM_SETFONT, (WPARAM)s_font, TRUE);
        }
    }
    EnableWindow(owner, FALSE);
    ShowWindow(w, SW_SHOW);
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

static void build(HWND w)
{
    const wchar_t *modes[] = { TR(L"Window", L"Janela"), TR(L"Fullscreen (borderless)", L"Tela cheia (sem bordas)") };
    const wchar_t *players[] = { TR(L"1 player", L"1 jogador"), TR(L"2 players (local)", L"2 jogadores (local)") };
    const wchar_t *p1[] = { TR(L"Keyboard + controller 1", L"Teclado + controle 1"),
                            TR(L"Keyboard only (controller 1 = player 2)", L"Só teclado (controle 1 = jogador 2)") };
    const wchar_t *langs[] = { L"English", L"Português (Brasil)" };
    unsigned h = (unsigned)atoi(ini_get("internal_height", "1440")), wh = (unsigned)atoi(ini_get("window_height", "960"));
    int i, res = 3, win = 1, aa = 1, aspect = 0, y = 16;
    const int has_pack = *s_pack != 0;
    wchar_t hd_label[160];

    for (i = 0; i < 5; i++) if (k_heights[i] == h) res = i;
    for (i = 0; i < 4; i++) if (k_win_h[i] == wh) win = i;
    for (i = 0; i < 3; i++) if (!_stricmp(ini_get("aspect", "original"), k_aspect[i])) aspect = i;
    for (i = 0; i < 3; i++) if (!_stricmp(ini_get("aa", "fxaa"), k_aa[i])) aa = i;

    combo(w, L"Language / Idioma", y, ID_LANG, langs, 2, s_lang);
    y += 40;
    add(w, L"BUTTON", TR(L"Disc", L"Disco"), BS_GROUPBOX, 8, y - 8, 494, 104, 0);
    y += 14;
    add(w, L"STATIC", L"", SS_LEFT | SS_NOPREFIX, 20, y, 470, 36, ID_DISC);
    y += 40;
    add(w, L"BUTTON", TR(L"Choose ISO...", L"Escolher a ISO..."), BS_PUSHBUTTON | WS_TABSTOP, 20, y, 150, 28, ID_CHOOSE);
    add(w, L"BUTTON", TR(L"Install and compile", L"Instalar e compilar"), BS_PUSHBUTTON | WS_TABSTOP, 180, y, 150, 28, ID_INSTALL);
    y += 50;

    add(w, L"BUTTON", TR(L"Video", L"Vídeo"), BS_GROUPBOX, 8, y - 8, 494, 210, 0);
    y += 14;
    combo(w, TR(L"Internal resolution", L"Resolução interna"), y, ID_RES, k_height_names[s_lang], 5, res); y += 32;
    combo(w, TR(L"Aspect ratio", L"Proporção da tela"), y, ID_ASPECT, k_aspect_names, 3, aspect); y += 32;
    combo(w, TR(L"Display mode", L"Modo de exibição"), y, ID_MODE, modes, 2, !_stricmp(ini_get("window_mode", "windowed"), "borderless"));
    y += 32;
    combo(w, TR(L"Window size", L"Tamanho da janela"), y, ID_WIN, NULL, 0, 0); y += 32;
    fill_win(win);
    combo(w, L"Anti-aliasing", y, ID_AA, k_aa_names[s_lang], 3, aa); y += 32;
    add(w, L"BUTTON", TR(L"V-Sync (no tearing)", L"V-Sync (sem cortes na imagem)"), BS_AUTOCHECKBOX | WS_TABSTOP, 200, y, 290, 20, ID_VSYNC);
    SendMessageW(s_ctl[ID_VSYNC], BM_SETCHECK, atoi(ini_get("vsync", "1")) ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 42;

    ini_load(L"launcher.ini");
    {
        const char *d = ini_get("disc", "");
        MultiByteToWideChar(CP_UTF8, 0, d, -1, s_disc, MAX_PATH);
    }
    show_disc();
    add(w, L"BUTTON", TR(L"Game", L"Jogo"), BS_GROUPBOX, 8, y - 8, 494, 314, 0);
    y += 14;
    swprintf(hd_label, 160, TR(L"HD textures (%s)", L"Texturas HD (%s)"),
             !has_pack ? TR(L"no pack in hd-textures: download them with the buttons below",
                            L"nenhum pacote em hd-textures: baixe pelos botões abaixo")
             : *s_remake ? TR(L"arts and menus remake + PS2 version pack", L"remake de artes e menus + pacote da versão de PS2")
                         : TR(L"PS2 version pack", L"pacote da versão de PS2"));
    add(w, L"BUTTON", hd_label, BS_AUTOCHECKBOX | WS_TABSTOP, 20, y, 470, 20, ID_HD);
    SendMessageW(s_ctl[ID_HD], BM_SETCHECK, has_pack && atoi(ini_get("hd_textures", "1")) ? BST_CHECKED : BST_UNCHECKED, 0);
    EnableWindow(s_ctl[ID_HD], has_pack);
    y += 26;
    add(w, L"BUTTON", TR(L"Open folder", L"Abrir a pasta"), BS_PUSHBUTTON | WS_TABSTOP, 20, y, 150, 26, ID_HD_FOLDER);
    add(w, L"BUTTON", TR(L"Get HD pack (PS2)", L"Baixar pacote HD (PS2)"), BS_PUSHBUTTON | WS_TABSTOP, 180, y, 150, 26, ID_HD_PS2);
    add(w, L"BUTTON", TR(L"Get HD remake", L"Baixar remake HD"), BS_PUSHBUTTON | WS_TABSTOP, 340, y, 150, 26, ID_HD_REMAKE);
    y += 36;
    add(w, L"BUTTON", TR(L"Skip intro movies", L"Pular vídeos de abertura"), BS_AUTOCHECKBOX | WS_TABSTOP, 20, y, 470, 20, ID_INTROS);
    SendMessageW(s_ctl[ID_INTROS], BM_SETCHECK, atoi(ini_get("skip_intros", "1")) ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 26;
    add(w, L"BUTTON", TR(L"Unlock everything (test session: nothing of it is saved to your profile)",
                          L"Desbloquear tudo (sessão de teste: nada dela é salvo no seu perfil)"),
        BS_AUTOCHECKBOX | WS_TABSTOP, 20, y, 470, 20, ID_UNLOCK);
    SendMessageW(s_ctl[ID_UNLOCK], BM_SETCHECK, atoi(ini_get("unlock_all", "0")) ? BST_CHECKED : BST_UNCHECKED, 0);
    y += 30;
    combo(w, TR(L"Players", L"Jogadores"), y, ID_PLAYERS, players, 2, atoi(ini_get("players", "2")) == 1 ? 0 : 1); y += 32;
    combo(w, TR(L"Player 1 uses", L"Jogador 1 usa"), y, ID_P1, p1, 2, atoi(ini_get("p1_keyboard_only", "0")) ? 1 : 0); y += 32;
    add(w, L"BUTTON", TR(L"Controller buttons (Xbox, DualShock 4, DualSense)...",
                          L"Configurar botões do controle (Xbox, DualShock 4, DualSense)..."),
        BS_PUSHBUTTON | WS_TABSTOP, 20, y, 470, 26, ID_PADMAP);
    map_load();
    y += 32;
    add(w, L"BUTTON", TR(L"Add to Steam (as a non-Steam game; close Steam first)",
                          L"Adicionar à Steam (como jogo não-Steam; feche a Steam antes)"),
        BS_PUSHBUTTON | WS_TABSTOP, 20, y, 470, 26, ID_STEAM);
    y += 44;

    add(w, L"STATIC",
        TR(L"Keyboard: WASD/arrows move, J punch, U uppercut, K kick, I throw, L/Space block,\n"
           L"O style, N weapon, H grab, Esc pause, Tab back, F11 fullscreen. Motor Kombat: Space accelerates.",
           L"Teclado: WASD/setas andar, J soco, U gancho, K chute, I arremesso, L/Espaço defesa,\n"
           L"O estilo, N arma, H agarrão, Esc pausa, Tab voltar, F11 tela cheia. Motor Kombat: Espaço acelera."),
        0, 12, y, 490, 52, 0);
    y += 60;
    add(w, L"BUTTON", TR(L"Play", L"Jogar"), BS_DEFPUSHBUTTON | WS_TABSTOP, 282, y, 104, 30, ID_PLAY);
    add(w, L"BUTTON", TR(L"Close", L"Fechar"), BS_PUSHBUTTON | WS_TABSTOP, 396, y, 104, 30, ID_CLOSE);
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
    show_disc();
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

/* The dialog's settings into video.ini and launcher.ini. */
static void save_settings(void)
{
    char v[32], pad_map[256];
    int res = sel(ID_RES), win = sel(ID_WIN), aa = sel(ID_AA), aspect = sel(ID_ASPECT);

    ini_load(L"video.ini");
    snprintf(v, sizeof(v), "%u", k_heights[res < 0 ? 3 : res]); ini_set("internal_height", v);
    ini_set("window_mode", sel(ID_MODE) == 1 ? "borderless" : "windowed");
    if (win < 0) win = 1;
    if (aspect < 0) aspect = 0;
    ini_set("aspect", k_aspect[aspect]);
    snprintf(v, sizeof(v), "%u", win_width(aspect, win)); ini_set("window_width", v);
    snprintf(v, sizeof(v), "%u", k_win_h[win]); ini_set("window_height", v);
    ini_set("aa", k_aa[aa < 0 ? 1 : aa]);
    ini_set("vsync", checked(ID_VSYNC) ? "1" : "0");
    ini_save(L"video.ini");

    ini_load(L"launcher.ini");
    ini_set("language", s_lang ? "pt-BR" : "en");
    ini_set("hd_textures", checked(ID_HD) ? "1" : "0");
    ini_set("skip_intros", checked(ID_INTROS) ? "1" : "0");
    ini_set("players", sel(ID_PLAYERS) == 1 ? "2" : "1");
    ini_set("p1_keyboard_only", sel(ID_P1) == 1 ? "1" : "0");
    ini_set("unlock_all", checked(ID_UNLOCK) ? "1" : "0");
    map_string(pad_map, sizeof(pad_map));
    ini_set("pad_map", pad_map);
    ini_save(L"launcher.ini");
}

/* Every control gone, then built again (in the language just chosen). */
static BOOL CALLBACK destroy_child(HWND c, LPARAM unused) { (void)unused; DestroyWindow(c); return TRUE; }
static void rebuild(HWND w)
{
    EnumChildWindows(w, destroy_child, 0);
    memset(s_ctl, 0, sizeof(s_ctl));
    ini_load(L"video.ini");
    build(w);
    InvalidateRect(w, NULL, TRUE);
}

static void save_and_play(HWND w)
{
    wchar_t path[MAX_PATH], game[MAX_PATH], out[MAX_PATH], err[MAX_PATH], cmd[MAX_PATH + 4];
    int two = sel(ID_PLAYERS) == 1, kb_only = sel(ID_P1) == 1, hd = checked(ID_HD), intros = checked(ID_INTROS);
    int unlock = checked(ID_UNLOCK);
    char pad_map[256];
    wchar_t wmap[256], packs[2 * MAX_PATH + 2];
    SECURITY_ATTRIBUTES sa = { sizeof(sa), NULL, TRUE };
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    HANDLE ho, he;

    save_settings();
    map_string(pad_map, sizeof(pad_map));

    path_in_dir(game, L"build\\Release\\mka_recomp.exe");
    if (!exists(game)) {
        MessageBoxW(w, TR(L"The game is not installed yet: choose your ISO and click Install and compile.",
                          L"O jogo ainda não foi instalado: escolha a sua ISO e clique em Instalar e compilar."),
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
    env(L"RECOMP_USB_PADS", two ? L"2" : L"1");
    /* Player one on a controller slot nothing occupies leaves it to the keyboard; controller 0 then drives
     * player two. */
    env(L"RECOMP_PAD1_XINPUT", kb_only ? L"3" : NULL);
    env(L"RECOMP_PAD2_XINPUT", kb_only ? L"0" : NULL);
    env(L"RECOMP_KEY_LAYOUT", L"pad");
    env(L"RECOMP_FB_WINDOW", L"1");
    env(L"RECOMP_WINDOW_TITLE", L"Mortal Kombat: Armageddon");
    env(L"RECOMP_HIDE_FILES", intros ? L"moviexbox\\LOGON.SFD;moviexbox\\OPENINGN.SFD" : NULL);
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
    env(L"RECOMP_TEXTURE_PACK", hd && *s_pack ? packs : NULL);
    env(L"RECOMP_TEXTURE_SKIP", L"Optional*;_*;Bonus Test");
    MultiByteToWideChar(CP_UTF8, 0, pad_map, -1, wmap, 256);
    env(L"RECOMP_PAD_MAP", *pad_map ? wmap : NULL);
    path_in_dir(path, L"save"); CreateDirectoryW(path, NULL);
    /* Everything unlocked plays on a copy of the saves, made afresh each time: the player's profile never sees it. */
    if (unlock) {
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

static LRESULT CALLBACK wndproc(HWND w, UINT m, WPARAM wp, LPARAM lp)
{
    switch (m) {
    case WM_CREATE:
        ini_load(L"video.ini");
        build(w);
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_ASPECT: if (HIWORD(wp) == CBN_SELCHANGE) fill_win(sel(ID_WIN)); break;
        case ID_LANG:
            if (HIWORD(wp) == CBN_SELCHANGE && sel(ID_LANG) != s_lang) {
                save_settings();
                s_lang = sel(ID_LANG) == 1;
                save_settings();
                SetWindowTextW(w, TR(L"Mortal Kombat: Armageddon — Settings", L"Mortal Kombat: Armageddon — Configurações"));
                PostMessageW(w, WM_APP, 0, 0);       /* not inside the list's own notification */
            }
            break;
        case ID_CHOOSE: choose_disc(w); break;
        case ID_INSTALL: install_game(w); break;
        case ID_HD_FOLDER: open_hd_folder(); break;
        case ID_HD_PS2: ShellExecuteW(NULL, L"open", k_ps2_pack_url, NULL, NULL, SW_SHOWNORMAL); break;
        case ID_HD_REMAKE: ShellExecuteW(NULL, L"open", k_remake_url, NULL, NULL, SW_SHOWNORMAL); break;
        case ID_PADMAP: map_open(w); break;
        case ID_STEAM: add_to_steam(w); break;
        case ID_PLAY: save_and_play(w); break;
        case ID_CLOSE: DestroyWindow(w); break;
        }
        return 0;
    case WM_APP:
        rebuild(w);
        return 0;
    case WM_CTLCOLORSTATIC:
        SetBkMode((HDC)wp, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_WINDOW);
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(w, m, wp, lp);
}

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE prev, PWSTR args, int show)
{
    NONCLIENTMETRICSW ncm = { sizeof(ncm) };
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    WNDCLASSW wc = { 0 };
    RECT r = { 0, 0, 510, 792 };
    wchar_t *slash, probe[MAX_PATH];
    HWND w;
    MSG msg;
    (void)prev; (void)args;

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

    InitCommonControlsEx(&icc);
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    s_font = CreateFontIndirectW(&ncm.lfMessageFont);

    wc.lpfnWndProc = wndproc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(1));
    wc.lpszClassName = L"MkaLauncher";
    RegisterClassW(&wc);
    AdjustWindowRect(&r, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    w = CreateWindowW(wc.lpszClassName, TR(L"Mortal Kombat: Armageddon — Settings", L"Mortal Kombat: Armageddon — Configurações"),
                      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT,
                      r.right - r.left, r.bottom - r.top, NULL, NULL, inst, NULL);
    ShowWindow(w, show);
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(w, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return 0;
}
