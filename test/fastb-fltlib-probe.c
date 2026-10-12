/* fltlib: no filter manager, so no filters, volumes instances or ports */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  %s:%d  ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define E(code) HRESULT_FROM_WIN32(code)
#define FLT_FILTER_NOT_FOUND ((HRESULT)0x801F0013)
#define FLT_VOLUME_NOT_FOUND ((HRESULT)0x801F0014)

#define FN(name) static HRESULT (WINAPI *p##name)()
FN(FilterFindFirst); FN(FilterFindNext); FN(FilterFindClose);
FN(FilterVolumeFindFirst); FN(FilterVolumeFindNext); FN(FilterVolumeFindClose); FN(FilterVolumeClose);
FN(FilterVolumeInstanceFindFirst); FN(FilterVolumeInstanceFindNext); FN(FilterVolumeInstanceFindClose);
FN(FilterInstanceFindFirst); FN(FilterInstanceFindNext); FN(FilterInstanceFindClose);
FN(FilterCreate); FN(FilterClose); FN(FilterGetInformation); FN(FilterInstanceCreate); FN(FilterInstanceClose);
FN(FilterInstanceGetInformation); FN(FilterAttach); FN(FilterAttachAtAltitude); FN(FilterDetach);
FN(FilterGetDosName); FN(FilterSendMessage); FN(FilterGetMessage); FN(FilterReplyMessage);
FN(FilterConnectCommunicationPort); FN(FilterLoad); FN(FilterUnload);

