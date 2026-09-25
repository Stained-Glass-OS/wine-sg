/* dispatcherq-probe: Windows.System.DispatcherQueue (patches/sg/0174).
 *
 * CreateDispatcherQueueController was E_NOTIMPL, so programs built on
 * DispatcherQueue -- WinUI/Windows App SDK programs, Paint.NET's installer
 * and program -- stopped at their first line. Checks the calling thread's
 * queue (priority order, other threads' work runs on its thread,
 * HasThreadAccess, GetForCurrentThread), timers, and a dedicated thread's
 * queue with its shutdown events and IAsyncAction.
 *
 * Prints name=value lines; see test/dispatcherq-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_System
#include <windows.h>
#include <initguid.h>
#include <roapi.h>
#include <winstring.h>
#include <windows.foundation.h>
#include <windows.system.h>
/* dispatcherqueue.h (mingw's is C++ only) */
typedef enum { DQTYPE_THREAD_DEDICATED = 1, DQTYPE_THREAD_CURRENT = 2 } DISPATCHERQUEUE_THREAD_TYPE;
typedef enum { DQTAT_COM_NONE = 0, DQTAT_COM_ASTA = 1, DQTAT_COM_STA = 2 } DISPATCHERQUEUE_THREAD_APARTMENTTYPE;
typedef struct
{
    DWORD dwSize;
    DISPATCHERQUEUE_THREAD_TYPE threadType;
    DISPATCHERQUEUE_THREAD_APARTMENTTYPE apartmentType;
} DispatcherQueueOptions;
typedef IDispatcherQueueController *PDISPATCHERQUEUECONTROLLER;
#include <stdio.h>

static HRESULT (WINAPI *pCreateDispatcherQueueController)( DispatcherQueueOptions, PDISPATCHERQUEUECONTROLLER * );
static HRESULT (WINAPI *pRoGetActivationFactory)( HSTRING, REFIID, void ** );
static HRESULT (WINAPI *pWindowsCreateString)( const WCHAR *, UINT32, HSTRING * );

/* a DispatcherQueueHandler that records the order it ran in and its thread */
struct handler
{
    IDispatcherQueueHandler IDispatcherQueueHandler_iface;
    LONG ref;
    char tag;
    DWORD thread;
};

static char order[16];
static int norder;

