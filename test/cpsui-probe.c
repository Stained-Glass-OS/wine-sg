/* The cpsui gate's program (patches/sg/1026): it builds a property sheet
 * through CPSUI the way printer drivers' UI DLLs do -- a PFNPROPSHEETUI
 * function that adds another, pages, a group parent, pages inserted before
 * others, a COMPROPSHEETUI on the treeview page and another on a page of
 * its own with controls numbered from BegCtrlID, data blocks, CPSUI's own
 * strings, a page title changed -- then works the sheet like a user: picks
 * values in the tree and on its own page, and presses OK.  It prints what
 * it saw, one fact a line.  Our own code, from the compstui.h reference. */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <commctrl.h>
#include <prsht.h>
#include <compstui.h>

#define WM_SG_CHECK (WM_APP + 1)
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static OPTPARAM colours[3], papers[2], copies[2], collate[1], name[2];
static OPTTYPE t_colour, t_paper, t_copies, t_collate, t_name;
static OPTITEM tree_items[5], own_items[1];
static WCHAR job_name[64] = L"first";
static COMPROPSHEETUI tree_ui, own_ui;
static DLGPAGE own_page;
static HANDLE child_handle, plain_page, tree_group;
static PFNCOMPROPSHEET compropsheet;
static int applied, sel_changed, child_results, top_results;
static PSPINFO plain_info;
static BYTE template_plain[512], template_own[1024], template_before[512];

static INT_PTR CALLBACK own_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam );
static void say( const char *fmt, ... )
{
    va_list args;
    va_start( args, fmt );
    vprintf( fmt, args );
    va_end( args );
    fflush( stdout );
}

/* an in-memory dialog template: a page with some controls */
struct ctl { const WCHAR *cls; DWORD style; WORD id; short x, y, cx, cy; const WCHAR *text; };

static void make_template( BYTE *buf, const struct ctl *ctls, int count )
{
    DLGTEMPLATE *t = (DLGTEMPLATE *)buf;
    WORD *p;
    int i;

    memset( buf, 0, 16 );
    t->style = WS_CHILD | DS_CONTROL | DS_SETFONT;
    t->cdit = count;
    t->cx = 252;
    t->cy = 216;
    p = (WORD *)(t + 1);
    *p++ = 0; *p++ = 0; *p++ = 0;
    *p++ = 8;
    wcscpy( (WCHAR *)p, L"MS Shell Dlg" );
    p += wcslen( L"MS Shell Dlg" ) + 1;
    for (i = 0; i < count; i++)
    {
        DLGITEMTEMPLATE *it;
        p = (WORD *)(((ULONG_PTR)p + 3) & ~3);
        it = (DLGITEMTEMPLATE *)p;
        it->style = WS_CHILD | WS_VISIBLE | ctls[i].style;
        it->dwExtendedStyle = 0;
        it->x = ctls[i].x; it->y = ctls[i].y; it->cx = ctls[i].cx; it->cy = ctls[i].cy;
        it->id = ctls[i].id;
        p = (WORD *)(it + 1);
        wcscpy( (WCHAR *)p, ctls[i].cls );
        p += wcslen( ctls[i].cls ) + 1;
        wcscpy( (WCHAR *)p, ctls[i].text ? ctls[i].text : L"" );
        p += wcslen( ctls[i].text ? ctls[i].text : L"" ) + 1;
        *p++ = 0;
    }
}

static LONG CALLBACK tree_callback( CPSUICBPARAM *p )
{
    if (p->Reason == CPSUICB_REASON_SEL_CHANGED)
    {
        sel_changed++;
        say( "callback sel_changed item %d old %ld new %ld\n", (int)(p->pCurItem - p->pOptItem), p->OldSel,
             p->pCurItem->Sel );
        /* choosing Blue hides the collation option */
        if (p->pCurItem == &tree_items[1] && p->pCurItem->Sel == 2)
        {
            tree_items[3].Flags |= OPTIF_HIDE;
            return CPSUICB_ACTION_OPTIF_CHANGED;
        }
    }
    if (p->Reason == CPSUICB_REASON_APPLYNOW)
    {
        applied++;
        say( "callback applynow userdata %Iu\n", p->UserData );
        return CPSUICB_ACTION_ITEMS_APPLIED;
    }
    return CPSUICB_ACTION_NONE;
}

static LONG CALLBACK own_callback( CPSUICBPARAM *p )
{
    if (p->Reason == CPSUICB_REASON_SEL_CHANGED)
        say( "own callback sel_changed new %ld\n", p->pCurItem->Sel );
    return CPSUICB_ACTION_NONE;
}

