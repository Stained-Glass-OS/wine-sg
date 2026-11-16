/* For test/comrestrict-gate.sh: COM classes from a sandboxed process with a
 * per-user install's HKCU\Software\Classes (Chrome's audio service, David's
 * box 2026-11-15: no sound).
 *   setup        writes a per-user install's classes under HKCU\Software\Classes
 *   run          starts "child" with a token as Chromium's USER_LIMITED
 *                (restricting SIDs Users, Everyone, RESTRICTED, logon; every
 *                other group deny-only; no privileges) at low integrity
 *   child        what that process sees, a line each
 *   systemcom    (as SYSTEM) takes Wine's admin token, as the shell's desktop
 *                process on the SYSTEM account does, then uses COM
 *   sd           the machine's Software\Classes descriptor (SDDL) */
#define COBJMACROS
#include <windows.h>
#include <sddl.h>
#include <aclapi.h>
#include <winternl.h>
#include <initguid.h>
#include <mmdeviceapi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define ARRAY_SIZE(x) (sizeof(x)/sizeof((x)[0]))

static void child(void)
{
    HKEY key;
    LONG r;
    HRESULT hr;
    IMMDeviceEnumerator *e = NULL;

    r = RegOpenKeyExW( HKEY_CLASSES_ROOT, L"CLSID\\{BCDE0395-E52F-467C-8E3D-C4579291692E}\\InprocServer32", 0, KEY_READ, &key );
    printf( "hkcr_clsid=%ld\n", r );
    if (!r) RegCloseKey( key );
    r = RegOpenKeyExW( HKEY_CURRENT_USER, L"Software\\Classes", 0, KEY_READ, &key );
    printf( "hkcu_classes=%ld\n", r );
    if (!r) RegCloseKey( key );
    r = RegOpenKeyExW( HKEY_CLASSES_ROOT, L"SgPerUserApp.Document", 0, KEY_READ, &key );
    printf( "hkcr_user_progid=%ld\n", r );
    if (!r) RegCloseKey( key );
    CoInitializeEx( NULL, COINIT_MULTITHREADED );
    hr = CoCreateInstance( &CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &IID_IMMDeviceEnumerator, (void **)&e );
    printf( "mmdevenum=%#lx\n", hr );
    if (e) IMMDeviceEnumerator_Release( e );
    CoUninitialize();
}

static void setup(void)
{
    static const WCHAR *keys[] = {
        L"Software\\Classes\\SgPerUserApp.Document\\shell\\open\\command",
        L"Software\\Classes\\CLSID\\{5A1E3F70-1D2B-4C3E-9F4A-0B1C2D3E4F50}\\LocalServer32",
        L"Software\\Classes\\AppID\\{5A1E3F70-1D2B-4C3E-9F4A-0B1C2D3E4F50}",
        L"Software\\Classes\\Interface\\{5A1E3F71-1D2B-4C3E-9F4A-0B1C2D3E4F50}",
        NULL };
    int i;
    for (i = 0; keys[i]; i++)
    {
        HKEY k;
        LONG r = RegCreateKeyExW( HKEY_CURRENT_USER, keys[i], 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL );
        printf( "create %ls -> %ld\n", keys[i], r );
        if (!r) RegCloseKey( k );
    }
}

static PSID well_known( WELL_KNOWN_SID_TYPE type )
{
    DWORD size = SECURITY_MAX_SID_SIZE;
    PSID sid = LocalAlloc( LPTR, size );
    if (!CreateWellKnownSid( type, NULL, sid, &size )) return NULL;
    return sid;
}

