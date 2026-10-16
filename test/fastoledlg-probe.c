/* fastoledlg-probe: the OLE UI dialogs of oledlg.dll (Convert, Change Icon, Busy, prompts, Links, Object Properties) */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <oledlg.h>
#include <commctrl.h>
#include <prsht.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the ids the headers of mingw have too */
#ifndef IDC_CV_OBJECTTYPE
#error need the OLE UI ids
#endif

static int checks, fails;

#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const CLSID clsid_src  = {0x7a0c1001, 0x1111, 0x4222, {0x83, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66}};
static const CLSID clsid_conv = {0x7a0c1002, 0x1111, 0x4222, {0x83, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66}};
static const CLSID clsid_rw   = {0x7a0c1003, 0x1111, 0x4222, {0x83, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66}};
static const CLSID clsid_none = {0x7a0c1004, 0x1111, 0x4222, {0x83, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66}};

static UINT msg_enddialog, msg_closebusy, msg_help;

static void set_value(HKEY root, const char *path, const char *name, const char *value)
{
    HKEY key;
    if (RegCreateKeyExA(root, path, 0, NULL, 0, KEY_WRITE, NULL, &key, NULL)) return;
    RegSetValueExA(key, name, 0, REG_SZ, (const BYTE *)value, strlen(value) + 1);
    RegCloseKey(key);
}

static void make_class(const CLSID *id, const char *name, const char *defaultfile, const char *readable, const char *rw)
{
    char guid[64], path[256];
    WCHAR wguid[64];

    StringFromGUID2(id, wguid, 64);
    WideCharToMultiByte(CP_ACP, 0, wguid, -1, guid, sizeof(guid), NULL, NULL);
    sprintf(path, "CLSID\\%s", guid);
    set_value(HKEY_CLASSES_ROOT, path, NULL, name);
    if (defaultfile)
    {
        sprintf(path, "CLSID\\%s\\DataFormats\\DefaultFile", guid);
        set_value(HKEY_CLASSES_ROOT, path, NULL, defaultfile);
    }
    if (readable)
    {
        sprintf(path, "CLSID\\%s\\Conversion\\Readable\\Main", guid);
        set_value(HKEY_CLASSES_ROOT, path, NULL, readable);
    }
    if (rw)
    {
        sprintf(path, "CLSID\\%s\\Conversion\\ReadWritable\\Main", guid);
        set_value(HKEY_CLASSES_ROOT, path, NULL, rw);
    }
}

static BOOL is_err(UINT ret, UINT code) { return ret == code; }

static void test_validation(void)
{
    OLEUICONVERTW cv;
    OLEUICHANGEICONW ci;
    OLEUIEDITLINKSW el;
    OLEUIBUSYW busy;
    OLEUICHANGESOURCEW cs;
    OLEUIOBJECTPROPSW op;
    UINT ret;

    ret = OleUIConvertW(NULL);          CHECK(ret == OLEUI_ERR_STRUCTURENULL, "convert null %u", ret);
    ret = OleUIChangeIconW(NULL);       CHECK(ret == OLEUI_ERR_STRUCTURENULL, "changeicon null %u", ret);
    ret = OleUIEditLinksW(NULL);        CHECK(ret == OLEUI_ERR_STRUCTURENULL, "editlinks null %u", ret);
    ret = OleUIBusyW(NULL);             CHECK(ret == OLEUI_ERR_STRUCTURENULL, "busy null %u", ret);
    ret = OleUIChangeSourceW(NULL);     CHECK(ret == OLEUI_ERR_STRUCTURENULL, "changesource null %u", ret);
    ret = OleUIObjectPropertiesW(NULL); CHECK(ret == OLEUI_ERR_STRUCTURENULL, "objprops null %u", ret);
    ret = OleUIInsertObjectW(NULL);     CHECK(ret == OLEUI_ERR_STRUCTURENULL, "insert null %u", ret);
    ret = OleUIConvertA(NULL);          CHECK(ret == OLEUI_ERR_STRUCTURENULL, "convertA null %u", ret);
    ret = OleUIChangeIconA(NULL);       CHECK(ret == OLEUI_ERR_STRUCTURENULL, "changeiconA null %u", ret);
    ret = OleUIEditLinksA(NULL);        CHECK(ret == OLEUI_ERR_STRUCTURENULL, "editlinksA null %u", ret);
    ret = OleUIBusyA(NULL);             CHECK(ret == OLEUI_ERR_STRUCTURENULL, "busyA null %u", ret);
    ret = OleUIChangeSourceA(NULL);     CHECK(ret == OLEUI_ERR_STRUCTURENULL, "changesourceA null %u", ret);
    ret = OleUIObjectPropertiesA(NULL); CHECK(ret == OLEUI_ERR_STRUCTURENULL, "objpropsA null %u", ret);

    memset(&cv, 0, sizeof(cv));
    ret = OleUIConvertW(&cv);           CHECK(ret == OLEUI_ERR_CBSTRUCTINCORRECT, "convert cb %u", ret);
    cv.cbStruct = sizeof(cv);
    cv.hWndOwner = (HWND)0x12345670;
    ret = OleUIConvertW(&cv);           CHECK(ret == OLEUI_ERR_HWNDOWNERINVALID, "convert owner %u", ret);
    cv.hWndOwner = NULL;
    cv.lpfnHook = (LPFNOLEUIHOOK)0x10;
    ret = OleUIConvertW(&cv);           CHECK(ret == OLEUI_ERR_LPFNHOOKINVALID, "convert hook %u", ret);
    cv.lpfnHook = NULL;
    ret = OleUIConvertW(&cv);           CHECK(ret == OLEUI_CTERR_CLASSIDINVALID, "convert clsid %u", ret);
    cv.clsid = clsid_src;
    cv.dvAspect = 77;
    ret = OleUIConvertW(&cv);           CHECK(ret == OLEUI_CTERR_DVASPECTINVALID, "convert aspect %u", ret);
    cv.dvAspect = DVASPECT_CONTENT;
    cv.hMetaPict = (HGLOBAL)0x10;
    ret = OleUIConvertW(&cv);           CHECK(ret == OLEUI_CTERR_HMETAPICTINVALID, "convert metapict %u", ret);

    memset(&ci, 0, sizeof(ci));
    ci.cbStruct = sizeof(ci);
    ret = OleUIChangeIconW(&ci);        CHECK(ret == OLEUI_CIERR_MUSTHAVECURRENTMETAFILE, "ci meta %u", ret);

    memset(&el, 0, sizeof(el));
    el.cbStruct = sizeof(el);
    ret = OleUIEditLinksW(&el);         CHECK(ret == OLEUI_ELERR_LINKCNTRNULL, "el cntr %u", ret);
    el.lpOleUILinkContainer = (LPOLEUILINKCONTAINERW)0x10;
    ret = OleUIEditLinksW(&el);         CHECK(ret == OLEUI_ELERR_LINKCNTRINVALID, "el cntr invalid %u", ret);

    memset(&busy, 0, sizeof(busy));
    busy.cbStruct = sizeof(busy);
    ret = OleUIBusyW(&busy);            CHECK(ret == OLEUI_BZERR_HTASKINVALID, "busy task %u", ret);
    busy.hTask = (HTASK)0x7ffff0;
    ret = OleUIBusyW(&busy);            CHECK(ret == OLEUI_BZERR_HTASKINVALID, "busy bad task %u", ret);

    memset(&cs, 0, sizeof(cs));
    cs.cbStruct = sizeof(cs);
    ret = OleUIChangeSourceW(&cs);      CHECK(ret == OLEUI_CSERR_LINKCNTRNULL, "cs null cntr %u", ret);

    memset(&op, 0, sizeof(op));
    op.cbStruct = sizeof(op);
    ret = OleUIObjectPropertiesW(&op);  CHECK(ret == OLEUI_OPERR_PROPSHEETNULL, "op ps null %u", ret);
    (void)is_err;
}

static void test_can_convert(void)
{
    BOOL ret;
    WORD cf = (WORD)RegisterClipboardFormatA("FmtSrcX");

    make_class(&clsid_src, "Source Type", "FmtSrc", NULL, NULL);
    make_class(&clsid_conv, "Readable Type", NULL, "FmtOther, FmtSrc", NULL);
    make_class(&clsid_rw, "Writable Type", NULL, "FmtZzz", "FmtSrc");
    make_class(&clsid_none, "Other Type", NULL, "FmtNope", NULL);

    ret = OleUICanConvertOrActivateAs(&clsid_src, FALSE, 0);
    CHECK(ret == TRUE, "convertible %d", ret);
    ret = OleUICanConvertOrActivateAs(&clsid_src, TRUE, 0);
    CHECK(ret == TRUE, "linked convertible %d", ret);
    /* a class no other class reads */
    make_class(&clsid_none, "Other Type", "FmtLonely", "FmtNope", NULL);
    ret = OleUICanConvertOrActivateAs(&clsid_none, FALSE, 0);
    CHECK(ret == FALSE, "lonely %d", ret);
    /* no default file format: the format given decides */
    {
        CLSID unknown = {0x7a0c1fff, 0x1111, 0x4222, {0x83, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66}};
        make_class(&clsid_conv, "Readable Type", NULL, "FmtOther, FmtSrc, FmtSrcX", NULL);
        ret = OleUICanConvertOrActivateAs(&unknown, FALSE, cf);
        CHECK(ret == TRUE, "format given %d", ret);
        ret = OleUICanConvertOrActivateAs(&unknown, FALSE, 0);
        CHECK(ret == FALSE, "no format %d", ret);
    }
    /* a linked object only converts: a class that is only readwritable does not count */
    make_class(&clsid_conv, "Readable Type", NULL, "FmtOther", NULL);
    ret = OleUICanConvertOrActivateAs(&clsid_src, TRUE, 0);
    CHECK(ret == FALSE, "linked, rw only %d", ret);
    ret = OleUICanConvertOrActivateAs(&clsid_src, FALSE, 0);
    CHECK(ret == TRUE, "embedded, rw %d", ret);
    make_class(&clsid_conv, "Readable Type", NULL, "FmtOther, FmtSrc", NULL);
}

/* ------------------------------------------------------------------ Convert */
static int hook_calls;
static BOOL conv_saw_init;

