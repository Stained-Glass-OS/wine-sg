/* dqweak-probe: a DispatcherQueue can be held weakly (patches/sg/6202).
 *
 * Programs built with C++/WinRT keep their thread's queue as a weak reference (React Native for
 * Windows does, in OneDrive): the queue gave E_NOINTERFACE for IWeakReferenceSource and the program
 * used the null it got back. Checks the weak reference resolves to the queue while it lives, and to
 * nothing once the queue is gone.
 *
 * Prints name=value lines; see test/dqweak-gate.sh.
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
#include <stdio.h>

/* dispatcherqueue.h (mingw's is C++ only) */
typedef enum { DQTYPE_THREAD_DEDICATED = 1, DQTYPE_THREAD_CURRENT = 2 } DISPATCHERQUEUE_THREAD_TYPE;
typedef enum { DQTAT_COM_NONE = 0, DQTAT_COM_ASTA = 1, DQTAT_COM_STA = 2 } DISPATCHERQUEUE_THREAD_APARTMENTTYPE;
typedef struct
{
    DWORD dwSize;
    DISPATCHERQUEUE_THREAD_TYPE threadType;
    DISPATCHERQUEUE_THREAD_APARTMENTTYPE apartmentType;
} DispatcherQueueOptions;

/* IWeakReferenceSource and IWeakReference, by their vtable slots */
static const GUID iid_weak_source = { 0x00000038, 0x0000, 0x0000, { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
typedef HRESULT (WINAPI *get_weak_fn)( IUnknown *, IUnknown ** );
typedef HRESULT (WINAPI *resolve_fn)( IUnknown *, REFIID, IInspectable ** );
#define SLOT(obj, n) (((void **)((IUnknown *)(obj))->lpVtbl)[n])

static HRESULT (WINAPI *pCreateDispatcherQueueController)( DispatcherQueueOptions, IDispatcherQueueController ** );

/* a strong reference to what the weak one names, or NULL */
static IInspectable *resolve( IUnknown *weak, REFIID iid, HRESULT *hr )
{
    IInspectable *out = (IInspectable *)0xdeadbeef;
    *hr = ((resolve_fn)SLOT( weak, 3 ))( weak, iid, &out );
    return out;
}

int main( void )
{
    DispatcherQueueOptions options = { sizeof(options), DQTYPE_THREAD_DEDICATED, DQTAT_COM_STA };
    IDispatcherQueueController *controller = NULL;
    IDispatcherQueue *queue = NULL;
    IUnknown *source = NULL, *weak = NULL, *weak2 = NULL;
    IInspectable *strong;
    IAsyncAction *action = NULL;
    IAsyncInfo *info;
    AsyncStatus status = Started;
    HRESULT hr;
    int i;

    pCreateDispatcherQueueController = (void *)GetProcAddress( LoadLibraryW( L"coremessaging.dll" ),
                                                               "CreateDispatcherQueueController" );
    if (!pCreateDispatcherQueueController || FAILED(pCreateDispatcherQueueController( options, &controller )))
    {
        printf( "controller=0\n" );
        return 1;
    }
    IDispatcherQueueController_get_DispatcherQueue( controller, &queue );

    hr = IDispatcherQueue_QueryInterface( queue, &iid_weak_source, (void **)&source );
    printf( "source=%d\n", hr == S_OK && source != NULL );
    if (!source) return 1;
    hr = ((get_weak_fn)SLOT( source, 3 ))( source, &weak );
    printf( "weak=%d\n", hr == S_OK && weak != NULL );
    ((get_weak_fn)SLOT( source, 3 ))( source, &weak2 );
    IUnknown_Release( source );
    if (!weak) return 1;

    /* while it lives, the weak reference gives the queue (each interface asked for) */
    strong = resolve( weak, &IID_IDispatcherQueue, &hr );
    printf( "live_queue=%d\n", hr == S_OK && strong == (IInspectable *)queue );
    if (hr == S_OK && strong) IInspectable_Release( strong );
    strong = resolve( weak, &IID_IDispatcherQueue2, &hr );
    printf( "live_queue2=%d\n", hr == S_OK && strong != NULL );
    if (hr == S_OK && strong) IInspectable_Release( strong );
    strong = resolve( weak2, &IID_IDispatcherQueue, &hr );
    printf( "second_weak=%d\n", hr == S_OK && strong == (IInspectable *)queue );
    if (hr == S_OK && strong) IInspectable_Release( strong );

    /* shut down, every strong reference let go: it resolves to nothing, without failing */
    IDispatcherQueueController_ShutdownQueueAsync( controller, &action );
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
        IAsyncAction_Release( action );
    }
    IDispatcherQueue_Release( queue );
    IDispatcherQueueController_Release( controller );
    Sleep( 100 );
    strong = resolve( weak, &IID_IDispatcherQueue, &hr );
    printf( "gone=%d\n", hr == S_OK && strong == NULL );
    IUnknown_Release( weak );
    if (weak2) IUnknown_Release( weak2 );
    printf( "done=1\n" );
    return 0;
}
