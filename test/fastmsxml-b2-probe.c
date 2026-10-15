#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <msxml2.h>
#include <docobj.h>
#include <perhist.h>
#include <stdio.h>
#define P(...) printf(__VA_ARGS__)
static int fails, checks;
#define CHK(c) do { checks++; if (!(c)) { fails++; P("FAIL line %d: %s\n", __LINE__, #c); } } while (0)
static const GUID IID_IPersistHistory_ = {0x91a565c1,0xe38f,0x11d0,{0x94,0xbf,0x00,0xa0,0xc9,0x05,0x5c,0xbf}};
static const CLSID CLSID_DOM30 = {0xf5078f32,0xc551,0x11d3,{0x89,0xb9,0x00,0x00,0xf8,0x1f,0xe2,0x21}};
static const CLSID CLSID_XMLView_ = {0x48123bc4,0x99d9,0x11d1,{0xa6,0xb3,0x00,0xc0,0x4f,0xd9,0x15,0x55}};

int main(void)
{
    IXMLDOMDocument *doc; IXMLDOMDocumentFragment *frag; IXMLDOMNode *fn; HRESULT hr, h1, h2; VARIANT v;
    IUnknown *view; IPersistMoniker *pm; IOleObject *oo; IOleCommandTarget *ct; IPersistHistory *ph; CLSID clsid; IMoniker *mon;
    DWORD st = 0xdeadbeef; OLECMD cmd;
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    CoCreateInstance(&CLSID_DOM30, NULL, CLSCTX_INPROC_SERVER, &IID_IXMLDOMDocument, (void**)&doc);
    IXMLDOMDocument_createDocumentFragment(doc, &frag);
    IXMLDOMDocumentFragment_QueryInterface(frag, &IID_IXMLDOMNode, (void**)&fn);
    VariantInit(&v); V_VT(&v) = VT_BSTR; V_BSTR(&v) = SysAllocString(L"t");
    h1 = IXMLDOMNode_put_nodeValue(fn, v); h2 = IXMLDOMNode_put_nodeTypedValue(fn, v);
    CHK(h2 == h1);
    CHK(h2 != E_NOTIMPL);
    VariantClear(&v);
    IXMLDOMNode_Release(fn); IXMLDOMDocumentFragment_Release(frag);

    hr = CoCreateInstance(&CLSID_XMLView_, NULL, CLSCTX_INPROC_SERVER, &IID_IUnknown, (void**)&view);
    P("xmlview create %lx\n", hr);
    CHK(hr == S_OK);
    if (hr == S_OK) {
        hr = IUnknown_QueryInterface(view, &IID_IPersistMoniker, (void**)&pm); CHK(hr == S_OK);
        CHK(IPersistMoniker_GetClassID(pm, &clsid) == S_OK && IsEqualGUID(&clsid, &CLSID_XMLView_));
        mon = (IMoniker*)1; hr = IPersistMoniker_GetCurMoniker(pm, &mon); CHK(hr == E_UNEXPECTED);
        hr = IPersistMoniker_IsDirty(pm); CHK(hr != E_NOTIMPL);
        hr = IUnknown_QueryInterface(view, &IID_IOleObject, (void**)&oo); CHK(hr == S_OK);
        hr = IOleObject_GetMiscStatus(oo, DVASPECT_CONTENT, &st); CHK(hr == S_OK && st != 0xdeadbeef);
        hr = IOleObject_Update(oo); CHK(hr != E_NOTIMPL);
        { CLSID uc; hr = IOleObject_GetUserClassID(oo, &uc); CHK(hr == S_OK); }
        hr = IUnknown_QueryInterface(view, &IID_IOleCommandTarget, (void**)&ct); CHK(hr == S_OK);
        cmd.cmdID = OLECMDID_COPY; cmd.cmdf = 0;
        hr = IOleCommandTarget_QueryStatus(ct, NULL, 1, &cmd, NULL); CHK(hr != E_NOTIMPL);
        hr = IUnknown_QueryInterface(view, &IID_IPersistHistory_, (void**)&ph); CHK(hr == S_OK); if (hr != S_OK) goto done;
        hr = IPersistHistory_GetClassID(ph, &clsid); CHK(hr == S_OK);
        IPersistHistory_Release(ph); IOleCommandTarget_Release(ct); IOleObject_Release(oo); IPersistMoniker_Release(pm);
        done: IUnknown_Release(view);
    }
    IXMLDOMDocument_Release(doc);
    P("checks %d fails %d\nRESULT: %s\n", checks, fails, fails ? "FAIL" : "PASS");
    return fails != 0;
}
