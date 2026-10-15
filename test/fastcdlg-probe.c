/* comdlg32 gap-A batch (patch 3220): PrintDlgEx[AW] property sheet, IShellBrowser of the
 * file dialog, CF_SELECTSCRIPT, CDM_SETDEFEXT, shortcuts in the Open dialog, envelope
 * sample pages of Page Setup. The dialogs are driven from a helper thread; printers are
 * made on a FILE: port, nothing is ever printed. Mutants SG_MUTANT_CDLG_*. */
#define COBJMACROS
#include <windows.h>
#include <winspool.h>
#include <commdlg.h>
#include <commctrl.h>
#include <prsht.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <ocidl.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <dlgs.h>

static int failures, checks;

#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const char ppd[] =
    "*PPD-Adobe: \"4.3\"\n*FormatVersion: \"4.3\"\n*FileVersion: \"1.0\"\n*LanguageVersion: English\n"
    "*LanguageEncoding: ISOLatin1\n*PCFileName: \"SGCDLG.PPD\"\n*Manufacturer: \"Stained Glass OS\"\n"
    "*ModelName: \"SG Cdlg PS\"\n*NickName: \"SG Cdlg PS\"\n*ShortNickName: \"SG Cdlg PS\"\n"
    "*PSVersion: \"(3010.000) 0\"\n*LanguageLevel: \"3\"\n*ColorDevice: True\n*DefaultColorSpace: RGB\n"
    "*OpenUI *PageSize/Media Size: PickOne\n*DefaultPageSize: Letter\n"
    "*PageSize Letter/Letter: \"<</PageSize[612 792]>>setpagedevice\"\n*CloseUI: *PageSize\n"
    "*PageSize Env10/Envelope #10: \"<</PageSize[297 684]>>setpagedevice\"\n"
    "*DefaultImageableArea: Letter\n*ImageableArea Letter/Letter: \"18 18 594 774\"\n"
    "*ImageableArea Env10/Envelope #10: \"10 10 287 674\"\n"
    "*DefaultPaperDimension: Letter\n*PaperDimension Letter/Letter: \"612 792\"\n"
    "*PaperDimension Env10/Envelope #10: \"297 684\"\n*DefaultResolution: 300dpi\n";

static BOOL make_printers(void)
{
    WCHAR dir[MAX_PATH], path[MAX_PATH], sys[MAX_PATH], out[MAX_PATH];
    DRIVER_INFO_3W di = { 3 };
    PRINTER_INFO_2W pi = { 0 };
    DWORD size, written;
    HANDLE f, p;
    BOOL ok;

    GetPrinterDriverDirectoryW(NULL, NULL, 1, (BYTE *)dir, sizeof(dir), &size);
    CreateDirectoryW(dir, NULL);
    GetSystemDirectoryW(sys, MAX_PATH);
    swprintf(path, MAX_PATH, L"%ls\\wineps.drv", sys);
    swprintf(out, MAX_PATH, L"%ls\\wineps.drv", dir);
    CopyFileW(path, out, FALSE);
    swprintf(out, MAX_PATH, L"%ls\\sgcdlg.ppd", dir);
    f = CreateFileW(out, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, ppd, sizeof(ppd) - 1, &written, NULL);
    CloseHandle(f);
    di.pName = L"SG Cdlg PS";
    di.pDriverPath = L"wineps.drv";
    di.pDataFile = L"sgcdlg.ppd";
    di.pConfigFile = L"wineps.drv";
    ok = AddPrinterDriverW(NULL, 3, (BYTE *)&di);
    printf("AddPrinterDriver: %d (%lu)\n", ok, ok ? 0 : GetLastError());
    pi.pDriverName = L"SG Cdlg PS";
    pi.pPortName = L"FILE:";
    pi.pPrintProcessor = L"WinPrint";
    pi.pDatatype = L"RAW";
    pi.pPrinterName = L"SG Cdlg A";
    p = AddPrinterW(NULL, 2, (BYTE *)&pi);
    if (!p) return FALSE;
    ClosePrinter(p);
    pi.pPrinterName = L"SG Cdlg B";
    p = AddPrinterW(NULL, 2, (BYTE *)&pi);
    if (!p) return FALSE;
    ClosePrinter(p);
    SetDefaultPrinterW(L"SG Cdlg A");
    return TRUE;
}


static HGLOBAL make_devnames(const WCHAR *printer)
{
    HGLOBAL hdn = GlobalAlloc(GMEM_MOVEABLE, sizeof(DEVNAMES) + 100 * sizeof(WCHAR));
    DEVNAMES *dn = GlobalLock(hdn);
    WCHAR *p = (WCHAR *)(dn + 1);

    wcscpy(p, L"wineps.drv");
    dn->wDriverOffset = (WORD)(p - (WCHAR *)dn);
    p += 11;
    wcscpy(p, printer);
    dn->wDeviceOffset = (WORD)(p - (WCHAR *)dn);
    p += wcslen(printer) + 1;
    wcscpy(p, L"FILE:");
    dn->wOutputOffset = (WORD)(p - (WCHAR *)dn);
    GlobalUnlock(hdn);
    return hdn;
}

/* ------------------------------------------------------------------ driving dialogs */

struct find { int id; HWND hwnd; };

static BOOL CALLBACK find_id_proc(HWND hwnd, LPARAM lp)
{
    struct find *f = (struct find *)lp;

    if (GetDlgCtrlID(hwnd) == f->id && !f->hwnd) f->hwnd = hwnd;
    return TRUE;
}

static HWND ctl(HWND top, int id)
{
    struct find f = { id, NULL };

    EnumChildWindows(top, find_id_proc, (LPARAM)&f);
    return f.hwnd;
}

static BOOL CALLBACK find_sheet_proc(HWND hwnd, LPARAM lp)
{
    WCHAR cls[32], title[32];

    GetClassNameW(hwnd, cls, 32);
    GetWindowTextW(hwnd, title, 32);
    if (!wcscmp(cls, L"#32770") && !wcscmp(title, L"Print") && ctl(hwnd, lst1) && IsWindowVisible(hwnd))
        *(HWND *)lp = hwnd;
    return TRUE;
}

static HWND wait_sheet(void)
{
    HWND h = NULL;
    int i;

    for (i = 0; i < 200 && !h; i++)
    {
        EnumWindows(find_sheet_proc, (LPARAM)&h);
        if (!h) Sleep(50);
    }
    if (h) Sleep(150);
    return h;
}

struct script
{
    void (*fn)(HWND sheet, void *arg);
    void *arg;
};

static DWORD WINAPI script_thread(void *p)
{
    struct script *s = p;
    HWND sheet = wait_sheet();


    if (!sheet)
    {
        printf("FAIL no print sheet appeared\n");
        failures++;
        ExitProcess(2);
    }
    s->fn(sheet, s->arg);
    return 0;
}

static HWND owner;

static HRESULT run_ex(PRINTDLGEXW *pd, void (*fn)(HWND, void *), void *arg)
{
    struct script s = { fn, arg };
    HANDLE th = CreateThread(NULL, 0, script_thread, &s, 0, NULL);
    HRESULT hr = PrintDlgExW(pd);

    WaitForSingleObject(th, 10000);
    CloseHandle(th);
    return hr;
}

static HRESULT run_exA(PRINTDLGEXA *pd, void (*fn)(HWND, void *), void *arg)
{
    struct script s = { fn, arg };
    HANDLE th = CreateThread(NULL, 0, script_thread, &s, 0, NULL);
    HRESULT hr = PrintDlgExA(pd);

    WaitForSingleObject(th, 10000);
    CloseHandle(th);
    return hr;
}

