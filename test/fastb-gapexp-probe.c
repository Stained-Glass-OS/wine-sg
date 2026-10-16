/* explorer: IShellWindows list/enumeration/pending, the desktop's IWebBrowser2 and IShellBrowser (patch 3615) */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <exdisp.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <servprov.h>
#include <stdio.h>

static int f;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); f++; } } while (0)

/* a minimal IDispatch the probe registers: GetTypeInfoCount answers its marker */
typedef struct { IDispatch iface; LONG ref; UINT marker; } Disp;
static HRESULT WINAPI d_qi(IDispatch *i, REFIID r, void **o)
{
    if (IsEqualGUID(r, &IID_IUnknown) || IsEqualGUID(r, &IID_IDispatch)) { *o = i; IDispatch_AddRef(i); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI d_ar(IDispatch *i) { return InterlockedIncrement(&((Disp *)i)->ref); }
static ULONG WINAPI d_rel(IDispatch *i) { return InterlockedDecrement(&((Disp *)i)->ref); }
static HRESULT WINAPI d_tic(IDispatch *i, UINT *n) { *n = ((Disp *)i)->marker; return S_OK; }
static HRESULT WINAPI d_ti(IDispatch *i, UINT a, LCID l, ITypeInfo **t) { return E_NOTIMPL; }
static HRESULT WINAPI d_ids(IDispatch *i, REFIID r, LPOLESTR *n, UINT c, LCID l, DISPID *d) { return E_NOTIMPL; }
static HRESULT WINAPI d_inv(IDispatch *i, DISPID d, REFIID r, LCID l, WORD w, DISPPARAMS *p, VARIANT *v, EXCEPINFO *e, UINT *a) { return E_NOTIMPL; }
static IDispatchVtbl dvtbl = { d_qi, d_ar, d_rel, d_tic, d_ti, d_ids, d_inv };
static Disp disp1 = { { &dvtbl }, 1, 41 }, disp2 = { { &dvtbl }, 1, 42 };

static UINT marker_of(IDispatch *d)
{
    UINT n = 0;
    return d && SUCCEEDED(IDispatch_GetTypeInfoCount(d, &n)) ? n : 0;
}

static LONG count_of(IShellWindows *sw)
{
    LONG n = -1;
    HRESULT hr = IShellWindows_get_Count(sw, &n);
    return hr == S_OK ? n : -1;
}

int main(void)
{
    IShellWindows *sw = NULL;
    HRESULT hr;
    HWND h1, h2;
    LONG c0, ck1 = 0, ck2 = 0, ckp = 0, ret = 0, n;
    VARIANT v, v2;
    IDispatch *d = NULL;
    IUnknown *unk = NULL;
    IEnumVARIANT *en = NULL, *en2 = NULL;

    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = CoCreateInstance(&CLSID_ShellWindows, NULL, CLSCTX_LOCAL_SERVER, &IID_IShellWindows, (void **)&sw);
    if (hr != S_OK) { printf("FAIL no ShellWindows %#lx\nRESULT: FAIL\n", hr); return 1; }

    h1 = CreateWindowA("static", "w1", WS_POPUP, 0, 0, 10, 10, NULL, NULL, NULL, NULL);
    h2 = CreateWindowA("static", "w2", WS_POPUP, 0, 0, 10, 10, NULL, NULL, NULL, NULL);
    c0 = count_of(sw);
    CHECK(c0 >= 0);

    hr = IShellWindows_Register(sw, &disp1.iface, HandleToLong(h1), SWC_EXPLORER, &ck1);
    CHECK(hr == S_OK && ck1);
    CHECK(count_of(sw) == c0 + 1);
    hr = IShellWindows_Register(sw, NULL, HandleToLong(h2), SWC_EXPLORER, &ck2);
    CHECK(hr == S_OK && ck2);
    CHECK(count_of(sw) == c0 + 1); /* a window without an IDispatch is not listed */

    V_VT(&v) = VT_I4; V_I4(&v) = c0;
    hr = IShellWindows_Item(sw, v, &d);
    CHECK(hr == S_OK && d);
    CHECK(marker_of(d) == 41);
    if (d) IDispatch_Release(d);
    d = (IDispatch *)1;
    V_I4(&v) = 100;
    hr = IShellWindows_Item(sw, v, &d);
    CHECK(hr == S_FALSE && !d);
    d = (IDispatch *)1;
    V_I4(&v) = -1;
    hr = IShellWindows_Item(sw, v, &d);
    CHECK(hr == S_FALSE && !d);

    hr = IShellWindows__NewEnum(sw, &unk);
    CHECK(hr == S_OK && unk);
    if (unk)
    {
        VARIANT item;
        ULONG got = 0;
        int seen = 0;

        IUnknown_QueryInterface(unk, &IID_IEnumVARIANT, (void **)&en);
        CHECK(en != NULL);
        if (en)
        {
            hr = IEnumVARIANT_Clone(en, &en2);
            CHECK(hr == S_OK && en2);
            VariantInit(&item);
            while (IEnumVARIANT_Next(en, 1, &item, &got) == S_OK)
            {
                CHECK(V_VT(&item) == VT_DISPATCH);
                if (marker_of(V_DISPATCH(&item)) == 41) seen++;
                VariantClear(&item);
            }
            CHECK(seen == 1);
            hr = IEnumVARIANT_Next(en, 1, &item, &got);
            CHECK(hr == S_FALSE && got == 0);
            hr = IEnumVARIANT_Reset(en);
            CHECK(hr == S_OK);
            hr = IEnumVARIANT_Skip(en, 1000);
            CHECK(hr == S_FALSE);
            if (en2)
            {
                seen = 0;
                while (IEnumVARIANT_Next(en2, 1, &item, &got) == S_OK) { seen++; VariantClear(&item); }
                CHECK(seen == count_of(sw));
                IEnumVARIANT_Release(en2);
            }
            IEnumVARIANT_Release(en);
        }
        IUnknown_Release(unk);
    }

    /* pending windows are not listed until the object arrives */
    VariantInit(&v); VariantInit(&v2);
    hr = IShellWindows_RegisterPending(sw, GetCurrentThreadId(), &v, &v2, SWC_BROWSER, &ckp);
    CHECK(hr == S_OK && ckp);
    CHECK(count_of(sw) == c0 + 1);
    hr = IShellWindows_OnCreated(sw, ckp, (IUnknown *)&disp2.iface);
    CHECK(hr == S_OK);
    CHECK(count_of(sw) == c0 + 2);
    hr = IShellWindows_OnCreated(sw, 0x7fffff, (IUnknown *)&disp2.iface);
    CHECK(hr == E_INVALIDARG);

    /* by cookie */
    V_VT(&v) = VT_I4; V_I4(&v) = ck1; VariantInit(&v2);
    d = NULL; ret = 0xdead;
    hr = IShellWindows_FindWindowSW(sw, &v, &v2, SWC_EXPLORER, &ret, SWFO_COOKIEPASSED | SWFO_NEEDDISPATCH, &d);
    CHECK(hr == S_OK && ret == HandleToLong(h1) && marker_of(d) == 41);
    if (d) IDispatch_Release(d);
    d = (IDispatch *)1; ret = 0xdead;
    hr = IShellWindows_FindWindowSW(sw, &v, &v2, SWC_BROWSER, &ret, SWFO_COOKIEPASSED, &d);
    CHECK(hr == S_FALSE && ret == 0 && !d);

    hr = IShellWindows_OnActivated(sw, ck1, VARIANT_TRUE);
    CHECK(hr == S_OK);
    hr = IShellWindows_OnActivated(sw, 0x7fffff, VARIANT_TRUE);
    CHECK(hr == E_INVALIDARG);
    hr = IShellWindows_ProcessAttachDetach(sw, VARIANT_TRUE);
    CHECK(hr == S_OK);
    hr = IShellWindows_ProcessAttachDetach(sw, VARIANT_FALSE);
    CHECK(hr == S_OK);

    /* a window that is gone drops out of the list */
    DestroyWindow(h1);
    CHECK(count_of(sw) == c0 + 1);

    hr = IShellWindows_Revoke(sw, ckp);
    CHECK(hr == S_OK);
    hr = IShellWindows_Revoke(sw, ckp);
    CHECK(hr == S_FALSE);
    hr = IShellWindows_Revoke(sw, ck2);
    CHECK(hr == S_OK);
    n = count_of(sw);
    CHECK(n == c0);

    /* the desktop's browser */
    {
        IWebBrowser2 *wb = NULL;
        IServiceProvider *sp = NULL;
        IShellBrowser *sb = NULL;
        BSTR b = NULL;
        VARIANT_BOOL vb;
        LONG l, l2;
        READYSTATE rs;
        VARIANT prop;
        HWND hw = NULL;
        void *x = (void *)1;

        VariantInit(&v);
        hr = IShellWindows_FindWindowSW(sw, &v, &v, SWC_DESKTOP, &ret, SWFO_NEEDDISPATCH, &d);
        CHECK(hr == S_OK && d);
        if (d && SUCCEEDED(IDispatch_QueryInterface(d, &IID_IWebBrowser2, (void **)&wb)))
        {
            hr = IWebBrowser2_get_Name(wb, &b); CHECK(hr == S_OK && b && b[0]); SysFreeString(b); b = NULL;
            hr = IWebBrowser2_get_FullName(wb, &b); CHECK(hr == S_OK && b && StrStrIW(b, L"explorer.exe")); SysFreeString(b); b = NULL;
            hr = IWebBrowser2_get_Path(wb, &b); CHECK(hr == S_OK && b && b[0] && b[wcslen(b) - 1] == '\\'); SysFreeString(b); b = NULL;
            hr = IWebBrowser2_get_LocationName(wb, &b); CHECK(hr == S_OK && b && b[0]); SysFreeString(b); b = NULL;
            hr = IWebBrowser2_get_LocationURL(wb, &b); CHECK(hr == S_OK && b && !wcsncmp(b, L"file:///", 8)); SysFreeString(b); b = NULL;
            hr = IWebBrowser2_get_StatusText(wb, &b); CHECK(hr == S_OK && b && !b[0]); SysFreeString(b);
            hr = IWebBrowser2_get_Visible(wb, &vb); CHECK(hr == S_OK && vb == VARIANT_TRUE);
            hr = IWebBrowser2_put_Visible(wb, VARIANT_FALSE); CHECK(FAILED(hr));
            hr = IWebBrowser2_get_StatusBar(wb, &vb); CHECK(hr == S_OK && vb == VARIANT_FALSE);
            hr = IWebBrowser2_get_Busy(wb, &vb); CHECK(hr == S_OK && vb == VARIANT_FALSE);
            hr = IWebBrowser2_get_ReadyState(wb, &rs); CHECK(hr == S_OK && rs == READYSTATE_COMPLETE);
            hr = IWebBrowser2_get_Width(wb, &l); CHECK(hr == S_OK && l > 0);
            hr = IWebBrowser2_get_Height(wb, &l2); CHECK(hr == S_OK && l2 > 0);
            { SHANDLE_PTR hp = 0; hr = IWebBrowser2_get_HWND(wb, &hp); CHECK(hr == S_OK && hp != 0); }
            hr = IWebBrowser2_put_Offline(wb, VARIANT_TRUE); CHECK(hr == S_OK);
            hr = IWebBrowser2_get_Offline(wb, &vb); CHECK(hr == S_OK && vb == VARIANT_TRUE);
            hr = IWebBrowser2_Refresh(wb); CHECK(hr == S_OK);
            hr = IWebBrowser2_Stop(wb); CHECK(hr == S_OK);
            hr = IWebBrowser2_GoBack(wb); CHECK(FAILED(hr));
            {
                OLECMDF cmdf = 0xff;
                hr = IWebBrowser2_QueryStatusWB(wb, OLECMDID_PRINT, &cmdf); CHECK(hr == S_OK && cmdf == 0);
            }
            {
                BSTR name = SysAllocString(L"sgprop");
                VariantInit(&prop);
                hr = IWebBrowser2_GetProperty(wb, name, &prop); CHECK(hr == S_OK && V_VT(&prop) == VT_EMPTY);
                V_VT(&v) = VT_I4; V_I4(&v) = 1234;
                hr = IWebBrowser2_PutProperty(wb, name, v); CHECK(hr == S_OK);
                hr = IWebBrowser2_GetProperty(wb, name, &prop); CHECK(hr == S_OK && V_VT(&prop) == VT_I4 && V_I4(&prop) == 1234);
                SysFreeString(name);
            }
            if (SUCCEEDED(IWebBrowser2_QueryInterface(wb, &IID_IServiceProvider, (void **)&sp)))
            {
                hr = IServiceProvider_QueryService(sp, &SID_STopLevelBrowser, &IID_IShellBrowser, (void **)&sb);
                CHECK(hr == S_OK && sb);
                if (sb)
                {
                    hr = IShellBrowser_GetWindow(sb, &hw); CHECK(hr == S_OK && IsWindow(hw));
                    hr = IShellBrowser_EnableModelessSB(sb, TRUE); CHECK(hr == S_OK);
                    hr = IShellBrowser_BrowseObject(sb, NULL, 0); CHECK(FAILED(hr));
                    IShellBrowser_Release(sb);
                }
                x = (void *)1;
                hr = IServiceProvider_QueryService(sp, &CLSID_ShellWindows, &IID_IUnknown, &x);
                CHECK(hr == (HRESULT)0x80040200 && !x);
                IServiceProvider_Release(sp);
            }
            IWebBrowser2_Release(wb);
        }
        else CHECK(0 && "desktop IWebBrowser2");
        if (d) IDispatch_Release(d);
    }

    DestroyWindow(h2);
    IShellWindows_Release(sw);
    puts(f ? "RESULT: FAIL" : "RESULT: PASS");
    return f;
}
