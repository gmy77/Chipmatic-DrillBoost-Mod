/*
 * Chipmatic Mod Manager - Win32 GUI (C99)
 * Compile (MinGW64):
 *   gcc -O2 -o chipmatic_mod.exe chipmatic_mod.c -mwindows -lcomctl32 -lshell32 -lm
 * Compile (MSVC):
 *   cl chipmatic_mod.c /O2 /Fe:chipmatic_mod.exe /link comctl32.lib shell32.lib user32.lib gdi32.lib
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shell32.lib")

/* ================================================================
 * Paths
 * ================================================================ */
#define GAME_WWW  "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Chipmatic\\www"
#define PS_FILE   GAME_WWW "\\playerStats.json"
#define BS_FILE   GAME_WWW "\\buildingsStats.json"

static char g_indexeddb[MAX_PATH];   /* filled at startup */
static HWND g_tooltip;               /* tooltip control   */

/* ================================================================
 * Control IDs
 * ================================================================ */
#define ID_EDIT_DIG      200
#define ID_EDIT_SPEED    201
#define ID_EDIT_FLY      202
#define ID_EDIT_STR      203
#define ID_EDIT_VIS      204
#define ID_EDIT_CAP      205
#define ID_EDIT_TANK     206

#define ID_EDIT_DMULT    300
#define ID_EDIT_DCAP     301
#define ID_EDIT_OREMULT  302
#define ID_EDIT_TIMEDIV  303

#define ID_RADIO_UC0     400   /* upgrade costs 100% */
#define ID_RADIO_UC1     401   /* 50%             */
#define ID_RADIO_UC2     402   /* 25%             */
#define ID_RADIO_UC3     403   /* free (1)        */

#define ID_RADIO_SC0     410
#define ID_RADIO_SC1     411
#define ID_RADIO_SC2     412
#define ID_RADIO_SC3     413

#define ID_BTN_APPLY     500
#define ID_BTN_RESET     501
#define ID_BTN_RESTORE   502

#define ID_STATUS        600

/* ================================================================
 * Minimal dynamic-buffer JSON helpers
 * ================================================================ */
typedef struct { char *buf; size_t len, cap; } JB;

static JB *jb_load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
    JB *jb = (JB*)malloc(sizeof(JB));
    jb->cap = sz + 256;
    jb->buf = (char*)malloc(jb->cap);
    jb->len = fread(jb->buf, 1, sz, f);
    jb->buf[jb->len] = '\0';
    fclose(f);
    return jb;
}
static int jb_save(JB *jb, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fwrite(jb->buf, 1, jb->len, f);
    fclose(f); return 1;
}
static void jb_free(JB *jb) { if (jb) { free(jb->buf); free(jb); } }

/* ensure capacity */
static void jb_grow(JB *jb, size_t need) {
    if (need <= jb->cap) return;
    jb->cap = need + 512;
    jb->buf = (char*)realloc(jb->buf, jb->cap);
}

/*
 * Find "key": inside buf[from..until) and return offset of value start.
 * until < 0  ->  search to end of buffer.
 * Returns -1 if not found.
 */
static int jb_key_pos(JB *jb, int from, int until, const char *key) {
    char pat[128];
    int plen = snprintf(pat, sizeof(pat), "\"%s\"", key);
    int lim  = (until < 0 || until > (int)jb->len) ? (int)jb->len : until;
    for (int i = from; i <= lim - plen; i++) {
        if (memcmp(jb->buf + i, pat, plen) != 0) continue;
        int j = i + plen;
        while (j < lim && (jb->buf[j]==' '||jb->buf[j]=='\t'||jb->buf[j]=='\n'||jb->buf[j]=='\r')) j++;
        if (j < lim && jb->buf[j] == ':') {
            j++;
            while (j < lim && (jb->buf[j]==' '||jb->buf[j]=='\t'||jb->buf[j]=='\n'||jb->buf[j]=='\r')) j++;
            return j;   /* offset of value */
        }
    }
    return -1;
}

/* Read integer value for key within [from, until) */
static int jb_get(JB *jb, int from, int until, const char *key) {
    int p = jb_key_pos(jb, from, until, key);
    if (p < 0) return 0;
    return atoi(jb->buf + p);
}

/*
 * Replace integer for key within [from, until).
 * Returns byte-delta (new length minus old length) or 0 on failure.
 * Positions >= vend shift by delta after this call.
 */
static int jb_set(JB *jb, int from, int until, const char *key, int val) {
    int vstart = jb_key_pos(jb, from, until, key);
    if (vstart < 0) return 0;

    /* find end of existing number */
    int vend = vstart;
    if (jb->buf[vend] == '-') vend++;
    while (vend < (int)jb->len && jb->buf[vend] >= '0' && jb->buf[vend] <= '9') vend++;

    char nv[32]; int nlen = snprintf(nv, sizeof(nv), "%d", val);
    int olen = vend - vstart, delta = nlen - olen;

    jb_grow(jb, jb->len + delta + 1);
    memmove(jb->buf + vstart + nlen, jb->buf + vend, jb->len - vend + 1);
    memcpy(jb->buf + vstart, nv, nlen);
    jb->len += delta;
    return delta;
}

