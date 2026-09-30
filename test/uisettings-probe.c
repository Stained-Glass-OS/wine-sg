/* uisettings-probe: Windows.UI.ViewManagement.UISettings and
 * Windows.UI.Core.CoreWindow's statics (patches/sg/0521).  Microsoft
 * OneDrive subscribes to UISettings.TextScaleFactorChanged at start and ended
 * on the stub's E_NOTIMPL (winrt::hresult_not_implemented); its React Native
 * host then asks CoreWindow.GetForCurrentThread() and ended on "class not
 * available".  The probe reads every IUISettings value against the Win32
 * setting it mirrors, and checks each Changed event is raised when its
 * setting changes (and no longer once the handler is removed).  One
 * "name=value" line per check.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <stdio.h>

typedef HRESULT (WINAPI *fn)(void *, ...);
#define CALL(obj, idx, ...) (((fn *)*(void **)(obj))[idx]((obj), ##__VA_ARGS__))

static const GUID IID_IActivationFactory_ = { 0x00000035, 0, 0, { 0xc0, 0, 0, 0, 0, 0, 0, 0x46 } };
static const GUID IID_IUISettings_[6] =
{
    { 0x85361600, 0x1c63, 0x4627, { 0xbc, 0xb1, 0x3a, 0x89, 0xe0, 0xbc, 0x9c, 0x55 } },
    { 0xbad82401, 0x2721, 0x44f9, { 0xbb, 0x91, 0x2b, 0xb2, 0x28, 0xbe, 0x44, 0x2f } },
    { 0x03021be4, 0x5254, 0x4781, { 0x81, 0x94, 0x51, 0x68, 0xf7, 0xd0, 0x6d, 0x7b } },
    { 0x52bb3002, 0x919b, 0x4d6b, { 0x9b, 0x78, 0x8d, 0xd6, 0x6f, 0xf4, 0xb9, 0x3b } },
    { 0x5349d588, 0x0cb5, 0x5f05, { 0xbd, 0x34, 0x70, 0x6b, 0x32, 0x31, 0xf0, 0xbd } },
    { 0xaef19bd7, 0xfe31, 0x5a04, { 0xad, 0xa4, 0x46, 0x9a, 0xae, 0xc6, 0xdf, 0xa9 } },
};
static const GUID IID_ICoreWindowStatic_ = { 0x4d239005, 0x3c2a, 0x41b1, { 0x90, 0x22, 0x53, 0x6b, 0xb9, 0xcf, 0x93, 0xb1 } };

static HRESULT (WINAPI *pRoInitialize)( int );
static HRESULT (WINAPI *pRoGetActivationFactory)( void *, REFIID, void ** );
static HRESULT (WINAPI *pWindowsCreateString)( const WCHAR *, UINT32, void ** );

/* a TypedEventHandler: counts its calls, keeps whether sender and args came */
struct handler
{
    void **vtbl;
    LONG ref;
    HANDLE event;
    LONG calls;
    void *sender;
    void *args;
};

static HRESULT WINAPI h_qi( struct handler *h, REFIID iid, void **out ) { *out = h; InterlockedIncrement( &h->ref ); return S_OK; }
static ULONG WINAPI h_addref( struct handler *h ) { return InterlockedIncrement( &h->ref ); }
static ULONG WINAPI h_release( struct handler *h ) { return InterlockedDecrement( &h->ref ); }
static HRESULT WINAPI h_invoke( struct handler *h, void *sender, void *args )
{
    h->sender = sender;
    h->args = args;
    InterlockedIncrement( &h->calls );
    SetEvent( h->event );
    return S_OK;
}
static void *h_vtbl[] = { h_qi, h_addref, h_release, h_invoke };

static void handler_init( struct handler *h )
{
    memset( h, 0, sizeof(*h) );
    h->vtbl = h_vtbl;
    h->ref = 1;
    h->event = CreateEventW( NULL, FALSE, FALSE, NULL );
}

static void set_dword( const WCHAR *key, const WCHAR *name, DWORD value )
{
    RegSetKeyValueW( HKEY_CURRENT_USER, key, name, REG_DWORD, &value, sizeof(value) );
}

static const WCHAR *accessibility = L"Software\\Microsoft\\Accessibility";
static const WCHAR *personalize = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
static const WCHAR *cpl_access = L"Control Panel\\Accessibility";