/* wake the sheet's message loop so it notices that it ended */
static void press(HWND sheet, int btn) { SendMessageW(sheet, PSM_PRESSBUTTON, btn, 0); PostMessageW(sheet, WM_NULL, 0, 0); }

static const WCHAR *devname(HGLOBAL hdn, WCHAR *buf, size_t n)
{
    DEVNAMES *dn = hdn ? GlobalLock(hdn) : NULL;

    buf[0] = 0;
    if (dn)
    {
        wcsncpy(buf, (const WCHAR *)dn + dn->wDeviceOffset, n - 1);
        buf[n - 1] = 0;
        GlobalUnlock(hdn);
    }
    return buf;
}

static const WCHAR *portname(HGLOBAL hdn, WCHAR *buf, size_t n)
{
    DEVNAMES *dn = hdn ? GlobalLock(hdn) : NULL;

    buf[0] = 0;
    if (dn)
    {
        wcsncpy(buf, (const WCHAR *)dn + dn->wOutputOffset, n - 1);
        buf[n - 1] = 0;
        GlobalUnlock(hdn);
    }
    return buf;
}

static void free_results(PRINTDLGEXW *pd)
{
    if (pd->hDevMode) GlobalFree(pd->hDevMode);
    if (pd->hDevNames) GlobalFree(pd->hDevNames);
    if (pd->hDC) DeleteDC(pd->hDC);
    pd->hDevMode = pd->hDevNames = NULL;
    pd->hDC = NULL;
}

/* ------------------------------------------------------------------ the callback object */

typedef struct
{
    IPrintDialogCallback cb;
    IObjectWithSite site;
    LONG ref;
    IPrintDialogServices *services;
    PRINTDLGEXW *pd;
    int set_site, init_done, sel_change, msgs, init_msgs, abort_on_app;
    BOOL init_lp_ok;
    BOOL services_at_init;
    WCHAR init_name[64], init_port[64], init_dm_name[64], sel_name[64];
    UINT init_name_len, init_dm_size;
    HWND child;
    HRESULT msg_result;     /* HandleMessage result for WM_APP + 2 */
    int app2_seen;
} Cb;

static void cb_read_services(Cb *c, WCHAR *name, WCHAR *port, WCHAR *dmname)
{
    UINT n;

    if (!c->services) return;
    n = 0;
    if (SUCCEEDED(c->services->lpVtbl->GetCurrentPrinterName(c->services, NULL, &n)) && n && n < 64)
    {
        if (name == c->init_name) c->init_name_len = n;
        c->services->lpVtbl->GetCurrentPrinterName(c->services, name, &n);
    }
    n = 0;
    if (port && SUCCEEDED(c->services->lpVtbl->GetCurrentPortName(c->services, NULL, &n)) && n && n < 64)
        c->services->lpVtbl->GetCurrentPortName(c->services, port, &n);
    n = 0;
    if (dmname && SUCCEEDED(c->services->lpVtbl->GetCurrentDevMode(c->services, NULL, &n)) && n)
    {
        DEVMODEW *dm = malloc(n);
        UINT n2 = n;

        c->init_dm_size = n;
        if (dm && SUCCEEDED(c->services->lpVtbl->GetCurrentDevMode(c->services, dm, &n2)) && n2 == n)
            wcsncpy(dmname, dm->dmDeviceName, 63);
        free(dm);
    }
}

static HRESULT WINAPI cb_QI(IPrintDialogCallback *iface, REFIID riid, void **out)
{
    Cb *c = (Cb *)iface;

    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IPrintDialogCallback)) *out = &c->cb;
    else if (IsEqualIID(riid, &IID_IObjectWithSite)) *out = &c->site;
    else { *out = NULL; return E_NOINTERFACE; }
    InterlockedIncrement(&c->ref);
    return S_OK;
}
static ULONG WINAPI cb_AddRef(IPrintDialogCallback *iface) { return InterlockedIncrement(&((Cb *)iface)->ref); }
static ULONG WINAPI cb_Release(IPrintDialogCallback *iface) { return InterlockedDecrement(&((Cb *)iface)->ref); }

static HRESULT WINAPI cb_InitDone(IPrintDialogCallback *iface)
{
    Cb *c = (Cb *)iface;

    c->init_done++;
    c->services_at_init = c->services != NULL;
    cb_read_services(c, c->init_name, c->init_port, c->init_dm_name);
    return S_OK;
}

static HRESULT WINAPI cb_SelectionChange(IPrintDialogCallback *iface)
{
    Cb *c = (Cb *)iface;

    c->sel_change++;
    cb_read_services(c, c->sel_name, NULL, NULL);
    return S_FALSE;
}

static HRESULT WINAPI cb_HandleMessage(IPrintDialogCallback *iface, HWND hdlg, UINT msg, WPARAM wp, LPARAM lp, LRESULT *res)
{
    Cb *c = (Cb *)iface;

    c->msgs++;
    c->child = hdlg;
    if (msg == WM_INITDIALOG)
    {
        c->init_msgs++;
        c->init_lp_ok = (PRINTDLGEXW *)lp == c->pd;
    }
    if (msg == WM_APP + 1 && c->abort_on_app)
    {
        PostMessageW(hdlg, WM_COMMAND, IDABORT, 0);
        *res = TRUE;
        return S_OK;
    }
    if (msg == WM_APP + 2)
    {
        c->app2_seen++;
        *res = 0x1234;
        return c->msg_result;
    }
    return S_FALSE;
}

static HRESULT WINAPI site_QI(IObjectWithSite *iface, REFIID riid, void **out)
{
    Cb *c = CONTAINING_RECORD(iface, Cb, site);
    return cb_QI(&c->cb, riid, out);
}
static ULONG WINAPI site_AddRef(IObjectWithSite *iface) { return InterlockedIncrement(&CONTAINING_RECORD(iface, Cb, site)->ref); }
static ULONG WINAPI site_Release(IObjectWithSite *iface) { return InterlockedDecrement(&CONTAINING_RECORD(iface, Cb, site)->ref); }

static HRESULT WINAPI site_SetSite(IObjectWithSite *iface, IUnknown *site)
{
    Cb *c = CONTAINING_RECORD(iface, Cb, site);

    c->set_site++;
    if (c->services) { c->services->lpVtbl->Release(c->services); c->services = NULL; }
    if (site) IUnknown_QueryInterface(site, &IID_IPrintDialogServices, (void **)&c->services);
    return S_OK;
}
static HRESULT WINAPI site_GetSite(IObjectWithSite *iface, REFIID riid, void **out) { *out = NULL; return E_FAIL; }

static IPrintDialogCallbackVtbl cb_vtbl = { cb_QI, cb_AddRef, cb_Release, cb_InitDone, cb_SelectionChange, cb_HandleMessage };
static IObjectWithSiteVtbl site_vtbl = { site_QI, site_AddRef, site_Release, site_SetSite, site_GetSite };

static void cb_init(Cb *c, PRINTDLGEXW *pd)
{
    memset(c, 0, sizeof(*c));
    c->cb.lpVtbl = &cb_vtbl;
    c->site.lpVtbl = &site_vtbl;
    c->ref = 1;
    c->pd = pd;
}

/* ------------------------------------------------------------------ PrintDlgEx scenarios */

