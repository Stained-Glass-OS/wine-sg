/* impersonatename-probe.c (wine-sg 6104): a thread impersonating a token of the process's own user asks
 * for its user name, looks its SID up and connects to the Task Scheduler (Zoom's installer's custom
 * action). KEY=VALUE lines; a stack overflow ends the probe before done=1. */
#define COBJMACROS
#include <windows.h>
#include <taskschd.h>
#include <stdio.h>

int main(void)
{
    char buf[sizeof(TOKEN_USER) + SECURITY_MAX_SID_SIZE];
    WCHAR plain[64], name[64], account[64], domain[64];
    DWORD len, account_len = 64, domain_len = 64, got;
    HANDLE token, dup;
    ITaskService *service;
    SID_NAME_USE use;
    VARIANT empty;
    HRESULT hr;
    BOOL ok;

    len = ARRAYSIZE(plain);
    GetUserNameW( plain, &len );
    OpenProcessToken( GetCurrentProcess(), TOKEN_ALL_ACCESS, &token );
    DuplicateTokenEx( token, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenPrimary, &dup );
    printf( "impersonate=%d\n", ImpersonateLoggedOnUser( dup ) );
    fflush( stdout );
    len = ARRAYSIZE(name);
    ok = GetUserNameW( name, &len );
    printf( "username=%d\nsame=%d\n", ok, ok && !wcscmp( name, plain ) );
    fflush( stdout );
    GetTokenInformation( dup, TokenUser, buf, sizeof(buf), &got );
    ok = LookupAccountSidW( NULL, ((TOKEN_USER *)buf)->User.Sid, account, &account_len, domain, &domain_len, &use );
    printf( "lookup=%d\nlookup_same=%d\n", ok, ok && !wcscmp( account, plain ) );
    fflush( stdout );
    CoInitializeEx( NULL, COINIT_MULTITHREADED );
    hr = CoCreateInstance( &CLSID_TaskScheduler, NULL, CLSCTX_INPROC_SERVER, &IID_ITaskService, (void **)&service );
    VariantInit( &empty );
    if (SUCCEEDED(hr)) hr = ITaskService_Connect( service, empty, empty, empty, empty );
    printf( "connect=%#lx\n", hr );
    RevertToSelf();
    printf( "done=1\n" );
    return 0;
}