/*
 * Find bounding offsets of the object/array value for 'key' in buf.
 * E.g.  "drill": { ... }   →  *s points to '{', *e points just after '}'.
 */
static int jb_scope(JB *jb, int from, const char *key, int *s, int *e) {
    int p = jb_key_pos(jb, from, -1, key);
    if (p < 0) return 0;
    int j = p;
    if (jb->buf[j] != '{' && jb->buf[j] != '[') return 0;
    char open = jb->buf[j], close = (open=='{') ? '}' : ']';
    int depth = 1; *s = j; j++;
    while (j < (int)jb->len && depth) {
        if (jb->buf[j] == open)  depth++;
        else if (jb->buf[j] == close) depth--;
        j++;
    }
    *e = j; return 1;
}

/* ----------------------------------------------------------------
 * Bulk replacement: collect (pos, old_len, new_val) then apply
 * back-to-front so earlier positions stay valid.
 * ---------------------------------------------------------------- */
typedef struct { int pos, olen, nval; } Rep;

static int rep_cmp_desc(const void *a, const void *b) {
    return ((Rep*)b)->pos - ((Rep*)a)->pos;
}

static void apply_reps(JB *jb, Rep *reps, int count) {
    qsort(reps, count, sizeof(Rep), rep_cmp_desc);
    char nv[32];
    for (int i = 0; i < count; i++) {
        int nlen = snprintf(nv, sizeof(nv), "%d", reps[i].nval);
        int delta = nlen - reps[i].olen;
        jb_grow(jb, jb->len + delta + 1);
        memmove(jb->buf + reps[i].pos + nlen,
                jb->buf + reps[i].pos + reps[i].olen,
                jb->len - reps[i].pos - reps[i].olen + 1);
        memcpy(jb->buf + reps[i].pos, nv, nlen);
        jb->len += delta;
    }
}

/*
 * Collect all  "cost": N, "currency":  patterns within buf[from..until)
 * and scale them by `factor` (min 1).
 */
static int collect_cost_reps(JB *jb, int from, int until, double factor,
                              Rep *reps, int maxr) {
    int count = 0, i = from;
    int lim = (until < 0 || until > (int)jb->len) ? (int)jb->len : until;
    while (i < lim && count < maxr) {
        int p = jb_key_pos(jb, i, lim, "cost");
        if (p < 0) break;
        /* must be a plain decimal number (not '[' or '{') */
        if (jb->buf[p] < '0' || jb->buf[p] > '9') { i = p + 1; continue; }
        int vs = p, ve = p;
        while (ve < lim && jb->buf[ve] >= '0' && jb->buf[ve] <= '9') ve++;
        /* peek for "currency": */
        int k = ve;
        while (k < lim && (jb->buf[k]==' '||jb->buf[k]=='\t'||jb->buf[k]=='\n'||jb->buf[k]=='\r'||jb->buf[k]==',')) k++;
        if (strncmp(jb->buf + k, "\"currency\":", 11) == 0) {
            int old = atoi(jb->buf + vs);
            int nval = (int)fmax(1.0, round(old * factor));
            reps[count].pos = vs; reps[count].olen = ve - vs; reps[count].nval = nval;
            count++;
        }
        i = p + 1;
    }
    return count;
}

/*
 * Scale "amount" values inside drill.items (all items get x mult)
 * and "time" values get /tdiv (min 5).
 */
static void drill_items_scale(JB *jb, int scope_s, int scope_e,
                               int ore_mult, int time_div) {
    #define MAX_ITEMS 64
    Rep reps[MAX_ITEMS * 2];
    int count = 0, i = scope_s;
    while (i < scope_e && count < MAX_ITEMS * 2 - 2) {
        /* "amount": N */
        int pa = jb_key_pos(jb, i, scope_e, "amount");
        int pt = jb_key_pos(jb, i, scope_e, "time");
        int next = -1;

        if (pa >= 0 && (pt < 0 || pa < pt)) {
            int vs = pa, ve = pa;
            while (ve < scope_e && jb->buf[ve] >= '0' && jb->buf[ve] <= '9') ve++;
            int old = atoi(jb->buf + vs);
            reps[count].pos  = vs;
            reps[count].olen = ve - vs;
            reps[count].nval = old * ore_mult;
            count++;
            next = pa + 1;
        } else if (pt >= 0) {
            int vs = pt, ve = pt;
            while (ve < scope_e && jb->buf[ve] >= '0' && jb->buf[ve] <= '9') ve++;
            int old = atoi(jb->buf + vs);
            reps[count].pos  = vs;
            reps[count].olen = ve - vs;
            reps[count].nval = (int)fmax(5.0, round((double)old / time_div));
            count++;
            next = pt + 1;
        } else break;

        i = next;
    }
    apply_reps(jb, reps, count);
}

/* ================================================================
 * File helpers
 * ================================================================ */
