/*
 * msgboxicons-probe.c -- test/msgboxicons-gate.sh's probe: the message box
 * icons (user32's IDI_ERROR, IDI_WARNING, IDI_INFORMATION, IDI_QUESTION),
 * shell32's stock icons of the same, and what a message box and a task
 * dialog actually show.
 *
 * Prints lines the gate reads:
 *   frames NAME 16 20 24 ...      the sizes user32's icon group carries
 *   hash NAME HHHH...             FNV-1a of each frame's image (old art check)
 *   colour WHERE NAME SIZE BG body=R,G,B glyph=R,G,B
 *                                  the icon drawn on white (BG=l) and on
 *                                  near-black (BG=d): a pixel of the shape's
 *                                  body and one of its glyph
 *   stock NAME iIcon=N            SHGetStockIconInfo's resource
 *
 * WHERE is "user32" (LoadImage), "stock" (SHGetStockIconInfo's icon),
 * "msgbox" (the icon a MessageBox with MB_ICON* shows) or "taskdlg" (the
 * icon a task dialog with TD_*_ICON shows).
 *
 * Copyright (C) 2026 Stained Glass OS contributors; LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <stdio.h>

static const struct icon
{
    const char *name;
    WORD id;              /* user32's resource (IDI_*) */
    UINT mb;              /* MessageBox flag */
    int stock;            /* SHSTOCKICONID */
    PCWSTR td;            /* task dialog icon */
    /* where to look, in 1/100 of the icon: the body, the glyph */
    int bx, by, gx, gy;
} icons[] =
{
    { "error",       32513, MB_ICONERROR,       80 /* SIID_ERROR */,   TD_ERROR_ICON, 50, 18, 50, 50 },
    { "warning",     32515, MB_ICONWARNING,     78 /* SIID_WARNING */, TD_WARNING_ICON, 30, 82, 50, 46 },
    { "information", 32516, MB_ICONINFORMATION, 79 /* SIID_INFO */,    TD_INFORMATION_ICON, 22, 50, 50, 62 },
    { "question",    32514, MB_ICONQUESTION,    23 /* SIID_HELP */,    NULL,                 22, 50, 50, 75 },
};

#pragma pack(push, 2)
typedef struct { BYTE w, h, colours, reserved; WORD planes, bpp; DWORD bytes; WORD id; } GRPENTRY;
typedef struct { WORD reserved, type, count; GRPENTRY e[1]; } GRPDIR;
#pragma pack(pop)

static unsigned long long fnv(const BYTE *p, DWORD n)
{
    unsigned long long h = 0xcbf29ce484222325ull;
    while (n--) h = (h ^ *p++) * 0x100000001b3ull;
    return h;
}

static void frames(const struct icon *ic)
{
    HMODULE u = GetModuleHandleW(L"user32.dll");
    HRSRC r = FindResourceW(u, MAKEINTRESOURCEW(ic->id), (LPCWSTR)RT_GROUP_ICON);
    const GRPDIR *dir;
    int i;

    if (!r) { printf("frames %s none\n", ic->name); return; }
    dir = LockResource(LoadResource(u, r));
    printf("frames %s", ic->name);
    for (i = 0; i < dir->count; i++) printf(" %d", dir->e[i].w ? dir->e[i].w : 256);
    printf("\nhash %s", ic->name);
    for (i = 0; i < dir->count; i++)
    {
        HRSRC f = FindResourceW(u, MAKEINTRESOURCEW(dir->e[i].id), (LPCWSTR)RT_ICON);
        if (f) printf(" %016llx", fnv(LockResource(LoadResource(u, f)), SizeofResource(u, f)));
    }
    printf("\n");
}

/* draw icon at size on a background, print the body's and the glyph's colour */
static void sample(const char *where, const struct icon *ic, HICON icon, int size)
{
    static const struct { char tag; COLORREF bg; } bgs[] = { { 'l', RGB(255, 255, 255) }, { 'd', RGB(32, 32, 32) } };
    BITMAPINFO bi = {{ sizeof(BITMAPINFOHEADER), size, -size, 1, 32, BI_RGB }};
    HDC dc = CreateCompatibleDC(NULL);
    DWORD *bits;
    HBITMAP bmp = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, (void **)&bits, NULL, 0);
    int k;

    SelectObject(dc, bmp);
    for (k = 0; k < 2; k++)
    {
        RECT rc = { 0, 0, size, size };
        HBRUSH br = CreateSolidBrush(bgs[k].bg);
        DWORD b, g;
        FillRect(dc, &rc, br);
        DeleteObject(br);
        DrawIconEx(dc, 0, 0, icon, size, size, 0, NULL, DI_NORMAL);
        GdiFlush();
        b = bits[(ic->by * size / 100) * size + ic->bx * size / 100];
        g = bits[(ic->gy * size / 100) * size + ic->gx * size / 100];
        printf("colour %s %s %d %c body=%lu,%lu,%lu glyph=%lu,%lu,%lu\n", where, ic->name, size, bgs[k].tag,
               (b >> 16) & 0xff, (b >> 8) & 0xff, b & 0xff, (g >> 16) & 0xff, (g >> 8) & 0xff, g & 0xff);
    }
    DeleteDC(dc);
    DeleteObject(bmp);
}

