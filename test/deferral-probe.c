/* deferral-probe: Windows.Foundation.Deferral (patches/sg/6209).
 *
 * DeferralFactory did not exist: OneDrive (React Native for Windows) makes deferrals and, refused,
 * retried in a loop. Complete runs the handler once and lets it go; Close lets it go unrun.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <stdio.h>

DEFINE_GUID(IID_DeferralFactory, 0x65a1ecc5, 0x3fb5, 0x4832, 0x8c, 0xa9, 0xf0, 0x61, 0xb2, 0x81, 0xd1, 0x3a);
DEFINE_GUID(IID_Closable, 0x30d5a829, 0x7fa4, 0x4026, 0x83, 0xbb, 0xd7, 0x5b, 0xae, 0x4e, 0xa9, 0x9e);

typedef struct { void **vtbl; } obj;
#define CALL(o, n, type) ((type)((o)->vtbl[n]))

/* a DeferralCompletedHandler: Invoke() counted; its references counted */
struct handler { void **vtbl; LONG ref; int calls; };
static HRESULT WINAPI h_qi( struct handler *h, REFIID iid, void **out ) { *out = h; h->ref++; return S_OK; }
static ULONG WINAPI h_addref( struct handler *h ) { return ++h->ref; }
static ULONG WINAPI h_release( struct handler *h ) { return --h->ref; }
static HRESULT WINAPI h_invoke( struct handler *h ) { h->calls++; return S_OK; }
static void *h_vtbl[] = { h_qi, h_addref, h_release, h_invoke };

int main( void )
{
    struct handler a = { h_vtbl, 1 }, b = { h_vtbl, 1 };
    obj *factory = NULL, *d1 = NULL, *d2 = NULL, *closable = NULL;
    HSTRING name;
    HRESULT hr;

    RoInitialize( RO_INIT_MULTITHREADED );
    WindowsCreateString( L"Windows.Foundation.Deferral", 27, &name );
    hr = RoGetActivationFactory( name, &IID_DeferralFactory, (void **)&factory );
    printf( "factory=%08lx\n", hr );
    if (!factory) return 1;
    hr = CALL(factory, 6, HRESULT (WINAPI *)(obj *, void *, obj **))( factory, &a, &d1 );   /* Create */
    printf( "create=%08lx held=%d\n", hr, a.ref > 1 );
    if (!d1) return 1;
    CALL(d1, 6, HRESULT (WINAPI *)(obj *))( d1 );   /* Complete */
    CALL(d1, 6, HRESULT (WINAPI *)(obj *))( d1 );
    printf( "complete=%d released=%d\n", a.calls, a.ref == 1 );

    CALL(factory, 6, HRESULT (WINAPI *)(obj *, void *, obj **))( factory, &b, &d2 );
    CALL(d2, 0, HRESULT (WINAPI *)(obj *, REFIID, void **))( d2, &IID_Closable, (void **)&closable );
    if (closable) CALL(closable, 6, HRESULT (WINAPI *)(obj *))( closable );   /* Close */
    CALL(d2, 6, HRESULT (WINAPI *)(obj *))( d2 );
    printf( "closed=%d,%d released=%d\n", closable != NULL, b.calls, b.ref == 1 );
    hr = CALL(factory, 6, HRESULT (WINAPI *)(obj *, void *, obj **))( factory, NULL, &d2 );
    printf( "no_handler=%08lx\n", hr );
    printf( "done=1\n" );
    return 0;
}
