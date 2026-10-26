/* pkgclass-dll: an in-process runtime class server for test/pkgclass-gate.sh (patches/sg/6124).
 * Built twice: the framework package's sgclassfw.dll (-DTAG=\"fw\") and the application package's
 * own sgclassapp.dll (-DTAG=\"app\"). DllGetActivationFactory gives a factory whose runtime class
 * name is "TAG:<the class asked for>".
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <activation.h>
#include <stdio.h>

struct factory
{
    IActivationFactory IActivationFactory_iface;
    LONG ref;
    WCHAR name[256];
};

static HRESULT WINAPI f_QueryInterface(IActivationFactory *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IInspectable) || IsEqualGUID(iid, &IID_IActivationFactory))
    {
        *out = iface;
        IActivationFactory_AddRef(iface);
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI f_AddRef(IActivationFactory *iface)
{
    struct factory *f = CONTAINING_RECORD(iface, struct factory, IActivationFactory_iface);
    return InterlockedIncrement(&f->ref);
}
static ULONG WINAPI f_Release(IActivationFactory *iface)
{
    struct factory *f = CONTAINING_RECORD(iface, struct factory, IActivationFactory_iface);
    LONG ref = InterlockedDecrement(&f->ref);
    if (!ref) HeapFree(GetProcessHeap(), 0, f);
    return ref;
}
static HRESULT WINAPI f_GetIids(IActivationFactory *iface, ULONG *count, IID **iids) { return E_NOTIMPL; }
static HRESULT WINAPI f_GetRuntimeClassName(IActivationFactory *iface, HSTRING *name)
{
    struct factory *f = CONTAINING_RECORD(iface, struct factory, IActivationFactory_iface);
    return WindowsCreateString(f->name, lstrlenW(f->name), name);
}
static HRESULT WINAPI f_GetTrustLevel(IActivationFactory *iface, TrustLevel *level) { *level = BaseTrust; return S_OK; }
static HRESULT WINAPI f_ActivateInstance(IActivationFactory *iface, IInspectable **out) { return E_NOTIMPL; }

static IActivationFactoryVtbl vtbl =
{
    f_QueryInterface, f_AddRef, f_Release, f_GetIids, f_GetRuntimeClassName, f_GetTrustLevel, f_ActivateInstance
};

__declspec(dllexport) HRESULT WINAPI DllGetActivationFactory(HSTRING id, IActivationFactory **out)
{
    struct factory *f = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*f));
    if (!f) return E_OUTOFMEMORY;
    f->IActivationFactory_iface.lpVtbl = &vtbl;
    f->ref = 1;
    swprintf(f->name, 256, L"%hs:%ls", TAG, WindowsGetStringRawBuffer(id, NULL));
    *out = &f->IActivationFactory_iface;
    return S_OK;
}

__declspec(dllexport) HRESULT WINAPI DllCanUnloadNow(void) { return S_FALSE; }
