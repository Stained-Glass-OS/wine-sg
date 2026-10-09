/* IAccPropServices (patches/sg/2602): every method past QueryInterface
 * returned E_NOTIMPL. Checks the identity-string round trip for both hwnd
 * and hmenu identities, that Set/Clear correctly take and release a
 * registered IAccPropServer, and that overwriting a property releases the
 * old registration rather than leaking it. */
#define COBJMACROS
#include <windows.h>
#include <oleacc.h>
#include <initguid.h>
#include <stdio.h>

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

DEFINE_GUID(prop_test1, 0x11111111, 0, 0, 0,0,0,0,0,0,0,1);
DEFINE_GUID(prop_test2, 0x22222222, 0, 0, 0,0,0,0,0,0,0,2);

struct mock_server
{
    IAccPropServer IAccPropServer_iface;
    LONG refcount;
};

static struct mock_server *impl_from_server(IAccPropServer *iface)
{
    return CONTAINING_RECORD(iface, struct mock_server, IAccPropServer_iface);
}

static HRESULT STDMETHODCALLTYPE server_QI(IAccPropServer *iface, REFIID riid, void **ppv)
{
    if (IsEqualGUID(riid, &IID_IUnknown) || IsEqualGUID(riid, &IID_IAccPropServer))
    {
        *ppv = iface;
        IAccPropServer_AddRef(iface);
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE server_AddRef(IAccPropServer *iface)
{
    return InterlockedIncrement(&impl_from_server(iface)->refcount);
}

static ULONG STDMETHODCALLTYPE server_Release(IAccPropServer *iface)
{
    return InterlockedDecrement(&impl_from_server(iface)->refcount);
}

static HRESULT STDMETHODCALLTYPE server_GetPropValue(IAccPropServer *iface, const BYTE *id,
        DWORD id_len, MSAAPROPID idProp, VARIANT *ret, BOOL *done)
{
    VariantInit(ret);
    *done = FALSE;
    return S_OK;
}

static const IAccPropServerVtbl mock_server_vtbl =
{
    server_QI, server_AddRef, server_Release, server_GetPropValue,
};

int main(void)
{
    IAccPropServices *accprop;
    HRESULT hr;
    BYTE *id1, *id2;
    DWORD len1, len2;
    HWND hwnd, hwnd_out;
    HMENU hmenu, hmenu_out;
    DWORD obj_out, child_out;
    struct mock_server srv1 = { { &mock_server_vtbl }, 1 };
    struct mock_server srv2 = { { &mock_server_vtbl }, 1 };
    VARIANT v;
    MSAAPROPID props[2];

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_AccPropServices, NULL, CLSCTX_INPROC_SERVER,
            &IID_IAccPropServices, (void **)&accprop);
    if (FAILED(hr))
    {
        printf("FAIL  CoCreateInstance(CLSID_AccPropServices) (%#lx)\nRESULT: FAIL\n", hr);
        return 1;
    }

    hwnd = (HWND)(UINT_PTR)0x1234;
    hr = IAccPropServices_ComposeHwndIdentityString(accprop, hwnd, 1, 2, &id1, &len1);
    check(SUCCEEDED(hr) && id1 && len1, "ComposeHwndIdentityString succeeds");
    hr = IAccPropServices_DecomposeHwndIdentityString(accprop, id1, len1, &hwnd_out, &obj_out, &child_out);
    check(SUCCEEDED(hr) && hwnd_out == hwnd && obj_out == 1 && child_out == 2,
          "DecomposeHwndIdentityString round-trips hwnd/idObject/idChild");

    hmenu = (HMENU)(UINT_PTR)0x5678;
    hr = IAccPropServices_ComposeHmenuIdentityString(accprop, hmenu, 3, &id2, &len2);
    check(SUCCEEDED(hr) && id2 && len2, "ComposeHmenuIdentityString succeeds");
    hr = IAccPropServices_DecomposeHmenuIdentityString(accprop, id2, len2, &hmenu_out, &child_out);
    check(SUCCEEDED(hr) && hmenu_out == hmenu && child_out == 3,
          "DecomposeHmenuIdentityString round-trips hmenu/idChild");

    /* An hwnd identity string must not decompose as an hmenu one or vice versa. */
    hr = IAccPropServices_DecomposeHmenuIdentityString(accprop, id1, len1, &hmenu_out, &child_out);
    check(hr == E_INVALIDARG, "an hwnd identity string is rejected by DecomposeHmenuIdentityString");

    V_VT(&v) = VT_I4;
    V_I4(&v) = 42;
    hr = IAccPropServices_SetHwndProp(accprop, hwnd, 1, 2, prop_test1, v);
    check(SUCCEEDED(hr), "SetHwndProp succeeds");

    hr = IAccPropServices_SetHwndPropServer(accprop, hwnd, 1, 2, &prop_test1, 1,
            &srv1.IAccPropServer_iface, ANNO_THIS);
    check(SUCCEEDED(hr), "SetHwndPropServer succeeds");
    check(srv1.refcount == 2, "...and takes a reference on the server (1 + our own)");

    /* Registering a different server for the same identity+property must
     * release the old one, not just leak it. */
    hr = IAccPropServices_SetHwndPropServer(accprop, hwnd, 1, 2, &prop_test1, 1,
            &srv2.IAccPropServer_iface, ANNO_THIS);
    check(SUCCEEDED(hr), "re-registering a server for the same prop succeeds");
    check(srv1.refcount == 1, "...and releases the previous server's registration");
    check(srv2.refcount == 2, "...while taking a reference on the new one");

    props[0] = prop_test1;
    props[1] = prop_test2;
    hr = IAccPropServices_ClearHwndProps(accprop, hwnd, 1, 2, props, 2);
    check(SUCCEEDED(hr), "ClearHwndProps succeeds");
    check(srv2.refcount == 1, "...and releases the registered server");

    /* A second register-then-clear on an hmenu identity, to cover that path too. */
    hr = IAccPropServices_SetHmenuPropServer(accprop, hmenu, 3, &prop_test2, 1,
            &srv1.IAccPropServer_iface, ANNO_THIS);
    check(SUCCEEDED(hr) && srv1.refcount == 2, "SetHmenuPropServer registers and refs the server");
    hr = IAccPropServices_ClearHmenuProps(accprop, hmenu, 3, &prop_test2, 1);
    check(SUCCEEDED(hr) && srv1.refcount == 1, "ClearHmenuProps releases it again");

    CoTaskMemFree(id1);
    CoTaskMemFree(id2);
    IAccPropServices_Release(accprop);
    CoUninitialize();

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