static void setup_pd(PRINTDLGEXW *pd, PRINTPAGERANGE *ranges)
{
    memset(pd, 0, sizeof(*pd));
    pd->lStructSize = sizeof(*pd);
    pd->hwndOwner = owner;
    pd->Flags = PD_RETURNDC;
    pd->nStartPage = START_PAGE_GENERAL;
    pd->nPageRanges = 0;
    pd->nMaxPageRanges = 8;
    pd->lpPageRanges = ranges;
    pd->nMinPage = 1;
    pd->nMaxPage = 20;
    pd->nCopies = 1;
}

/* pages 1-3,5, then Print */
static void script_ranges(HWND sheet, void *arg)
{
    HWND edit = ctl(sheet, edt1);

    SendMessageW(edit, WM_SETTEXT, 0, (LPARAM)L"1-3, 5");
    *(int *)arg = SendMessageW(ctl(sheet, rad3), BM_GETCHECK, 0, 0) == BST_CHECKED;
    press(sheet, PSBTN_OK);
}

struct invalid_state { int box_seen, still_open, radio3; };

static BOOL CALLBACK find_box_proc(HWND hwnd, LPARAM lp)
{
    WCHAR cls[32];

    GetClassNameW(hwnd, cls, 32);
    if (!wcscmp(cls, L"#32770") && !ctl(hwnd, lst1) && IsWindowVisible(hwnd) && GetDlgItem(hwnd, IDOK))
        *(HWND *)lp = hwnd;
    return TRUE;
}

static void script_invalid(HWND sheet, void *arg)
{
    struct invalid_state *s = arg;
    HWND box = NULL;
    int i;

    SendMessageW(ctl(sheet, edt1), WM_SETTEXT, 0, (LPARAM)L"0-3");
    PostMessageW(sheet, PSM_PRESSBUTTON, PSBTN_OK, 0);
    for (i = 0; i < 100 && !box; i++) { EnumWindows(find_box_proc, (LPARAM)&box); if (!box) Sleep(50); }
    s->box_seen = box != NULL;
    if (box) PostMessageW(box, WM_COMMAND, IDOK, 0);
    Sleep(300);
    s->still_open = IsWindow(sheet);
    SendMessageW(ctl(sheet, edt1), WM_SETTEXT, 0, (LPARAM)L"21");
    PostMessageW(sheet, PSM_PRESSBUTTON, PSBTN_OK, 0);
    box = NULL;
    for (i = 0; i < 100 && !box; i++) { EnumWindows(find_box_proc, (LPARAM)&box); if (!box) Sleep(50); }
    if (box) { s->box_seen++; PostMessageW(box, WM_COMMAND, IDOK, 0); }
    Sleep(300);
    SendMessageW(ctl(sheet, edt1), WM_SETTEXT, 0, (LPARAM)L"2");
    press(sheet, PSBTN_OK);
}

static void test_ranges(void)
{
    PRINTDLGEXW pd;
    PRINTPAGERANGE ranges[8];
    WCHAR name[64];
    int rad3_checked = 0;
    HRESULT hr;
    struct invalid_state is = { 0 };

    setup_pd(&pd, ranges);
    hr = run_ex(&pd, script_ranges, &rad3_checked);
    CHECK(hr == S_OK, "PrintDlgEx: %#lx", hr);
    CHECK(pd.dwResultAction == PD_RESULT_PRINT, "action %lu", pd.dwResultAction);
    CHECK(rad3_checked, "typing in the pages edit selects the Pages radio");
    CHECK((pd.Flags & PD_PAGENUMS) != 0, "PD_PAGENUMS: %#lx", pd.Flags);
    CHECK(pd.nPageRanges == 2, "two ranges: %lu", pd.nPageRanges);
    if (pd.nPageRanges == 2)
        CHECK(ranges[0].nFromPage == 1 && ranges[0].nToPage == 3 && ranges[1].nFromPage == 5 && ranges[1].nToPage == 5,
              "ranges %lu-%lu, %lu-%lu", ranges[0].nFromPage, ranges[0].nToPage, ranges[1].nFromPage, ranges[1].nToPage);
    CHECK(!wcscmp(devname(pd.hDevNames, name, 64), L"SG Cdlg A"), "default printer: %ls", name);
    CHECK(!wcscmp(portname(pd.hDevNames, name, 64), L"FILE:"), "port: %ls", name);
    CHECK(pd.hDC != NULL, "PD_RETURNDC gave a DC");
    free_results(&pd);

    /* invalid input keeps the sheet open and says so */
    setup_pd(&pd, ranges);
    hr = run_ex(&pd, script_invalid, &is);
    CHECK(hr == S_OK && pd.dwResultAction == PD_RESULT_PRINT, "after correcting: %#lx %lu", hr, pd.dwResultAction);
    CHECK(is.box_seen == 2, "message boxes for 0-3 and 21: %d", is.box_seen);
    CHECK(is.still_open, "the sheet stayed open after the invalid range");
    CHECK(pd.nPageRanges == 1 && ranges[0].nFromPage == 2 && ranges[0].nToPage == 2, "corrected range");
    free_results(&pd);
}

/* initial state: ranges shown, current page radio, disabled controls */
struct init_state
{
    WCHAR pages[64];
    int rad[5];
    BOOL en_rad2, en_rad3, en_rad4, en_edt1, listcount, list_sel;
    WCHAR sel_text[64];
    int chx1_checked, chx1_visible;
};

static void script_init_state(HWND sheet, void *arg)
{
    struct init_state *s = arg;
    int i;
    LVITEMW item = { 0 };
    int sel;

    GetWindowTextW(ctl(sheet, edt1), s->pages, 64);
    for (i = 1; i <= 4; i++) s->rad[i] = SendMessageW(ctl(sheet, rad1 + i - 1), BM_GETCHECK, 0, 0) == BST_CHECKED;
    s->en_rad2 = IsWindowEnabled(ctl(sheet, rad2));
    s->en_rad3 = IsWindowEnabled(ctl(sheet, rad3));
    s->en_rad4 = IsWindowEnabled(ctl(sheet, rad4));
    s->en_edt1 = IsWindowEnabled(ctl(sheet, edt1));
    s->listcount = SendMessageW(ctl(sheet, lst1), LVM_GETITEMCOUNT, 0, 0);
    sel = SendMessageW(ctl(sheet, lst1), LVM_GETNEXTITEM, -1, LVNI_SELECTED);
    s->list_sel = sel;
    if (sel >= 0)
    {
        item.pszText = s->sel_text;
        item.cchTextMax = 64;
        SendMessageW(ctl(sheet, lst1), LVM_GETITEMTEXTW, sel, (LPARAM)&item);
    }
    s->chx1_checked = SendMessageW(ctl(sheet, chx1), BM_GETCHECK, 0, 0) == BST_CHECKED;
    s->chx1_visible = IsWindowVisible(ctl(sheet, chx1));
    press(sheet, PSBTN_CANCEL);
}