static HWND find_child( HWND parent, const WCHAR *cls )
{
    return FindWindowExW( parent, NULL, cls, NULL );
}

static HTREEITEM find_tree_item( HWND tree, HTREEITEM from, const WCHAR *prefix, WCHAR *text )
{
    HTREEITEM item, found;
    TVITEMW tvi;

    for (item = from; item; item = (HTREEITEM)SendMessageW( tree, TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)item ))
    {
        tvi.mask = TVIF_TEXT;
        tvi.hItem = item;
        tvi.pszText = text;
        tvi.cchTextMax = 128;
        SendMessageW( tree, TVM_GETITEMW, 0, (LPARAM)&tvi );
        if (!wcsncmp( text, prefix, wcslen( prefix ) )) return item;
        if ((found = find_tree_item( tree, (HTREEITEM)SendMessageW( tree, TVM_GETNEXTITEM, TVGN_CHILD, (LPARAM)item ),
                                     prefix, text )))
            return found;
    }
    return NULL;
}

static void check_sheet( HWND page )
{
    HWND sheet = GetParent( page ), tab = (HWND)SendMessageW( sheet, PSM_GETTABCONTROL, 0, 0 ), tree, tree_page,
         own, ctl;
    WCHAR text[128], tabs[512] = L"";
    HTREEITEM item;
    TCITEMW tci;
    int i, count = SendMessageW( tab, TCM_GETITEMCOUNT, 0, 0 );

    for (i = 0; i < count; i++)
    {
        tci.mask = TCIF_TEXT;
        tci.pszText = text;
        tci.cchTextMax = ARRAY_SIZE(text);
        SendMessageW( tab, TCM_GETITEMW, i, (LPARAM)&tci );
        wcscat( tabs, text );
        wcscat( tabs, L"|" );
    }
    say( "tabs %d %ls\n", count, tabs );
    SendMessageW( sheet, PSM_SETCURSEL, 1, 0 );

    /* the treeview page */
    SendMessageW( sheet, PSM_SETCURSEL, 2, 0 );
    tree_page = (HWND)SendMessageW( sheet, PSM_INDEXTOHWND, 2, 0 );
    tree = find_child( tree_page, WC_TREEVIEWW );
    say( "tree items %d\n", tree ? (int)SendMessageW( tree, TVM_GETCOUNT, 0, 0 ) : -1 );
    if (tree && (item = find_tree_item( tree, (HTREEITEM)SendMessageW( tree, TVM_GETNEXTITEM, TVGN_ROOT, 0 ),
                                        L"Colour", text )))
    {
        say( "tree text %ls\n", text );
        SendMessageW( tree, TVM_SELECTITEM, TVGN_CARET, (LPARAM)item );
        if ((ctl = find_child( tree_page, WC_COMBOBOXW )))
        {
            say( "change area combo entries %d sel %d\n", (int)SendMessageW( ctl, CB_GETCOUNT, 0, 0 ),
                 (int)SendMessageW( ctl, CB_GETCURSEL, 0, 0 ) );
            SendMessageW( ctl, CB_SETCURSEL, 2, 0 );
            SendMessageW( tree_page, WM_COMMAND, MAKEWPARAM( GetDlgCtrlID( ctl ), CBN_SELCHANGE ), (LPARAM)ctl );
        }
        else say( "change area combo missing\n" );
        if ((item = find_tree_item( tree, (HTREEITEM)SendMessageW( tree, TVM_GETNEXTITEM, TVGN_ROOT, 0 ),
                                    L"Colour", text )))
            say( "tree text after %ls\n", text );
        say( "tree items after %d\n", (int)SendMessageW( tree, TVM_GETCOUNT, 0, 0 ) );
    }
    if (tree && (item = find_tree_item( tree, (HTREEITEM)SendMessageW( tree, TVM_GETNEXTITEM, TVGN_ROOT, 0 ),
                                        L"Copies", text )))
    {
        SendMessageW( tree, TVM_SELECTITEM, TVGN_CARET, (LPARAM)item );
        if ((ctl = find_child( tree_page, WC_EDITW ))) SetWindowTextW( ctl, L"5" );
        else say( "change area edit missing\n" );
    }

    /* the page of its own */
    SendMessageW( sheet, PSM_SETCURSEL, 3, 0 );
    own = (HWND)SendMessageW( sheet, PSM_INDEXTOHWND, 3, 0 );
    if ((ctl = GetDlgItem( own, 302 )))
    {
        GetDlgItemTextW( own, 301, text, ARRAY_SIZE(text) );
        say( "own page title %ls combo entries %d sel %d\n", text, (int)SendMessageW( ctl, CB_GETCOUNT, 0, 0 ),
             (int)SendMessageW( ctl, CB_GETCURSEL, 0, 0 ) );
        SendMessageW( ctl, CB_SETCURSEL, 1, 0 );
        SendMessageW( own, WM_COMMAND, MAKEWPARAM( 302, CBN_SELCHANGE ), (LPARAM)ctl );
    }
    else say( "own page combo missing\n" );

    /* CPSUI_SHOW=N leaves the sheet up on page N, for a look at it */
    if (GetEnvironmentVariableW( L"CPSUI_SHOW", text, ARRAY_SIZE(text) ))
        SendMessageW( sheet, PSM_SETCURSEL, _wtoi( text ), 0 );
    else PostMessageW( sheet, PSM_PRESSBUTTON, PSBTN_OK, 0 );
}

