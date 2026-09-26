/* meetings-probe: what the meeting programs (Zoom, Teams) need at start
 * (patches/sg/0425-0426).
 *
 *  - SetThreadpoolTimerEx and SetThreadpoolWaitEx exist in kernel32 and say
 *    whether a timer or wait was pending; the timer still fires (Zoom);
 *  - Windows.ApplicationModel.LimitedAccessFeatures answers TryUnlockFeature
 *    with the feature id it was asked for, unavailable (Teams).
 *
 * Functions a build may lack are looked up at run time, so the probe runs
 * everywhere. Prints name=value lines; see test/meetings-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <roapi.h>
#include <winstring.h>
#include <stdio.h>

#define SLOT(obj, n, type) ((type)((*(void ***)(obj))[n]))

/* Windows.ApplicationModel.ILimitedAccessFeaturesStatics */
DEFINE_GUID(probe_IID_ILimitedAccessFeaturesStatics, 0x8be612d4, 0x302b, 0x5fbf, 0xa6, 0x32, 0x1a, 0x99, 0xe4, 0x3e, 0x89, 0x25);
/* ILimitedAccessFeatureRequestResult */
DEFINE_GUID(probe_IID_ILimitedAccessFeatureRequestResult, 0xd45156a6, 0x1e24, 0x5ddd, 0xab, 0xb4, 0x61, 0x88, 0xab, 0xa4, 0xd5, 0xbf);

/* IInspectable has 6 slots */
#define STATICS_TRY_UNLOCK  6
#define RESULT_FEATURE_ID   6
#define RESULT_STATUS       7

static LONG fired;

static void CALLBACK timer_cb( TP_CALLBACK_INSTANCE *instance, void *context, TP_TIMER *timer )
{
    InterlockedIncrement( &fired );
}

static void CALLBACK wait_cb( TP_CALLBACK_INSTANCE *instance, void *context, TP_WAIT *wait, TP_WAIT_RESULT result )
{
}

static void check_threadpool( void )
{
    typedef BOOL (WINAPI *timer_ex_fn)( TP_TIMER *, FILETIME *, DWORD, DWORD );
    typedef BOOL (WINAPI *wait_ex_fn)( TP_WAIT *, HANDLE, FILETIME *, void * );
    HMODULE kernel32 = GetModuleHandleW( L"kernel32.dll" );
    timer_ex_fn set_timer_ex = (timer_ex_fn)GetProcAddress( kernel32, "SetThreadpoolTimerEx" );
    wait_ex_fn set_wait_ex = (wait_ex_fn)GetProcAddress( kernel32, "SetThreadpoolWaitEx" );
    LARGE_INTEGER due;
    FILETIME ft;
    TP_TIMER *timer;
    TP_WAIT *wait;
    HANDLE event;
    BOOL a, b, c, d;

    printf( "timer_ex_exported=%d\n", set_timer_ex != NULL );
    printf( "wait_ex_exported=%d\n", set_wait_ex != NULL );

    if (set_timer_ex)
    {
        timer = CreateThreadpoolTimer( timer_cb, NULL, NULL );
        due.QuadPart = -10000000;  /* 1 s */
        ft.dwLowDateTime = due.LowPart;
        ft.dwHighDateTime = due.HighPart;
        a = set_timer_ex( timer, &ft, 0, 0 );      /* nothing pending before */
        b = set_timer_ex( timer, &ft, 0, 0 );      /* replaces a pending one */
        c = set_timer_ex( timer, NULL, 0, 0 );     /* cancels it */
        d = set_timer_ex( timer, NULL, 0, 0 );     /* nothing to cancel */
        printf( "timer_ex_pending=%d (%d %d %d %d)\n", !a && b && c && !d, a, b, c, d );
        due.QuadPart = -500000;    /* 50 ms */
        ft.dwLowDateTime = due.LowPart;
        ft.dwHighDateTime = due.HighPart;
        set_timer_ex( timer, &ft, 0, 0 );
        Sleep( 500 );
        WaitForThreadpoolTimerCallbacks( timer, FALSE );
        printf( "timer_ex_fires=%d (%ld)\n", fired == 1, fired );
        CloseThreadpoolTimer( timer );
    }

    if (set_wait_ex)
    {
        event = CreateEventW( NULL, TRUE, FALSE, NULL );
        wait = CreateThreadpoolWait( wait_cb, NULL, NULL );
        a = set_wait_ex( wait, event, NULL, NULL );
        b = set_wait_ex( wait, event, NULL, NULL );
        c = set_wait_ex( wait, NULL, NULL, NULL );
        d = set_wait_ex( wait, NULL, NULL, NULL );
        printf( "wait_ex_pending=%d (%d %d %d %d)\n", !a && b && c && !d, a, b, c, d );
        WaitForThreadpoolWaitCallbacks( wait, TRUE );
        CloseThreadpoolWait( wait );
        CloseHandle( event );
    }
}

static void check_limited_access( void )
{
    static const WCHAR class_name[] = L"Windows.ApplicationModel.LimitedAccessFeatures";
    static const WCHAR feature[] = L"com.example.probe.feature";
    HSTRING hclass, hfeature, htoken, hattest, id = NULL;
    void *statics = NULL, *result = NULL;
    INT32 status = -1;
    HRESULT hr;

    WindowsCreateString( class_name, (UINT32)wcslen( class_name ), &hclass );
    hr = RoGetActivationFactory( hclass, &probe_IID_ILimitedAccessFeaturesStatics, &statics );
    WindowsDeleteString( hclass );
    printf( "laf_factory=%d (%#lx)\n", SUCCEEDED(hr) && statics, hr );
    if (FAILED( hr )) return;

    WindowsCreateString( feature, (UINT32)wcslen( feature ), &hfeature );
    WindowsCreateString( L"token", 5, &htoken );
    WindowsCreateString( L"attestation", 11, &hattest );
    hr = SLOT( statics, STATICS_TRY_UNLOCK, HRESULT (WINAPI *)( void *, HSTRING, HSTRING, HSTRING, void ** ) )(
            statics, hfeature, htoken, hattest, &result );
    printf( "laf_try_unlock=%d (%#lx)\n", SUCCEEDED(hr) && result, hr );
    if (result)
    {
        SLOT( result, RESULT_FEATURE_ID, HRESULT (WINAPI *)( void *, HSTRING * ) )( result, &id );
        SLOT( result, RESULT_STATUS, HRESULT (WINAPI *)( void *, INT32 * ) )( result, &status );
        printf( "laf_feature_id=%d\n", id && !wcscmp( WindowsGetStringRawBuffer( id, NULL ), feature ) );
        printf( "laf_unavailable=%d (%d)\n", status == 0, status );
        WindowsDeleteString( id );
        IUnknown_Release( (IUnknown *)result );
    }
    WindowsDeleteString( hfeature );
    WindowsDeleteString( htoken );
    WindowsDeleteString( hattest );
    IUnknown_Release( (IUnknown *)statics );
}

int main( void )
{
    RoInitialize( RO_INIT_MULTITHREADED );
    check_threadpool();
    fflush( stdout );
    check_limited_access();
    printf( "done=1\n" );
    fflush( stdout );
    return 0;
}