static void test_initial_state(void)
{
    PRINTDLGEXW pd;
    PRINTPAGERANGE ranges[8];
    struct init_state s;
    HRESULT hr;
    HGLOBAL hdn;

    /* ranges given: shown in the pages edit with Pages selected */
    setup_pd(&pd, ranges);
    pd.Flags |= PD_PAGENUMS | PD_NOSELECTION | PD_PRINTTOFILE;
    pd.nPageRanges = 2;
    ranges[0].nFromPage = 1; ranges[0].nToPage = 3;
    ranges[1].nFromPage = 7; ranges[1].nToPage = 7;
    memset(&s, 0, sizeof(s));
    hr = run_ex(&pd, script_init_state, &s);
    CHECK(hr == S_OK && pd.dwResultAction == PD_RESULT_CANCEL, "cancel: %#lx %lu", hr, pd.dwResultAction);
    CHECK(!wcscmp(s.pages, L"1-3, 7"), "pages text '%ls'", s.pages);
    CHECK(s.rad[3] && !s.rad[1], "Pages radio selected");
    CHECK(!s.en_rad2, "PD_NOSELECTION disables Selection");
    CHECK(s.en_rad3 && s.en_rad4 && s.en_edt1, "other controls enabled");
    CHECK(s.listcount >= 2, "both printers listed: %d", s.listcount);
    CHECK(!wcscmp(s.sel_text, L"SG Cdlg A"), "default printer selected: '%ls'", s.sel_text);
    CHECK(s.chx1_checked, "PD_PRINTTOFILE checks Print to file");
    CHECK(pd.hDC == NULL && pd.hDevMode == NULL && pd.hDevNames == NULL, "a cancel leaves the structure alone");

    /* current page, no page numbers, hidden print to file */
    setup_pd(&pd, ranges);
    pd.Flags |= PD_CURRENTPAGE | PD_NOPAGENUMS | PD_HIDEPRINTTOFILE;
    memset(&s, 0, sizeof(s));
    run_ex(&pd, script_init_state, &s);
    CHECK(s.rad[4], "Current Page selected");
    CHECK(!s.en_rad3 && !s.en_edt1, "PD_NOPAGENUMS disables Pages");
    CHECK(!s.chx1_visible, "PD_HIDEPRINTTOFILE hides Print to file");

    setup_pd(&pd, ranges);
    pd.Flags |= PD_CURRENTPAGE | PD_NOCURRENTPAGE;
    memset(&s, 0, sizeof(s));
    run_ex(&pd, script_init_state, &s);
    CHECK(!s.en_rad4 && s.rad[1], "PD_NOCURRENTPAGE disables Current Page (All selected)");

    /* the printer of the DEVNAMES is the one selected */
    hdn = make_devnames(L"SG Cdlg B");
    setup_pd(&pd, ranges);
    pd.hDevNames = hdn;
    memset(&s, 0, sizeof(s));
    run_ex(&pd, script_init_state, &s);
    CHECK(!wcscmp(s.sel_text, L"SG Cdlg B"), "DEVNAMES printer selected: '%ls'", s.sel_text);
    GlobalFree(hdn);
}

/* a printer chosen in the list, services and callback, copies, collate, apply, cancel */
struct pick_state { int copies_enabled, collate_enabled; WCHAR names[3][64]; int sel_changes; };

static void list_select(HWND sheet, int index)
{
    LVITEMW item = { 0 };

    item.stateMask = item.state = LVIS_SELECTED | LVIS_FOCUSED;
    SendMessageW(ctl(sheet, lst1), LVM_SETITEMSTATE, index, (LPARAM)&item);
}

static int list_find(HWND sheet, const WCHAR *name)
{
    HWND list = ctl(sheet, lst1);
    int i, n = SendMessageW(list, LVM_GETITEMCOUNT, 0, 0);
    WCHAR buf[64];
    LVITEMW item = { 0 };

    for (i = 0; i < n; i++)
    {
        item.pszText = buf;
        item.cchTextMax = 64;
        SendMessageW(list, LVM_GETITEMTEXTW, i, (LPARAM)&item);
        if (!wcscmp(buf, name)) return i;
    }
    return -1;
}

static void script_pick(HWND sheet, void *arg)
{
    list_select(sheet, list_find(sheet, L"SG Cdlg B"));
    SendMessageW(ctl(sheet, edt3), WM_SETTEXT, 0, (LPARAM)L"3");
    SendMessageW(ctl(sheet, chx2), BM_CLICK, 0, 0);
    SendMessageW(ctl(sheet, chx1), BM_CLICK, 0, 0);
    press(sheet, PSBTN_OK);
}

static void test_pick_and_callback(void)
{
    PRINTDLGEXW pd;
    PRINTPAGERANGE ranges[8];
    Cb cb;
    WCHAR name[64];
    HRESULT hr;
    DEVMODEW *dm;

    setup_pd(&pd, ranges);
    pd.Flags |= PD_NOPAGENUMS;
    cb_init(&cb, &pd);
    pd.lpCallback = (IUnknown *)&cb.cb;
    hr = run_ex(&pd, script_pick, NULL);
    CHECK(hr == S_OK && pd.dwResultAction == PD_RESULT_PRINT, "print: %#lx %lu", hr, pd.dwResultAction);
    CHECK(cb.set_site >= 2, "SetSite called with the services and again with NULL: %d", cb.set_site);
    CHECK(cb.init_done == 1 && cb.services_at_init, "InitDone once, services set before: %d %d", cb.init_done, cb.services_at_init);
    CHECK(!wcscmp(cb.init_name, L"SG Cdlg A"), "services printer name at InitDone: '%ls'", cb.init_name);
    CHECK(cb.init_name_len == 10, "size query for the printer name: %u", cb.init_name_len);
    CHECK(!wcscmp(cb.init_port, L"FILE:"), "services port name: '%ls'", cb.init_port);
    CHECK(!wcscmp(cb.init_dm_name, L"SG Cdlg A"), "services devmode device: '%ls'", cb.init_dm_name);
    CHECK(cb.init_dm_size >= sizeof(DEVMODEW), "services devmode size %u", cb.init_dm_size);
    CHECK(cb.init_msgs == 1 && cb.init_lp_ok, "HandleMessage got WM_INITDIALOG once with the PRINTDLGEX: %d %d", cb.init_msgs, cb.init_lp_ok);
    CHECK(cb.msgs > 3, "HandleMessage got the child's messages: %d", cb.msgs);
    CHECK(!wcscmp(cb.sel_name, L"SG Cdlg B"), "SelectionChange saw the new printer through the services: '%ls'", cb.sel_name);
    CHECK(!wcscmp(devname(pd.hDevNames, name, 64), L"SG Cdlg B"), "chosen printer in DEVNAMES: '%ls'", name);
    CHECK(!wcscmp(portname(pd.hDevNames, name, 64), L"FILE:"), "print to file: port '%ls'", name);
    CHECK((pd.Flags & PD_PRINTTOFILE) != 0, "PD_PRINTTOFILE");
    CHECK(pd.nCopies == 3 && (pd.Flags & PD_COLLATE), "copies and collate for the application: %lu %#lx", pd.nCopies, pd.Flags);
    dm = pd.hDevMode ? GlobalLock(pd.hDevMode) : NULL;
    CHECK(dm && !wcscmp(dm->dmDeviceName, L"SG Cdlg B") && dm->dmCopies == 1, "DEVMODE of the chosen printer, copies 1: %u", dm ? dm->dmCopies : 0);
    if (dm) GlobalUnlock(pd.hDevMode);
    CHECK(cb.ref == 1, "callback released: %ld", cb.ref);
    free_results(&pd);

    /* the driver does the copies: nCopies 1, no collate flag */
    setup_pd(&pd, ranges);
    pd.Flags |= PD_NOPAGENUMS | PD_USEDEVMODECOPIESANDCOLLATE;
    hr = run_ex(&pd, script_pick, NULL);
    CHECK(pd.dwResultAction == PD_RESULT_PRINT && pd.nCopies == 1 && !(pd.Flags & PD_COLLATE),
          "PD_USEDEVMODECOPIESANDCOLLATE: %lu %#lx", pd.nCopies, pd.Flags);
    dm = pd.hDevMode ? GlobalLock(pd.hDevMode) : NULL;
    CHECK(dm && (!(dm->dmFields & DM_COPIES) || dm->dmCopies == 3), "devmode copies %u (fields %#lx)", dm ? dm->dmCopies : 0, dm ? dm->dmFields : 0);
    if (dm) GlobalUnlock(pd.hDevMode);
    free_results(&pd);
}

