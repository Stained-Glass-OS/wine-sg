/* dllhost-gate.sh's client: activates the class given as argument out of process and prints the path of the
 * server process, then the class object route. */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <oaidl.h>
#include <stdio.h>
int main(int argc, char **argv)
{
    GUID CLSID_SgSurr;
    WCHAR wclsid[64];
    IDispatch *d = NULL; IClassFactory *cf = NULL; HRESULT hr; DISPID id; OLECHAR *name = (OLECHAR *)L"x"; DISPPARAMS dp = {0}; VARIANT v;
    if (argc < 2) return 2;
    MultiByteToWideChar(CP_ACP, 0, argv[1], -1, wclsid, 64);
    if (FAILED(CLSIDFromString(wclsid, &CLSID_SgSurr))) return 2;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoCreateInstance(&CLSID_SgSurr, NULL, CLSCTX_LOCAL_SERVER, &IID_IDispatch, (void **)&d);
    printf("create %08lx\n", hr);
    if (d) { VariantInit(&v); IDispatch_GetIDsOfNames(d, &IID_NULL, &name, 1, 0, &id); hr = IDispatch_Invoke(d, id, &IID_NULL, 0, DISPATCH_METHOD, &dp, &v, NULL, NULL); printf("invoke %08lx %ls\n", hr, V_VT(&v)==VT_BSTR?V_BSTR(&v):L"-"); IDispatch_Release(d); }
    hr = CoGetClassObject(&CLSID_SgSurr, CLSCTX_LOCAL_SERVER, NULL, &IID_IClassFactory, (void **)&cf);
    printf("getclassobject %08lx\n", hr);
    if (cf) { d = NULL; hr = IClassFactory_CreateInstance(cf, NULL, &IID_IDispatch, (void **)&d); printf("factory-create %08lx\n", hr); if (d) IDispatch_Release(d); IClassFactory_Release(cf); }
    return 0;
}
