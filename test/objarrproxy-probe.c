/* test/objarrproxy-gate.sh's probe (7781): IObjectArray and IObjectCollection cross apartments
 * (actxprxy has their proxies, as on Windows). Office marshals object arrays and logged
 * "CoMarshalInterface failed" for {92ca9dcd-...} without them.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <initguid.h>
#include <shobjidl.h>
#include <stdio.h>

static DWORD main_tid, call_tid;

/* an object array of two items, made in the STA */
static HRESULT WINAPI arr_QueryInterface(IObjectArray *iface, REFIID iid, void **out)
{
    if (IsEqualGUID(iid, &IID_IUnknown) || IsEqualGUID(iid, &IID_IObjectArray)) { *out = iface; return S_OK; }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI arr_AddRef(IObjectArray *iface) { return 2; }
static ULONG WINAPI arr_Release(IObjectArray *iface) { return 1; }
static HRESULT WINAPI arr_GetCount(IObjectArray *iface, UINT *count) { call_tid = GetCurrentThreadId(); *count = 2; return S_OK; }
static HRESULT WINAPI arr_GetAt(IObjectArray *iface, UINT index, REFIID iid, void **out) { *out = NULL; return E_NOINTERFACE; }
static IObjectArrayVtbl arr_vtbl = { arr_QueryInterface, arr_AddRef, arr_Release, arr_GetCount, arr_GetAt };
static IObjectArray arr = { &arr_vtbl };

static IStream *arr_stream, *coll_stream;

static DWORD WINAPI mta_thread(void *arg)
{
    IObjectArray *proxy;
    IObjectCollection *coll;
    UINT count = 0;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    hr = CoGetInterfaceAndReleaseStream(arr_stream, &IID_IObjectArray, (void **)&proxy);
    printf("UnmarshalArray=0x%08lx\n", hr);
    if (SUCCEEDED(hr))
    {
        hr = IObjectArray_GetCount(proxy, &count);
        printf("GetCount=0x%08lx %u\n", hr, count);
        IObjectArray_Release(proxy);
    }
    if (coll_stream)
    {
        hr = CoGetInterfaceAndReleaseStream(coll_stream, &IID_IObjectCollection, (void **)&coll);
        printf("UnmarshalCollection=0x%08lx\n", hr);
        if (SUCCEEDED(hr))
        {
            hr = IObjectCollection_AddObject(coll, (IUnknown *)&arr);
            printf("AddObject=0x%08lx\n", hr);
            IObjectCollection_GetCount(coll, &count);
            printf("CollectionCount=%u\n", count);
            IObjectCollection_Release(coll);
        }
    }
    CoUninitialize();
    return 0;
}

int main(void)
{
    IObjectCollection *coll = NULL;
    HANDLE thread;
    HRESULT hr;
    MSG msg;

    main_tid = GetCurrentThreadId();
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = CoMarshalInterThreadInterfaceInStream(&IID_IObjectArray, (IUnknown *)&arr, &arr_stream);
    printf("MarshalArray=0x%08lx\n", hr);
    if (FAILED(hr)) return 0;
    hr = CoCreateInstance(&CLSID_EnumerableObjectCollection, NULL, CLSCTX_INPROC_SERVER, &IID_IObjectCollection, (void **)&coll);
    printf("CreateCollection=0x%08lx\n", hr);
    if (SUCCEEDED(hr))
    {
        hr = CoMarshalInterThreadInterfaceInStream(&IID_IObjectCollection, (IUnknown *)coll, &coll_stream);
        printf("MarshalCollection=0x%08lx\n", hr);
        if (FAILED(hr)) coll_stream = NULL;
    }
    thread = CreateThread(NULL, 0, mta_thread, NULL, 0, NULL);
    while (MsgWaitForMultipleObjects(1, &thread, FALSE, 10000, QS_ALLINPUT) == WAIT_OBJECT_0 + 1)
        while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
    printf("CalledOnOwnThread=%d\n", call_tid == main_tid);
    if (coll) IObjectCollection_Release(coll);
    CoUninitialize();
    return 0;
}