/* Apply, then Cancel -> PD_RESULT_APPLY with the applied data; Cancel alone -> CANCEL */
static void script_apply_cancel(HWND sheet, void *arg)
{
    HWND apply = GetDlgItem(sheet, 0x3021);
    int enabled_before = apply && IsWindowEnabled(apply);

    SendMessageW(ctl(sheet, edt3), WM_SETTEXT, 0, (LPARAM)L"4");
    *(int *)arg = (apply && IsWindowEnabled(apply)) * 2 + !enabled_before;
    press(sheet, PSBTN_APPLYNOW);
    SendMessageW(ctl(sheet, edt3), WM_SETTEXT, 0, (LPARAM)L"9");
    press(sheet, PSBTN_CANCEL);
}

static void test_apply(void)
{
    PRINTDLGEXW pd;
    PRINTPAGERANGE ranges[8];
    HRESULT hr;
    int state = 0;

    setup_pd(&pd, ranges);
    pd.Flags |= PD_NOPAGENUMS;
    hr = run_ex(&pd, script_apply_cancel, &state);
    CHECK(hr == S_OK && pd.dwResultAction == PD_RESULT_APPLY, "Apply then Cancel: %#lx %lu", hr, pd.dwResultAction);
    CHECK(pd.nCopies == 4, "the data at the time of Apply: %lu", pd.nCopies);
    CHECK(state == 3, "Apply disabled until something changes, enabled after: %d", state);
    free_results(&pd);
}

/* IDABORT posted by a callback closes the sheet as a cancel */
static void script_abort(HWND sheet, void *arg)
{
    Cb *c = arg;

    SendMessageW(ctl(sheet, edt3), WM_SETTEXT, 0, (LPARAM)L"6");
    press(sheet, PSBTN_APPLYNOW);
    SendMessageW(c->child, WM_APP + 1, 0, 0);
    Sleep(500);
    if (IsWindow(sheet)) press(sheet, PSBTN_CANCEL);
}

static void script_handled(HWND sheet, void *arg)
{
    Cb *c = arg;
    LRESULT r;

    c->msg_result = S_OK;
    r = SendMessageW(c->child, WM_APP + 2, 0, 0);
    c->app2_seen += 100 * (r == 0x1234);
    c->msg_result = S_FALSE;
    r = SendMessageW(c->child, WM_APP + 2, 0, 0);
    c->app2_seen += 1000 * (r != 0x1234);
    press(sheet, PSBTN_CANCEL);
}

static void test_callback_messages(void)
{
    PRINTDLGEXW pd;
    PRINTPAGERANGE ranges[8];
    Cb cb;
    HRESULT hr;

    setup_pd(&pd, ranges);
    pd.Flags |= PD_NOPAGENUMS;
    cb_init(&cb, &pd);
    cb.abort_on_app = 1;
    pd.lpCallback = (IUnknown *)&cb.cb;
    hr = run_ex(&pd, script_abort, &cb);
    CHECK(hr == S_OK && pd.dwResultAction == PD_RESULT_CANCEL, "IDABORT: %#lx %lu", hr, pd.dwResultAction);
    free_results(&pd);

    setup_pd(&pd, ranges);
    pd.Flags |= PD_NOPAGENUMS;
    cb_init(&cb, &pd);
    pd.lpCallback = (IUnknown *)&cb.cb;
    hr = run_ex(&pd, script_handled, &cb);
    CHECK(cb.app2_seen == 1102, "S_OK from HandleMessage is the dialog result, S_FALSE is default handling: %d", cb.app2_seen);
}

/* current page / selection results and the ANSI entry point */
static void script_current(HWND sheet, void *arg)
{
    SendMessageW(ctl(sheet, rad4), BM_CLICK, 0, 0);
    press(sheet, PSBTN_OK);
}

static void script_selection(HWND sheet, void *arg)
{
    SendMessageW(ctl(sheet, rad2), BM_CLICK, 0, 0);
    press(sheet, PSBTN_OK);
}

static void test_radios(void)
{
    PRINTDLGEXW pd;
    PRINTPAGERANGE ranges[8];

    setup_pd(&pd, ranges);
    run_ex(&pd, script_current, NULL);
    CHECK(pd.dwResultAction == PD_RESULT_PRINT && (pd.Flags & PD_CURRENTPAGE) && !(pd.Flags & (PD_PAGENUMS | PD_SELECTION)),
          "Current Page: %#lx", pd.Flags);
    free_results(&pd);
    setup_pd(&pd, ranges);
    run_ex(&pd, script_selection, NULL);
    CHECK(pd.dwResultAction == PD_RESULT_PRINT && (pd.Flags & PD_SELECTION) && !(pd.Flags & (PD_PAGENUMS | PD_CURRENTPAGE)),
          "Selection: %#lx", pd.Flags);
    free_results(&pd);
}

static void test_ansi(void)
{
    PRINTDLGEXA pd;
    PRINTPAGERANGE ranges[8];
    HRESULT hr;
    DEVNAMES *dn;
    DEVMODEA *dm;
    int ok = 0;

    memset(&pd, 0, sizeof(pd));
    pd.lStructSize = sizeof(pd);
    pd.hwndOwner = owner;
    pd.Flags = PD_RETURNDC;
    pd.nStartPage = START_PAGE_GENERAL;
    pd.nMaxPageRanges = 8;
    pd.lpPageRanges = ranges;
    pd.nMinPage = 1;
    pd.nMaxPage = 20;
    pd.nCopies = 1;
    hr = run_exA(&pd, script_ranges, &ok);
    CHECK(hr == S_OK && pd.dwResultAction == PD_RESULT_PRINT, "PrintDlgExA: %#lx %lu", hr, pd.dwResultAction);
    CHECK(pd.nPageRanges == 2 && ranges[1].nFromPage == 5, "ANSI ranges: %lu", pd.nPageRanges);
    dn = pd.hDevNames ? GlobalLock(pd.hDevNames) : NULL;
    CHECK(dn && !strcmp((char *)dn + dn->wDeviceOffset, "SG Cdlg A") && !strcmp((char *)dn + dn->wOutputOffset, "FILE:"),
          "ANSI DEVNAMES: '%s'", dn ? (char *)dn + dn->wDeviceOffset : "(none)");
    if (dn) GlobalUnlock(pd.hDevNames);
    dm = pd.hDevMode ? GlobalLock(pd.hDevMode) : NULL;
    CHECK(dm && !strcmp((char *)dm->dmDeviceName, "SG Cdlg A"), "ANSI DEVMODE device name");
    if (dm) GlobalUnlock(pd.hDevMode);
    if (pd.hDevMode) GlobalFree(pd.hDevMode);
    if (pd.hDevNames) GlobalFree(pd.hDevNames);
    if (pd.hDC) DeleteDC(pd.hDC);
}

/* custom pages: start on the first, Print still collects the General page */
static INT_PTR CALLBACK custom_page_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    static int inits;

    if (msg == WM_INITDIALOG) inits++;
    if (msg == WM_USER + 99) return inits;
    return FALSE;
}

