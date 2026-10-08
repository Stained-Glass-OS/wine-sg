/* inkdisp-probe: the InkDisp class (patches/sg/1521). OneNote opens
 * HKCR\CLSID\{937C1A34-151D-4610-9CA6-A8CC9BDB5D83} at start; without it it
 * says "You'll need to install the Desktop Experience" and quits. Prints:
 * "key=<1|0> create=<hr> dirty0=<0|-1> dirty1=<..> clone=<hr> clonedirty=<..> disp=<hr>" */
#define COBJMACROS
#include <windows.h>
#include <stdio.h>
#include <msinkaut.h>

static const GUID clsid_inkdisp = {0x937c1a34,0x151d,0x4610,{0x9c,0xa6,0xa8,0xcc,0x9b,0xdb,0x5d,0x83}};
static const GUID iid_inkdisp = {0x9d398fa0,0xc4e2,0x4fcd,{0x99,0x73,0x97,0x5c,0xaa,0xf4,0x7e,0xa6}};

int main(void)
{
    IInkDisp *ink = NULL, *copy = NULL;
    IDispatch *disp = NULL;
    VARIANT_BOOL d0 = 7, d1 = 7, dc = 7;
    HRESULT hr, hrc = E_FAIL, hrd = E_FAIL;
    HKEY key;
    int haskey = !RegOpenKeyExA(HKEY_CLASSES_ROOT,
            "CLSID\\{937C1A34-151D-4610-9CA6-A8CC9BDB5D83}\\InprocServer32", 0, KEY_READ, &key);

    if (haskey) RegCloseKey(key);
    CoInitialize(NULL);
    hr = CoCreateInstance(&clsid_inkdisp, NULL, CLSCTX_INPROC_SERVER, &iid_inkdisp, (void **)&ink);
    if (SUCCEEDED(hr))
    {
        IInkDisp_get_Dirty(ink, &d0);
        IInkDisp_put_Dirty(ink, VARIANT_TRUE);
        IInkDisp_get_Dirty(ink, &d1);
        hrc = IInkDisp_Clone(ink, &copy);
        if (SUCCEEDED(hrc)) { IInkDisp_get_Dirty(copy, &dc); IInkDisp_Release(copy); }
        hrd = IInkDisp_QueryInterface(ink, &IID_IDispatch, (void **)&disp);
        if (SUCCEEDED(hrd)) IDispatch_Release(disp);
        IInkDisp_Release(ink);
    }
    printf("key=%d create=%#lx dirty0=%d dirty1=%d clone=%#lx clonedirty=%d disp=%#lx\n",
           haskey, hr, d0, d1, hrc, dc, hrd);
    CoUninitialize();
    return 0;
}