static HRESULT WINAPI handler_QueryInterface( IDispatcherQueueHandler *iface, REFIID iid, void **out )
{
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, &IID_IDispatcherQueueHandler ) ||
        IsEqualGUID( iid, &IID_IAgileObject ))
    {
        *out = iface;
        IDispatcherQueueHandler_AddRef( iface );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI handler_AddRef( IDispatcherQueueHandler *iface )
{
    return InterlockedIncrement( &((struct handler *)iface)->ref );
}

static ULONG WINAPI handler_Release( IDispatcherQueueHandler *iface )
{
    return InterlockedDecrement( &((struct handler *)iface)->ref );   /* static objects */
}

static HRESULT WINAPI handler_Invoke( IDispatcherQueueHandler *iface )
{
    struct handler *h = (struct handler *)iface;
    h->thread = GetCurrentThreadId();
    if (norder < (int)sizeof(order) - 1) order[norder++] = h->tag;
    return S_OK;
}

static IDispatcherQueueHandlerVtbl handler_vtbl = { handler_QueryInterface, handler_AddRef, handler_Release, handler_Invoke };

/* one TypedEventHandler vtable serves the timer's Tick and the shutdown
 * events: all take two pointers */
struct event_handler
{
    IUnknown IUnknown_iface;   /* vtbl: QI, AddRef, Release, Invoke(sender, args) */
    LONG ref;
    LONG count;
    const GUID *iid;
};

typedef struct
{
    HRESULT (WINAPI *QueryInterface)( IUnknown *, REFIID, void ** );
    ULONG (WINAPI *AddRef)( IUnknown * );
    ULONG (WINAPI *Release)( IUnknown * );
    HRESULT (WINAPI *Invoke)( IUnknown *, IInspectable *, IInspectable * );
} event_vtbl_t;

static HRESULT WINAPI event_QueryInterface( IUnknown *iface, REFIID iid, void **out )
{
    struct event_handler *h = (struct event_handler *)iface;
    if (IsEqualGUID( iid, &IID_IUnknown ) || IsEqualGUID( iid, h->iid ) || IsEqualGUID( iid, &IID_IAgileObject ))
    {
        *out = iface;
        IUnknown_AddRef( iface );
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}

static ULONG WINAPI event_AddRef( IUnknown *iface ) { return InterlockedIncrement( &((struct event_handler *)iface)->ref ); }
static ULONG WINAPI event_Release( IUnknown *iface ) { return InterlockedDecrement( &((struct event_handler *)iface)->ref ); }

static HRESULT WINAPI event_Invoke( IUnknown *iface, IInspectable *sender, IInspectable *args )
{
    struct event_handler *h = (struct event_handler *)iface;
    InterlockedIncrement( &h->count );
    if (h->iid == &IID_ITypedEventHandler_DispatcherQueue_DispatcherQueueShutdownStartingEventArgs && args)
    {
        IDeferral *deferral = NULL;
        IDispatcherQueueShutdownStartingEventArgs_GetDeferral( (IDispatcherQueueShutdownStartingEventArgs *)args, &deferral );
        if (deferral)
        {
            IDeferral_Complete( deferral );
            IDeferral_Release( deferral );
        }
    }
    return S_OK;
}

static event_vtbl_t event_vtbl = { event_QueryInterface, event_AddRef, event_Release, event_Invoke };

static void pump( DWORD ms )
{
    DWORD start = GetTickCount();
    MSG msg;
    while (GetTickCount() - start < ms)
    {
        while (PeekMessageW( &msg, NULL, 0, 0, PM_REMOVE ))
        {
            TranslateMessage( &msg );
            DispatchMessageW( &msg );
        }
        MsgWaitForMultipleObjects( 0, NULL, FALSE, 10, QS_ALLINPUT );
    }
}

static IDispatcherQueue *other_queue;
static boolean other_access, other_enqueued;
static IDispatcherQueue *other_current;
static struct handler from_other = { { &handler_vtbl }, 1, 'O' };

static DWORD WINAPI other_thread( void *arg )
{
    IDispatcherQueue2 *q2;
    IDispatcherQueueStatics *statics = arg;

    IDispatcherQueue_QueryInterface( other_queue, &IID_IDispatcherQueue2, (void **)&q2 );
    IDispatcherQueue2_get_HasThreadAccess( q2, &other_access );
    IDispatcherQueue2_Release( q2 );
    IDispatcherQueue_TryEnqueue( other_queue, &from_other.IDispatcherQueueHandler_iface, &other_enqueued );
    if (statics) IDispatcherQueueStatics_GetForCurrentThread( statics, &other_current );
    return 0;
}

int main( void )
{
    HMODULE cm = LoadLibraryW( L"coremessaging.dll" ), combase = LoadLibraryW( L"combase.dll" );
    DispatcherQueueOptions options = { sizeof(options), DQTYPE_THREAD_CURRENT, DQTAT_COM_NONE };
    IDispatcherQueueController *controller = NULL, *dedicated = NULL, *made = NULL;
    IDispatcherQueueStatics *statics = NULL;
    IDispatcherQueueControllerStatics *cstatics = NULL;
    IDispatcherQueue *queue = NULL, *current = NULL, *dqueue = NULL;
    IDispatcherQueue2 *q2;
    struct handler low = { { &handler_vtbl }, 1, 'L' }, normal = { { &handler_vtbl }, 1, 'N' }, high = { { &handler_vtbl }, 1, 'H' };
    struct handler on_dedicated = { { &handler_vtbl }, 1, 'D' }, late = { { &handler_vtbl }, 1, 'X' };
    struct event_handler tick = { { (IUnknownVtbl *)&event_vtbl }, 1, 0, &IID_ITypedEventHandler_DispatcherQueueTimer_IInspectable };
    struct event_handler once = { { (IUnknownVtbl *)&event_vtbl }, 1, 0, &IID_ITypedEventHandler_DispatcherQueueTimer_IInspectable };
    struct event_handler starting = { { (IUnknownVtbl *)&event_vtbl }, 1, 0, &IID_ITypedEventHandler_DispatcherQueue_DispatcherQueueShutdownStartingEventArgs };
    struct event_handler completed = { { (IUnknownVtbl *)&event_vtbl }, 1, 0, &IID_ITypedEventHandler_DispatcherQueue_IInspectable };
    IDispatcherQueueTimer *timer;
    EventRegistrationToken token;
    IAsyncAction *action = NULL;
    IAsyncInfo *info;
    AsyncStatus status = Started;
    boolean ok1 = 0, ok2 = 0, ok3 = 0, access = 0, running = 1;
    TimeSpan interval;
    HSTRING name;
    HANDLE thread;
    HRESULT hr;
    int i;

    pCreateDispatcherQueueController = (void *)GetProcAddress( cm, "CreateDispatcherQueueController" );
    pRoGetActivationFactory = (void *)GetProcAddress( combase, "RoGetActivationFactory" );
    pWindowsCreateString = (void *)GetProcAddress( combase, "WindowsCreateString" );
    if (!pCreateDispatcherQueueController || !pRoGetActivationFactory) { printf( "exports=0\n" ); return 0; }
    CoInitializeEx( NULL, COINIT_APARTMENTTHREADED );

    hr = pCreateDispatcherQueueController( options, &controller );
    printf( "create_current=%#lx\n", hr );
    if (FAILED(hr)) return 0;
    IDispatcherQueueController_get_DispatcherQueue( controller, &queue );

    /* priority order, from what was queued before the loop ran */
    IDispatcherQueue_TryEnqueueWithPriority( queue, DispatcherQueuePriority_Low, &low.IDispatcherQueueHandler_iface, &ok1 );
    IDispatcherQueue_TryEnqueue( queue, &normal.IDispatcherQueueHandler_iface, &ok2 );
    IDispatcherQueue_TryEnqueueWithPriority( queue, DispatcherQueuePriority_High, &high.IDispatcherQueueHandler_iface, &ok3 );
    pump( 200 );
    printf( "enqueued=%d%d%d order=%s\n", ok1, ok2, ok3, order );

    IDispatcherQueue_QueryInterface( queue, &IID_IDispatcherQueue2, (void **)&q2 );
    IDispatcherQueue2_get_HasThreadAccess( q2, &access );
    IDispatcherQueue2_Release( q2 );

    pWindowsCreateString( L"Windows.System.DispatcherQueue", 30, &name );
    pRoGetActivationFactory( name, &IID_IDispatcherQueueStatics, (void **)&statics );
    if (statics) IDispatcherQueueStatics_GetForCurrentThread( statics, &current );
    printf( "statics=%d current_is_queue=%d\n", !!statics, current == queue );

    /* another thread: no access, its work runs here, no queue of its own */
    other_queue = queue;
    norder = 0; order[0] = 0;
    thread = CreateThread( NULL, 0, other_thread, statics, 0, NULL );
    WaitForSingleObject( thread, 5000 );
    pump( 200 );
    printf( "access=%d other_access=%d other_enqueued=%d ran_here=%d other_has_none=%d\n", access, other_access,
            other_enqueued, from_other.thread == GetCurrentThreadId(), !other_current );

    /* timers */
    IDispatcherQueue_CreateTimer( queue, &timer );
    interval.Duration = 50 * 10000;
    IDispatcherQueueTimer_put_Interval( timer, interval );
    IDispatcherQueueTimer_add_Tick( timer, (void *)&tick, &token );
    IDispatcherQueueTimer_Start( timer );
    pump( 530 );
    IDispatcherQueueTimer_Stop( timer );
    pump( 150 );
    i = tick.count;
    pump( 200 );
    printf( "ticks=%ld stopped=%d\n", tick.count, tick.count == i );
    IDispatcherQueueTimer_Release( timer );

    IDispatcherQueue_CreateTimer( queue, &timer );
    IDispatcherQueueTimer_put_Interval( timer, interval );
    IDispatcherQueueTimer_put_IsRepeating( timer, FALSE );
    IDispatcherQueueTimer_add_Tick( timer, (void *)&once, &token );
    IDispatcherQueueTimer_Start( timer );
    pump( 400 );
    IDispatcherQueueTimer_get_IsRunning( timer, &running );
    printf( "once=%ld running=%d\n", once.count, running );
    IDispatcherQueueTimer_Release( timer );

    /* a second controller for this thread is refused */
    printf( "second_current=%d\n", FAILED( pCreateDispatcherQueueController( options, &made ) ) );

    /* a dedicated thread */
    options.threadType = DQTYPE_THREAD_DEDICATED;
    options.apartmentType = DQTAT_COM_STA;
    hr = pCreateDispatcherQueueController( options, &dedicated );
    printf( "create_dedicated=%#lx\n", hr );
    if (SUCCEEDED(hr))
    {
        IDispatcherQueueController_get_DispatcherQueue( dedicated, &dqueue );
        IDispatcherQueue_add_ShutdownStarting( dqueue, (void *)&starting, &token );
        IDispatcherQueue_add_ShutdownCompleted( dqueue, (void *)&completed, &token );
        IDispatcherQueue_TryEnqueue( dqueue, &on_dedicated.IDispatcherQueueHandler_iface, &ok1 );
        for (i = 0; i < 100 && !on_dedicated.thread; i++) Sleep( 20 );
        printf( "dedicated_ran=%d own_thread=%d\n", !!on_dedicated.thread, on_dedicated.thread && on_dedicated.thread != GetCurrentThreadId() );

        IDispatcherQueueController_ShutdownQueueAsync( dedicated, &action );
        if (action)
        {
            IAsyncAction_QueryInterface( action, &IID_IAsyncInfo, (void **)&info );
            for (i = 0; i < 100; i++)
            {
                IAsyncInfo_get_Status( info, &status );
                if (status != Started) break;
                Sleep( 20 );
            }
            IAsyncInfo_Release( info );
        }
        ok1 = 1;
        IDispatcherQueue_TryEnqueue( dqueue, &late.IDispatcherQueueHandler_iface, &ok1 );
        printf( "shutdown=%d starting=%ld completed=%ld refused_after=%d\n", status == Completed, starting.count,
                completed.count, !ok1 );
    }

    pWindowsCreateString( L"Windows.System.DispatcherQueueController", 40, &name );
    pRoGetActivationFactory( name, &IID_IDispatcherQueueControllerStatics, (void **)&cstatics );
    made = NULL;
    if (cstatics) IDispatcherQueueControllerStatics_CreateOnDedicatedThread( cstatics, &made );
    printf( "on_dedicated_thread=%d\n", !!made );
    return 0;
}
