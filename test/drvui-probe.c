/* The drvui gate's program (patches/sg/1027): opens a printer's
 * preferences (DocumentProperties with DM_IN_PROMPT) or its properties
 * (PrinterProperties) and works them like a user: picks the last paper
 * size on Paper/Quality, the plug-in's "SG Stamp" option in the Advanced
 * tree (or the printer's tray option in Device Settings), and presses OK.
 * One fact a line.  Our own code.
 *   drvui-probe doc PRINTER | cancel PRINTER | dev PRINTER */
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <winspool.h>
#include <commctrl.h>
#include <prsht.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

static BOOL device, cancel;

static BOOL CALLBACK find_sheet( HWND hwnd, LPARAM lparam )
{
    DWORD pid;
    WCHAR cls[32];
    GetWindowThreadProcessId( hwnd, &pid );
    GetClassNameW( hwnd, cls, ARRAY_SIZE(cls) );
    if (pid != GetCurrentProcessId() || wcscmp( cls, L"#32770" ) || !IsWindowVisible( hwnd )) return TRUE;
    *(HWND *)lparam = hwnd;
    return FALSE;
}

/* the combo box that follows a label on a page */
static HWND combo_after( HWND page, const WCHAR *label )
{
    HWND child;
    WCHAR text[128], cls[32];

    for (child = GetWindow( page, GW_CHILD ); child; child = GetWindow( child, GW_HWNDNEXT ))
    {
        GetWindowTextW( child, text, ARRAY_SIZE(text) );
        if (wcscmp( text, label )) continue;
        for (child = GetWindow( child, GW_HWNDNEXT ); child; child = GetWindow( child, GW_HWNDNEXT ))
        {
            GetClassNameW( child, cls, ARRAY_SIZE(cls) );
            if (!wcsicmp( cls, L"ComboBox" )) return child;
        }
        return NULL;
    }
    return NULL;
}

static void choose( HWND page, HWND combo, int index )
{
    SendMessageW( combo, CB_SETCURSEL, index, 0 );
    SendMessageW( page, WM_COMMAND, MAKEWPARAM( GetDlgCtrlID( combo ), CBN_SELCHANGE ), (LPARAM)combo );
}

static HTREEITEM find_item( HWND tree, HTREEITEM item, const WCHAR *prefix, WCHAR *text )
{
    HTREEITEM found;
    TVITEMW tvi;

    for (; item; item = (HTREEITEM)SendMessageW( tree, TVM_GETNEXTITEM, TVGN_NEXT, (LPARAM)item ))
    {
        tvi.mask = TVIF_TEXT;
        tvi.hItem = item;
        tvi.pszText = text;
        tvi.cchTextMax = 128;
        SendMessageW( tree, TVM_GETITEMW, 0, (LPARAM)&tvi );
        if (!wcsncmp( text, prefix, wcslen( prefix ) )) return item;
        if ((found = find_item( tree, (HTREEITEM)SendMessageW( tree, TVM_GETNEXTITEM, TVGN_CHILD, (LPARAM)item ),
                                prefix, text )))
            return found;
    }
    return NULL;
}

/* chooses an option in a treeview page */
static void tree_choose( HWND page, const WCHAR *prefix, int index )
{
    HWND tree = FindWindowExW( page, NULL, WC_TREEVIEWW, NULL ), combo;
    WCHAR text[128];
    HTREEITEM item;

    if (!tree || !(item = find_item( tree, (HTREEITEM)SendMessageW( tree, TVM_GETNEXTITEM, TVGN_ROOT, 0 ), prefix,
                                     text )))
    {
        printf( "tree item %ls missing\n", prefix );
        return;
    }
    printf( "tree item %ls\n", text );
    SendMessageW( tree, TVM_SELECTITEM, TVGN_CARET, (LPARAM)item );
    if ((combo = FindWindowExW( page, NULL, WC_COMBOBOXW, NULL ))) choose( page, combo, index );
    if ((item = find_item( tree, (HTREEITEM)SendMessageW( tree, TVM_GETNEXTITEM, TVGN_ROOT, 0 ), prefix, text )))
        printf( "tree item after %ls\n", text );
}