static void script_custom(HWND sheet, void *arg)
{
    HWND tab = FindWindowExW(sheet, NULL, L"SysTabControl32", NULL);

    *(int *)arg = tab ? (int)SendMessageW(tab, TCM_GETITEMCOUNT, 0, 0) * 100 + (int)SendMessageW(tab, TCM_GETCURSEL, 0, 0) : -1;
    press(sheet, PSBTN_OK);
}

#pragma pack(push, 2)
static struct { DLGTEMPLATE t; WORD menu, cls, title; } page_tmpl = { { WS_CHILD | WS_VISIBLE | WS_CAPTION, 0, 0, 0, 0, 100, 60 }, 0, 0, 0 };
#pragma pack(pop)

static void test_custom_pages(void)
{
    PRINTDLGEXW pd;
    PRINTPAGERANGE ranges[8];
    PROPSHEETPAGEW psp = { sizeof(psp) };
    HPROPSHEETPAGE pages[1];
    HRESULT hr;
    int tabs = 0;

    psp.dwFlags = PSP_DLGINDIRECT | PSP_USETITLE;
    psp.pResource = &page_tmpl.t;
    psp.pszTitle = L"SG Custom";
    psp.pfnDlgProc = custom_page_proc;
    pages[0] = CreatePropertySheetPageW(&psp);

    setup_pd(&pd, ranges);
    pd.Flags |= PD_NOPAGENUMS;
    pd.nPropertyPages = 1;
    pd.lphPropertyPages = pages;
    pd.nStartPage = 0;
    hr = run_ex(&pd, script_custom, &tabs);
    CHECK(hr == S_OK && pd.dwResultAction == PD_RESULT_PRINT, "custom page: %#lx %lu", hr, pd.dwResultAction);
    CHECK(tabs == 201, "two tabs (General + custom), the custom one first shown (index 1): %d", tabs);
    CHECK(pd.hDevNames != NULL, "the General page was collected without being visited");
    free_results(&pd);

    /* a start page past the pages given is an error already checked by the argument tests */
    setup_pd(&pd, ranges);
    pd.nStartPage = 0;
    hr = PrintDlgExW(&pd);
    CHECK(hr == E_INVALIDARG, "nStartPage without pages: %#lx", hr);
}


/* ------------------------------------------------------------------ file dialog */

static WCHAR testdir[MAX_PATH];

static void make_link(const WCHAR *lnk, const WCHAR *target)
{
    IShellLinkW *sl;
    IPersistFile *pf;

    if (SUCCEEDED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, &IID_IShellLinkW, (void **)&sl)))
    {
        IShellLinkW_SetPath(sl, target);
        if (SUCCEEDED(IShellLinkW_QueryInterface(sl, &IID_IPersistFile, (void **)&pf)))
        {
            IPersistFile_Save(pf, lnk, TRUE);
            IPersistFile_Release(pf);
        }
        IShellLinkW_Release(sl);
    }
}

static void make_file(const WCHAR *path, const char *text)
{
    DWORD n;
    HANDLE f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);

    WriteFile(f, text, strlen(text), &n, NULL);
    CloseHandle(f);
}

static struct
{
    const WCHAR *typed;         /* text for the file name box */
    const WCHAR *defext;        /* CDM_SETDEFEXT, if set */
    BOOL browser;               /* run the IShellBrowser checks */
    WCHAR folder[MAX_PATH];     /* CDM_GETFOLDERPATH at the end (before closing) */
    int closed_by_us;
    BOOL modern;                /* no hook: the Common Item Dialog based one */
} fd;

#define WM_GETISHELLBROWSER_ (WM_USER + 7)

static void test_browser(IShellBrowser *b, HWND dlg)
{
    HWND h = (HWND)1;
    OLEMENUGROUPWIDTHS w = { { 7, 7, 7, 7, 7, 7 } };
    IStream *s;
    char buf[8] = { 0 };
    ULONG n;
    LARGE_INTEGER zero = { { 0 } };
    HRESULT hr;

    CHECK(b != NULL, "WM_GETISHELLBROWSER gave the browser");
    if (!b) return;
    hr = b->lpVtbl->ContextSensitiveHelp(b, FALSE);
    CHECK(hr == S_OK, "ContextSensitiveHelp %#lx", hr);
    hr = b->lpVtbl->EnableModelessSB(b, TRUE);
    CHECK(hr == S_OK, "EnableModelessSB %#lx", hr);
    hr = b->lpVtbl->GetControlWindow(b, FCW_TOOLBAR, &h);
    CHECK(hr == S_OK && h == GetDlgItem(dlg, 1) && h, "GetControlWindow(toolbar) %#lx %p", hr, h);
    h = (HWND)1;
    hr = b->lpVtbl->GetControlWindow(b, FCW_STATUS, &h);
    CHECK(hr == S_OK && h == NULL, "GetControlWindow(status) %#lx %p", hr, h);
    h = (HWND)1;
    hr = b->lpVtbl->GetControlWindow(b, FCW_TREE, &h);
    CHECK(hr == S_OK && h == NULL, "GetControlWindow(tree) %#lx %p", hr, h);
    hr = b->lpVtbl->GetControlWindow(b, 77, &h);
    CHECK(hr == E_INVALIDARG, "GetControlWindow(77) %#lx", hr);
    hr = b->lpVtbl->GetControlWindow(b, FCW_TOOLBAR, NULL);
    CHECK(hr == E_INVALIDARG, "GetControlWindow(NULL) %#lx", hr);
    hr = b->lpVtbl->InsertMenusSB(b, NULL, &w);
    CHECK(hr == S_OK && w.width[0] == 0 && w.width[2] == 0 && w.width[4] == 0 && w.width[1] == 7 && w.width[3] == 7,
          "InsertMenusSB %#lx %ld %ld %ld %ld", hr, w.width[0], w.width[1], w.width[2], w.width[4]);
    hr = b->lpVtbl->RemoveMenusSB(b, NULL);
    CHECK(hr == S_OK, "RemoveMenusSB %#lx", hr);
    hr = b->lpVtbl->SetMenuSB(b, NULL, NULL, NULL);
    CHECK(hr == S_OK, "SetMenuSB %#lx", hr);
    hr = b->lpVtbl->OnViewWindowActive(b, NULL);
    CHECK(hr == S_OK, "OnViewWindowActive %#lx", hr);
    hr = b->lpVtbl->SetStatusTextSB(b, L"text");
    CHECK(hr == S_OK, "SetStatusTextSB %#lx", hr);
    hr = b->lpVtbl->SetToolbarItems(b, NULL, 0, 0);
    CHECK(hr == S_OK, "SetToolbarItems %#lx", hr);
    hr = b->lpVtbl->TranslateAcceleratorSB(b, NULL, 0);
    CHECK(hr == S_FALSE, "TranslateAcceleratorSB %#lx", hr);

    hr = b->lpVtbl->GetViewStateStream(b, STGM_WRITE, &s);
    CHECK(hr == S_OK && s, "GetViewStateStream(write) %#lx", hr);
    if (SUCCEEDED(hr))
    {
        hr = IStream_Write(s, "state", 5, &n);
        CHECK(hr == S_OK && n == 5, "write the state");
        IStream_Release(s);
    }
    hr = b->lpVtbl->GetViewStateStream(b, STGM_READ, &s);
    CHECK(hr == S_OK && s, "GetViewStateStream(read) %#lx", hr);
    if (SUCCEEDED(hr))
    {
        IStream_Seek(s, zero, STREAM_SEEK_SET, NULL);
        hr = IStream_Read(s, buf, 7, &n);
        CHECK(n == 5 && !memcmp(buf, "state", 5), "the state is read back: %lu '%s'", n, buf);
        IStream_Release(s);
    }
    hr = b->lpVtbl->GetViewStateStream(b, STGM_WRITE, &s);
    if (SUCCEEDED(hr))
    {
        IStream_Write(s, "xy", 2, &n);
        IStream_Release(s);
    }
    hr = b->lpVtbl->GetViewStateStream(b, STGM_READ, &s);
    if (SUCCEEDED(hr))
    {
        memset(buf, 0, 8);
        IStream_Read(s, buf, 7, &n);
        CHECK(n == 2, "a write replaces the state: %lu", n);
        IStream_Release(s);
    }
    hr = b->lpVtbl->GetViewStateStream(b, STGM_READ, NULL);
    CHECK(hr == E_INVALIDARG, "GetViewStateStream(NULL) %#lx", hr);
}