/* the icon a dialog shows: its static control with SS_ICON */
static HICON dialog_icon(HWND dlg)
{
    HWND child = NULL;
    while ((child = FindWindowExW(dlg, child, NULL, NULL)))
    {
        WCHAR cls[32];
        HICON h;
        GetClassNameW(child, cls, ARRAYSIZE(cls));
        if (lstrcmpiW(cls, L"Static")) continue;
        if ((h = (HICON)SendMessageW(child, STM_GETICON, 0, 0))) return h;
        if ((h = (HICON)SendMessageW(child, STM_GETIMAGE, IMAGE_ICON, 0))) return h;
    }
    return NULL;
}

static const struct icon *current;
static const char *current_where;

static void CALLBACK msgbox_timer(HWND hwnd, UINT msg, UINT_PTR id, DWORD t)
{
    HWND dlg = FindWindowW(L"#32770", L"sg-msgboxicons");
    HICON h;

    if (!dlg) return;
    KillTimer(NULL, id);
    if ((h = dialog_icon(dlg))) sample(current_where, current, h, GetSystemMetrics(SM_CXICON));
    else printf("colour %s %s none\n", current_where, current->name);
    PostMessageW(dlg, WM_COMMAND, IDOK, 0);
}

static HRESULT CALLBACK td_callback(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, LONG_PTR data)
{
    if (msg == TDN_CREATED)
    {
        /* the task dialog's icon is drawn by a static of its own */
        HICON h = NULL;
        HWND child = NULL;
        while (!h && (child = FindWindowExW(hwnd, child, NULL, NULL)))
        {
            WCHAR cls[32];
            GetClassNameW(child, cls, ARRAYSIZE(cls));
            if (!lstrcmpiW(cls, L"Static")) h = (HICON)SendMessageW(child, STM_GETICON, 0, 0);
        }
        if (h) sample("taskdlg", current, h, 32);
        else printf("colour taskdlg %s none\n", current->name);
        PostMessageW(hwnd, TDM_CLICK_BUTTON, IDOK, 0);
    }
    return S_OK;
}

int wmain(int argc, WCHAR **argv)
{
    HRESULT (WINAPI *pSHGetStockIconInfo)(int, UINT, SHSTOCKICONINFO *);
    HRESULT (WINAPI *pTaskDialogIndirect)(const TASKDIALOGCONFIG *, int *, int *, BOOL *);
    BOOL gui = argc > 1 && !lstrcmpW(argv[1], L"gui");
    unsigned int i;
    int sizes[] = { 32, 48, 256 }, s;

    setvbuf(stdout, NULL, _IONBF, 0);
    pSHGetStockIconInfo = (void *)GetProcAddress(LoadLibraryW(L"shell32.dll"), "SHGetStockIconInfo");
    pTaskDialogIndirect = (void *)GetProcAddress(LoadLibraryW(L"comctl32.dll"), "TaskDialogIndirect");
    for (i = 0; i < ARRAYSIZE(icons); i++)
    {
        const struct icon *ic = &icons[i];
        SHSTOCKICONINFO sii = { sizeof(sii) };

        frames(ic);
        for (s = 0; s < ARRAYSIZE(sizes); s++)
        {
            HICON h = LoadImageW(NULL, MAKEINTRESOURCEW(ic->id), IMAGE_ICON, sizes[s], sizes[s], LR_SHARED);
            if (h) sample("user32", ic, h, sizes[s]);
            else printf("colour user32 %s %d none\n", ic->name, sizes[s]);
        }
        if (pSHGetStockIconInfo && SUCCEEDED(pSHGetStockIconInfo(ic->stock, SHGSI_ICON | SHGSI_LARGEICON, &sii)))
        {
            printf("stock %s iIcon=%d\n", ic->name, sii.iIcon);
            if (sii.hIcon) { sample("stock", ic, sii.hIcon, 32); DestroyIcon(sii.hIcon); }
            else printf("colour stock %s none\n", ic->name);
        }
        if (!gui) continue;

        current = ic;
        current_where = "msgbox";
        SetTimer(NULL, 1, 200, msgbox_timer);
        MessageBoxW(NULL, L"Stained Glass OS gate", L"sg-msgboxicons", MB_OK | ic->mb);

        if (pTaskDialogIndirect && ic->td)
        {
            TASKDIALOGCONFIG c = { sizeof(c) };
            c.pszWindowTitle = L"sg-msgboxicons-td";
            c.pszMainInstruction = L"Stained Glass OS gate";
            c.pszMainIcon = ic->td;
            c.pfCallback = td_callback;
            c.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION;
            pTaskDialogIndirect(&c, NULL, NULL, NULL);
        }
    }
    return 0;
}
