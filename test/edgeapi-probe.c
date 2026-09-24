/* The functions Microsoft Edge needed that Wine lacked (patches/sg/0049-0054),
 * each asked directly. Prints KEY=VALUE lines for test/edge-e2e.sh.
 *
 * Built with mingw: x86_64-w64-mingw32-gcc -o edgeapi-probe.exe edgeapi-probe.c
 *                   -luser32 -lpowrprof -lwininet -luserenv -ladvapi32
 * (wofutil is loaded at run time: mingw has no import library for it).
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#include <sddl.h>
#include <stdio.h>

typedef enum { ModeBatterySaver, ModeBetterBattery, ModeBalanced } EFFECTIVE_POWER_MODE_T;
typedef VOID WINAPI power_callback_t( EFFECTIVE_POWER_MODE_T, VOID * );
typedef HRESULT (WINAPI *power_register_t)( ULONG, power_callback_t *, VOID *, VOID ** );
typedef HRESULT (WINAPI *power_unregister_t)( VOID * );
typedef HRESULT (WINAPI *wof_set_t)( HANDLE, ULONG, void *, ULONG );
typedef HRESULT (WINAPI *derive_t)( const WCHAR *, PSID * );
typedef BOOL (WINAPI *arranged_t)( HWND );
typedef BOOL (WINAPI *pen_t)( UINT32, void * );
typedef BOOL (WINAPI *cond_ace_t)( PACL, DWORD, DWORD, UCHAR, DWORD, PSID, WCHAR *, DWORD * );

typedef struct { WCHAR *name, *value, *domain, *path; DWORD flags; FILETIME expires; BOOL expires_set; } cookie2_t;
typedef DWORD (WINAPI *get_cookie2_t)( const WCHAR *, const WCHAR *, DWORD, cookie2_t **, DWORD * );
typedef VOID (WINAPI *free_cookies_t)( cookie2_t *, DWORD );

static HANDLE mode_event;
static LONG mode_seen = -1;

static VOID WINAPI on_mode( EFFECTIVE_POWER_MODE_T mode, VOID *context )
{
    InterlockedExchange( &mode_seen, mode );
    SetEvent( context );
}

int main( void )
{
    HMODULE user32 = LoadLibraryA( "user32.dll" ), powrprof = LoadLibraryA( "powrprof.dll" );
    HMODULE wininet = LoadLibraryA( "wininet.dll" ), userenv = LoadLibraryA( "userenv.dll" );
    HMODULE advapi32 = LoadLibraryA( "advapi32.dll" ), wofutil = LoadLibraryA( "wofutil.dll" );
    arranged_t arranged = (arranged_t)GetProcAddress( user32, "IsWindowArranged" );
    pen_t pen = (pen_t)GetProcAddress( user32, "GetPointerPenInfo" );
    power_register_t reg = (power_register_t)GetProcAddress( powrprof, "PowerRegisterForEffectivePowerModeNotifications" );
    power_unregister_t unreg = (power_unregister_t)GetProcAddress( powrprof, "PowerUnregisterFromEffectivePowerModeNotifications" );
    get_cookie2_t get2 = (get_cookie2_t)GetProcAddress( wininet, "InternetGetCookieEx2" );
    free_cookies_t freec = (free_cookies_t)GetProcAddress( wininet, "InternetFreeCookies" );
    derive_t derive = (derive_t)GetProcAddress( userenv, "DeriveAppContainerSidFromAppContainerName" );
    cond_ace_t cond = (cond_ace_t)GetProcAddress( advapi32, "AddConditionalAce" );
    wof_set_t wof = wofutil ? (wof_set_t)GetProcAddress( wofutil, "WofSetFileDataLocation" ) : NULL;

    printf( "Exports=%d\n", arranged && pen && reg && unreg && get2 && freec && derive && cond && wof );

    if (arranged) printf( "IsWindowArranged=%d\n", arranged( GetDesktopWindow() ) );
    if (pen) { BOOL r; SetLastError( 0 ); r = pen( 1, NULL ); printf( "GetPointerPenInfo=%d,%lu\n", r, GetLastError() ); }

    if (reg && unreg)
    {
        void *handle = NULL;
        HRESULT hr;
        mode_event = CreateEventA( NULL, FALSE, FALSE, NULL );
        hr = reg( 1, on_mode, mode_event, &handle );
        printf( "PowerRegister=0x%08lx\n", (unsigned long)hr );
        if (SUCCEEDED( hr ) && WaitForSingleObject( mode_event, 5000 ) == WAIT_OBJECT_0)
            printf( "PowerMode=%ld\n", mode_seen );
        else printf( "PowerMode=none\n" );
        if (SUCCEEDED( hr )) printf( "PowerUnregister=0x%08lx\n", (unsigned long)unreg( handle ) );
    }

    if (derive)
    {
        PSID sid = NULL;
        char *str = NULL;
        HRESULT hr = derive( L"Microsoft.MicrosoftEdge_8wekyb3d8bbwe", &sid );
        if (SUCCEEDED( hr ) && ConvertSidToStringSidA( sid, &str )) { printf( "AppContainerSid=%s\n", str ); LocalFree( str ); }
        else printf( "AppContainerSid=error 0x%08lx\n", (unsigned long)hr );
        if (sid) FreeSid( sid );
    }

    if (get2 && freec)
    {
        cookie2_t *cookies = NULL;
        DWORD count = 0, res, i;
        InternetSetCookieW( L"http://sgtest.example/", L"sgcookie", L"stained; expires=Sat, 01-Jan-2050 00:00:00 GMT" );
        res = get2( L"http://sgtest.example/", NULL, 0, &cookies, &count );
        printf( "GetCookieEx2=%lu,%lu\n", res, count );
        for (i = 0; i < count; i++)
            printf( "Cookie=%ls=%ls domain=%ls path=%ls expires=%d\n", cookies[i].name, cookies[i].value,
                    cookies[i].domain, cookies[i].path, cookies[i].expires_set );
        freec( cookies, count );
    }

    if (cond)
    {
        BYTE acl_buf[256];
        PACL acl = (PACL)acl_buf;
        BYTE sid_buf[SECURITY_MAX_SID_SIZE];
        DWORD sid_size = sizeof(sid_buf), len = 0;
        BOOL r;
        InitializeAcl( acl, sizeof(acl_buf), ACL_REVISION );
        CreateWellKnownSid( WinWorldSid, NULL, sid_buf, &sid_size );
        SetLastError( 0 );
        r = cond( acl, ACL_REVISION, 0, 9 /* ACCESS_ALLOWED_CALLBACK_ACE_TYPE */, GENERIC_READ, sid_buf,
                  (WCHAR *)L"(Exists WIN://SYSAPPID)", &len );
        printf( "AddConditionalAce=%d,%lu\n", r, GetLastError() );
    }

    if (wof)
    {
        struct { ULONG version, algorithm, flags; } info = { 1, 0, 0 };
        HANDLE file = CreateFileA( "C:\\wof-probe.bin", GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL );
        DWORD written;
        HRESULT hr;
        WriteFile( file, "stained glass", 13, &written, NULL );
        hr = wof( file, 2 /* WOF_PROVIDER_FILE */, &info, sizeof(info) );
        printf( "WofSetFileDataLocation=0x%08lx\n", (unsigned long)hr );
        CloseHandle( file );
        DeleteFileA( "C:\\wof-probe.bin" );
    }
    return 0;
}