static int file_copy(const char *src, const char *dst) {
    return CopyFileA(src, dst, FALSE) != 0;
}

/* Recursively delete a directory (for wiping old IndexedDB backup) */
static void rmdir_recursive(const char *path) {
    char pattern[MAX_PATH];
    snprintf(pattern, MAX_PATH, "%s\\*", path);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, "..")) continue;
        char child[MAX_PATH];
        snprintf(child, MAX_PATH, "%s\\%s", path, fd.cFileName);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            rmdir_recursive(child);
        else
            DeleteFileA(child);
    } while (FindNextFileA(h, &fd));
    FindClose(h);
    RemoveDirectoryA(path);
}

/* Move (rename) a directory */
static int dir_rename(const char *src, const char *dst) {
    return MoveFileExA(src, dst, MOVEFILE_REPLACE_EXISTING) != 0;
}

/* ================================================================
 * Global window handle + status helper
 * ================================================================ */
static HWND g_hwnd;

static void set_status(const char *msg) {
    HWND sb = GetDlgItem(g_hwnd, ID_STATUS);
    SendMessageA(sb, SB_SETTEXTA, 0, (LPARAM)msg);
}

/* ================================================================
 * Read JSON -> fill UI controls
 * ================================================================ */
static void load_values(void) {
    /* --- playerStats.json --- */
    JB *ps = jb_load(PS_FILE);
    if (ps) {
        int data_s, data_e;
        if (!jb_scope(ps, 0, "data", &data_s, &data_e)) { data_s = 0; data_e = -1; }
        char tmp[32];

#define SET_EDIT(id, key) \
    snprintf(tmp, sizeof(tmp), "%d", jb_get(ps, data_s, data_e, key)); \
    SetDlgItemTextA(g_hwnd, id, tmp);

        SET_EDIT(ID_EDIT_DIG,   "dig_speed")
        SET_EDIT(ID_EDIT_SPEED, "speed")
        SET_EDIT(ID_EDIT_FLY,   "fly_speed")
        SET_EDIT(ID_EDIT_STR,   "strength")
        SET_EDIT(ID_EDIT_VIS,   "vision")
        SET_EDIT(ID_EDIT_CAP,   "capacity")
        SET_EDIT(ID_EDIT_TANK,  "tank")
#undef SET_EDIT
        jb_free(ps);
    }

    /* --- buildingsStats.json  (drill section) --- */
    JB *bs = jb_load(BS_FILE);
    if (bs) {
        int ds, de;
        if (jb_scope(bs, 0, "drill", &ds, &de)) {
            char tmp[32];
            snprintf(tmp, sizeof(tmp), "%d", jb_get(bs, ds, de, "speedMultiplier"));
            SetDlgItemTextA(g_hwnd, ID_EDIT_DMULT, tmp);
            snprintf(tmp, sizeof(tmp), "%d", jb_get(bs, ds, de, "capacity"));
            SetDlgItemTextA(g_hwnd, ID_EDIT_DCAP, tmp);
        }
        jb_free(bs);
    }

    /* ore/time preset defaults */
    SetDlgItemTextA(g_hwnd, ID_EDIT_OREMULT, "1");
    SetDlgItemTextA(g_hwnd, ID_EDIT_TIMEDIV, "1");

    set_status("Valori caricati. Modifica e premi \"Applica tutto\".");
}

/* ================================================================
 * Apply: read UI -> write JSON files
 * ================================================================ */