static DWORD WINAPI work( void *arg )
{
    HWND sheet = NULL, tab, page, combo;
    WCHAR text[128];
    TCITEMW item;
    int i, count;

    for (i = 0; i < 600 && !sheet; i++)
    {
        Sleep( 100 );
        EnumWindows( find_sheet, (LPARAM)&sheet );
    }
    if (!sheet) { printf( "sheet none\n" ); return 0; }
    Sleep( 500 );
    tab = (HWND)SendMessageW( sheet, PSM_GETTABCONTROL, 0, 0 );
    count = SendMessageW( tab, TCM_GETITEMCOUNT, 0, 0 );
    printf( "tabs" );
    for (i = 0; i < count; i++)
    {
        item.mask = TCIF_TEXT;
        item.pszText = text;
        item.cchTextMax = ARRAY_SIZE(text);
        SendMessageW( tab, TCM_GETITEMW, i, (LPARAM)&item );
        printf( " [%ls]", text );
    }
    printf( "\n" );
    if (device)
    {
        SendMessageW( sheet, PSM_SETCURSEL, 0, 0 );
        page = (HWND)SendMessageW( sheet, PSM_INDEXTOHWND, 0, 0 );
        tree_choose( page, L"SG Tray", 1 );
    }
    else
    {
        SendMessageW( sheet, PSM_SETCURSEL, 1, 0 );
        page = (HWND)SendMessageW( sheet, PSM_INDEXTOHWND, 1, 0 );
        if ((combo = combo_after( page, L"Paper Size:" )))
        {
            printf( "paper sizes %d\n", (int)SendMessageW( combo, CB_GETCOUNT, 0, 0 ) );
            choose( page, combo, SendMessageW( combo, CB_GETCOUNT, 0, 0 ) - 1 );   /* the last */
        }
        else printf( "paper size missing\n" );
        SendMessageW( sheet, PSM_SETCURSEL, 2, 0 );
        page = (HWND)SendMessageW( sheet, PSM_INDEXTOHWND, 2, 0 );
        tree_choose( page, L"SG Stamp", 2 );
    }
    PostMessageW( sheet, PSM_PRESSBUTTON, cancel ? PSBTN_CANCEL : PSBTN_OK, 0 );
    return 0;
}

int wmain( int argc, WCHAR **argv )
{
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_WIN95_CLASSES };
    HANDLE h;
    DEVMODEW *dm;
    LONG size, ret;

    setvbuf( stdout, NULL, _IONBF, 0 );
    InitCommonControlsEx( &icc );
    if (argc < 3 || !OpenPrinterW( argv[2], &h, NULL )) return 1;
    /* jobattr PRINTER: what a maker's print processor asks of the job
     * (spoolss GetJobAttributes, answered by the driver) */
    if (!wcscmp( argv[1], L"jobattr" ))
    {
        BOOL (WINAPI *get)( WCHAR *, DEVMODEW *, void * ) =
            (void *)GetProcAddress( LoadLibraryW( L"spoolss.dll" ), "GetJobAttributes" );
        DWORD attr[9] = { 0 };

        size = DocumentPropertiesW( NULL, h, argv[2], NULL, NULL, 0 );
        dm = calloc( 1, size );
        DocumentPropertiesW( NULL, h, argv[2], dm, NULL, DM_OUT_BUFFER );
        if (!get || !get( argv[2], dm, attr )) { printf( "jobattr failed\n" ); return 1; }
        printf( "jobattr pages %lu/%lu order %lu/%lu copies %lu/%lu\n", attr[0], attr[1], attr[3], attr[4], attr[5],
                attr[6] );
        return 0;
    }
    device = !wcscmp( argv[1], L"dev" );
    cancel = !wcscmp( argv[1], L"cancel" );
    CloseHandle( CreateThread( NULL, 0, work, NULL, 0, NULL ) );
    if (device)
    {
        BOOL ok = PrinterProperties( NULL, h );
        DWORD type, needed = 0;
        char buf[512] = "", *p;

        printf( "printerproperties %d\n", ok );
        GetPrinterDataW( h, (WCHAR *)L"SG Printer Features", &type, (BYTE *)buf, sizeof(buf) - 2, &needed );
        for (p = buf; *p; p += strlen( p ) + 1) printf( "printer feature %s\n", p );
    }
    else
    {
        size = DocumentPropertiesW( NULL, h, argv[2], NULL, NULL, 0 );
        dm = calloc( 1, size );
        DocumentPropertiesW( NULL, h, argv[2], dm, NULL, DM_OUT_BUFFER );
        printf( "before paper %d form %ls\n", dm->dmPaperSize, dm->dmFormName );
        ret = DocumentPropertiesW( NULL, h, argv[2], dm, dm, DM_IN_BUFFER | DM_OUT_BUFFER | DM_IN_PROMPT );
        printf( "documentproperties %s\n", ret == IDOK ? "IDOK" : ret == IDCANCEL ? "IDCANCEL" : "error" );
        printf( "after form %ls\n", dm->dmFormName );
        /* the plug-in's part of the devmode: its signature, then its stamp */
        {
            BYTE *p = (BYTE *)dm + dm->dmSize, *end = p + dm->dmDriverExtra;
            for (; p + 16 <= end; p += 4)
                if (*(DWORD *)(p + 4) == 0x49554753 && *(DWORD *)p == 16)
                {
                    printf( "stamp %lu\n", *(DWORD *)(p + 12) );
                    break;
                }
        }
    }
    ClosePrinter( h );
    return 0;
}