int main(void)
{
    HMODULE m = LoadLibraryA("fltlib.dll");
    HANDLE h = (HANDLE)0x1234, port = (HANDLE)0x77;
    char buf[256];
    DWORD ret = 99;
    WCHAR dos[8], target[MAX_PATH];
    HRESULT hr;

#define LOAD(name) p##name = (void *)GetProcAddress(m, #name); CHECK(p##name != NULL, "export " #name)
    LOAD(FilterFindFirst); LOAD(FilterFindNext); LOAD(FilterFindClose);
    LOAD(FilterVolumeFindFirst); LOAD(FilterVolumeFindNext); LOAD(FilterVolumeFindClose); LOAD(FilterVolumeClose);
    LOAD(FilterVolumeInstanceFindFirst); LOAD(FilterVolumeInstanceFindNext); LOAD(FilterVolumeInstanceFindClose);
    LOAD(FilterInstanceFindFirst); LOAD(FilterInstanceFindNext); LOAD(FilterInstanceFindClose);
    LOAD(FilterCreate); LOAD(FilterClose); LOAD(FilterGetInformation); LOAD(FilterInstanceCreate); LOAD(FilterInstanceClose);
    LOAD(FilterInstanceGetInformation); LOAD(FilterAttach); LOAD(FilterAttachAtAltitude); LOAD(FilterDetach);
    LOAD(FilterGetDosName); LOAD(FilterSendMessage); LOAD(FilterGetMessage); LOAD(FilterReplyMessage);
    LOAD(FilterConnectCommunicationPort); LOAD(FilterLoad); LOAD(FilterUnload);
    if (!m || failures) goto end;

    hr = pFilterFindFirst(1, buf, sizeof(buf), &ret, &h);
    CHECK(hr == E(ERROR_NO_MORE_ITEMS) && ret == 0, "FilterFindFirst %08lx", hr);
    hr = pFilterFindFirst(7, buf, sizeof(buf), &ret, &h);
    CHECK(hr == E_INVALIDARG, "bad class %08lx", hr);
    hr = pFilterFindFirst(1, NULL, 0, &ret, &h);
    CHECK(hr == E_INVALIDARG, "NULL buffer %08lx", hr);
    CHECK(pFilterFindNext(h, 1, buf, sizeof(buf), &ret) == E(ERROR_INVALID_HANDLE), "FindNext");
    CHECK(pFilterFindClose(h) == E(ERROR_INVALID_HANDLE), "FindClose");

    hr = pFilterVolumeFindFirst(1, buf, sizeof(buf), &ret, &h);
    CHECK(hr == E(ERROR_NO_MORE_ITEMS), "FilterVolumeFindFirst %08lx", hr);
    CHECK(pFilterVolumeFindNext(h, 1, buf, sizeof(buf), &ret) == E(ERROR_INVALID_HANDLE), "VolumeFindNext");
    CHECK(pFilterVolumeFindClose(h) == E(ERROR_INVALID_HANDLE), "VolumeFindClose");
    CHECK(pFilterVolumeClose(h) == E(ERROR_INVALID_HANDLE), "VolumeClose");
    hr = pFilterVolumeInstanceFindFirst(L"\\Device\\HarddiskVolume1", 0, buf, sizeof(buf), &ret, &h);
    CHECK(hr == E(ERROR_NO_MORE_ITEMS), "VolumeInstanceFindFirst %08lx", hr);
    CHECK(pFilterVolumeInstanceFindNext(h, 0, buf, sizeof(buf), &ret) == E(ERROR_INVALID_HANDLE), "VolumeInstanceFindNext");
    CHECK(pFilterVolumeInstanceFindClose(h) == E(ERROR_INVALID_HANDLE), "VolumeInstanceFindClose");

    hr = pFilterInstanceFindFirst(L"NoSuchFilter", 0, buf, sizeof(buf), &ret, &h);
    CHECK(hr == FLT_FILTER_NOT_FOUND, "InstanceFindFirst %08lx", hr);
    CHECK(pFilterInstanceFindNext(h, 0, buf, sizeof(buf), &ret) == E(ERROR_INVALID_HANDLE), "InstanceFindNext");
    CHECK(pFilterInstanceFindClose(h) == E(ERROR_INVALID_HANDLE), "InstanceFindClose");

    h = (HANDLE)5;
    hr = pFilterCreate(L"NoSuchFilter", &h);
    CHECK(hr == FLT_FILTER_NOT_FOUND && h == INVALID_HANDLE_VALUE, "FilterCreate %08lx", hr);
    CHECK(pFilterClose(h) == E(ERROR_INVALID_HANDLE), "FilterClose");
    CHECK(pFilterGetInformation(h, 0, buf, sizeof(buf), &ret) == E(ERROR_INVALID_HANDLE), "GetInformation");
    hr = pFilterInstanceCreate(L"NoSuchFilter", L"C:", NULL, &h);
    CHECK(hr == FLT_FILTER_NOT_FOUND, "InstanceCreate %08lx", hr);
    CHECK(pFilterInstanceClose(h) == E(ERROR_INVALID_HANDLE), "InstanceClose");
    CHECK(pFilterInstanceGetInformation(h, 0, buf, sizeof(buf), &ret) == E(ERROR_INVALID_HANDLE), "InstanceGetInformation");
    CHECK(pFilterAttach(L"NoSuchFilter", L"C:", NULL, 0, NULL) == FLT_FILTER_NOT_FOUND, "Attach");
    CHECK(pFilterAttachAtAltitude(L"NoSuchFilter", L"C:", L"100", NULL, 0, NULL) == FLT_FILTER_NOT_FOUND, "AttachAtAltitude");
    CHECK(pFilterDetach(L"NoSuchFilter", L"C:", NULL) == FLT_FILTER_NOT_FOUND, "Detach");
    CHECK(pFilterAttach(NULL, L"C:", NULL, 0, NULL) == E_INVALIDARG, "Attach(NULL)");

    /* volume names */
    CHECK(QueryDosDeviceW(L"C:", target, MAX_PATH) != 0, "C: maps somewhere");
    hr = pFilterGetDosName(target, dos, 8);
    CHECK(hr == S_OK && !wcscmp(dos, L"C:"), "GetDosName %08lx %ls", hr, dos);
    hr = pFilterGetDosName(target, dos, 1);
    CHECK(hr == E(ERROR_INSUFFICIENT_BUFFER), "GetDosName small %08lx", hr);
    hr = pFilterGetDosName(L"\\Device\\NoSuchVolume", dos, 8);
    CHECK(hr == FLT_VOLUME_NOT_FOUND, "GetDosName unknown %08lx", hr);

    /* ports */
    hr = pFilterConnectCommunicationPort(L"\\NoSuchPort", 0, NULL, 0, NULL, &port);
    CHECK(hr == E(ERROR_FILE_NOT_FOUND) && port == INVALID_HANDLE_VALUE, "Connect %08lx", hr);
    hr = pFilterConnectCommunicationPort(NULL, 0, NULL, 0, NULL, &port);
    CHECK(hr == E_INVALIDARG, "Connect(NULL name) %08lx", hr);
    CHECK(pFilterSendMessage(port, buf, 4, buf, 4, &ret) == E(ERROR_INVALID_HANDLE), "SendMessage");
    CHECK(pFilterGetMessage(port, buf, sizeof(buf), NULL) == E(ERROR_INVALID_HANDLE), "GetMessage");
    CHECK(pFilterReplyMessage(port, buf, 4) == E(ERROR_INVALID_HANDLE), "ReplyMessage");

    /* loading and unloading */
    CHECK(pFilterLoad(NULL) == E(ERROR_INVALID_PARAMETER), "Load(NULL)");
    CHECK(pFilterUnload(NULL) == E(ERROR_INVALID_PARAMETER), "Unload(NULL)");
    hr = pFilterLoad(L"NoSuchFilterDriver");
    CHECK(hr == E(ERROR_SERVICE_DOES_NOT_EXIST), "Load unknown %08lx", hr);
    hr = pFilterUnload(L"NoSuchFilterDriver");
    CHECK(hr == E(ERROR_SERVICE_DOES_NOT_EXIST), "Unload unknown %08lx", hr);
    hr = pFilterLoad(L"EventLog");
    CHECK(hr == E(ERROR_NOT_SUPPORTED), "Load a service that is not a filter %08lx", hr);
    hr = pFilterUnload(L"EventLog");
    CHECK(hr == FLT_FILTER_NOT_FOUND, "Unload %08lx", hr);
end:
    printf("%s\n", failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