static void on_apply(void) {
    char tmp[64];

    /* ---- playerStats.json ---- */
    JB *ps = jb_load(PS_FILE);
    if (!ps) { set_status("ERRORE: impossibile aprire playerStats.json"); return; }

    int data_s, data_e;
    if (!jb_scope(ps, 0, "data", &data_s, &data_e)) { data_s = 0; data_e = -1; }

#define WRITE_PS(id, key) \
    GetDlgItemTextA(g_hwnd, id, tmp, sizeof(tmp)); \
    jb_set(ps, 0, -1, key, atoi(tmp));

    WRITE_PS(ID_EDIT_DIG,   "dig_speed")
    WRITE_PS(ID_EDIT_SPEED, "speed")
    WRITE_PS(ID_EDIT_FLY,   "fly_speed")
    WRITE_PS(ID_EDIT_STR,   "strength")
    WRITE_PS(ID_EDIT_VIS,   "vision")
    WRITE_PS(ID_EDIT_CAP,   "capacity")
    WRITE_PS(ID_EDIT_TANK,  "tank")
#undef WRITE_PS

    if (!jb_save(ps, PS_FILE)) {
        jb_free(ps);
        set_status("ERRORE: impossibile scrivere playerStats.json (avvia come admin?)");
        return;
    }
    jb_free(ps);

    /* ---- buildingsStats.json ---- */

    /* if no .original backup, create one now */
    char orig[MAX_PATH];
    snprintf(orig, MAX_PATH, "%s.original", BS_FILE);
    if (GetFileAttributesA(orig) == INVALID_FILE_ATTRIBUTES)
        file_copy(BS_FILE, orig);

    /* start fresh from original for cost presets */
    int uc_sel = IsDlgButtonChecked(g_hwnd, ID_RADIO_UC0) ? 0 :
                 IsDlgButtonChecked(g_hwnd, ID_RADIO_UC1) ? 1 :
                 IsDlgButtonChecked(g_hwnd, ID_RADIO_UC2) ? 2 : 3;
    int sc_sel = IsDlgButtonChecked(g_hwnd, ID_RADIO_SC0) ? 0 :
                 IsDlgButtonChecked(g_hwnd, ID_RADIO_SC1) ? 1 :
                 IsDlgButtonChecked(g_hwnd, ID_RADIO_SC2) ? 2 : 3;

    /* load original as base so we don't stack reductions */
    JB *bs = jb_load(orig);
    if (!bs) bs = jb_load(BS_FILE); /* fallback */
    if (!bs) { set_status("ERRORE: impossibile aprire buildingsStats.json"); return; }

    /* Upgrade costs (garage upgrades) - all "cost"/currency pairs */
    static const double factors[4] = { 1.0, 0.5, 0.25, 0.0 };
    double ucf = factors[uc_sel];
    if (ucf != 1.0) {
        int gs, ge;
        if (jb_scope(bs, 0, "garage", &gs, &ge)) {
            #define MAX_REPS 4096
            Rep *reps = (Rep*)malloc(sizeof(Rep)*MAX_REPS);
            if (ucf <= 0.0) {
                /* free = cost 1 */
                int count = collect_cost_reps(bs, gs, ge, 1.0/9999, reps, MAX_REPS);
                apply_reps(bs, reps, count);
            } else {
                int count = collect_cost_reps(bs, gs, ge, ucf, reps, MAX_REPS);
                apply_reps(bs, reps, count);
            }
            free(reps);
        }
    }

    /* Station build/upgrade costs - everything except garage */
    double scf = factors[sc_sel];
    if (scf != 1.0) {
        const char *stations[] = {"battery","furnace","crystal","atom","solar",
                                   "spaceship","refiner","laboratory","drill",NULL};
        Rep *reps = (Rep*)malloc(sizeof(Rep)*MAX_REPS);
        for (int si = 0; stations[si]; si++) {
            int ss, se;
            if (!jb_scope(bs, 0, stations[si], &ss, &se)) continue;
            double f = (scf <= 0.0) ? 1.0/9999 : scf;
            int count = collect_cost_reps(bs, ss, se, f, reps, MAX_REPS);
            apply_reps(bs, reps, count);
        }
        free(reps);
    }

    /* Drill: speedMultiplier, capacity */
    int ds, de;
    if (jb_scope(bs, 0, "drill", &ds, &de)) {
        GetDlgItemTextA(g_hwnd, ID_EDIT_DMULT, tmp, sizeof(tmp));
        int dm = atoi(tmp); if (dm < 1) dm = 1;
        jb_set(bs, ds, de, "speedMultiplier", dm);

        GetDlgItemTextA(g_hwnd, ID_EDIT_DCAP, tmp, sizeof(tmp));
        int dc = atoi(tmp); if (dc < 1) dc = 1;
        jb_set(bs, ds, de, "capacity", dc);

        /* ore amount x / time / */
        GetDlgItemTextA(g_hwnd, ID_EDIT_OREMULT, tmp, sizeof(tmp)); int om = atoi(tmp);
        GetDlgItemTextA(g_hwnd, ID_EDIT_TIMEDIV, tmp, sizeof(tmp)); int td = atoi(tmp);
        if (om > 1 || td > 1) {
            int is, ie;
            if (jb_scope(bs, ds, "items", &is, &ie))
                drill_items_scale(bs, is, ie, om > 1 ? om : 1, td > 1 ? td : 1);
        }
    }

    if (!jb_save(bs, BS_FILE)) {
        jb_free(bs);
        set_status("ERRORE: impossibile scrivere buildingsStats.json");
        return;
    }
    jb_free(bs);

    set_status("Modifiche salvate!  Ora premi \"Reset Salvataggio\" e avvia una nuova partita.");
}

/* ================================================================
 * Reset save: backup IndexedDB so game starts fresh
 * ================================================================ */
static void on_reset_save(void) {
    if (GetFileAttributesA(g_indexeddb) == INVALID_FILE_ATTRIBUTES) {
        set_status("Nessun salvataggio trovato (o gia' resettato).");
        return;
    }
    int r = MessageBoxA(g_hwnd,
        "Questa operazione resettera' il salvataggio del gioco.\n"
        "Il vecchio salvataggio viene spostato in IndexedDB.bak\n\n"
        "Procedere?",
        "Reset salvataggio", MB_YESNO | MB_ICONWARNING);
    if (r != IDYES) return;

    char bak[MAX_PATH];
    snprintf(bak, MAX_PATH, "%s.bak", g_indexeddb);
    /* remove old backup if present */
    if (GetFileAttributesA(bak) != INVALID_FILE_ATTRIBUTES)
        rmdir_recursive(bak);

    if (dir_rename(g_indexeddb, bak)) {
        set_status("Salvataggio spostato in IndexedDB.bak. Avvia il gioco -> Nuova Partita.");
    } else {
        set_status("ERRORE: impossibile rinominare IndexedDB. Chiudi il gioco prima.");
    }
}