static INT_PTR CALLBACK plain_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    if (msg == WM_INITDIALOG)
    {
        PROPSHEETPAGEW *psp = (PROPSHEETPAGEW *)lparam;
        PSPINFO *info = (PSPINFO *)((BYTE *)psp + psp->dwSize);
        say( "plain page pspinfo %d group %d\n", info->cbSize == sizeof(PSPINFO), info->hComPropSheet == child_handle );
        plain_info = *info;
        return TRUE;
    }
    if (msg == WM_NOTIFY && ((NMHDR *)lparam)->code == PSN_APPLY)
    {
        /* as some makers' pages do: the result, through the page's own handle */
        say( "page set_result %ld\n", (LONG)plain_info.pfnComPropSheet( plain_info.hCPSUIPage, CPSFUNC_SET_RESULT,
             (LPARAM)plain_info.hCPSUIPage, 7 ) );
        SetWindowLongPtrW( hwnd, DWLP_MSGRESULT, PSNRET_NOERROR );
        return TRUE;
    }
    return FALSE;
}

/* the first page: the sheet's pages are made as they are shown */
static INT_PTR CALLBACK before_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    if (msg == WM_INITDIALOG)
    {
        PostMessageW( hwnd, WM_SG_CHECK, 0, 0 );
        return TRUE;
    }
    if (msg == WM_SG_CHECK)
    {
        check_sheet( hwnd );
        return TRUE;
    }
    return FALSE;
}