static UINT CALLBACK convert_hook(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_INITDIALOG)
    {
        OLEUICONVERTW *cv = (OLEUICONVERTW *)lp;
        char text[200];
        int n;

        hook_calls++;
        conv_saw_init = TRUE;
        CHECK(cv->lCustData == 0x1234, "custdata %Ix", cv->lCustData);
        GetDlgItemTextA(hdlg, IDC_CV_OBJECTTYPE, text, sizeof(text));
        CHECK(!strcmp(text, "Given Type"), "current type '%s'", text);
        n = SendDlgItemMessageW(hdlg, IDC_CV_CONVERTLIST, LB_GETCOUNT, 0, 0);
        CHECK(n >= 1, "convert list %d", n);
        SendDlgItemMessageA(hdlg, IDC_CV_CONVERTLIST, LB_GETTEXT, 0, (LPARAM)text);
        CHECK(!strcmp(text, "Readable Type"), "convert item '%s'", text);
        n = SendDlgItemMessageW(hdlg, IDC_CV_ACTIVATELIST, LB_GETCOUNT, 0, 0);
        CHECK(n >= 1, "activate list %d", n);
        SendDlgItemMessageA(hdlg, IDC_CV_ACTIVATELIST, LB_GETTEXT, 0, (LPARAM)text);
        CHECK(!strcmp(text, "Writable Type"), "activate item '%s'", text);
        CHECK(IsDlgButtonChecked(hdlg, IDC_CV_CONVERTTO) == BST_CHECKED, "convert-to default");
        CHECK(IsWindowEnabled(GetDlgItem(hdlg, IDC_CV_ACTIVATEAS)), "activate-as enabled");
        GetDlgItemTextA(hdlg, IDC_CV_RESULTTEXT, text, sizeof(text));
        CHECK(strstr(text, "Given Type") && strstr(text, "Readable Type"), "result text '%s'", text);
        /* switch to activate as */
        SendMessageW(hdlg, WM_COMMAND, MAKEWPARAM(IDC_CV_ACTIVATEAS, BN_CLICKED), 0);
        CHECK(IsDlgButtonChecked(hdlg, IDC_CV_ACTIVATEAS) == BST_CHECKED, "activate-as checked");
        CHECK(GetWindowLongW(GetDlgItem(hdlg, IDC_CV_ACTIVATELIST), GWL_STYLE) & WS_VISIBLE, "activate list shown");
        GetDlgItemTextA(hdlg, IDC_CV_RESULTTEXT, text, sizeof(text));
        CHECK(strstr(text, "Writable Type"), "activate result '%s'", text);
        PostMessageW(hdlg, WM_COMMAND, IDOK, 0);
        return 1;
    }
    return 0;
}

static UINT CALLBACK convert_only_hook(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_INITDIALOG)
    {
        CHECK(!GetDlgItem(hdlg, IDC_CV_ACTIVATEAS), "no activate-as in the convert only dialog");
        CHECK(SendDlgItemMessageW(hdlg, IDC_CV_CONVERTLIST, LB_GETCOUNT, 0, 0) >= 1, "convert only list");
        PostMessageW(hdlg, WM_COMMAND, IDCANCEL, 0);
        return 1;
    }
    return 0;
}

static UINT CALLBACK ci_hook(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp);
static BOOL ci_label_set;

/* closes a dialog opened inside a hook that waits for it */
struct closer { const char *title; int cmd; DWORD thread; int done; };

static BOOL CALLBACK find_titled(HWND hwnd, LPARAM lp)
{
    struct closer *c = (struct closer *)lp;
    char title[100], cls[40];

    GetClassNameA(hwnd, cls, sizeof(cls));
    GetWindowTextA(hwnd, title, sizeof(title));
    if (!strcmp(cls, "#32770") && !strcmp(title, c->title) && IsWindowVisible(hwnd))
    {
        PostMessageW(hwnd, WM_COMMAND, c->cmd, 0);
        c->done = 1;
        return FALSE;
    }
    return TRUE;
}

static DWORD WINAPI closer_thread(void *arg)
{
    struct closer *c = arg;
    int i;

    for (i = 0; i < 200 && !c->done; i++)
    {
        Sleep(50);
        EnumThreadWindows(c->thread, find_titled, (LPARAM)c);
    }
    return 0;
}

static HANDLE start_closer(struct closer *c, const char *title, int cmd)
{
    c->title = title;
    c->cmd = cmd;
    c->thread = GetCurrentThreadId();
    c->done = 0;
    return CreateThread(NULL, 0, closer_thread, c, 0, NULL);
}

static struct closer icon_closer;

static UINT CALLBACK convert_icon_hook(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_INITDIALOG)
    {
        CHECK(IsDlgButtonChecked(hdlg, IDC_CV_DISPLAYASICON) == BST_CHECKED, "display as icon checked");
        CHECK(IsWindowEnabled(GetDlgItem(hdlg, IDC_CV_CHANGEICON)), "change icon enabled");
        {
            HANDLE th = start_closer(&icon_closer, "Change Icon", IDOK);
            SendMessageW(hdlg, WM_COMMAND, MAKEWPARAM(IDC_CV_CHANGEICON, BN_CLICKED), 0);
            WaitForSingleObject(th, 5000);
            CloseHandle(th);
            CHECK(icon_closer.done, "change icon dialog was shown");
        }
        PostMessageW(hdlg, WM_COMMAND, IDOK, 0);
        return 1;
    }
    return 0;
}

static void test_convert(void)
{
    OLEUICONVERTW cv;
    UINT ret;

    memset(&cv, 0, sizeof(cv));
    cv.cbStruct = sizeof(cv);
    cv.dwFlags = CF_SELECTCONVERTTO;
    cv.lpfnHook = convert_hook;
    cv.lCustData = 0x1234;
    cv.clsid = clsid_src;
    cv.dvAspect = DVASPECT_CONTENT;
    cv.lpszUserType = (WCHAR *)CoTaskMemAlloc(sizeof(L"Given Type"));
    memcpy(cv.lpszUserType, L"Given Type", sizeof(L"Given Type"));
    ret = OleUIConvertW(&cv);
    CHECK(ret == OLEUI_OK, "convert ret %u", ret);
    CHECK(conv_saw_init, "convert hook init");
    CHECK(IsEqualGUID(&cv.clsidNew, &clsid_rw), "clsidNew %s", "not the activate-as class");
    CHECK((cv.dwFlags & CF_SELECTACTIVATEAS) && !(cv.dwFlags & CF_SELECTCONVERTTO), "flags %#lx", cv.dwFlags);
    CHECK(cv.dvAspect == DVASPECT_CONTENT, "aspect %lu", cv.dvAspect);
    CHECK(cv.lpszUserType == NULL, "user type freed");

    /* convert only dialog */
    memset(&cv, 0, sizeof(cv));
    cv.cbStruct = sizeof(cv);
    cv.dwFlags = CF_CONVERTONLY;
    cv.lpfnHook = convert_only_hook;
    cv.clsid = clsid_src;
    cv.dvAspect = DVASPECT_CONTENT;
    ret = OleUIConvertW(&cv);
    CHECK(ret == OLEUI_CANCEL, "convert only ret %u", ret);

    /* the ANSI function, with the strings it frees */
    {
        OLEUICONVERTA cva;
        char *ut = CoTaskMemAlloc(11);

        strcpy(ut, "Given Type");
        memset(&cva, 0, sizeof(cva));
        cva.cbStruct = sizeof(cva);
        cva.dwFlags = CF_SELECTCONVERTTO;
        cva.lpfnHook = convert_hook;
        cva.lCustData = 0x1234;
        cva.clsid = clsid_src;
        cva.dvAspect = DVASPECT_CONTENT;
        cva.lpszUserType = ut;
        ret = OleUIConvertA(&cva);
        CHECK(ret == OLEUI_OK, "convertA ret %u", ret);
        CHECK(IsEqualGUID(&cva.clsidNew, &clsid_rw), "convertA clsidNew");
        CHECK(cva.lpszUserType == NULL, "convertA user type freed");
    }
}

static HGLOBAL meta_for(const CLSID *id, const WCHAR *label)
{
    return OleGetIconOfClass(id, (LPOLESTR)label, FALSE);
}

static UINT CALLBACK ci_hook(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_INITDIALOG)
    {
        OLEUICHANGEICONW *ci = (OLEUICHANGEICONW *)lp;
        char text[100];

        hook_calls++;
        CHECK(ci->lCustData == 0x55, "ci custdata");
        GetDlgItemTextA(hdlg, IDC_CI_LABELEDIT, text, sizeof(text));
        CHECK(!strcmp(text, "Orig Label"), "label '%s'", text);
        CHECK(IsDlgButtonChecked(hdlg, IDC_CI_CURRENT) == BST_CHECKED, "current selected");
        SetDlgItemTextA(hdlg, IDC_CI_LABELEDIT, "New Label");
        SendMessageW(hdlg, WM_COMMAND, MAKEWPARAM(IDC_CI_DEFAULT, BN_CLICKED), 0);
        CHECK(IsDlgButtonChecked(hdlg, IDC_CI_DEFAULT) == BST_CHECKED, "default selected");
        PostMessageW(hdlg, WM_COMMAND, IDOK, 0);
        return 1;
    }
    return 0;
}

static UINT CALLBACK ci_cancel_hook(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_INITDIALOG)
    {
        PostMessageW(hdlg, msg_enddialog, OLEUI_CANCEL, 0);
        return 1;
    }
    return 0;
}

static void test_changeicon(void)
{
    OLEUICHANGEICONW ci;
    HGLOBAL orig;
    UINT ret;

    orig = meta_for(&clsid_src, L"Orig Label");
    CHECK(orig != NULL, "icon metafile");
    memset(&ci, 0, sizeof(ci));
    ci.cbStruct = sizeof(ci);
    ci.dwFlags = CIF_SELECTCURRENT;
    ci.lpfnHook = ci_hook;
    ci.lCustData = 0x55;
    ci.clsid = clsid_src;
    ci.hMetaPict = orig;
    ret = OleUIChangeIconW(&ci);
    CHECK(ret == OLEUI_OK, "changeicon ret %u", ret);
    CHECK(ci.hMetaPict != NULL, "result metafile");
    CHECK((ci.dwFlags & CIF_SELECTDEFAULT) && !(ci.dwFlags & CIF_SELECTCURRENT), "flags %#lx", ci.dwFlags);
    if (ci.hMetaPict)
    {
        METAFILEPICT *mfp = GlobalLock(ci.hMetaPict);
        CHECK(mfp && mfp->hMF, "metafile");
        if (mfp && mfp->hMF)
        {
            UINT size = GetMetaFileBitsEx(mfp->hMF, 0, NULL);
            BYTE *bits = malloc(size ? size : 1);
            BOOL found = FALSE;
            UINT i;
            if (size && GetMetaFileBitsEx(mfp->hMF, size, bits))
                for (i = 0; i + 9 <= size; i++) if (!memcmp(bits + i, "New Label", 9)) found = TRUE;
            CHECK(found, "the label of the dialog is in the new metafile");
            free(bits);
        }
        if (mfp) GlobalUnlock(ci.hMetaPict);
    }

    /* closed with the end dialog message: nothing changes */
    memset(&ci, 0, sizeof(ci));
    ci.cbStruct = sizeof(ci);
    ci.dwFlags = CIF_SELECTCURRENT;
    ci.lpfnHook = ci_cancel_hook;
    ci.clsid = clsid_src;
    orig = meta_for(&clsid_src, L"Orig Label");
    ci.hMetaPict = orig;
    ret = OleUIChangeIconW(&ci);
    CHECK(ret == OLEUI_CANCEL, "changeicon cancel ret %u", ret);
    CHECK(ci.hMetaPict == orig, "metafile kept on cancel");

    /* a bad icon exe */
    ci.dwFlags = CIF_USEICONEXE;
    ci.cchIconExe = 5000;
    ret = OleUIChangeIconW(&ci);
    CHECK(ret == OLEUI_CIERR_SZICONEXEINVALID, "iconexe %u", ret);

    /* the convert dialog hands the icon to the change icon dialog */
    {
        OLEUICONVERTW cv;
        memset(&cv, 0, sizeof(cv));
        cv.cbStruct = sizeof(cv);
        cv.dwFlags = CF_SELECTCONVERTTO;
        cv.lpfnHook = convert_icon_hook;
        cv.clsid = clsid_src;
        cv.dvAspect = DVASPECT_ICON;
        cv.hMetaPict = meta_for(&clsid_src, L"Orig Label");
        ret = OleUIConvertW(&cv);
        CHECK(ret == OLEUI_OK, "convert with icon ret %u", ret);
        CHECK(cv.dvAspect == DVASPECT_ICON, "aspect icon kept");
        CHECK(cv.hMetaPict != NULL, "metapict kept");
    }
}

