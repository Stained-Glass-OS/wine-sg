/* propsys long tail (patches/sg/2000), run by test/propkeyname-gate.sh:
 * PSGetPropertyKeyFromName (a canonical-name -> PROPERTYKEY table),
 * PSRegisterPropertySchema/PSUnregisterPropertySchema/PSRefreshPropertySchema
 * (registry-backed bookkeeping, no FIXME spam), PropertyStore_Commit (a
 * correct no-op on the in-memory store), and propvar.c's PROPVAR_ConvertNumber
 * (VT_R4/VT_BOOL), PropVariantToBuffer (scalar numerics), PropVariantToGUID
 * (VT_LPSTR), VariantToGUID (VT_LPWSTR/VT_BYREF|VT_BSTR) and VariantToString
 * (any formattable type, via VariantChangeType). These were E_NOTIMPL stubs
 * or FIXME-and-succeed fake stubs.
 *
 *   propkeyname-probe.exe */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <propkey.h>
#include <propvarutil.h>
#include <propsys.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

int main(void)
{
    HMODULE propsys = LoadLibraryA("propsys.dll");
    HRESULT (WINAPI *pPSGetPropertyKeyFromName)(PCWSTR, PROPERTYKEY *) =
        (void *)GetProcAddress(propsys, "PSGetPropertyKeyFromName");
    HRESULT (WINAPI *pPSRegisterPropertySchema)(PCWSTR) =
        (void *)GetProcAddress(propsys, "PSRegisterPropertySchema");
    HRESULT (WINAPI *pPSUnregisterPropertySchema)(PCWSTR) =
        (void *)GetProcAddress(propsys, "PSUnregisterPropertySchema");
    HRESULT (WINAPI *pPSRefreshPropertySchema)(void) =
        (void *)GetProcAddress(propsys, "PSRefreshPropertySchema");
    PROPERTYKEY key;
    HRESULT hr;
    HKEY hkey;
    IPropertyStoreCache *cache;
    PROPVARIANT pv;
    GUID guid;
    WCHAR buf[64];
    VARIANT var;
    BYTE bbuf[8];

    CoInitialize(NULL);

    check(pPSGetPropertyKeyFromName != NULL, "PSGetPropertyKeyFromName is there");
    if (pPSGetPropertyKeyFromName)
    {
        hr = pPSGetPropertyKeyFromName(L"System.ItemName", &key);
        check(hr == S_OK && IsEqualPropertyKey(key, PKEY_ItemName),
              "System.ItemName resolves to PKEY_ItemName");

        hr = pPSGetPropertyKeyFromName(L"system.itemname", &key);
        check(hr == S_OK && IsEqualPropertyKey(key, PKEY_ItemName),
              "lookup is case-insensitive");

        hr = pPSGetPropertyKeyFromName(L"System.Title", &key);
        check(hr == S_OK && IsEqualPropertyKey(key, PKEY_Title), "System.Title resolves to PKEY_Title");

        hr = pPSGetPropertyKeyFromName(L"System.Nonexistent.Nothing", &key);
        check(hr == E_INVALIDARG, "an unknown canonical name is E_INVALIDARG");
    }

    /* PSRegisterPropertySchema/PSUnregisterPropertySchema: real registry
     * bookkeeping, not a fake success/E_NOTIMPL pair. */
    check(pPSRegisterPropertySchema && pPSUnregisterPropertySchema, "register/unregister are there");
    if (pPSRegisterPropertySchema && pPSUnregisterPropertySchema)
    {
        static const WCHAR schema_path[] = L"C:\\sg-probe-schema.propdesc";
        static const WCHAR key_path[] =
            L"Software\\Microsoft\\Windows\\CurrentVersion\\PropertySystem\\PropertySchema";

        hr = pPSUnregisterPropertySchema(schema_path);
        check(hr == S_FALSE, "unregistering a never-registered schema is S_FALSE");

        hr = pPSRegisterPropertySchema(schema_path);
        check(hr == S_OK, "PSRegisterPropertySchema succeeds");

        hr = RegOpenKeyExW(HKEY_LOCAL_MACHINE, key_path, 0, KEY_READ, &hkey) == 0 ? S_OK : E_FAIL;
        if (hr == S_OK)
        {
            DWORD type, size = 0;
            LSTATUS ls = RegQueryValueExW(hkey, schema_path, NULL, &type, NULL, &size);
            check(ls == ERROR_SUCCESS, "registering persisted it to the registry");
            RegCloseKey(hkey);
        }
        else check(0, "PropertySchema key exists after registering");

        hr = pPSUnregisterPropertySchema(schema_path);
        check(hr == S_OK, "unregistering a registered schema is S_OK");
        hr = pPSUnregisterPropertySchema(schema_path);
        check(hr == S_FALSE, "unregistering it again is S_FALSE");
    }
    check(pPSRefreshPropertySchema && pPSRefreshPropertySchema() == S_OK, "PSRefreshPropertySchema succeeds");

    /* PropertyStore_Commit, through CLSID_InMemoryPropertyStore. */
    hr = CoCreateInstance(&CLSID_InMemoryPropertyStore, NULL, CLSCTX_INPROC_SERVER,
            &IID_IPropertyStoreCache, (void **)&cache);
    check(hr == S_OK, "in-memory property store created");
    if (hr == S_OK)
    {
        hr = IPropertyStoreCache_Commit(cache);
        check(hr == S_OK, "PropertyStore_Commit succeeds");
        IPropertyStoreCache_Release(cache);
    }

    /* PROPVAR_ConvertNumber via PropVariantToInt32, VT_R4 and VT_BOOL. */
    {
        LONG out;
        PropVariantInit(&pv);
        pv.vt = VT_R4;
        pv.fltVal = 42.0f;
        hr = PropVariantToInt32(&pv, &out);
        check(hr == S_OK && out == 42, "PropVariantToInt32 converts VT_R4");

        pv.vt = VT_BOOL;
        pv.boolVal = VARIANT_TRUE;
        hr = PropVariantToInt32(&pv, &out);
        check(hr == S_OK && out == 1, "PropVariantToInt32 converts VT_BOOL");
    }

    /* PropVariantToBuffer: scalar numeric types, not just VT_VECTOR|VT_UI1. */
    {
        PropVariantInit(&pv);
        pv.vt = VT_UI4;
        pv.ulVal = 0x11223344;
        memset(bbuf, 0xcc, sizeof(bbuf));
        hr = PropVariantToBuffer(&pv, bbuf, sizeof(DWORD));
        check(hr == S_OK && *(DWORD *)bbuf == 0x11223344, "PropVariantToBuffer copies a VT_UI4's bytes");

        pv.vt = VT_UI8;
        pv.uhVal.QuadPart = 0x1122334455667788ULL;
        memset(bbuf, 0xcc, sizeof(bbuf));
        hr = PropVariantToBuffer(&pv, bbuf, sizeof(UINT64));
        check(hr == S_OK && *(UINT64 *)bbuf == 0x1122334455667788ULL, "PropVariantToBuffer copies a VT_UI8's bytes");
    }

    /* PropVariantToGUID: VT_LPSTR (ANSI). */
    {
        PropVariantInit(&pv);
        pv.vt = VT_LPSTR;
        pv.pszVal = (char *)"{12345678-1234-1234-1234-123456789abc}";
        hr = PropVariantToGUID(&pv, &guid);
        check(hr == S_OK && guid.Data1 == 0x12345678, "PropVariantToGUID parses VT_LPSTR");
    }

    /* VariantToGUID: VT_LPWSTR and VT_BYREF|VT_BSTR. */
    {
        BSTR bstr = SysAllocString(L"{abcdef00-1234-1234-1234-123456789abc}");
        VariantInit(&var);
        V_VT(&var) = VT_LPWSTR;
        V_BSTR(&var) = (BSTR)L"{abcdef00-1234-1234-1234-123456789abc}";
        hr = VariantToGUID(&var, &guid);
        check(hr == S_OK && guid.Data1 == 0xabcdef00, "VariantToGUID parses VT_LPWSTR");

        VariantInit(&var);
        V_VT(&var) = VT_BYREF | VT_BSTR;
        V_BSTRREF(&var) = &bstr;
        hr = VariantToGUID(&var, &guid);
        check(hr == S_OK && guid.Data1 == 0xabcdef00, "VariantToGUID parses VT_BYREF|VT_BSTR");
        SysFreeString(bstr);
    }

    /* VariantToString: a type with no hand-written case (VT_R8), via the
     * VariantChangeType fallback. */
    {
        VariantInit(&var);
        V_VT(&var) = VT_R8;
        V_R8(&var) = 3.5;
        hr = VariantToString(&var, buf, ARRAYSIZE(buf));
        check(hr == S_OK && wcsstr(buf, L"3.5") != NULL, "VariantToString formats VT_R8");

        V_VT(&var) = VT_BOOL;
        V_BOOL(&var) = VARIANT_TRUE;
        hr = VariantToString(&var, buf, ARRAYSIZE(buf));
        check(hr == S_OK && buf[0], "VariantToString formats VT_BOOL");
    }

    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