/* ================================================================
 * Restore originals
 * ================================================================ */
static void on_restore(void) {
    int r = MessageBoxA(g_hwnd,
        "Ripristinare i file JSON originali del gioco?\n"
        "(Tutte le modifiche correnti verranno perse.)",
        "Ripristina originali", MB_YESNO | MB_ICONQUESTION);
    if (r != IDYES) return;

    int ok = 1;
    char orig[MAX_PATH];

    /* playerStats */
    snprintf(orig, MAX_PATH, "%s.original", PS_FILE);
    if (GetFileAttributesA(orig) != INVALID_FILE_ATTRIBUTES)
        ok &= file_copy(orig, PS_FILE);

    /* buildingsStats */
    snprintf(orig, MAX_PATH, "%s.original", BS_FILE);
    if (GetFileAttributesA(orig) != INVALID_FILE_ATTRIBUTES)
        ok &= file_copy(orig, BS_FILE);

    if (ok) {
        load_values();
        set_status("File originali ripristinati.");
    } else {
        set_status("Alcuni file originali non trovati (verranno creati al prossimo Applica).");
    }
}

/* ================================================================
 * UI creation helpers
 * ================================================================ */
static HFONT g_font, g_font_bold, g_font_title;

static HWND mkctl(const char *cls, const char *txt, DWORD style,
                  int x, int y, int w, int h, int id) {
    return CreateWindowExA(0, cls, txt,
        WS_CHILD | WS_VISIBLE | style,
        x, y, w, h, g_hwnd, (HMENU)(intptr_t)id, GetModuleHandleA(NULL), NULL);
}

static void set_font(HWND h, HFONT f) { SendMessageA(h, WM_SETFONT, (WPARAM)f, TRUE); }

/* labelled edit box - returns HWND of edit */
static HWND mkrow(const char *lbl, int id, int x, int y, int ew) {
    HWND hl = mkctl("STATIC", lbl, SS_RIGHT, x, y+2, 110, 16, 0);
    HWND he = mkctl("EDIT", "", WS_BORDER | ES_NUMBER | ES_CENTER,
                    x+115, y, ew, 20, id);
    set_font(hl, g_font); set_font(he, g_font);
    return he;
}

/* group box — returns HWND for tooltip attachment */
static HWND mkgrp(const char *txt, int x, int y, int w, int h) {
    HWND hg = mkctl("BUTTON", txt, BS_GROUPBOX, x, y, w, h, 0);
    set_font(hg, g_font_bold);
    return hg;
}

/* radio button */
static HWND mkradio(const char *txt, DWORD extra, int x, int y, int w, int id) {
    HWND h = mkctl("BUTTON", txt, BS_AUTORADIOBUTTON | extra, x, y, w, 18, id);
    set_font(h, g_font);
    return h;
}

/* push button — returns HWND for tooltip attachment */
static HWND mkbtn(const char *txt, int x, int y, int w, int id) {
    HWND h = mkctl("BUTTON", txt, BS_PUSHBUTTON, x, y, w, 28, id);
    set_font(h, g_font_bold);
    return h;
}

/* attach tooltip text to a control */
static void tip(HWND h, const char *txt) {
    TOOLINFOA ti = {0};
    ti.cbSize   = sizeof(ti);
    ti.uFlags   = TTF_IDISHWND | TTF_SUBCLASS;
    ti.hwnd     = g_hwnd;
    ti.uId      = (UINT_PTR)h;
    ti.lpszText = (char *)txt;
    SendMessageA(g_tooltip, TTM_ADDTOOLA, 0, (LPARAM)&ti);
}