/* --------------------------------------------------------------------- Busy */
static HWND busy_wnd;

static UINT CALLBACK busy_hook(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_INITDIALOG)
    {
        OLEUIBUSYW *b = (OLEUIBUSYW *)lp;
        char text[300];

        CHECK(!b->lphWndDialog || *b->lphWndDialog == hdlg, "dialog window stored");
        CHECK(!IsWindowEnabled(GetDlgItem(hdlg, IDCANCEL)) == !!(b->dwFlags & (BZ_DISABLECANCELBUTTON | BZ_NOTRESPONDINGDIALOG)),
              "cancel state flags %#lx", b->dwFlags);
        CHECK(!IsWindowEnabled(GetDlgItem(hdlg, IDC_BZ_RETRY)) == !!(b->dwFlags & BZ_DISABLERETRYBUTTON), "retry state");
        CHECK(!IsWindowEnabled(GetDlgItem(hdlg, IDC_BZ_SWITCHTO)) == !!(b->dwFlags & BZ_DISABLESWITCHTOBUTTON), "switchto state");
        GetDlgItemTextA(hdlg, IDC_BZ_MESSAGE1, text, sizeof(text));
        if (b->dwFlags & BZ_NOTRESPONDINGDIALOG) CHECK(strstr(text, "not responding"), "not responding text '%s'", text);
        else CHECK(strstr(text, "busy"), "busy text '%s'", text);
        GetWindowTextA(hdlg, text, sizeof(text));
        CHECK(!strcmp(text, b->lpszCaption ? "My Busy" : (b->dwFlags & BZ_NOTRESPONDINGDIALOG ? "Not Responding" : "Busy")), "caption '%s'", text);
        busy_wnd = hdlg;
        switch (b->lCustData)
        {
        case 1: PostMessageW(hdlg, WM_COMMAND, IDC_BZ_RETRY, 0); break;
        case 2: PostMessageW(hdlg, msg_closebusy, 0, 0); break;
        case 3: PostMessageW(hdlg, WM_COMMAND, IDCANCEL, 0); break;
        case 4: PostMessageW(hdlg, WM_COMMAND, IDC_BZ_SWITCHTO, 0); break;
        }
        return 1;
    }
    return 0;
}

static void test_busy(void)
{
    OLEUIBUSYW busy;
    HWND dlg = NULL;
    UINT ret;

    memset(&busy, 0, sizeof(busy));
    busy.cbStruct = sizeof(busy);
    busy.hTask = (HTASK)(DWORD_PTR)GetCurrentThreadId();
    busy.lpfnHook = busy_hook;
    busy.lphWndDialog = &dlg;
    busy.lCustData = 1;
    ret = OleUIBusyW(&busy);
    CHECK(ret == OLEUI_BZ_RETRYSELECTED, "busy retry %u", ret);
    CHECK(dlg == NULL, "dialog window reset");

    busy.lCustData = 2;
    busy.lpszCaption = L"My Busy";
    ret = OleUIBusyW(&busy);
    CHECK(ret == OLEUI_BZ_CALLUNBLOCKED, "busy unblocked %u", ret);

    busy.lCustData = 3;
    busy.lpszCaption = NULL;
    ret = OleUIBusyW(&busy);
    CHECK(ret == OLEUI_CANCEL, "busy cancel %u", ret);

    /* not responding: no program to switch to */
    busy.lCustData = 4;
    busy.dwFlags = BZ_NOTRESPONDINGDIALOG;
    busy.hTask = NULL;
    ret = OleUIBusyW(&busy);
    CHECK(ret == OLEUI_BZ_SWITCHTOSELECTED, "busy switch %u", ret);

    busy.lCustData = 1;
    busy.dwFlags = BZ_DISABLECANCELBUTTON | BZ_DISABLESWITCHTOBUTTON;
    busy.hTask = (HTASK)(DWORD_PTR)GetCurrentThreadId();
    ret = OleUIBusyW(&busy);
    CHECK(ret == OLEUI_BZ_RETRYSELECTED, "busy disabled buttons %u", ret);

    busy.lCustData = 3;
    busy.dwFlags = BZ_DISABLERETRYBUTTON;
    ret = OleUIBusyW(&busy);
    CHECK(ret == OLEUI_CANCEL, "busy retry disabled %u", ret);

    {
        OLEUIBUSYA bza;
        memset(&bza, 0, sizeof(bza));
        bza.cbStruct = sizeof(bza);
        bza.hTask = (HTASK)(DWORD_PTR)GetCurrentThreadId();
        bza.lpfnHook = busy_hook;
        bza.lCustData = 1;
        bza.lpszCaption = "My Busy";
        ret = OleUIBusyA(&bza);
        CHECK(ret == OLEUI_BZ_RETRYSELECTED, "busyA %u", ret);
    }
}

/* ------------------------------------------------------------------ Prompts */
struct prompt_info
{
    int click;
    char text[600];
    char title[100];
    int found;
};

static struct prompt_info prompt;
static DWORD main_thread;

static BOOL CALLBACK find_dialog(HWND hwnd, LPARAM lp)
{
    char cls[40];
    GetClassNameA(hwnd, cls, sizeof(cls));
    if (!strcmp(cls, "#32770") && IsWindowVisible(hwnd)) { *(HWND *)lp = hwnd; return FALSE; }
    return TRUE;
}

static DWORD WINAPI prompt_thread(void *arg)
{
    HWND dlg = NULL;
    int i;

    for (i = 0; i < 200 && !dlg; i++)
    {
        Sleep(50);
        EnumThreadWindows(main_thread, find_dialog, (LPARAM)&dlg);
    }
    if (!dlg) return 1;
    prompt.found = 1;
    SendMessageA(GetDlgItem(dlg, IDC_PU_TEXT), WM_GETTEXT, sizeof(prompt.text), (LPARAM)prompt.text);
    GetWindowTextA(dlg, prompt.title, sizeof(prompt.title));
    PostMessageW(dlg, WM_COMMAND, prompt.click, 0);
    return 0;
}

static int run_prompt(int (*call)(void), int click)
{
    HANDLE th;
    int ret;

    memset(&prompt, 0, sizeof(prompt));
    prompt.click = click;
    th = CreateThread(NULL, 0, prompt_thread, NULL, 0, NULL);
    ret = call();
    WaitForSingleObject(th, 5000);
    CloseHandle(th);
    return ret;
}

static int call_prompt_w(void) { return OleUIPromptUserW(IDD_SERVERNOTREGW, NULL, L"Prompt Title", L"Foo Object"); }
static int call_prompt_a(void) { return OleUIPromptUserA(IDD_SERVERNOTREGA, NULL, "Prompt Title A", "Bar Object"); }
static int call_prompt_src(void) { return OleUIPromptUserW(IDD_LINKSOURCEUNAVAILABLE, NULL, L"T"); }
static int call_prompt_bad(void) { return OleUIPromptUserW(9999, NULL, L"T"); }

static void test_prompt(void)
{
    int ret;

    main_thread = GetCurrentThreadId();
    ret = run_prompt(call_prompt_w, IDC_PU_CONVERT);
    CHECK(prompt.found, "prompt dialog found");
    CHECK(ret == IDC_PU_CONVERT, "prompt ret %d", ret);
    CHECK(strstr(prompt.text, "activate this Foo Object is unavailable"), "prompt text '%s'", prompt.text);
    CHECK(!strcmp(prompt.title, "Prompt Title"), "prompt title '%s'", prompt.title);

    ret = run_prompt(call_prompt_w, IDCANCEL);
    CHECK(ret == IDCANCEL, "prompt cancel %d", ret);

    ret = run_prompt(call_prompt_a, IDC_PU_CONVERT);
    CHECK(ret == IDC_PU_CONVERT, "promptA ret %d", ret);
    CHECK(strstr(prompt.text, "activate this Bar Object is unavailable"), "promptA text '%s'", prompt.text);
    CHECK(!strcmp(prompt.title, "Prompt Title A"), "promptA title '%s'", prompt.title);

    ret = run_prompt(call_prompt_src, IDC_PU_LINKS);
    CHECK(ret == IDC_PU_LINKS, "prompt links %d", ret);
    CHECK(strstr(prompt.text, "source is presently unavailable"), "prompt src text '%s'", prompt.text);

    ret = call_prompt_bad();
    CHECK(ret == OLEUI_ERR_FINDTEMPLATEFAILURE, "bad template %d", ret);
}


/* ------------------------------------------------------- a link container */
struct fake_link
{
    DWORD id;
    WCHAR src[160];
    ULONG file_len;
    const WCHAR *full, *shrt;
    BOOL avail, selected;
    DWORD update;
    int updated, updated_msg, cancelled, opened, setsrc, set_validate;
    BOOL fail_update;
    FILETIME last;
};

static struct fake_link links[3];
static int nlinks;
static HRESULT setsrc_result;
static WCHAR last_set_name[160];
static ULONG last_set_len;
static BOOL last_set_validate;
static int setsrc_calls;

static struct fake_link *find_link(DWORD id)
{
    int i;
    for (i = 0; i < nlinks; i++) if (links[i].id == id) return &links[i];
    return NULL;
}

static void init_links(void)
{
    memset(links, 0, sizeof(links));
    nlinks = 3;
    links[0].id = 1; lstrcpyW(links[0].src, L"C:\\olddir\\a.xls!Sheet1"); links[0].file_len = 15;
    links[0].full = L"Excel Worksheet"; links[0].shrt = L"Worksheet"; links[0].avail = TRUE; links[0].selected = TRUE; links[0].update = 1;
    links[1].id = 2; lstrcpyW(links[1].src, L"C:\\olddir\\b.xls!Chart2"); links[1].file_len = 15;
    links[1].full = L"Excel Chart"; links[1].shrt = L"Chart"; links[1].avail = TRUE; links[1].update = 3;
    links[2].id = 3; lstrcpyW(links[2].src, L"D:\\other\\c.doc"); links[2].file_len = 14;
    links[2].full = L"Word Document"; links[2].shrt = L"Doc"; links[2].avail = FALSE; links[2].update = 3;
    links[2].last.dwHighDateTime = 0x01d00000;
    setsrc_result = S_OK;
    setsrc_calls = 0;
    last_set_name[0] = 0;
}

static HRESULT WINAPI lc_QueryInterface(IOleUILinkInfoW *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IUnknown)) { *obj = iface; return S_OK; }
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI lc_AddRef(IOleUILinkInfoW *iface) { return 2; }
static ULONG WINAPI lc_Release(IOleUILinkInfoW *iface) { return 1; }

static DWORD WINAPI lc_GetNextLink(IOleUILinkInfoW *iface, DWORD link)
{
    int i;
    if (!link) return nlinks ? links[0].id : 0;
    for (i = 0; i < nlinks; i++) if (links[i].id == link) return i + 1 < nlinks ? links[i + 1].id : 0;
    return 0;
}