static UINT_PTR CALLBACK fd_hook(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_NOTIFY && ((NMHDR *)lp)->code == CDN_INITDONE)
    {
        HWND dlg = GetParent(hdlg);

        if (fd.browser) test_browser((IShellBrowser *)SendMessageW(dlg, WM_GETISHELLBROWSER_, 0, 0), dlg);
        if (fd.defext) SendMessageW(dlg, CDM_SETDEFEXT, 0, (LPARAM)fd.defext);
    }
    if (msg == WM_NOTIFY && ((NMHDR *)lp)->code == CDN_FOLDERCHANGE)
    {
        HWND dlg = GetParent(hdlg);

        SendMessageW(dlg, CDM_GETFOLDERPATH, MAX_PATH, (LPARAM)fd.folder);
    }
    return 0;
}

/* types the name once the dialog has settled (the first folder change clears the box), presses OK,
 * and cancels when the dialog is still there later (a shortcut to a folder keeps it open) */
static DWORD WINAPI fd_closer(void *p)
{
    int i;
    HWND dlg = NULL;
    BOOL save = (BOOL)(INT_PTR)p;

    for (i = 0; i < 100 && !dlg; i++)
    {
        Sleep(100);
        dlg = FindWindowW(L"#32770", save ? L"Save As" : L"Open");
    }
    if (!dlg) return 0;
    Sleep(1500);
    if (fd.typed && IsWindow(dlg))
    {
        HWND name = ctl(dlg, edt1);

        if (!name && !fd.modern) name = NULL;
        if (!name) name = (HWND)SendDlgItemMessageW(dlg, cmb13, CBEM_GETEDITCONTROL, 0, 0);
        SendMessageW(name, WM_SETTEXT, 0, (LPARAM)fd.typed);
        PostMessageW(dlg, WM_COMMAND, IDOK, 0);
    }
    for (i = 0; i < 40 && IsWindow(dlg); i++) Sleep(100);
    if (IsWindow(dlg)) { fd.closed_by_us = 1; PostMessageW(dlg, WM_COMMAND, IDCANCEL, 0); }
    return 0;
}

static BOOL open_dialog(const WCHAR *typed, DWORD extra_flags, WCHAR *result, BOOL save)
{
    OPENFILENAMEW ofn = { sizeof(ofn) };
    WCHAR file[MAX_PATH] = L"";
    BOOL ret;
    HANDLE th = NULL;

    fd.typed = typed;
    fd.folder[0] = 0;
    fd.closed_by_us = 0;
    ofn.hwndOwner = owner;
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrInitialDir = testdir;
    ofn.lpstrFilter = L"All\0*.*\0";
    ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | extra_flags;
    if (!fd.modern) { ofn.Flags |= OFN_ENABLEHOOK; ofn.lpfnHook = fd_hook; }
    th = CreateThread(NULL, 0, fd_closer, (void *)(INT_PTR)save, 0, NULL);
    ret = save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
    if (th) { WaitForSingleObject(th, 5000); CloseHandle(th); }
    wcscpy(result, ret ? file : L"");
    return ret;
}

static void test_filedialog(void)
{
    WCHAR path[MAX_PATH], res[MAX_PATH], want[MAX_PATH];
    BOOL ret;

    swprintf(path, MAX_PATH, L"%ls\\target.txt", testdir);
    make_file(path, "x");
    swprintf(path, MAX_PATH, L"%ls\\sub", testdir);
    CreateDirectoryW(path, NULL);
    swprintf(path, MAX_PATH, L"%ls\\ln.lnk", testdir);
    swprintf(want, MAX_PATH, L"%ls\\target.txt", testdir);
    make_link(path, want);
    swprintf(path, MAX_PATH, L"%ls\\dirln.lnk", testdir);
    swprintf(want, MAX_PATH, L"%ls\\sub", testdir);
    make_link(path, want);

    /* CDM_SETDEFEXT */
    memset(&fd, 0, sizeof(fd));
    fd.defext = L"sgx";
    ret = open_dialog(L"newname", OFN_OVERWRITEPROMPT, res, TRUE);
    swprintf(want, MAX_PATH, L"%ls\\newname.sgx", testdir);
    CHECK(ret && !_wcsicmp(res, want), "CDM_SETDEFEXT: '%ls'", res);
    memset(&fd, 0, sizeof(fd));
    ret = open_dialog(L"other", OFN_OVERWRITEPROMPT, res, TRUE);
    swprintf(want, MAX_PATH, L"%ls\\other", testdir);
    CHECK(ret && !_wcsicmp(res, want), "no default extension without it: '%ls'", res);

    /* IShellBrowser of the dialog */
    memset(&fd, 0, sizeof(fd));
    fd.browser = TRUE;
    fd.typed = L"target.txt";
    ret = open_dialog(L"target.txt", 0, res, FALSE);
    CHECK(ret && !fd.closed_by_us, "open dialog with the browser checks: closed by us %d err %#lx", fd.closed_by_us, CommDlgExtendedError());

    /* shortcuts */
    memset(&fd, 0, sizeof(fd));
    ret = open_dialog(L"ln.lnk", OFN_FILEMUSTEXIST, res, FALSE);
    swprintf(want, MAX_PATH, L"%ls\\target.txt", testdir);
    CHECK(ret && !_wcsicmp(res, want), "a shortcut stands for its target: '%ls'", res);
    memset(&fd, 0, sizeof(fd));
    ret = open_dialog(L"ln.lnk", OFN_FILEMUSTEXIST | OFN_NODEREFERENCELINKS, res, FALSE);
    swprintf(want, MAX_PATH, L"%ls\\ln.lnk", testdir);
    CHECK(ret && !_wcsicmp(res, want), "OFN_NODEREFERENCELINKS keeps the .lnk: '%ls'", res);
    /* the same through the Common Item Dialog based Open dialog */
    memset(&fd, 0, sizeof(fd));
    fd.modern = TRUE;
    ret = open_dialog(L"ln.lnk", OFN_FILEMUSTEXIST, res, FALSE);
    swprintf(want, MAX_PATH, L"%ls\\target.txt", testdir);
    CHECK(ret && !_wcsicmp(res, want), "modern dialog: a shortcut stands for its target: '%ls' (closed by us %d)", res, fd.closed_by_us);
    memset(&fd, 0, sizeof(fd));
    fd.modern = TRUE;
    ret = open_dialog(L"ln.lnk", OFN_FILEMUSTEXIST | OFN_NODEREFERENCELINKS, res, FALSE);
    swprintf(want, MAX_PATH, L"%ls\\ln.lnk", testdir);
    CHECK(ret && !_wcsicmp(res, want), "modern dialog: OFN_NODEREFERENCELINKS: '%ls'", res);
    memset(&fd, 0, sizeof(fd));
    ret = open_dialog(L"dirln.lnk", OFN_FILEMUSTEXIST, res, FALSE);
    swprintf(want, MAX_PATH, L"%ls\\sub", testdir);
    CHECK(!ret && fd.closed_by_us && !_wcsicmp(fd.folder, want), "a shortcut to a folder opens the folder: ret %d folder '%ls'", ret, fd.folder);
}