static void setup_items(void)
{
    static const WCHAR *colour_names[] = { L"Red", L"Green", L"Blue" };
    int i;

    for (i = 0; i < 3; i++) { colours[i].cbSize = sizeof(OPTPARAM); colours[i].pData = (WCHAR *)colour_names[i]; }
    t_colour.cbSize = sizeof(OPTTYPE); t_colour.Type = TVOT_COMBOBOX; t_colour.Count = 3; t_colour.pOptParam = colours;
    copies[0].cbSize = copies[1].cbSize = sizeof(OPTPARAM);
    copies[0].pData = (WCHAR *)L"copies";
    copies[1].IconID = 1; copies[1].lParam = 99;
    t_copies.cbSize = sizeof(OPTTYPE); t_copies.Type = TVOT_UDARROW; t_copies.Count = 2; t_copies.pOptParam = copies;
    collate[0].cbSize = sizeof(OPTPARAM);
    t_collate.cbSize = sizeof(OPTTYPE); t_collate.Type = TVOT_CHKBOX; t_collate.Count = 1; t_collate.pOptParam = collate;
    t_collate.Style = CHKBOXS_NO_YES;
    name[0].cbSize = name[1].cbSize = sizeof(OPTPARAM);
    name[1].IconID = sizeof(job_name);
    t_name.cbSize = sizeof(OPTTYPE); t_name.Type = TVOT_EDITBOX; t_name.Count = 2; t_name.pOptParam = name;

    for (i = 0; i < 5; i++) tree_items[i].cbSize = sizeof(OPTITEM);
    tree_items[0].pName = (WCHAR *)L"Quality";
    tree_items[1].Level = 1; tree_items[1].pName = (WCHAR *)L"Colour"; tree_items[1].pOptType = &t_colour;
    tree_items[1].Flags = OPTIF_CALLBACK;
    tree_items[2].Level = 1; tree_items[2].pName = (WCHAR *)L"Copies"; tree_items[2].pOptType = &t_copies;
    tree_items[2].Sel = 1;
    tree_items[3].Level = 1; tree_items[3].pName = (WCHAR *)L"Collate"; tree_items[3].pOptType = &t_collate;
    tree_items[4].pName = (WCHAR *)L"Job name"; tree_items[4].pOptType = &t_name; tree_items[4].pSel = job_name;

    tree_ui.cbSize = sizeof(tree_ui);
    tree_ui.Flags = CPSUIF_UPDATE_PERMISSION;
    tree_ui.hInstCaller = GetModuleHandleW( NULL );
    tree_ui.pCallerName = (WCHAR *)L"SG Test Driver";
    tree_ui.UserData = 4242;
    tree_ui.pfnCallBack = tree_callback;
    tree_ui.pOptItem = tree_items;
    tree_ui.cOptItem = 5;
    tree_ui.pDlgPage = CPSUI_PDLGPAGE_TREEVIEWONLY;
    tree_ui.pOptItemName = (WCHAR *)L"SG Printer";

    papers[0].cbSize = papers[1].cbSize = sizeof(OPTPARAM);
    papers[0].pData = (WCHAR *)L"Letter";
    papers[1].pData = (WCHAR *)L"A4";
    t_paper.cbSize = sizeof(OPTTYPE); t_paper.Type = TVOT_COMBOBOX; t_paper.Count = 2; t_paper.pOptParam = papers;
    t_paper.BegCtrlID = 300;
    own_items[0].cbSize = sizeof(OPTITEM);
    own_items[0].pName = (WCHAR *)L"Paper";
    own_items[0].pOptType = &t_paper;
    own_items[0].Flags = OPTIF_CALLBACK;
    {
        static const struct ctl ctls[] =
        {
            { L"BUTTON", BS_GROUPBOX, 300, 7, 7, 200, 50, L"" },
            { L"STATIC", SS_LEFT, 301, 14, 20, 80, 10, L"" },
            { L"COMBOBOX", CBS_DROPDOWNLIST | WS_VSCROLL, 302, 100, 18, 90, 60, NULL },
        };
        make_template( template_own, ctls, 3 );
    }
    own_page.cbSize = sizeof(own_page);
    own_page.Flags = DPF_USE_HDLGTEMPLATE;
    own_page.pTabName = (WCHAR *)L"SG Custom";
    own_page.DlgProc = own_proc;
    own_page.hDlgTemplate = (HANDLE)template_own;
    own_ui = tree_ui;
    own_ui.pfnCallBack = own_callback;
    own_ui.pOptItem = own_items;
    own_ui.cOptItem = 1;
    own_ui.pDlgPage = &own_page;
    own_ui.cDlgPage = 1;
    {
        static const struct ctl plain[] = { { L"STATIC", SS_LEFT, 200, 7, 7, 100, 10, L"plain" } };
        make_template( template_plain, plain, 1 );
        make_template( template_before, plain, 1 );
    }
}

/* a maker's page procedure reads its CPSUI user data at once, as Brother's
 * label driver's does: it is the COMPROPSHEETUI's UserData */
static INT_PTR CALLBACK own_proc( HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam )
{
    if (msg == WM_INITDIALOG) say( "own page userdata %Iu\n", GetCPSUIUserData( hwnd ) );
    return FALSE;
}