static HRESULT WINAPI lc_SetLinkUpdateOptions(IOleUILinkInfoW *iface, DWORD link, DWORD opt)
{
    struct fake_link *l = find_link(link);
    if (!l) return E_INVALIDARG;
    l->update = opt;
    return S_OK;
}

static HRESULT WINAPI lc_GetLinkUpdateOptions(IOleUILinkInfoW *iface, DWORD link, DWORD *opt)
{
    struct fake_link *l = find_link(link);
    if (!l) return E_INVALIDARG;
    *opt = l->update;
    return S_OK;
}

static HRESULT WINAPI lc_SetLinkSource(IOleUILinkInfoW *iface, DWORD link, LPWSTR name, ULONG len, ULONG *eaten, BOOL validate)
{
    struct fake_link *l = find_link(link);
    if (!l) return E_INVALIDARG;
    lstrcpyW(last_set_name, name);
    last_set_len = len;
    last_set_validate = validate;
    setsrc_calls++;
    l->setsrc++;
    l->set_validate = validate;
    if (eaten) *eaten = lstrlenW(name);
    if (SUCCEEDED(setsrc_result) || !validate)
    {
        lstrcpyW(l->src, name);
        l->file_len = len;
        l->avail = SUCCEEDED(setsrc_result);
    }
    return validate ? setsrc_result : S_OK;
}

static HRESULT WINAPI lc_GetLinkSource(IOleUILinkInfoW *iface, DWORD link, LPWSTR *name, ULONG *len, LPWSTR *full,
                                       LPWSTR *shrt, BOOL *avail, BOOL *selected)
{
    struct fake_link *l = find_link(link);
    if (!l) return E_INVALIDARG;
    if (name) *name = lstrcpyW(CoTaskMemAlloc((lstrlenW(l->src) + 1) * 2), l->src);
    if (len) *len = l->file_len;
    if (full) *full = lstrcpyW(CoTaskMemAlloc((lstrlenW(l->full) + 1) * 2), l->full);
    if (shrt) *shrt = lstrcpyW(CoTaskMemAlloc((lstrlenW(l->shrt) + 1) * 2), l->shrt);
    if (avail) *avail = l->avail;
    if (selected) *selected = l->selected;
    return S_OK;
}

static HRESULT WINAPI lc_OpenLinkSource(IOleUILinkInfoW *iface, DWORD link)
{
    struct fake_link *l = find_link(link);
    if (!l) return E_INVALIDARG;
    l->opened++;
    return S_OK;
}

static HRESULT WINAPI lc_UpdateLink(IOleUILinkInfoW *iface, DWORD link, BOOL msg, BOOL reserved)
{
    struct fake_link *l = find_link(link);
    if (!l) return E_INVALIDARG;
    l->updated++;
    l->updated_msg = msg;
    return l->fail_update ? E_FAIL : S_OK;
}

static HRESULT WINAPI lc_CancelLink(IOleUILinkInfoW *iface, DWORD link)
{
    struct fake_link *l = find_link(link);
    if (!l) return E_INVALIDARG;
    l->cancelled++;
    return S_OK;
}

static HRESULT WINAPI lc_GetLastUpdate(IOleUILinkInfoW *iface, DWORD link, FILETIME *ft)
{
    struct fake_link *l = find_link(link);
    if (!l) return E_INVALIDARG;
    if (l->last.dwHighDateTime) *ft = l->last;
    return S_OK;
}

static const IOleUILinkInfoWVtbl lc_vtbl =
{
    lc_QueryInterface, lc_AddRef, lc_Release, lc_GetNextLink, lc_SetLinkUpdateOptions, lc_GetLinkUpdateOptions,
    lc_SetLinkSource, lc_GetLinkSource, lc_OpenLinkSource, lc_UpdateLink, lc_CancelLink, lc_GetLastUpdate
};
static IOleUILinkInfoW fake_container = { &lc_vtbl };

/* the same container through the ANSI interface */
static void a_of_w(const WCHAR *w, char *a, int size) { WideCharToMultiByte(CP_ACP, 0, w, -1, a, size, NULL, NULL); }
static char *dup_a(const WCHAR *w)
{
    char *a = CoTaskMemAlloc(lstrlenW(w) * 2 + 2);
    a_of_w(w, a, lstrlenW(w) * 2 + 2);
    return a;
}

static HRESULT WINAPI lca_QueryInterface(IOleUILinkInfoA *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IUnknown)) { *obj = iface; return S_OK; }
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI lca_AddRef(IOleUILinkInfoA *iface) { return 2; }
static ULONG WINAPI lca_Release(IOleUILinkInfoA *iface) { return 1; }
static DWORD WINAPI lca_GetNextLink(IOleUILinkInfoA *iface, DWORD link) { return lc_GetNextLink(NULL, link); }
static HRESULT WINAPI lca_SetLinkUpdateOptions(IOleUILinkInfoA *iface, DWORD link, DWORD opt) { return lc_SetLinkUpdateOptions(NULL, link, opt); }
static HRESULT WINAPI lca_GetLinkUpdateOptions(IOleUILinkInfoA *iface, DWORD link, DWORD *opt) { return lc_GetLinkUpdateOptions(NULL, link, opt); }
static HRESULT WINAPI lca_SetLinkSource(IOleUILinkInfoA *iface, DWORD link, LPSTR name, ULONG len, ULONG *eaten, BOOL validate)
{
    WCHAR w[160];
    ULONG wlen = MultiByteToWideChar(CP_ACP, 0, name, len, NULL, 0);
    MultiByteToWideChar(CP_ACP, 0, name, -1, w, 160);
    return lc_SetLinkSource(NULL, link, w, wlen, eaten, validate);
}
static HRESULT WINAPI lca_GetLinkSource(IOleUILinkInfoA *iface, DWORD link, LPSTR *name, ULONG *len, LPSTR *full,
                                        LPSTR *shrt, BOOL *avail, BOOL *selected)
{
    struct fake_link *l = find_link(link);
    if (!l) return E_INVALIDARG;
    *name = dup_a(l->src); *len = l->file_len;
    *full = dup_a(l->full); *shrt = dup_a(l->shrt);
    *avail = l->avail; *selected = l->selected;
    return S_OK;
}
static HRESULT WINAPI lca_OpenLinkSource(IOleUILinkInfoA *iface, DWORD link) { return lc_OpenLinkSource(NULL, link); }
static HRESULT WINAPI lca_UpdateLink(IOleUILinkInfoA *iface, DWORD link, BOOL msg, BOOL r) { return lc_UpdateLink(NULL, link, msg, r); }
static HRESULT WINAPI lca_CancelLink(IOleUILinkInfoA *iface, DWORD link) { return lc_CancelLink(NULL, link); }
static HRESULT WINAPI lca_GetLastUpdate(IOleUILinkInfoA *iface, DWORD link, FILETIME *ft) { return lc_GetLastUpdate(NULL, link, ft); }

static const IOleUILinkInfoAVtbl lca_vtbl =
{
    lca_QueryInterface, lca_AddRef, lca_Release, lca_GetNextLink, lca_SetLinkUpdateOptions, lca_GetLinkUpdateOptions,
    lca_SetLinkSource, lca_GetLinkSource, lca_OpenLinkSource, lca_UpdateLink, lca_CancelLink, lca_GetLastUpdate
};
static IOleUILinkInfoA fake_container_a = { &lca_vtbl };

static void test_update_links(void)
{
    BOOL ret;

    init_links();
    links[2].update = 1;
    ret = OleUIUpdateLinksW((LPOLEUILINKCONTAINERW)&fake_container, NULL, (WCHAR *)L"Updating", 3);
    CHECK(ret == TRUE, "update links %d", ret);
    CHECK(links[0].updated == 1 && links[1].updated == 0 && links[2].updated == 1,
          "updated %d %d %d", links[0].updated, links[1].updated, links[2].updated);
    CHECK(!links[0].updated_msg, "no error message while updating all");

    links[0].updated = links[2].updated = 0;
    links[2].fail_update = TRUE;
    ret = OleUIUpdateLinksW((LPOLEUILINKCONTAINERW)&fake_container, NULL, (WCHAR *)L"Updating", 3);
    CHECK(ret == FALSE, "failed update %d", ret);
    CHECK(links[2].updated == 1, "the failing link was tried");

    init_links();
    ret = OleUIUpdateLinksA((LPOLEUILINKCONTAINERA)&fake_container_a, NULL, (char *)"Updating", 3);
    CHECK(ret == TRUE, "update links A %d", ret);
    CHECK(links[0].updated == 1 && links[1].updated == 0, "updated A %d %d", links[0].updated, links[1].updated);

    ret = OleUIUpdateLinksW(NULL, NULL, NULL, 0);
    CHECK(ret == FALSE, "null container");
}

/* -------------------------------------------------------------- Edit links */
static int el_mode;
static HANDLE start_cs_driver(int mode, const WCHAR *file, const WCHAR *item);
static struct closer cs_closer;
static WCHAR cs_new_file[MAX_PATH], cs_new_item[100];

static int el_find_row(HWND hdlg, const char *prefix)
{
    int i, n = SendDlgItemMessageW(hdlg, IDC_EL_LINKSLISTBOX, LB_GETCOUNT, 0, 0);
    char text[300];

    for (i = 0; i < n; i++)
    {
        SendDlgItemMessageA(hdlg, IDC_EL_LINKSLISTBOX, LB_GETTEXT, i, (LPARAM)text);
        if (!strncmp(text, prefix, strlen(prefix))) return i;
    }
    return -1;
}

static void el_select_only(HWND hdlg, int row)
{
    SendDlgItemMessageW(hdlg, IDC_EL_LINKSLISTBOX, LB_SETSEL, FALSE, -1);
    if (row >= 0) SendDlgItemMessageW(hdlg, IDC_EL_LINKSLISTBOX, LB_SETSEL, TRUE, row);
    SendMessageW(hdlg, WM_COMMAND, MAKEWPARAM(IDC_EL_LINKSLISTBOX, LBN_SELCHANGE), 0);
}