static void build_ui(void) {
    /* ---- fonts ---- */
    LOGFONTA lf = {0};
    lf.lfHeight = -13; lf.lfWeight = FW_NORMAL;
    strcpy(lf.lfFaceName, "Segoe UI");
    g_font      = CreateFontIndirectA(&lf);
    lf.lfWeight = FW_BOLD;
    g_font_bold = CreateFontIndirectA(&lf);
    lf.lfHeight = -15; lf.lfWeight = FW_BOLD;
    g_font_title = CreateFontIndirectA(&lf);

    /* ---- tooltip control (create BEFORE any tip() calls) ---- */
    /* NOTE: no WS_EX_TOPMOST here — it can confuse the shell/DWM z-order */
    g_tooltip = CreateWindowExA(0, TOOLTIPS_CLASSA, NULL,
        WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        g_hwnd, NULL, GetModuleHandleA(NULL), NULL);
    SendMessageA(g_tooltip, TTM_SETMAXTIPWIDTH,  0, 340);
    SendMessageA(g_tooltip, TTM_SETDELAYTIME, TTDT_INITIAL,  500);
    SendMessageA(g_tooltip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 12000);

    /* ================================================================
     * Player - Statistiche Base   (window width 560 => group w=544)
     * ================================================================ */
    HWND hGrpPlayer = mkgrp("Player - Statistiche Base", 8, 6, 544, 192);
    tip(hGrpPlayer,
        "Valori iniziali del giocatore all'avvio di una NUOVA PARTITA.\n"
        "\n"
        "Dig Speed   = velocita' di scavo della trivella manuale\n"
        "Speed       = velocita' di movimento a piedi\n"
        "Fly Speed   = velocita' di volo\n"
        "Strength    = resistenza (punti vita)\n"
        "Vision      = raggio visivo del radar\n"
        "Capacity    = spazio nell'inventario\n"
        "Tank        = capacita' serbatoio energia\n"
        "\n"
        "Valori piu' alti = personaggio piu' potente.");

    /* col 1  x=16 */
    mkrow("Dig Speed:",  ID_EDIT_DIG,   16,  26, 55);
    mkrow("Speed:",      ID_EDIT_SPEED, 16,  50, 55);
    mkrow("Fly Speed:",  ID_EDIT_FLY,   16,  74, 55);
    mkrow("Strength:",   ID_EDIT_STR,   16,  98, 55);
    /* col 2  x=296 */
    mkrow("Vision:",     ID_EDIT_VIS,  296,  26, 55);
    mkrow("Capacity:",   ID_EDIT_CAP,  296,  50, 55);
    mkrow("Tank:",       ID_EDIT_TANK, 296,  74, 55);

    /* preset buttons */
    HWND hpre = mkctl("BUTTON", "Preset Boost x5", BS_PUSHBUTTON,
                      16, 130, 140, 24, 700);
    set_font(hpre, g_font);
    tip(hpre,
        "Carica valori molto potenziati (consigliati per giocare easy):\n"
        "Dig Speed 100 | Speed 200 | Fly Speed 160 | Strength 5\n"
        "Vision 8 | Capacity 120 | Tank 30\n"
        "Drill Mult x3 | Ore Amount x3 | Time /2\n"
        "\nPremi poi 'Applica tutto' per salvare.");

    HWND hpre2 = mkctl("BUTTON", "Preset Originale", BS_PUSHBUTTON,
                       165, 130, 140, 24, 701);
    set_font(hpre2, g_font);
    tip(hpre2,
        "Carica i valori originali del gioco nei campi di testo.\n"
        "I file NON vengono modificati finche' non premi 'Applica tutto'.");

    {
        HWND h = mkctl("STATIC",
            "Questi sono i valori di partenza (nuova partita).",
            SS_LEFT, 16, 162, 528, 14, 0);
        set_font(h, g_font);
    }

    /* ================================================================
     * Drill - Estrazione Mineraria
     * ================================================================ */
    HWND hGrpDrill = mkgrp("Drill - Estrazione Mineraria", 8, 204, 544, 90);
    tip(hGrpDrill,
        "Parametri della trivella automatica (stazione di estrazione).\n"
        "\n"
        "Speed Mult  = moltiplicatore velocita' di estrazione\n"
        "Capacita'   = quanti minerali accumula prima di fermarsi\n"
        "Ore Amt x   = moltiplica la quantita' estratta per ciclo\n"
        "              (es. x3 = tre volte il minerale originale)\n"
        "Time / x    = divide il tempo di ogni ciclo\n"
        "              (es. /2 = estrae due volte piu' velocemente)\n"
        "\n"
        "Ore/Time si applicano solo se impostati > 1.");

    mkrow("Speed Mult:",  ID_EDIT_DMULT,   16, 222, 55);
    mkrow("Capacita':",   ID_EDIT_DCAP,   296, 222, 55);
    mkrow("Ore Amt x:",   ID_EDIT_OREMULT, 16, 248, 55);
    mkrow("Time / x:",    ID_EDIT_TIMEDIV,296, 248, 55);

    /* ================================================================
     * Costi  (due gruppi affiancati, 268px ciascuno)
     * ================================================================ */
    /* -- Garage upgrades -- */
    HWND hGrpUC = mkgrp("Costi Upgrade Garage", 8, 300, 270, 68);
    tip(hGrpUC,
        "Riduce i costi degli UPGRADE acquistabili nel Garage.\n"
        "Include tutti i livelli di: Dig Speed, Speed, Fly Speed,\n"
        "Capacita', Tank e Vision del giocatore.\n"
        "\n"
        "100%  = costi originali invariati\n"
        "50%   = costi dimezzati\n"
        "25%   = un quarto del costo originale\n"
        "Gratis = costo minimo 1 (mai 0, per evitare bug)");

    { HWND h = mkctl("STATIC","Costi:",SS_LEFT,16,318,46,16,0); set_font(h,g_font); }
    mkradio("100%",  WS_GROUP, 66,  316, 50, ID_RADIO_UC0);
    mkradio("50%",   0,       120,  316, 46, ID_RADIO_UC1);
    mkradio("25%",   0,       168,  316, 46, ID_RADIO_UC2);
    HWND hUCfree = mkradio("Gratis", 0, 216, 316, 60, ID_RADIO_UC3);
    tip(hUCfree,
        "Imposta tutti i costi upgrade a 1 (il minimo possibile).\n"
        "Non si usa 0 perche' potrebbe causare comportamenti\n"
        "inaspettati nel gioco.");
    CheckDlgButton(g_hwnd, ID_RADIO_UC1, BST_CHECKED);

    /* -- Station costs -- */
    HWND hGrpSC = mkgrp("Costi Costruzione Stazioni", 282, 300, 270, 68);
    tip(hGrpSC,
        "Riduce i costi per costruire e potenziare le stazioni:\n"
        "Batteria, Forno, Raffineria, Cristallo, Atomo,\n"
        "Solare, Navicella spaziale e Trivella automatica.\n"
        "\n"
        "100%  = costi originali invariati\n"
        "50%   = costi dimezzati\n"
        "25%   = un quarto del costo originale\n"
        "Gratis = costo minimo 1 (mai 0, per evitare bug)");

    { HWND h = mkctl("STATIC","Costi:",SS_LEFT,290,318,46,16,0); set_font(h,g_font); }
    mkradio("100%",  WS_GROUP, 340, 316, 50, ID_RADIO_SC0);
    mkradio("50%",   0,        394, 316, 46, ID_RADIO_SC1);
    mkradio("25%",   0,        442, 316, 46, ID_RADIO_SC2);
    HWND hSCfree = mkradio("Gratis", 0, 490, 316, 60, ID_RADIO_SC3);
    tip(hSCfree,
        "Imposta tutti i costi costruzione a 1 (il minimo possibile).\n"
        "Non si usa 0 perche' potrebbe causare comportamenti\n"
        "inaspettati nel gioco.");
    CheckDlgButton(g_hwnd, ID_RADIO_SC1, BST_CHECKED);

    /* ---- hint ---- */
    {
        HWND h = mkctl("STATIC",
            "\"Gratis\" = costo minimo 1 (no 0).  "
            "Ore/Time si applicano solo se > 1.",
            SS_LEFT, 16, 374, 528, 14, 0);
        set_font(h, g_font);
    }

    /* ---- reminder ---- */
    {
        HWND h = mkctl("STATIC",
            "Ad ogni modifica dei parametri, ricordatevi di cliccare su APPLICA TUTTO e RESET SALVATAGGIO.",
            SS_CENTER, 16, 392, 528, 18, 0);
        set_font(h, g_font_bold);
    }

    /* ================================================================
     * Buttons
     * ================================================================ */
    HWND hApply = mkbtn("Applica tutto",        16, 418, 170, ID_BTN_APPLY);
    tip(hApply,
        "Salva SUBITO tutte le modifiche nei file JSON del gioco.\n"
        "\n"
        "IMPORTANTE: dopo aver premuto questo tasto devi anche\n"
        "premere 'Reset Salvataggio' e avviare una NUOVA PARTITA\n"
        "per vedere le modifiche in gioco.\n"
        "\n"
        "La prima volta viene creato automaticamente un backup\n"
        "dei file originali (usato da 'Ripristina originali').");

    HWND hReset = mkbtn("Reset Salvataggio",   196, 418, 170, ID_BTN_RESET);
    tip(hReset,
        "PERCHE' SERVE: il gioco salva tutto il suo stato interno\n"
        "(inclusi i valori del giocatore) in un database locale\n"
        "chiamato IndexedDB.  Se hai gia' una partita salvata,\n"
        "il gioco la ricarica da li' e IGNORA i file JSON modificati.\n"
        "\n"
        "Questo tasto sposta il vecchio salvataggio in IndexedDB.bak\n"
        "e forza il gioco a ripartire da zero, caricando i tuoi\n"
        "nuovi valori dai file JSON.\n"
        "\n"
        "Il vecchio salvataggio e' al sicuro nel file .bak.");

    HWND hRestore = mkbtn("Ripristina originali", 376, 418, 172, ID_BTN_RESTORE);
    tip(hRestore,
        "Ripristina i file JSON ai valori originali del gioco\n"
        "(quelli prima di qualsiasi modifica con questo tool).\n"
        "\n"
        "Il backup originale viene creato automaticamente la prima\n"
        "volta che premi 'Applica tutto'.\n"
        "\n"
        "ATTENZIONE: sovrascrive le modifiche correnti nei file.");

    /* ---- Status bar ---- */
    HWND sb = CreateWindowExA(0, STATUSCLASSNAMEA, NULL,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0, g_hwnd, (HMENU)ID_STATUS,
        GetModuleHandleA(NULL), NULL);
    set_font(sb, g_font);
    (void)sb;
}