static LONG CALLBACK child( PROPSHEETUI_INFO *info, LPARAM lparam )
{
    PROPSHEETPAGEW psp;
    INSERTPSUIPAGE_INFO ins;
    CPSUIDATABLOCK block;
    BYTE data[16];
    HANDLE pages[8], h;
    WCHAR str[64];
    LONG result = 0;

    switch (info->Reason)
    {
    case PROPSHEETUI_REASON_INIT:
        child_handle = info->hComPropSheet;
        compropsheet = info->pfnComPropSheet;
        say( "child init lparam %Id own group %d\n", info->lParamInit, info->hComPropSheet != (HANDLE)lparam );

        memset( &psp, 0, sizeof(psp) );
        psp.dwSize = sizeof(psp);
        psp.dwFlags = PSP_DLGINDIRECT | PSP_USETITLE;
        psp.pResource = (DLGTEMPLATE *)template_plain;
        psp.pszTitle = L"Plain";
        psp.pfnDlgProc = plain_proc;
        plain_page = (HANDLE)info->pfnComPropSheet( info->hComPropSheet, CPSFUNC_ADD_PROPSHEETPAGEW, (LPARAM)&psp, 0 );

        memset( &ins, 0, sizeof(ins) );
        ins.cbSize = sizeof(ins);
        ins.Type = PSUIPAGEINSERT_GROUP_PARENT;
        ins.Mode = INSPSUIPAGE_MODE_LAST_CHILD;
        tree_group = (HANDLE)info->pfnComPropSheet( info->hComPropSheet, CPSFUNC_INSERT_PSUIPAGEW, 0, (LPARAM)&ins );
        h = (HANDLE)info->pfnComPropSheet( tree_group, CPSFUNC_ADD_PCOMPROPSHEETUIW, (LPARAM)&tree_ui, (LPARAM)&result );
        say( "tree compropsheetui %d pages %ld\n", h != NULL, result );

        psp.pResource = (DLGTEMPLATE *)template_before;
        psp.pszTitle = L"Before";
        psp.pfnDlgProc = before_proc;
        ins.Type = PSUIPAGEINSERT_PROPSHEETPAGE;
        ins.Mode = INSPSUIPAGE_MODE_BEFORE;
        ins.dwData1 = (ULONG_PTR)&psp;
        h = (HANDLE)info->pfnComPropSheet( info->hComPropSheet, CPSFUNC_INSERT_PSUIPAGEW, (LPARAM)plain_page,
                                           (LPARAM)&ins );
        say( "inserted before %d\n", h != NULL );

        h = (HANDLE)info->pfnComPropSheet( info->hComPropSheet, CPSFUNC_ADD_PCOMPROPSHEETUIW, (LPARAM)&own_ui,
                                           (LPARAM)&result );
        say( "own compropsheetui %d pages %ld\n", h != NULL, result );

        block.cbData = 5;
        block.pbData = (BYTE *)"hello";
        info->pfnComPropSheet( info->hComPropSheet, CPSFUNC_SET_DATABLOCK, (LPARAM)&block, 5 );
        memset( data, 0, sizeof(data) );
        block.cbData = sizeof(data);
        block.pbData = data;
        say( "datablock %ld %s\n", (LONG)info->pfnComPropSheet( info->hComPropSheet, CPSFUNC_QUERY_DATABLOCK,
             (LPARAM)&block, 5 ), data );
        say( "pagecount %ld\n", (LONG)info->pfnComPropSheet( info->hComPropSheet, CPSFUNC_GET_PAGECOUNT, 0, 0 ) );
        say( "hpsuipages %ld\n", (LONG)info->pfnComPropSheet( info->hComPropSheet, CPSFUNC_GET_HPSUIPAGES,
             (LPARAM)pages, ARRAY_SIZE(pages) ) );
        str[0] = 0;
        info->pfnComPropSheet( info->hComPropSheet, CPSFUNC_LOAD_CPSUI_STRINGW, (LPARAM)str,
                               MAKELPARAM( sizeof(str), IDS_CPSUI_PORTRAIT ) );
        say( "cpsui string %ls\n", str );
        say( "set title %ld\n", (LONG)info->pfnComPropSheet( info->hComPropSheet, CPSFUNC_SET_PSUIPAGE_TITLEW,
             (LPARAM)plain_page, (LPARAM)L"Renamed" ) );
        return 1;
    case PROPSHEETUI_REASON_SET_RESULT:
        child_results++;
        info->Result = ((SETRESULT_INFO *)lparam)->Result;
        return 1;
    }
    return 1;
}

static LONG CALLBACK top( PROPSHEETUI_INFO *info, LPARAM lparam )
{
    switch (info->Reason)
    {
    case PROPSHEETUI_REASON_INIT:
        return info->pfnComPropSheet( info->hComPropSheet, CPSFUNC_ADD_PFNPROPSHEETUIW, (LPARAM)child,
                                      (LPARAM)info->hComPropSheet ) ? 1 : -1;
    case PROPSHEETUI_REASON_GET_INFO_HEADER:
    {
        PROPSHEETUI_INFO_HEADER *h = (PROPSHEETUI_INFO_HEADER *)lparam;
        h->pTitle = (WCHAR *)L"SG Sheet";
        h->Flags = PSUIHDRF_EXACT_PTITLE;
        return 1;
    }
    case PROPSHEETUI_REASON_SET_RESULT:
        top_results++;
        info->Result = ((SETRESULT_INFO *)lparam)->Result;
        return 1;
    }
    return 1;
}

int wmain( void )
{
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES };
    DWORD res = 0xdead;
    LONG ret;

    InitCommonControlsEx( &icc );
    setup_items();
    ret = CommonPropertySheetUIW( NULL, top, 0, &res );
    say( "sheet ret %ld result %lu\n", ret, res );
    say( "applied %d sel_changed %d child_results %d top_results %d\n", applied, sel_changed, child_results,
         top_results );
    say( "colour %ld copies %ld collate hidden %d paper %ld\n", tree_items[1].Sel, tree_items[2].Sel,
         (tree_items[3].Flags & OPTIF_HIDE) != 0, own_items[0].Sel );
    return 0;
}
