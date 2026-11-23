/* Windows.Devices.Input.PenDevice (patches/sg/9801): the class Chromium's pen id handler activates
 * (Electron programs: Joplin, Logseq, Tabby, Zulip, Arduino IDE). Its statics answer GetFromPointerId
 * with S_OK and null for a pointer that is no pen with a unique id: the mouse (pointer 1), an unknown
 * pointer id. Prints KEY=VALUE lines. */
#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include <roapi.h>
#include <winstring.h>
#include <inspectable.h>
#include <activation.h>

static const GUID IID_IPenDeviceStatics_ = { 0x9dfbbe01, 0x0966, 0x5180, { 0xbc, 0xb4, 0xb8, 0x50, 0x60, 0xe3, 0x94, 0x79 } };
static const GUID IID_IActivationFactory_ = { 0x00000035, 0x0000, 0x0000, { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
static const GUID IID_IAgileObject_ = { 0x94ea2b94, 0xe9cc, 0x49e0, { 0xc0, 0xff, 0xee, 0x64, 0xca, 0x8f, 0x5b, 0x90 } };

typedef struct statics statics;
struct statics_vtbl
{
    HRESULT (WINAPI *QueryInterface)(statics *, REFIID, void **);
    ULONG (WINAPI *AddRef)(statics *);
    ULONG (WINAPI *Release)(statics *);
    HRESULT (WINAPI *GetIids)(statics *, ULONG *, IID **);
    HRESULT (WINAPI *GetRuntimeClassName)(statics *, HSTRING *);
    HRESULT (WINAPI *GetTrustLevel)(statics *, TrustLevel *);
    HRESULT (WINAPI *GetFromPointerId)(statics *, UINT32, IUnknown **);
};
struct statics { const struct statics_vtbl *lpVtbl; };

int main(void)
{
    static const WCHAR name[] = L"Windows.Devices.Input.PenDevice";
    IActivationFactory *factory = NULL;
    IInspectable *instance = (IInspectable *)1;
    IUnknown *agile = NULL, *pen = (IUnknown *)1;
    statics *st = NULL;
    HSTRING str, cls = NULL;
    HRESULT hr;

    RoInitialize( RO_INIT_MULTITHREADED );
    WindowsCreateString( name, ARRAYSIZE(name) - 1, &str );
    hr = RoGetActivationFactory( str, &IID_IActivationFactory_, (void **)&factory );
    printf( "factory=%#lx\n", hr );
    if (FAILED(hr)) return 1;
    hr = IActivationFactory_QueryInterface( factory, &IID_IPenDeviceStatics_, (void **)&st );
    printf( "statics=%#lx\n", hr );
    if (FAILED(hr)) return 1;
    hr = IActivationFactory_QueryInterface( factory, &IID_IAgileObject_, (void **)&agile );
    printf( "agile=%#lx\n", hr );
    if (agile) IUnknown_Release( agile );
    hr = st->lpVtbl->GetRuntimeClassName( st, &cls );
    printf( "class=%#lx %ls\n", hr, cls ? WindowsGetStringRawBuffer( cls, NULL ) : L"" );
    hr = IActivationFactory_ActivateInstance( factory, &instance );
    printf( "activate=%#lx %d\n", hr, instance == NULL );
    hr = st->lpVtbl->GetFromPointerId( st, 1, &pen );
    printf( "mouse=%#lx %d\n", hr, pen == NULL );
    pen = (IUnknown *)1;
    hr = st->lpVtbl->GetFromPointerId( st, 4242, &pen );
    printf( "unknown=%#lx %d\n", hr, pen == NULL );
    pen = (IUnknown *)1;
    hr = st->lpVtbl->GetFromPointerId( st, 0, &pen );
    printf( "zero=%#lx %d\n", hr, pen == NULL );
    hr = st->lpVtbl->GetFromPointerId( st, 1, NULL );
    printf( "nullout=%#lx\n", hr );
    st->lpVtbl->Release( st );
    IActivationFactory_Release( factory );
    printf( "done=1\n" );
    return 0;
}
