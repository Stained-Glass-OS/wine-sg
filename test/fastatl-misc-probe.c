/* atl100: marshalling helpers, error info, device modes, per-user registration */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define VT(obj) (*(void ***)(obj))
#define CALL(obj, idx, ret, ...) (((ret (WINAPI *)(void *, ...))VT(obj)[idx])(obj, ##__VA_ARGS__))

static const GUID clsid_script = {0x0e59f1d5, 0x1fbe, 0x11d0, {0x8f, 0xf2, 0x00, 0xa0, 0xd1, 0x00, 0x38, 0xbc}};
static const GUID iid_test = {0x12345678, 0x1234, 0x1234, {1, 2, 3, 4, 5, 6, 7, 8}};

int main(void)
{
    HMODULE atl = LoadLibraryA("atl100.dll");
    HRESULT (WINAPI *marshal)(IUnknown *, const IID *, IStream **);
    HRESULT (WINAPI *unmarshal)(IStream *, const IID *, IUnknown **);
    HRESULT (WINAPI *free_stream)(IStream *);
    HRESULT (WINAPI *set_error)(const GUID *, const WCHAR *, DWORD, const WCHAR *, const GUID *, HRESULT, HINSTANCE);
    DEVMODEA *(WINAPI *devmode)(DEVMODEA *, const DEVMODEW *);
    HRESULT (WINAPI *set_per_user)(BOOL);
    HRESULT (WINAPI *get_per_user)(BOOL *);
    HRESULT (WINAPI *create_registrar)(void **);
    IStream *obj = NULL, *marshalled = NULL;
    IUnknown *back = NULL;
    IErrorInfo *info = NULL;
    HRESULT hr;

    CoInitialize(NULL);
    CHECK(atl != NULL, "atl100 not loaded");
    if (!atl) return 1;
    marshal = (void *)GetProcAddress(atl, "AtlMarshalPtrInProc");
    unmarshal = (void *)GetProcAddress(atl, "AtlUnmarshalPtr");
    free_stream = (void *)GetProcAddress(atl, "AtlFreeMarshalStream");
    set_error = (void *)GetProcAddress(atl, "AtlSetErrorInfo");
    devmode = (void *)GetProcAddress(atl, "AtlDevModeW2A");
    set_per_user = (void *)GetProcAddress(atl, "AtlSetPerUserRegistration");
    get_per_user = (void *)GetProcAddress(atl, "AtlGetPerUserRegistration");
    create_registrar = (void *)GetProcAddress(atl, "AtlCreateRegistrar");
    CHECK(marshal && unmarshal && free_stream && set_error && devmode && set_per_user && get_per_user && create_registrar, "exports");

    /* marshalling an interface into a stream and back */
    CreateStreamOnHGlobal(NULL, TRUE, &obj);
    hr = marshal((IUnknown *)obj, &IID_IStream, &marshalled);
    CHECK(hr == S_OK && marshalled, "AtlMarshalPtrInProc %lx", hr);
    if (marshalled)
    {
        hr = unmarshal(marshalled, &IID_IStream, &back);
        CHECK(hr == S_OK && back, "AtlUnmarshalPtr %lx", hr);
        if (back)
        {
            IStream *again = NULL;
            hr = IUnknown_QueryInterface(back, &IID_IStream, (void **)&again);
            CHECK(hr == S_OK && again == obj, "same object back: %p vs %p", again, obj);
            if (again) IStream_Release(again);
            IUnknown_Release(back);
        }
        hr = free_stream(marshalled);
        CHECK(hr == S_OK, "AtlFreeMarshalStream %lx", hr);
    }
    hr = unmarshal(NULL, &IID_IStream, &back);
    CHECK(hr == E_INVALIDARG, "AtlUnmarshalPtr(NULL) %lx", hr);
    hr = free_stream(NULL);
    CHECK(hr == S_OK, "AtlFreeMarshalStream(NULL) %lx", hr);
    IStream_Release(obj);

    /* error information */
    hr = set_error(&clsid_script, L"it went wrong", 42, L"C:\\help.hlp", &iid_test, E_FAIL, NULL);
    CHECK(hr == E_FAIL, "AtlSetErrorInfo returned %lx", hr);
    hr = GetErrorInfo(0, &info);
    CHECK(hr == S_OK && info, "GetErrorInfo %lx", hr);
    if (info)
    {
        BSTR text = NULL;
        DWORD ctx = 0;
        GUID guid;

        IErrorInfo_GetDescription(info, &text);
        CHECK(text && !wcscmp(text, L"it went wrong"), "description %ls", text);
        SysFreeString(text); text = NULL;
        IErrorInfo_GetSource(info, &text);
        CHECK(text && !wcscmp(text, L"MSScriptControl.ScriptControl.1"), "source %ls", text);
        SysFreeString(text); text = NULL;
        IErrorInfo_GetHelpFile(info, &text);
        CHECK(text && !wcscmp(text, L"C:\\help.hlp"), "help file %ls", text);
        SysFreeString(text);
        IErrorInfo_GetHelpContext(info, &ctx);
        CHECK(ctx == 42, "help context %lu", ctx);
        IErrorInfo_GetGUID(info, &guid);
        CHECK(IsEqualGUID(&guid, &iid_test), "guid");
        IErrorInfo_Release(info);
    }
    hr = set_error(&clsid_script, L"zero", 0, NULL, &iid_test, S_OK, NULL);
    CHECK(hr == DISP_E_EXCEPTION, "AtlSetErrorInfo(S_OK) %lx", hr);
    SetErrorInfo(0, NULL);

    /* device modes */
    {
        DEVMODEW w = {0};
        DEVMODEA a;

        wcscpy(w.dmDeviceName, L"Test Printer");
        wcscpy(w.dmFormName, L"A4");
        w.dmSize = sizeof(w);
        w.dmSpecVersion = 0x401;
        w.dmPaperSize = 9;
        w.dmCopies = 3;
        w.dmPelsWidth = 640;
        w.dmDisplayFrequency = 75;
        memset(&a, 0xcc, sizeof(a));
        CHECK(devmode(&a, &w) == &a, "AtlDevModeW2A result");
        CHECK(!strcmp((char *)a.dmDeviceName, "Test Printer") && !strcmp((char *)a.dmFormName, "A4"), "names %s %s", a.dmDeviceName, a.dmFormName);
        CHECK(a.dmSize == sizeof(a) && a.dmSpecVersion == 0x401 && a.dmPaperSize == 9 && a.dmCopies == 3, "members");
        CHECK(a.dmPelsWidth == 640 && a.dmDisplayFrequency == 75, "display members");
        CHECK(devmode(&a, NULL) == NULL, "NULL source");
    }

    /* per-user registration: the classes go to the user's own set */
    {
        BOOL enabled = TRUE;
        void *registrar = NULL;
        HKEY key;

        hr = get_per_user(&enabled);
        CHECK(hr == S_OK && !enabled, "default per-user %lx %d", hr, enabled);
        hr = set_per_user(TRUE);
        CHECK(hr == S_OK, "AtlSetPerUserRegistration %lx", hr);
        enabled = FALSE;
        hr = get_per_user(&enabled);
        CHECK(hr == S_OK && enabled, "per-user after set %lx %d", hr, enabled);
        hr = create_registrar(&registrar);
        CHECK(hr == S_OK && registrar, "AtlCreateRegistrar %lx", hr);
        if (registrar)
        {
            hr = CALL(registrar, 9, HRESULT, L"HKCR { ForceRemove SgAtlPerUser { val Test = s 'yes' } }");
            CHECK(hr == S_OK, "StringRegister %lx", hr);
            CHECK(!RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\SgAtlPerUser", 0, KEY_READ, &key), "key not in HKCU\\Software\\Classes");
            if (!RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\SgAtlPerUser", 0, KEY_READ, &key)) RegCloseKey(key);
            CHECK(RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Classes\\SgAtlPerUser", 0, KEY_READ, &key), "key also in HKLM");
            hr = CALL(registrar, 10, HRESULT, L"HKCR { ForceRemove SgAtlPerUser { } }");
            CHECK(hr == S_OK, "StringUnregister %lx", hr);
            CHECK(RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\SgAtlPerUser", 0, KEY_READ, &key), "key left in HKCU");
            CALL(registrar, 2, ULONG);
        }
        set_per_user(FALSE);
    }
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails;
}