/* Preset button handler (inside WndProc, needs to be connected) */
static void apply_boost_preset(void) {
    SetDlgItemTextA(g_hwnd, ID_EDIT_DIG,   "100");
    SetDlgItemTextA(g_hwnd, ID_EDIT_SPEED, "200");
    SetDlgItemTextA(g_hwnd, ID_EDIT_FLY,   "160");
    SetDlgItemTextA(g_hwnd, ID_EDIT_STR,   "5");
    SetDlgItemTextA(g_hwnd, ID_EDIT_VIS,   "8");
    SetDlgItemTextA(g_hwnd, ID_EDIT_CAP,   "120");
    SetDlgItemTextA(g_hwnd, ID_EDIT_TANK,  "30");
    SetDlgItemTextA(g_hwnd, ID_EDIT_DMULT, "3");
    SetDlgItemTextA(g_hwnd, ID_EDIT_DCAP,  "80");
    SetDlgItemTextA(g_hwnd, ID_EDIT_OREMULT,"3");
    SetDlgItemTextA(g_hwnd, ID_EDIT_TIMEDIV,"2");
    set_status("Preset Boost caricato. Premi Applica tutto per salvare.");
}
static void apply_orig_preset(void) {
    SetDlgItemTextA(g_hwnd, ID_EDIT_DIG,   "20");
    SetDlgItemTextA(g_hwnd, ID_EDIT_SPEED, "100");
    SetDlgItemTextA(g_hwnd, ID_EDIT_FLY,   "80");
    SetDlgItemTextA(g_hwnd, ID_EDIT_STR,   "1");
    SetDlgItemTextA(g_hwnd, ID_EDIT_VIS,   "4");
    SetDlgItemTextA(g_hwnd, ID_EDIT_CAP,   "40");
    SetDlgItemTextA(g_hwnd, ID_EDIT_TANK,  "10");
    SetDlgItemTextA(g_hwnd, ID_EDIT_DMULT, "1");
    SetDlgItemTextA(g_hwnd, ID_EDIT_DCAP,  "20");
    SetDlgItemTextA(g_hwnd, ID_EDIT_OREMULT,"1");
    SetDlgItemTextA(g_hwnd, ID_EDIT_TIMEDIV,"1");
    CheckDlgButton(g_hwnd, ID_RADIO_UC0, BST_CHECKED);
    CheckDlgButton(g_hwnd, ID_RADIO_UC1, BST_UNCHECKED);
    CheckDlgButton(g_hwnd, ID_RADIO_SC0, BST_CHECKED);
    CheckDlgButton(g_hwnd, ID_RADIO_SC1, BST_UNCHECKED);
    set_status("Preset Originale caricato. Premi Applica tutto per salvare.");
}