/* ------------------------------------------------------------------ font dialog */

static struct { int count, script_enabled, mode; } fnt;

static UINT_PTR CALLBACK font_hook(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_INITDIALOG) PostMessageW(hdlg, WM_APP, 0, 0);
    if (msg == WM_APP)
    {
        fnt.count = SendDlgItemMessageW(hdlg, cmb1, CB_GETCOUNT, 0, 0);
        fnt.script_enabled = IsWindowEnabled(GetDlgItem(hdlg, cmb5));
        EndDialog(hdlg, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}

static int font_count(BYTE charset, DWORD flags, int *script_enabled)
{
    CHOOSEFONTW cf = { sizeof(cf) };
    LOGFONTW lf = { 0 };

    lf.lfCharSet = charset;
    lf.lfHeight = -12;
    cf.hwndOwner = owner;
    cf.lpLogFont = &lf;
    cf.Flags = CF_SCREENFONTS | CF_ENABLEHOOK | CF_INITTOLOGFONTSTRUCT | flags;
    cf.lpfnHook = font_hook;
    memset(&fnt, 0, sizeof(fnt));
    fnt.count = -1;
    ChooseFontW(&cf);
    if (script_enabled) *script_enabled = fnt.script_enabled;
    return fnt.count;
}

static void test_font(void)
{
    int all, ansi, thai, en_all, en_ansi, en_plain;

    all = font_count(DEFAULT_CHARSET, CF_SELECTSCRIPT, &en_all);
    ansi = font_count(ANSI_CHARSET, CF_SELECTSCRIPT, &en_ansi);
    thai = font_count(THAI_CHARSET, CF_SELECTSCRIPT, NULL);
    font_count(ANSI_CHARSET, 0, &en_plain);
    CHECK(all > 0, "fonts listed: %d", all);
    CHECK(ansi > 0 && ansi <= all, "CF_SELECTSCRIPT ANSI: %d of %d", ansi, all);
    CHECK(thai < all, "CF_SELECTSCRIPT THAI lists fewer fonts: %d of %d", thai, all);
    CHECK(!en_ansi && !en_all, "the script box is disabled with CF_SELECTSCRIPT");
    CHECK(en_plain, "and enabled without it");
}

/* ------------------------------------------------------------------ page setup */

static struct { int msgs[16]; int paper, orient; } ps;

static UINT_PTR CALLBACK paint_hook(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg >= WM_PSD_PAGESETUPDLG && msg <= WM_PSD_YAFULLPAGERECT)
    {
        ps.msgs[msg - WM_PSD_PAGESETUPDLG]++;
        if (msg == WM_PSD_PAGESETUPDLG) { ps.paper = LOWORD(wp); ps.orient = HIWORD(wp); }
    }
    return FALSE;
}

static UINT_PTR CALLBACK setup_hook(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_INITDIALOG) SetTimer(hdlg, 77, 1200, NULL);
    if (msg == WM_TIMER && wp == 77) { KillTimer(hdlg, 77); EndDialog(hdlg, IDCANCEL); return TRUE; }
    return FALSE;
}

static void run_setup(short paper, short orient)
{
    PAGESETUPDLGW psd = { sizeof(psd) };
    HGLOBAL hdm;
    DEVMODEW *dm;
    LONG size = DocumentPropertiesW(NULL, NULL, L"SG Cdlg A", NULL, NULL, 0);

    hdm = GlobalAlloc(GMEM_MOVEABLE, size);
    dm = GlobalLock(hdm);
    DocumentPropertiesW(NULL, NULL, L"SG Cdlg A", dm, NULL, DM_OUT_BUFFER);
    dm->dmFields |= DM_PAPERSIZE | DM_ORIENTATION;
    dm->dmPaperSize = paper;
    dm->dmOrientation = orient;
    GlobalUnlock(hdm);
    memset(&ps, 0, sizeof(ps));
    psd.hwndOwner = owner;
    psd.hDevMode = hdm;
    psd.hDevNames = make_devnames(L"SG Cdlg A");
    psd.Flags = PSD_ENABLEPAGEPAINTHOOK | PSD_ENABLEPAGESETUPHOOK;
    psd.lpfnPagePaintHook = paint_hook;
    psd.lpfnPageSetupHook = setup_hook;
    PageSetupDlgW(&psd);
    if (psd.hDevMode) GlobalFree(psd.hDevMode);
    if (psd.hDevNames) GlobalFree(psd.hDevNames);
}

static void test_pagesetup(void)
{
    run_setup(DMPAPER_LETTER, DMORIENT_PORTRAIT);
    CHECK(ps.msgs[0] > 0 && ps.paper == DMPAPER_LETTER && ps.orient == 7, "paper: WM_PSD_PAGESETUPDLG %d paper %d orient %#x", ps.msgs[0], ps.paper, ps.orient);
    CHECK(ps.msgs[5] == 0 && ps.msgs[6] == 0, "no envelope messages for paper: %d %d", ps.msgs[5], ps.msgs[6]);
    run_setup(DMPAPER_ENV_10, DMORIENT_LANDSCAPE);
    CHECK(ps.msgs[0] > 0 && ps.paper == DMPAPER_ENV_10 && ps.orient == 0x0b, "envelope: paper %d orient %#x", ps.paper, ps.orient);
    CHECK(ps.msgs[5] > 0 && ps.msgs[6] > 0, "envelope stamp and return address messages: %d %d", ps.msgs[5], ps.msgs[6]);
}

int main(void)
{
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES | ICC_USEREX_CLASSES };

    setvbuf(stdout, NULL, _IONBF, 0);
    InitCommonControlsEx(&icc);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    owner = CreateWindowW(L"STATIC", L"owner", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 10, 10, 300, 200, NULL, NULL, NULL, NULL);

    if (!make_printers())
    {
        printf("FAIL  no printers to test with\nRESULT: FAIL\n");
        return 1;
    }
    printf("== test_ranges\n");
    test_ranges();
    printf("== test_initial_state\n");
    test_initial_state();
    printf("== test_pick_and_callback\n");
    test_pick_and_callback();
    printf("== test_apply\n");
    test_apply();
    printf("== test_callback_messages\n");
    test_callback_messages();
    printf("== test_radios\n");
    test_radios();
    printf("== test_ansi\n");
    test_ansi();
    printf("== test_custom_pages\n");
    test_custom_pages();
    GetTempPathW(MAX_PATH, testdir);
    wcscat(testdir, L"sgcd");
    CreateDirectoryW(testdir, NULL);
    printf("== test_font\n");
    test_font();
    printf("== test_pagesetup\n");
    test_pagesetup();
    printf("== test_filedialog\n");
    test_filedialog();
    printf("DONE  checks %d, failures %d\n", checks, failures);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