int main(void)
{
    HMODULE combase = LoadLibraryW( L"combase.dll" );
    void *factory = NULL, *obj = NULL, *s[6] = { 0 }, *hstr, *unk_obj = NULL, *unk_sender = NULL;
    struct handler h;
    INT64 token;
    double scale = 0;
    UINT32 u = 0;
    BYTE b = 9;
    float size[2];
    BYTE color[4];
    int i, qi = 0;
    HRESULT hr;

    pRoInitialize = (void *)GetProcAddress( combase, "RoInitialize" );
    pRoGetActivationFactory = (void *)GetProcAddress( combase, "RoGetActivationFactory" );
    pWindowsCreateString = (void *)GetProcAddress( combase, "WindowsCreateString" );
    pRoInitialize( 1 );

    /* start from the defaults */
    set_dword( accessibility, L"TextScaleFactor", 100 );
    set_dword( personalize, L"AppsUseLightTheme", 1 );
    set_dword( personalize, L"EnableTransparency", 1 );
    set_dword( cpl_access, L"DynamicScrollbars", 1 );

    pWindowsCreateString( L"Windows.UI.ViewManagement.UISettings", 36, &hstr );
    hr = pRoGetActivationFactory( hstr, &IID_IActivationFactory_, &factory );
    if (FAILED(hr)) { printf( "factory=%08lx\n", hr ); return 0; }
    CALL( factory, 6, &obj );
    for (i = 0; i < 6; i++) if (SUCCEEDED(CALL( obj, 0, &IID_IUISettings_[i], &s[i] ))) qi++;
    printf( "interfaces=%d\n", qi );
    if (qi != 6) return 0;
    CALL( obj, 0, &IID_IUnknown, &unk_obj );

    /* IUISettings: the Win32 settings they mirror */
    hr = CALL( s[0], 14, &u );
    printf( "caretblink=%08lx %d\n", hr, u == GetCaretBlinkTime() );
    hr = CALL( s[0], 16, &u );
    printf( "doubleclick=%08lx %d\n", hr, u == GetDoubleClickTime() );
    hr = CALL( s[0], 8, size );
    printf( "scrollbar=%08lx %d\n", hr, (int)size[0] == GetSystemMetrics( SM_CXVSCROLL ) &&
            (int)size[1] == GetSystemMetrics( SM_CYHSCROLL ) );
    hr = CALL( s[0], 18, 11 /* UIElementType_Window */, color );
    {
        COLORREF c = GetSysColor( COLOR_WINDOW );
        printf( "windowcolor=%08lx %d\n", hr, color[1] == GetRValue( c ) && color[2] == GetGValue( c ) &&
                color[3] == GetBValue( c ) && color[0] == 0xff );
    }
    u = 0;
    hr = CALL( s[0], 11, &u );
    printf( "messageduration=%08lx %d\n", hr, u > 0 );

    /* IUISettings2: TextScaleFactor and its event */
    hr = CALL( s[1], 6, &scale );
    printf( "textscale=%08lx %d\n", hr, (int)(scale * 100 + 0.5) );
    handler_init( &h );
    hr = CALL( s[1], 7, &h, &token );
    set_dword( accessibility, L"TextScaleFactor", 150 );
    WaitForSingleObject( h.event, 5000 );
    CALL( s[1], 6, &scale );
    if (h.sender) CALL( h.sender, 0, &IID_IUnknown, &unk_sender );
    printf( "textscaleevent=%08lx %ld %d %d\n", hr, h.calls, unk_sender == unk_obj, (int)(scale * 100 + 0.5) );
    CALL( s[1], 8, token );
    set_dword( accessibility, L"TextScaleFactor", 125 );
    Sleep( 1500 );
    printf( "textscaleremoved=%ld\n", h.calls );

    /* IUISettings3: ColorValuesChanged on the app mode */
    handler_init( &h );
    hr = CALL( s[2], 7, &h, &token );
    set_dword( personalize, L"AppsUseLightTheme", 0 );
    WaitForSingleObject( h.event, 5000 );
    printf( "colorevent=%08lx %ld\n", hr, h.calls );
    CALL( s[2], 8, token );

    /* IUISettings4: transparency */
    b = 9;
    hr = CALL( s[3], 6, &b );
    printf( "effects=%08lx %d\n", hr, b );
    handler_init( &h );
    CALL( s[3], 7, &h, &token );
    set_dword( personalize, L"EnableTransparency", 0 );
    WaitForSingleObject( h.event, 5000 );
    b = 9;
    CALL( s[3], 6, &b );
    printf( "effectsevent=%ld %d\n", h.calls, b );
    CALL( s[3], 8, token );

    /* IUISettings5: scroll bars, the event with its own args */
    b = 9;
    hr = CALL( s[4], 6, &b );
    printf( "autohide=%08lx %d\n", hr, b );
    handler_init( &h );
    CALL( s[4], 7, &h, &token );
    set_dword( cpl_access, L"DynamicScrollbars", 0 );
    WaitForSingleObject( h.event, 5000 );
    b = 9;
    CALL( s[4], 6, &b );
    printf( "autohideevent=%ld %d %d\n", h.calls, h.args != NULL, b );
    CALL( s[4], 8, token );

    /* IUISettings6: its events register */
    handler_init( &h );
    printf( "settings6=%08lx %08lx\n", CALL( s[5], 6, &h, &token ), CALL( s[5], 7, token ) );

    /* CoreWindow: none for a desktop thread */
    pWindowsCreateString( L"Windows.UI.Core.CoreWindow", 26, &hstr );
    factory = NULL;
    hr = pRoGetActivationFactory( hstr, &IID_ICoreWindowStatic_, &factory );
    {
        void *window = (void *)1;
        HRESULT hr2 = factory ? CALL( factory, 6, &window ) : E_FAIL;
        printf( "corewindow=%08lx %08lx %d\n", hr, hr2, window == NULL );
    }

    set_dword( accessibility, L"TextScaleFactor", 100 );
    set_dword( personalize, L"AppsUseLightTheme", 1 );
    set_dword( personalize, L"EnableTransparency", 1 );
    set_dword( cpl_access, L"DynamicScrollbars", 1 );
    return 0;
}