/* ================================================================
 * Window procedure
 * ================================================================ */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:  g_hwnd = hwnd; return 0;
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_BTN_APPLY:   on_apply();           break;
        case ID_BTN_RESET:   on_reset_save();      break;
        case ID_BTN_RESTORE: on_restore();         break;
        case 700:            apply_boost_preset(); break;
        case 701:            apply_orig_preset();  break;
        }
        return 0;
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wp;
        SetBkMode(hdc, TRANSPARENT);
        return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO *mm = (MINMAXINFO*)lp;
        mm->ptMinTrackSize.x = 560;
        mm->ptMinTrackSize.y = 532;
        return 0;
    }
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

/* ================================================================
 * WinMain
 * ================================================================ */
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmd, int show) {

    /* init common controls */
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_BAR_CLASSES };
    InitCommonControlsEx(&icc);

    /* build IndexedDB path */
    char local[MAX_PATH];
    SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, local);
    snprintf(g_indexeddb, MAX_PATH,
             "%s\\Chipmatic\\EBWebView\\Default\\IndexedDB", local);

    /* register window class */
    WNDCLASSEXA wc = {0};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorA(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    wc.lpszClassName = "ChipmaticMod";
    wc.hIcon         = LoadIconA(NULL, IDI_APPLICATION);
    RegisterClassExA(&wc);

    /* create window — use CW_USEDEFAULT so Windows picks a position,
       then we reposition it within the work area right after */
    HWND hwnd = CreateWindowExA(
        0,
        "ChipmaticMod",
        "Chipmatic Mod Manager v1.0",
        WS_OVERLAPPEDWINDOW & ~WS_THICKFRAME & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 560, 532,
        NULL, NULL, hInst, NULL);

    build_ui();
    load_values();


    /* Centre the window inside the work area (taskbar excluded).
       This prevents any accidental overlap with the taskbar that
       could trigger the shell's full-screen-detection logic. */
    {
        RECT wa;
        SystemParametersInfoA(SPI_GETWORKAREA, 0, &wa, 0);
        int ww = 560, wh = 532;
        int wx = wa.left + (wa.right  - wa.left - ww) / 2;
        int wy = wa.top  + (wa.bottom - wa.top  - wh) / 2;
        SetWindowPos(hwnd, HWND_TOP, wx, wy, ww, wh, SWP_NOACTIVATE);
    }

    /* Always show as normal — ignore any SW_SHOWMAXIMIZED from the shell */
    ShowWindow(hwnd, SW_SHOWNORMAL);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    return (int)msg.wParam;
}