static void run(void)
{
    HANDLE token, restricted;
    static BYTE buf[8192];
    TOKEN_GROUPS *groups = (TOKEN_GROUPS *)buf;
    SID_AND_ATTRIBUTES disable[64], restrict_sids[4];
    DWORD len, i, ndisable = 0, nrestrict = 0;
    PSID users = well_known( WinBuiltinUsersSid ), world = well_known( WinWorldSid ),
         restr = well_known( WinRestrictedCodeSid ), interactive = well_known( WinInteractiveSid ),
         auth = well_known( WinAuthenticatedUserSid ), low = NULL;
    STARTUPINFOW si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    WCHAR cmd[MAX_PATH + 16], self[MAX_PATH];
    TOKEN_MANDATORY_LABEL label = {{ 0 }};

    if (!OpenProcessToken( GetCurrentProcess(), TOKEN_ALL_ACCESS, &token )) { printf( "token failed\n" ); return; }
    GetTokenInformation( token, TokenGroups, buf, sizeof(buf), &len );
    restrict_sids[nrestrict++].Sid = users;
    restrict_sids[nrestrict++].Sid = world;
    restrict_sids[nrestrict++].Sid = restr;
    for (i = 0; i < groups->GroupCount && ndisable < 64; i++)
    {
        PSID s = groups->Groups[i].Sid;
        if ((groups->Groups[i].Attributes & SE_GROUP_LOGON_ID) && nrestrict < 4) { restrict_sids[nrestrict++].Sid = s; continue; }
        if (EqualSid( s, users ) || EqualSid( s, world ) || EqualSid( s, interactive ) || EqualSid( s, auth )) continue;
        if (groups->Groups[i].Attributes & SE_GROUP_INTEGRITY) continue;
        disable[ndisable++].Sid = s;
    }
    for (i = 0; i < nrestrict; i++) restrict_sids[i].Attributes = 0;
    for (i = 0; i < ndisable; i++) disable[i].Attributes = 0;
    if (!CreateRestrictedToken( token, DISABLE_MAX_PRIVILEGE, ndisable, disable, 0, NULL, nrestrict, restrict_sids, &restricted ))
    {
        printf( "CreateRestrictedToken failed %lu\n", GetLastError() );
        return;
    }
    if (getenv( "COMR_LOW" ))
    {
        ConvertStringSidToSidW( L"S-1-16-4096", &low );
        label.Label.Attributes = SE_GROUP_INTEGRITY;
        label.Label.Sid = low;
        SetTokenInformation( restricted, TokenIntegrityLevel, &label, sizeof(label) + GetLengthSid( low ) );
    }
    if (getenv( "COMR_PLAIN" )) restricted = token;
    GetModuleFileNameW( NULL, self, MAX_PATH );
    swprintf( cmd, ARRAY_SIZE(cmd), L"\"%ls\" child", self );
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle( STD_INPUT_HANDLE );
    si.hStdOutput = si.hStdError = GetStdHandle( STD_OUTPUT_HANDLE );
    if (!CreateProcessAsUserW( restricted, NULL, cmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi ))
    {
        printf( "CreateProcessAsUser failed %lu\n", GetLastError() );
        return;
    }
    WaitForSingleObject( pi.hProcess, 60000 );
    CloseHandle( pi.hProcess );
    CloseHandle( pi.hThread );
}

/* the machine's Classes key's descriptor, as SDDL */
static void classes_sd(void)
{
    HKEY k;
    PSECURITY_DESCRIPTOR psd = NULL;
    LPWSTR str = NULL;
    if (RegOpenKeyExW( HKEY_LOCAL_MACHINE, L"Software\\Classes", 0, READ_CONTROL, &k )) { printf( "classes_sd=(open failed)\n" ); return; }
    if (!GetSecurityInfo( k, SE_REGISTRY_KEY, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, NULL, NULL, NULL, NULL, &psd ))
        ConvertSecurityDescriptorToStringSecurityDescriptorW( psd, SDDL_REVISION_1, OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &str, NULL );
    printf( "classes_sd=%ls\n", str ? str : L"(none)" );
    RegCloseKey( k );
}

/* as the shell's desktop process does on the SYSTEM account (an elevated program's display):
 * Wine's admin token for it (ProcessWineGrantAdminToken), then COM */
static void system_com(void)
{
    NTSTATUS (WINAPI *pNtSetInformationProcess)( HANDLE, PROCESSINFOCLASS, void *, ULONG ) =
        (void *)GetProcAddress( GetModuleHandleW( L"ntdll.dll" ), "NtSetInformationProcess" );
    IMMDeviceEnumerator *e = NULL;
    HRESULT hr;
    printf( "grant=%#lx\n", pNtSetInformationProcess( GetCurrentProcess(), (PROCESSINFOCLASS)1002, NULL, 0 ) );
    CoInitializeEx( NULL, COINIT_MULTITHREADED );
    hr = CoCreateInstance( &CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &IID_IMMDeviceEnumerator, (void **)&e );
    printf( "system_mmdevenum=%#lx\n", hr );
    if (e) IMMDeviceEnumerator_Release( e );
    CoUninitialize();
    classes_sd();
}

int main( int argc, char **argv )
{
    setvbuf( stdout, NULL, _IONBF, 0 );
    if (argc < 2) return 2;
    if (!strcmp( argv[1], "child" )) child();
    else if (!strcmp( argv[1], "setup" )) setup();
    else if (!strcmp( argv[1], "run" )) run();
    else if (!strcmp( argv[1], "systemcom" )) system_com();
    else if (!strcmp( argv[1], "sd" )) classes_sd();
    else return 2;
    return 0;
}