static UINT CALLBACK el_hook(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{
    int a, b, c;
    char text[300];

    if (msg != WM_INITDIALOG) return 0;

    CHECK(((OLEUIEDITLINKSW *)lp)->lCustData == 0x77, "el custdata");
    CHECK(SendDlgItemMessageW(hdlg, IDC_EL_LINKSLISTBOX, LB_GETCOUNT, 0, 0) == 3, "three links");
    a = el_find_row(hdlg, "C:\\olddir\\a.xls");
    b = el_find_row(hdlg, "C:\\olddir\\b.xls");
    c = el_find_row(hdlg, "D:\\other\\c.doc");
    CHECK(a >= 0 && b >= 0 && c >= 0, "rows %d %d %d", a, b, c);
    if (a < 0 || b < 0 || c < 0) { PostMessageW(hdlg, WM_COMMAND, IDCANCEL, 0); return 1; }

    SendDlgItemMessageA(hdlg, IDC_EL_LINKSLISTBOX, LB_GETTEXT, a, (LPARAM)text);
    CHECK(strstr(text, "\tWorksheet\tAutomatic"), "row a '%s'", text);
    SendDlgItemMessageA(hdlg, IDC_EL_LINKSLISTBOX, LB_GETTEXT, b, (LPARAM)text);
    CHECK(strstr(text, "\tChart\tManual"), "row b '%s'", text);
    SendDlgItemMessageA(hdlg, IDC_EL_LINKSLISTBOX, LB_GETTEXT, c, (LPARAM)text);
    CHECK(strstr(text, "\tDoc\tUnavailable"), "row c '%s'", text);

    if (el_mode == 0)
    {
        CHECK(SendDlgItemMessageW(hdlg, IDC_EL_LINKSLISTBOX, LB_GETSEL, a, 0) > 0, "a selected by the container");
        CHECK(SendDlgItemMessageW(hdlg, IDC_EL_LINKSLISTBOX, LB_GETSEL, b, 0) == 0, "b not selected");
        GetDlgItemTextA(hdlg, IDC_EL_LINKSOURCE, text, sizeof(text));
        CHECK(!strcmp(text, "C:\\olddir\\a.xls!Sheet1"), "source '%s'", text);
        GetDlgItemTextA(hdlg, IDC_EL_LINKTYPE, text, sizeof(text));
        CHECK(!strcmp(text, "Excel Worksheet"), "type '%s'", text);
        CHECK(IsDlgButtonChecked(hdlg, IDC_EL_AUTOMATIC) == BST_CHECKED, "automatic checked");
        CHECK(IsWindowEnabled(GetDlgItem(hdlg, IDC_EL_UPDATENOW)) && IsWindowEnabled(GetDlgItem(hdlg, IDC_EL_OPENSOURCE)) &&
              IsWindowEnabled(GetDlgItem(hdlg, IDC_EL_CHANGESOURCE)) && IsWindowEnabled(GetDlgItem(hdlg, IDC_EL_CANCELLINK)),
              "buttons for one available link");

        SendMessageW(hdlg, WM_COMMAND, MAKEWPARAM(IDC_EL_MANUAL, BN_CLICKED), 0);
        CHECK(links[0].update == 3, "manual set %lu", links[0].update);
        a = el_find_row(hdlg, "C:\\olddir\\a.xls");
        SendDlgItemMessageA(hdlg, IDC_EL_LINKSLISTBOX, LB_GETTEXT, a, (LPARAM)text);
        CHECK(strstr(text, "\tWorksheet\tManual"), "row a after manual '%s'", text);
        CHECK(SendDlgItemMessageW(hdlg, IDC_EL_LINKSLISTBOX, LB_GETSEL, a, 0) > 0, "still selected");

        SendMessageW(hdlg, WM_COMMAND, MAKEWPARAM(IDC_EL_UPDATENOW, BN_CLICKED), 0);
        CHECK(links[0].updated == 1 && links[0].updated_msg, "update now %d %d", links[0].updated, links[0].updated_msg);

        el_select_only(hdlg, c);
        CHECK(!IsWindowEnabled(GetDlgItem(hdlg, IDC_EL_UPDATENOW)), "no update of an unavailable source");
        CHECK(!IsWindowEnabled(GetDlgItem(hdlg, IDC_EL_OPENSOURCE)), "no open of an unavailable source");
        CHECK(IsWindowEnabled(GetDlgItem(hdlg, IDC_EL_CHANGESOURCE)), "change of an unavailable source");

        SendDlgItemMessageW(hdlg, IDC_EL_LINKSLISTBOX, LB_SETSEL, TRUE, -1);
        SendMessageW(hdlg, WM_COMMAND, MAKEWPARAM(IDC_EL_LINKSLISTBOX, LBN_SELCHANGE), 0);
        CHECK(!IsWindowEnabled(GetDlgItem(hdlg, IDC_EL_OPENSOURCE)), "no open of several links");
        CHECK(!IsWindowEnabled(GetDlgItem(hdlg, IDC_EL_CHANGESOURCE)), "no change of several links");
        CHECK(IsWindowEnabled(GetDlgItem(hdlg, IDC_EL_CANCELLINK)), "break several links");
        GetDlgItemTextA(hdlg, IDC_EL_LINKSOURCE, text, sizeof(text));
        CHECK(!text[0], "no source shown for several links '%s'", text);

        b = el_find_row(hdlg, "C:\\olddir\\b.xls");
        el_select_only(hdlg, b);
        SendMessageW(hdlg, WM_COMMAND, MAKEWPARAM(IDC_EL_AUTOMATIC, BN_CLICKED), 0);
        CHECK(links[1].update == 1, "automatic set %lu", links[1].update);
        SendMessageW(hdlg, WM_COMMAND, MAKEWPARAM(IDC_EL_CANCELLINK, BN_CLICKED), 0);
        CHECK(links[1].cancelled == 1, "break link");
        CHECK(SendDlgItemMessageW(hdlg, IDC_EL_LINKSLISTBOX, LB_GETCOUNT, 0, 0) == 2, "link removed from the list");
        GetDlgItemTextA(hdlg, IDCANCEL, text, sizeof(text));
        CHECK(!strcmp(text, "Close"), "cancel is close now '%s'", text);
        PostMessageW(hdlg, WM_COMMAND, IDCANCEL, 0);
    }
    else if (el_mode == 1)
    {
        /* the disable flags */
        el_select_only(hdlg, a);
        CHECK(!IsWindowEnabled(GetDlgItem(hdlg, IDC_EL_UPDATENOW)), "update now disabled");
        CHECK(!IsWindowEnabled(GetDlgItem(hdlg, IDC_EL_OPENSOURCE)), "open source disabled");
        CHECK(!IsWindowEnabled(GetDlgItem(hdlg, IDC_EL_CHANGESOURCE)), "change source disabled");
        CHECK(!IsWindowEnabled(GetDlgItem(hdlg, IDC_EL_CANCELLINK)), "break link disabled");
        CHECK(GetWindowLongW(GetDlgItem(hdlg, IDC_OLEUIHELP), GWL_STYLE) & WS_VISIBLE, "help shown");
        PostMessageW(hdlg, WM_COMMAND, IDCANCEL, 0);
    }
    else if (el_mode == 3)
    {
        HANDLE th = start_cs_driver(0, L"C:\\newdir\\a.xls", L"Item2");
        el_select_only(hdlg, a);
        SendMessageW(hdlg, WM_COMMAND, MAKEWPARAM(IDC_EL_CHANGESOURCE, BN_CLICKED), 0);
        WaitForSingleObject(th, 5000);
        CloseHandle(th);
        a = el_find_row(hdlg, "C:\\newdir\\a.xls");
        b = el_find_row(hdlg, "C:\\newdir\\b.xls");
        CHECK(a >= 0 && b >= 0, "rows moved %d %d", a, b);
        PostMessageW(hdlg, WM_COMMAND, IDCANCEL, 0);
    }
    else if (el_mode == 2)
    {
        el_select_only(hdlg, b);
        SendMessageW(hdlg, WM_COMMAND, MAKEWPARAM(IDC_EL_OPENSOURCE, BN_CLICKED), 0);
        CHECK(links[1].opened == 1, "open source");
    }
    return 1;
}

static void test_edit_links(void)
{
    OLEUIEDITLINKSW el;
    UINT ret;

    init_links();
    el_mode = 0;
    memset(&el, 0, sizeof(el));
    el.cbStruct = sizeof(el);
    el.lpfnHook = el_hook;
    el.lCustData = 0x77;
    el.lpOleUILinkContainer = (LPOLEUILINKCONTAINERW)&fake_container;
    ret = OleUIEditLinksW(&el);
    CHECK(ret == OLEUI_CANCEL, "edit links %u", ret);

    init_links();
    el_mode = 1;
    el.dwFlags = ELF_DISABLEUPDATENOW | ELF_DISABLEOPENSOURCE | ELF_DISABLECHANGESOURCE | ELF_DISABLECANCELLINK | ELF_SHOWHELP;
    ret = OleUIEditLinksW(&el);
    CHECK(ret == OLEUI_CANCEL, "edit links flags %u", ret);

    init_links();
    el_mode = 2;
    el.dwFlags = 0;
    ret = OleUIEditLinksW(&el);
    CHECK(ret == OLEUI_OK, "edit links open source closes the dialog %u", ret);

    /* the ANSI function and the ANSI container */
    {
        OLEUIEDITLINKSA ela;

        init_links();
        el_mode = 0;
        memset(&ela, 0, sizeof(ela));
        ela.cbStruct = sizeof(ela);
        ela.lpfnHook = el_hook;
        ela.lCustData = 0x77;
        ela.lpOleUILinkContainer = (LPOLEUILINKCONTAINERA)&fake_container_a;
        ret = OleUIEditLinksA(&ela);
        CHECK(ret == OLEUI_CANCEL, "edit links A %u", ret);
    }
}

/* ------------------------------------------------------------ Change source */
struct cs_driver
{
    DWORD thread;
    int mode;              /* 0 accept, 1 keep the invalid source, 2 try to fix it, then cancel */
    WCHAR file[MAX_PATH];
    WCHAR item[100];
    WCHAR initial_item[100];
    int saw_dialog, saw_prompt;
};

static struct cs_driver driver;

static BOOL CALLBACK find_item_child(HWND hwnd, LPARAM lp)
{
    if (GetDlgCtrlID(hwnd) == 0x481) { *(HWND *)lp = hwnd; return FALSE; }
    return TRUE;
}

struct find_prompt { HWND hwnd; };

static BOOL CALLBACK find_msgbox(HWND hwnd, LPARAM lp)
{
    char cls[40], title[100];
    HWND text;

    GetClassNameA(hwnd, cls, sizeof(cls));
    GetWindowTextA(hwnd, title, sizeof(title));
    if (strcmp(cls, "#32770") || strcmp(title, "Change Source") || !IsWindowVisible(hwnd)) return TRUE;
    text = GetDlgItem(hwnd, 0xffff);
    if (text) { *(HWND *)lp = hwnd; return FALSE; }
    return TRUE;
}

static BOOL CALLBACK find_ofn(HWND hwnd, LPARAM lp)
{
    char cls[40], title[100];
    HWND item = NULL;

    GetClassNameA(hwnd, cls, sizeof(cls));
    GetWindowTextA(hwnd, title, sizeof(title));
    if (strcmp(cls, "#32770") || strcmp(title, "Change Source") || !IsWindowVisible(hwnd)) return TRUE;
    EnumChildWindows(hwnd, find_item_child, (LPARAM)&item);
    if (item) { *(HWND *)lp = hwnd; return FALSE; }
    return TRUE;
}

static DWORD WINAPI cs_driver_thread(void *arg)
{
    struct cs_driver *d = arg;
    HWND ofn = NULL, item = NULL, box = NULL;
    int i;

    for (i = 0; i < 300 && !ofn; i++) { Sleep(50); EnumThreadWindows(d->thread, find_ofn, (LPARAM)&ofn); }
    if (!ofn) return 1;
    d->saw_dialog = 1;
    EnumChildWindows(ofn, find_item_child, (LPARAM)&item);
    GetWindowTextW(item, d->initial_item, 100);
    SendMessageW(ofn, CDM_SETCONTROLTEXT, 0x47c, (LPARAM)d->file);
    SetWindowTextW(item, d->item);
    Sleep(200);
    PostMessageW(ofn, WM_COMMAND, IDOK, 0);
    if (d->mode == 0) return 0;

    for (i = 0; i < 200 && !box; i++) { Sleep(50); EnumThreadWindows(d->thread, find_msgbox, (LPARAM)&box); }
    if (!box) return 1;
    d->saw_prompt = 1;
    PostMessageW(box, WM_COMMAND, d->mode == 1 ? IDNO : IDYES, 0);
    if (d->mode == 2)
    {
        Sleep(500);
        PostMessageW(ofn, WM_COMMAND, IDCANCEL, 0);
    }
    return 0;
}

static HANDLE start_cs_driver(int mode, const WCHAR *file, const WCHAR *item)
{
    memset(&driver, 0, sizeof(driver));
    driver.thread = GetCurrentThreadId();
    driver.mode = mode;
    lstrcpyW(driver.file, file);
    lstrcpyW(driver.item, item);
    return CreateThread(NULL, 0, cs_driver_thread, &driver, 0, NULL);
}

static void test_change_source(void)
{
    OLEUICHANGESOURCEW cs;
    HANDLE th;
    UINT ret;

    CreateDirectoryA("C:\\newdir", NULL);
    CreateDirectoryA("C:\\olddir", NULL);

    init_links();
    memset(&cs, 0, sizeof(cs));
    cs.cbStruct = sizeof(cs);
    cs.lpOleUILinkContainer = (LPOLEUILINKCONTAINERW)&fake_container;
    cs.dwLink = 1;
    cs.lpszFrom = (WCHAR *)L"x";
    ret = OleUIChangeSourceW(&cs);
    CHECK(ret == OLEUI_CSERR_FROMNOTNULL, "from not null %u", ret);
    cs.lpszFrom = NULL;
    cs.lpszTo = (WCHAR *)L"x";
    ret = OleUIChangeSourceW(&cs);
    CHECK(ret == OLEUI_CSERR_TONOTNULL, "to not null %u", ret);
    cs.lpszTo = NULL;
    ret = OleUIChangeSourceW(&cs);
    CHECK(ret == OLEUI_CSERR_SOURCENULL, "source null %u", ret);
    cs.lpszDisplayName = CoTaskMemAlloc(100);
    lstrcpyW(cs.lpszDisplayName, L"C:\\olddir\\a.xls!Sheet1");
    cs.nFileLength = 99;
    ret = OleUIChangeSourceW(&cs);
    CHECK(ret == OLEUI_CSERR_SOURCEPARSEERROR, "parse error %u", ret);

    /* accepted */
    cs.nFileLength = 15;
    th = start_cs_driver(0, L"C:\\newdir\\a.xls", L"NewItem");
    ret = OleUIChangeSourceW(&cs);
    WaitForSingleObject(th, 5000);
    CloseHandle(th);
    CHECK(driver.saw_dialog, "file dialog shown");
    CHECK(!lstrcmpW(driver.initial_item, L"Sheet1"), "item field starts with the item of the source");
    CHECK(ret == OLEUI_OK, "change source %u", ret);
    CHECK(cs.lpszDisplayName && !lstrcmpW(cs.lpszDisplayName, L"C:\\newdir\\a.xls!NewItem"), "name %ls",
          cs.lpszDisplayName ? cs.lpszDisplayName : L"(null)");
    CHECK(cs.nFileLength == 15, "file length %lu", cs.nFileLength);
    CHECK(cs.lpszFrom && !lstrcmpW(cs.lpszFrom, L"C:\\olddir"), "from %ls", cs.lpszFrom ? cs.lpszFrom : L"(null)");
    CHECK(cs.lpszTo && !lstrcmpW(cs.lpszTo, L"C:\\newdir"), "to %ls", cs.lpszTo ? cs.lpszTo : L"(null)");
    CHECK(cs.dwFlags & CSF_VALIDSOURCE, "valid source flag");
    CHECK(setsrc_calls == 1 && last_set_validate && !lstrcmpW(last_set_name, L"C:\\newdir\\a.xls!NewItem") &&
          last_set_len == 15, "container validation %d %d", setsrc_calls, last_set_validate);

    /* the caller validates: nothing is asked of the container */
    init_links();
    cs.lpszFrom = cs.lpszTo = NULL;
    cs.dwFlags = CSF_ONLYGETSOURCE;
    cs.lpszDisplayName = CoTaskMemAlloc(100);
    lstrcpyW(cs.lpszDisplayName, L"C:\\olddir\\a.xls!Sheet1");
    cs.nFileLength = 15;
    th = start_cs_driver(0, L"C:\\newdir\\z.xls", L"");
    ret = OleUIChangeSourceW(&cs);
    WaitForSingleObject(th, 5000);
    CloseHandle(th);
    CHECK(ret == OLEUI_OK && setsrc_calls == 0, "only get source: %u calls %d", ret, setsrc_calls);
    CHECK(!lstrcmpW(cs.lpszDisplayName, L"C:\\newdir\\z.xls"), "no item");
    CHECK(cs.lpszFrom && !lstrcmpW(cs.lpszFrom, L"C:\\olddir\\a.xls") && !lstrcmpW(cs.lpszTo, L"C:\\newdir\\z.xls"),
          "different file names: whole names '%ls' '%ls'", cs.lpszFrom, cs.lpszTo);
    CHECK(!(cs.dwFlags & CSF_VALIDSOURCE), "not validated");

    /* an invalid source that the user keeps */
    init_links();
    setsrc_result = E_FAIL;
    cs.lpszFrom = cs.lpszTo = NULL;
    cs.dwFlags = 0;
    cs.lpszDisplayName = CoTaskMemAlloc(100);
    lstrcpyW(cs.lpszDisplayName, L"C:\\olddir\\a.xls!Sheet1");
    th = start_cs_driver(1, L"C:\\newdir\\a.xls", L"Bad");
    ret = OleUIChangeSourceW(&cs);
    WaitForSingleObject(th, 8000);
    CloseHandle(th);
    CHECK(driver.saw_prompt, "invalid source question shown");
    CHECK(ret == OLEUI_OK && !(cs.dwFlags & CSF_VALIDSOURCE), "kept invalid %u flags %#lx", ret, cs.dwFlags);
    CHECK(setsrc_calls == 2 && !last_set_validate, "set again without validation %d", setsrc_calls);

    /* ... or fixed */
    init_links();
    setsrc_result = E_FAIL;
    cs.lpszFrom = cs.lpszTo = NULL;
    cs.dwFlags = 0;
    cs.lpszDisplayName = CoTaskMemAlloc(100);
    lstrcpyW(cs.lpszDisplayName, L"C:\\olddir\\a.xls!Sheet1");
    th = start_cs_driver(2, L"C:\\newdir\\a.xls", L"Bad");
    ret = OleUIChangeSourceW(&cs);
    WaitForSingleObject(th, 8000);
    CloseHandle(th);
    CHECK(ret == OLEUI_CANCEL, "fixing then cancel %u", ret);
    CHECK(setsrc_calls == 1, "not kept %d", setsrc_calls);
    setsrc_result = S_OK;

    /* the Links dialog moves the links to the same directory along */
    init_links();
    {
        OLEUIEDITLINKSW el;
        memset(&el, 0, sizeof(el));
        el.cbStruct = sizeof(el);
        el.lpfnHook = el_hook;
        el.lCustData = 0x77;
        el.lpOleUILinkContainer = (LPOLEUILINKCONTAINERW)&fake_container;
        el_mode = 3;
        ret = OleUIEditLinksW(&el);
        CHECK(ret == OLEUI_CANCEL, "edit links with change source %u", ret);
        CHECK(!lstrcmpW(links[0].src, L"C:\\newdir\\a.xls!Item2"), "a: %ls", links[0].src);
        CHECK(!lstrcmpW(links[1].src, L"C:\\newdir\\b.xls!Chart2"), "b follows: %ls", links[1].src);
        CHECK(!lstrcmpW(links[2].src, L"D:\\other\\c.doc"), "c stays: %ls", links[2].src);
    }
}

/* -------------------------------------------------------- Object properties */
struct fake_obj
{
    DWORD size;
    HGLOBAL meta;
    DWORD aspect;
    INT scale;
    int info_calls, convert_calls, setview_calls, setview_relative;
    CLSID converted_to;
    DWORD set_aspect;
    INT set_scale;
};

static struct fake_obj fobj;

static HRESULT WINAPI fo_QueryInterface(IOleUIObjInfoW *iface, REFIID riid, void **obj)
{
    if (IsEqualIID(riid, &IID_IUnknown)) { *obj = iface; return S_OK; }
    *obj = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI fo_AddRef(IOleUIObjInfoW *iface) { return 2; }
static ULONG WINAPI fo_Release(IOleUIObjInfoW *iface) { return 1; }

static WCHAR *cotask(const WCHAR *s) { return lstrcpyW(CoTaskMemAlloc((lstrlenW(s) + 1) * 2), s); }

static HRESULT WINAPI fo_GetObjectInfo(IOleUIObjInfoW *iface, DWORD obj, DWORD *size, LPWSTR *label, LPWSTR *type,
                                       LPWSTR *shrt, LPWSTR *loc)
{
    fobj.info_calls++;
    *size = fobj.size;
    if (label) *label = cotask(fobj.info_calls > 1 ? L"Converted Label" : L"Sheet Label");
    if (type) *type = cotask(L"Excel Worksheet");
    if (shrt) *shrt = cotask(L"Worksheet");
    if (loc) *loc = cotask(L"C:\\docs\\book.xls");
    return S_OK;
}

static HRESULT WINAPI fo_GetConvertInfo(IOleUIObjInfoW *iface, DWORD obj, CLSID *clsid, WORD *fmt, CLSID *def,
                                        LPCLSID *excl, UINT *nexcl)
{
    *clsid = clsid_src;
    *fmt = 0;
    *def = clsid_conv;
    *excl = NULL;
    *nexcl = 0;
    return S_OK;
}

static HRESULT WINAPI fo_ConvertObject(IOleUIObjInfoW *iface, DWORD obj, REFCLSID clsid)
{
    fobj.convert_calls++;
    fobj.converted_to = *clsid;
    return S_OK;
}

static HRESULT WINAPI fo_GetViewInfo(IOleUIObjInfoW *iface, DWORD obj, HGLOBAL *meta, DWORD *aspect, INT *scale)
{
    /* a copy: the dialog frees it */
    *meta = OleGetIconOfClass(&clsid_src, (LPOLESTR)L"Label", FALSE);
    *aspect = fobj.aspect;
    *scale = fobj.scale;
    return S_OK;
}

static HRESULT WINAPI fo_SetViewInfo(IOleUIObjInfoW *iface, DWORD obj, HGLOBAL meta, DWORD aspect, INT scale, BOOL rel)
{
    fobj.setview_calls++;
    fobj.set_aspect = aspect;
    fobj.set_scale = scale;
    fobj.setview_relative = rel;
    return S_OK;
}

static const IOleUIObjInfoWVtbl fo_vtbl =
{
    fo_QueryInterface, fo_AddRef, fo_Release, fo_GetObjectInfo, fo_GetConvertInfo, fo_ConvertObject, fo_GetViewInfo, fo_SetViewInfo
};
static IOleUIObjInfoW fake_obj = { &fo_vtbl };

struct op_driver
{
    DWORD thread;
    int press;           /* PSBTN_OK or PSBTN_CANCEL */
    int failures;
    HWND sheet;
    WCHAR title[40];
};

static struct op_driver opd;

#define OPCHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static BOOL CALLBACK find_sheet(HWND hwnd, LPARAM lp)
{
    char title[100], cls[40];

    GetClassNameA(hwnd, cls, sizeof(cls));
    GetWindowTextA(hwnd, title, sizeof(title));
    if (!strcmp(cls, "#32770") && !strcmp(title, "Object Props") && IsWindowVisible(hwnd)) { *(HWND *)lp = hwnd; return FALSE; }
    return TRUE;
}

static BOOL CALLBACK find_titled_dialog(HWND hwnd, LPARAM lp)
{
    char title[100], cls[40];
    HWND *info = (HWND *)lp;       /* [0] result, [1] title */
    GetClassNameA(hwnd, cls, sizeof(cls));
    GetWindowTextA(hwnd, title, sizeof(title));
    if (!strcmp(cls, "#32770") && !strcmp(title, (const char *)info[1]) && IsWindowVisible(hwnd)) { info[0] = hwnd; return FALSE; }
    return TRUE;
}

static HWND wait_dialog(DWORD thread, const char *title)
{
    HWND info[2] = { NULL, (HWND)title };
    int i;

    for (i = 0; i < 200 && !info[0]; i++) { Sleep(50); EnumThreadWindows(thread, find_titled_dialog, (LPARAM)info); }
    return info[0];
}

static HWND goto_page(HWND sheet, int index, int id)
{
    HWND page;
    int i;

    SendMessageW(sheet, PSM_SETCURSEL, index, 0);
    for (i = 0; i < 20; i++)
    {
        page = (HWND)SendMessageW(sheet, PSM_GETCURRENTPAGEHWND, 0, 0);
        if (page && GetDlgItem(page, id)) return page;
        Sleep(50);
    }
    return NULL;
}

static DWORD WINAPI op_thread(void *arg)
{
    struct op_driver *d = arg;
    HWND sheet = NULL, page, conv;
    char text[300];
    int i;

    for (i = 0; i < 300 && !sheet; i++) { Sleep(50); EnumThreadWindows(d->thread, find_sheet, (LPARAM)&sheet); }
    if (!sheet) return 1;
    d->sheet = sheet;

    page = goto_page(sheet, 0, IDC_GP_OBJECTNAME);
    OPCHECK(page != NULL, "general page");
    if (!page) { PostMessageW(sheet, PSM_PRESSBUTTON, PSBTN_CANCEL, 0); return 1; }
    GetDlgItemTextA(page, IDC_GP_OBJECTNAME, text, sizeof(text));
    OPCHECK(!strcmp(text, "Sheet Label"), "object name '%s'", text);
    GetDlgItemTextA(page, IDC_GP_OBJECTTYPE, text, sizeof(text));
    OPCHECK(!strcmp(text, "Excel Worksheet"), "object type '%s'", text);
    GetDlgItemTextA(page, IDC_GP_OBJECTSIZE, text, sizeof(text));
    OPCHECK(!strcmp(text, fobj.size == (DWORD)-1 ? "Unknown" : "3 KB"), "object size '%s'", text);
    GetDlgItemTextA(page, IDC_GP_OBJECTLOCATION, text, sizeof(text));
    OPCHECK(!strcmp(text, "C:\\docs\\book.xls"), "object location '%s'", text);
    OPCHECK(IsWindowEnabled(GetDlgItem(page, IDC_GP_CONVERT)) == !(d->press == 100), "convert button state");

    if (d->press == PSBTN_OK)
    {
        /* convert the object */
        PostMessageW(page, WM_COMMAND, MAKEWPARAM(IDC_GP_CONVERT, BN_CLICKED), 0);
        conv = wait_dialog(d->thread, "Convert");
        OPCHECK(conv != NULL, "convert dialog");
        if (conv)
        {
            SendDlgItemMessageW(conv, IDC_CV_CONVERTLIST, LB_SETCURSEL, 0, 0);
            PostMessageW(conv, WM_COMMAND, IDOK, 0);
            Sleep(500);
        }
        OPCHECK(fobj.convert_calls == 1 && IsEqualGUID(&fobj.converted_to, &clsid_conv), "object converted %d", fobj.convert_calls);
        GetDlgItemTextA(page, IDC_GP_OBJECTNAME, text, sizeof(text));
        OPCHECK(!strcmp(text, "Converted Label"), "page read the object again '%s'", text);

        /* the view page */
        page = goto_page(sheet, 1, IDC_VP_PERCENT);
        OPCHECK(page != NULL, "view page");
        if (page)
        {
            OPCHECK(IsDlgButtonChecked(page, IDC_VP_ASICON) == BST_CHECKED, "icon aspect");
            OPCHECK(IsWindowEnabled(GetDlgItem(page, IDC_VP_CHANGEICON)), "change icon enabled");
            GetDlgItemTextA(page, IDC_VP_PERCENT, text, sizeof(text));
            OPCHECK(!strcmp(text, "150"), "scale '%s'", text);
            SendMessageW(page, WM_COMMAND, MAKEWPARAM(IDC_VP_EDITABLE, BN_CLICKED), 0);
            OPCHECK(!IsWindowEnabled(GetDlgItem(page, IDC_VP_CHANGEICON)), "change icon disabled for content");
            SetDlgItemTextA(page, IDC_VP_PERCENT, "9999");
            CheckDlgButton(page, IDC_VP_RELATIVE, BST_CHECKED);
            SendMessageW(page, WM_COMMAND, MAKEWPARAM(IDC_VP_RELATIVE, BN_CLICKED), 0);
        }

        /* the link page */
        page = goto_page(sheet, 2, IDC_LP_LINKSOURCE);
        OPCHECK(page != NULL, "link page");
        if (page)
        {
            GetDlgItemTextA(page, IDC_LP_LINKSOURCE, text, sizeof(text));
            OPCHECK(!strcmp(text, "C:\\olddir\\a.xls\r\nSheet1"), "link source '%s'", text);
            OPCHECK(IsDlgButtonChecked(page, IDC_LP_AUTOMATIC) == BST_CHECKED, "link automatic");
            GetDlgItemTextA(page, IDC_LP_DATE, text, sizeof(text));
            OPCHECK(!strcmp(text, "Unknown"), "link date '%s'", text);
            SendMessageW(page, WM_COMMAND, MAKEWPARAM(IDC_LP_MANUAL, BN_CLICKED), 0);
            SendMessageW(page, WM_COMMAND, MAKEWPARAM(IDC_LP_UPDATENOW, BN_CLICKED), 0);
            SendMessageW(page, WM_COMMAND, MAKEWPARAM(IDC_LP_OPENSOURCE, BN_CLICKED), 0);
        }
    }
    else if (d->press == PSBTN_CANCEL)
    {
        page = goto_page(sheet, 1, IDC_VP_PERCENT);
        if (page) SetDlgItemTextA(page, IDC_VP_PERCENT, "77");
    }
    PostMessageW(sheet, PSM_PRESSBUTTON, d->press == 100 ? PSBTN_CANCEL : d->press, 0);
    return 0;
}

static UINT CALLBACK gp_hook(HWND hdlg, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_INITDIALOG)
    {
        OLEUIGNRLPROPSW *gp = (OLEUIGNRLPROPSW *)lp;
        OPCHECK(gp->lCustData == 0x99, "gp custdata");
        OPCHECK(gp->lpOP != NULL, "gp lpOP set");
    }
    return 0;
}

static void test_object_properties(void)
{
    OLEUIOBJECTPROPSW op;
    OLEUIGNRLPROPSW gp;
    OLEUIVIEWPROPSW vp;
    OLEUILINKPROPSW lp;
    PROPSHEETHEADERW ps;
    HANDLE th;
    UINT ret;

    init_links();
    memset(&gp, 0, sizeof(gp)); gp.cbStruct = sizeof(gp); gp.lpfnHook = gp_hook; gp.lCustData = 0x99;
    memset(&vp, 0, sizeof(vp)); vp.cbStruct = sizeof(vp); vp.nScaleMin = 10; vp.nScaleMax = 400;
    memset(&lp, 0, sizeof(lp)); lp.cbStruct = sizeof(lp);
    memset(&ps, 0, sizeof(ps));
    ps.dwSize = sizeof(ps);
    ps.dwFlags = PSH_NOAPPLYNOW;
    ps.pszCaption = L"Object Props";
    ps.hInstance = GetModuleHandleW(NULL);
    memset(&op, 0, sizeof(op));
    op.cbStruct = sizeof(op);
    op.dwFlags = OPF_OBJECTISLINK;
    op.lpPS = &ps;
    op.dwObject = 5;
    op.lpObjInfo = &fake_obj;
    op.dwLink = 1;
    op.lpLinkInfo = (LPOLEUILINKINFOW)&fake_container;
    op.lpGP = &gp; op.lpVP = &vp; op.lpLP = &lp;

    /* checks of the structures */
    op.lpGP = NULL;
    ret = OleUIObjectPropertiesW(&op);
    CHECK(ret == OLEUI_OPERR_SUBPROPNULL, "no general page structure %u", ret);
    op.lpGP = &gp;
    op.lpObjInfo = NULL;
    ret = OleUIObjectPropertiesW(&op);
    CHECK(ret == OLEUI_OPERR_OBJINFOINVALID, "no object info %u", ret);
    op.lpObjInfo = &fake_obj;
    op.lpLinkInfo = NULL;
    ret = OleUIObjectPropertiesW(&op);
    CHECK(ret == OLEUI_OPERR_LINKINFOINVALID, "no link info %u", ret);
    op.lpLinkInfo = (LPOLEUILINKINFOW)&fake_container;
    ps.dwSize = 3;
    ret = OleUIObjectPropertiesW(&op);
    CHECK(ret == OLEUI_OPERR_PROPERTYSHEET, "header size %u", ret);
    ps.dwSize = sizeof(ps);

    /* OK applies the view and the link settings */
    memset(&fobj, 0, sizeof(fobj));
    fobj.size = 3000;
    fobj.aspect = DVASPECT_ICON;
    fobj.scale = 150;
    fobj.meta = meta_for(&clsid_src, L"Label");
    links[0].update = 1;
    memset(&opd, 0, sizeof(opd));
    opd.thread = GetCurrentThreadId();
    opd.press = PSBTN_OK;
    th = CreateThread(NULL, 0, op_thread, &opd, 0, NULL);
    ret = OleUIObjectPropertiesW(&op);
    WaitForSingleObject(th, 10000);
    CloseHandle(th);
    CHECK(ret == OLEUI_OK, "object properties ok %u", ret);
    CHECK(fobj.setview_calls == 1 && fobj.set_aspect == DVASPECT_CONTENT && fobj.set_scale == 400 && fobj.setview_relative == 1,
          "set view %d aspect %lu scale %d relative %d", fobj.setview_calls, fobj.set_aspect, fobj.set_scale, fobj.setview_relative);
    CHECK(links[0].update == 3, "link made manual %lu", links[0].update);
    CHECK(links[0].updated == 1 && links[0].updated_msg, "link updated");
    CHECK(links[0].opened == 1, "link source opened");
    CHECK(gp.lpOP == &op, "internal pointer");
    CHECK(vp.dwFlags & VPF_SELECTRELATIVE, "relative flag returned %#lx", vp.dwFlags);

    /* Cancel changes nothing */
    init_links();
    memset(&fobj, 0, sizeof(fobj));
    fobj.size = (DWORD)-1;
    fobj.aspect = DVASPECT_ICON;
    fobj.scale = 150;
    fobj.meta = meta_for(&clsid_src, L"Label");
    memset(&opd, 0, sizeof(opd));
    opd.thread = GetCurrentThreadId();
    opd.press = PSBTN_CANCEL;
    th = CreateThread(NULL, 0, op_thread, &opd, 0, NULL);
    ret = OleUIObjectPropertiesW(&op);
    WaitForSingleObject(th, 10000);
    CloseHandle(th);
    CHECK(ret == OLEUI_CANCEL, "object properties cancel %u", ret);
    CHECK(fobj.setview_calls == 0 && links[0].update == 1, "nothing applied");

    /* the disabled convert button */
    init_links();
    fobj.size = 3000;
    fobj.info_calls = 0;
    op.dwFlags |= OPF_DISABLECONVERT;
    memset(&opd, 0, sizeof(opd));
    opd.thread = GetCurrentThreadId();
    opd.press = 100;
    th = CreateThread(NULL, 0, op_thread, &opd, 0, NULL);
    ret = OleUIObjectPropertiesW(&op);
    WaitForSingleObject(th, 10000);
    CloseHandle(th);
    CHECK(ret == OLEUI_CANCEL, "object properties no convert %u", ret);
}

/* ---------------------------------------------------------- ANSI functions */
static HRESULT WINAPI foa_GetObjectInfo(IOleUIObjInfoA *iface, DWORD obj, DWORD *size, LPSTR *label, LPSTR *type,
                                        LPSTR *shrt, LPSTR *loc)
{
    fobj.info_calls++;
    *size = fobj.size;
    if (label) *label = dup_a(L"Sheet Label");
    if (type) *type = dup_a(L"Excel Worksheet");
    if (shrt) *shrt = dup_a(L"Worksheet");
    if (loc) *loc = dup_a(L"C:\\docs\\book.xls");
    return S_OK;
}
static HRESULT WINAPI foa_QueryInterface(IOleUIObjInfoA *iface, REFIID riid, void **obj) { return fo_QueryInterface(NULL, riid, obj); }
static ULONG WINAPI foa_AddRef(IOleUIObjInfoA *iface) { return 2; }
static ULONG WINAPI foa_Release(IOleUIObjInfoA *iface) { return 1; }
static HRESULT WINAPI foa_GetConvertInfo(IOleUIObjInfoA *iface, DWORD obj, CLSID *clsid, WORD *fmt, CLSID *def, LPCLSID *excl, UINT *nexcl)
{ return fo_GetConvertInfo(NULL, obj, clsid, fmt, def, excl, nexcl); }
static HRESULT WINAPI foa_ConvertObject(IOleUIObjInfoA *iface, DWORD obj, REFCLSID clsid) { return fo_ConvertObject(NULL, obj, clsid); }
static HRESULT WINAPI foa_GetViewInfo(IOleUIObjInfoA *iface, DWORD obj, HGLOBAL *meta, DWORD *aspect, INT *scale)
{ return fo_GetViewInfo(NULL, obj, meta, aspect, scale); }
static HRESULT WINAPI foa_SetViewInfo(IOleUIObjInfoA *iface, DWORD obj, HGLOBAL meta, DWORD aspect, INT scale, BOOL rel)
{ return fo_SetViewInfo(NULL, obj, meta, aspect, scale, rel); }

static const IOleUIObjInfoAVtbl foa_vtbl =
{
    foa_QueryInterface, foa_AddRef, foa_Release, foa_GetObjectInfo, foa_GetConvertInfo, foa_ConvertObject, foa_GetViewInfo, foa_SetViewInfo
};
static IOleUIObjInfoA fake_obj_a = { &foa_vtbl };

static void test_ansi(void)
{
    OLEUICHANGEICONA cia;
    OLEUICHANGESOURCEA csa;
    OLEUIOBJECTPROPSA opa;
    OLEUIGNRLPROPSA gpa;
    OLEUIVIEWPROPSA vpa;
    OLEUILINKPROPSA lpa;
    PROPSHEETHEADERA psa;
    HANDLE th;
    UINT ret;

    memset(&cia, 0, sizeof(cia));
    cia.cbStruct = sizeof(cia);
    cia.dwFlags = CIF_SELECTCURRENT;
    cia.lpfnHook = ci_hook;
    cia.lCustData = 0x55;
    cia.clsid = clsid_src;
    cia.hMetaPict = meta_for(&clsid_src, L"Orig Label");
    ret = OleUIChangeIconA(&cia);
    CHECK(ret == OLEUI_OK, "changeiconA %u", ret);
    CHECK((cia.dwFlags & CIF_SELECTDEFAULT) != 0 && cia.hMetaPict != NULL, "changeiconA result %#lx", cia.dwFlags);

    /* change source */
    init_links();
    CreateDirectoryA("C:\\newdir", NULL);
    memset(&csa, 0, sizeof(csa));
    csa.cbStruct = sizeof(csa);
    csa.lpOleUILinkContainer = (LPOLEUILINKCONTAINERA)&fake_container_a;
    csa.dwLink = 1;
    csa.lpszDisplayName = CoTaskMemAlloc(100);
    strcpy(csa.lpszDisplayName, "C:\\olddir\\a.xls!Sheet1");
    csa.nFileLength = 15;
    th = start_cs_driver(0, L"C:\\newdir\\a.xls", L"AItem");
    ret = OleUIChangeSourceA(&csa);
    WaitForSingleObject(th, 5000);
    CloseHandle(th);
    CHECK(ret == OLEUI_OK, "changesourceA %u", ret);
    CHECK(csa.lpszDisplayName && !strcmp(csa.lpszDisplayName, "C:\\newdir\\a.xls!AItem"), "A name %s", csa.lpszDisplayName);
    CHECK(csa.nFileLength == 15, "A file length %lu", csa.nFileLength);
    CHECK(csa.lpszFrom && !strcmp(csa.lpszFrom, "C:\\olddir") && csa.lpszTo && !strcmp(csa.lpszTo, "C:\\newdir"), "A from to");
    CHECK(csa.dwFlags & CSF_VALIDSOURCE, "A valid");
    CHECK(!lstrcmpW(links[0].src, L"C:\\newdir\\a.xls!AItem") && links[0].file_len == 15, "A container got %ls", links[0].src);

    /* object properties */
    init_links();
    memset(&fobj, 0, sizeof(fobj));
    fobj.size = 3000;
    fobj.aspect = DVASPECT_CONTENT;
    fobj.scale = 100;
    memset(&gpa, 0, sizeof(gpa)); gpa.cbStruct = sizeof(gpa);
    memset(&vpa, 0, sizeof(vpa)); vpa.cbStruct = sizeof(vpa);
    memset(&lpa, 0, sizeof(lpa)); lpa.cbStruct = sizeof(lpa);
    memset(&psa, 0, sizeof(psa));
    psa.dwSize = sizeof(psa);
    psa.dwFlags = PSH_NOAPPLYNOW;
    psa.pszCaption = "Object Props";
    memset(&opa, 0, sizeof(opa));
    opa.cbStruct = sizeof(opa);
    opa.dwFlags = OPF_OBJECTISLINK;
    opa.lpPS = &psa;
    opa.dwObject = 5;
    opa.lpObjInfo = &fake_obj_a;
    opa.dwLink = 1;
    opa.lpLinkInfo = &fake_container_a;
    opa.lpGP = &gpa; opa.lpVP = &vpa; opa.lpLP = &lpa;
    memset(&opd, 0, sizeof(opd));
    opd.thread = GetCurrentThreadId();
    opd.press = PSBTN_CANCEL;
    th = CreateThread(NULL, 0, op_thread, &opd, 0, NULL);
    ret = OleUIObjectPropertiesA(&opa);
    WaitForSingleObject(th, 10000);
    CloseHandle(th);
    CHECK(ret == OLEUI_CANCEL, "object propertiesA %u", ret);
}

/* ------------------------------------------------------------ Insert object */
static DWORD WINAPI insert_thread(void *arg)
{
    HWND dlg = wait_dialog(main_thread, "Insert Object");

    if (!dlg) return 1;
    SetDlgItemTextA(dlg, 1011, "C:\\insobj.stg");
    SendMessageW(dlg, WM_COMMAND, MAKEWPARAM(1004, BN_CLICKED), 0);
    SetDlgItemTextA(dlg, 1011, "C:\\insobj.stg");
    PostMessageW(dlg, WM_COMMAND, IDOK, 0);
    return 0;
}

static void test_insert_object(void)
{
    OLEUIINSERTOBJECTW io;
    IStorage *stg;
    WCHAR file[MAX_PATH] = L"";
    HANDLE th;
    UINT ret;

    memset(&io, 0, sizeof(io));
    io.cbStruct = sizeof(io) + 1;
    ret = OleUIInsertObjectW(&io);
    CHECK(ret == OLEUI_ERR_CBSTRUCTINCORRECT, "insert cb %u", ret);

    if (SUCCEEDED(StgCreateDocfile(L"C:\\insobj.stg", STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stg)))
    {
        WriteClassStg(stg, &clsid_src);
        IStorage_Release(stg);
    }
    memset(&io, 0, sizeof(io));
    io.cbStruct = sizeof(io);
    io.dwFlags = IOF_SELECTCREATEFROMFILE;
    io.lpszFile = file;
    io.cchFile = MAX_PATH;
    main_thread = GetCurrentThreadId();
    th = CreateThread(NULL, 0, insert_thread, NULL, 0, NULL);
    ret = OleUIInsertObjectW(&io);
    WaitForSingleObject(th, 5000);
    CloseHandle(th);
    CHECK(ret == OLEUI_OK, "insert object %u", ret);
    CHECK(!lstrcmpiW(file, L"C:\\insobj.stg"), "file name %ls", file);
    CHECK(IsEqualGUID(&io.clsid, &clsid_src), "class of the file");
    DeleteFileA("C:\\insobj.stg");
}

int main(void)
{
    HRESULT hr = OleInitialize(NULL);

    setvbuf(stdout, NULL, _IONBF, 0);

    InitCommonControls();
    msg_enddialog = RegisterWindowMessageA("OLEUI_MSG_ENDDIALOG");
    msg_closebusy = RegisterWindowMessageA("OLEUI_MSG_CLOSEBUSYDIALOG");
    msg_help = RegisterWindowMessageA("OLEUI_MSG_HELP");
    printf("init %lx\n", hr);

    printf("-- test_validation\n");
    test_validation();
    printf("-- test_can_convert\n");
    test_can_convert();
    printf("-- test_convert\n");
    test_convert();
    printf("-- test_changeicon\n");
    test_changeicon();
    printf("-- test_busy\n");
    test_busy();
    printf("-- test_prompt\n");
    test_prompt();
    printf("-- test_update_links\n");
    test_update_links();
    printf("-- test_edit_links\n");
    test_edit_links();
    printf("-- test_change_source\n");
    test_change_source();
    printf("-- test_object_properties\n");
    test_object_properties();
    printf("-- test_ansi\n");
    test_ansi();
    printf("-- test_insert_object\n");
    test_insert_object();

    printf("checks %d fails %d\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
