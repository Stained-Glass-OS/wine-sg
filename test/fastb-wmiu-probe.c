#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <wbemcli.h>
#include <wmiutils.h>
#include <stdio.h>

static int fails, checks;
static const GUID my_CLSID_WbemDefPath = {0xcf4cc405,0xe2c5,0x4ddd,{0xb3,0xce,0x5e,0x75,0x82,0xd8,0xc9,0xfa}};
static const GUID my_IID_IWbemPath = {0x3bc15af2,0x736c,0x477e,{0x9e,0x51,0x23,0x8a,0xf8,0x66,0x7d,0xcc}};
static const GUID my_CLSID_StatusCode = {0xeb87e1bd,0x3233,0x11d2,{0xae,0xc9,0x00,0xc0,0x4f,0xb6,0x88,0x20}};
static const GUID my_IID_StatusCodeText = {0xeb87e1bc,0x3233,0x11d2,{0xae,0xc9,0x00,0xc0,0x4f,0xb6,0x88,0x20}};
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)
#define OK(e) do { HRESULT h_ = (e); checks++; if (FAILED(h_)) { fails++; printf("FAIL line %d: %s -> %08lx\n", __LINE__, #e, (long)h_); } } while (0)

int main(void)
{
    IWbemPath *path = NULL;
    IWbemPathKeyList *kl = NULL, *sk = NULL;
    IWbemStatusCodeText *sct = NULL;
    WCHAR buf[256], nbuf[64];
    ULONG len, nlen, vlen, count, cim;
    LONG lval;
    VARIANT v;
    ULONGLONG info;
    BSTR text;
    HRESULT hr;

    CoInitialize(NULL);
    OK(CoCreateInstance(&my_CLSID_WbemDefPath, NULL, CLSCTX_INPROC_SERVER, &my_IID_IWbemPath, (void **)&path));
    if (!path) return 1;

    /* key list on a parsed path */
    OK(IWbemPath_SetText(path, WBEMPATH_CREATE_ACCEPT_ALL, L"root\\cimv2:Win32_LogicalDisk.DeviceId=\"C:\",DriveType=3"));
    OK(IWbemPath_GetKeyList(path, &kl));
    OK(IWbemPathKeyList_GetCount(kl, &count));
    CHECK(count == 2);
    nlen = ARRAYSIZE(nbuf); vlen = sizeof(buf);
    OK(IWbemPathKeyList_GetKey(kl, 0, 0, &nlen, nbuf, &vlen, buf, &cim));
    CHECK(!wcscmp(nbuf, L"DeviceId") && nlen == 9);
    CHECK(cim == CIM_STRING && !wcscmp(buf, L"C:") && vlen == 6);
    nlen = ARRAYSIZE(nbuf); vlen = sizeof(lval);
    OK(IWbemPathKeyList_GetKey(kl, 1, 0, &nlen, nbuf, &vlen, &lval, &cim));
    CHECK(!wcscmp(nbuf, L"DriveType") && cim == CIM_SINT32 && lval == 3);
    nlen = ARRAYSIZE(nbuf);
    OK(IWbemPathKeyList_GetKey2(kl, 0, 0, &nlen, nbuf, &v, &cim));
    CHECK(V_VT(&v) == VT_BSTR && !wcscmp(V_BSTR(&v), L"C:"));
    VariantClear(&v);
    CHECK(IWbemPathKeyList_GetKey2(kl, 5, 0, &nlen, nbuf, &v, &cim) == WBEM_E_NOT_FOUND);

    /* set / replace / remove */
    lval = 5;
    OK(IWbemPathKeyList_SetKey(kl, L"DriveType", 0, CIM_SINT32, &lval));
    nlen = ARRAYSIZE(nbuf); vlen = sizeof(lval);
    OK(IWbemPathKeyList_GetKey(kl, 1, 0, &nlen, nbuf, &vlen, &lval, &cim));
    CHECK(lval == 5);
    OK(IWbemPathKeyList_SetKey(kl, L"Label", 0, CIM_STRING, (void *)L"say \"hi\""));
    V_VT(&v) = VT_I4; V_I4(&v) = 77;
    OK(IWbemPathKeyList_SetKey2(kl, L"Num", 0, CIM_SINT32, &v));
    OK(IWbemPathKeyList_GetCount(kl, &count));
    CHECK(count == 4);
    len = ARRAYSIZE(buf);
    OK(IWbemPathKeyList_GetText(kl, 0, &len, buf));
    CHECK(wcsstr(buf, L"DriveType=5") && wcsstr(buf, L"Num=77") && wcsstr(buf, L"Label=\"say \\\"hi\\\"\""));
    nlen = ARRAYSIZE(nbuf); vlen = sizeof(buf);
    OK(IWbemPathKeyList_GetKey(kl, 2, 0, &nlen, nbuf, &vlen, buf, &cim));
    CHECK(!wcscmp(buf, L"say \"hi\""));
    OK(IWbemPathKeyList_RemoveKey(kl, L"Num", 0));
    CHECK(IWbemPathKeyList_RemoveKey(kl, L"Num", 0) == WBEM_E_NOT_FOUND);
    OK(IWbemPathKeyList_GetCount(kl, &count));
    CHECK(count == 3);
    len = ARRAYSIZE(buf);
    OK(IWbemPath_GetText(path, WBEMPATH_GET_RELATIVE_ONLY, &len, buf));
    CHECK(wcsstr(buf, L"Win32_LogicalDisk.DeviceId=\"C:\",DriveType=5"));
    OK(IWbemPathKeyList_MakeSingleton(kl, TRUE));
    OK(IWbemPathKeyList_GetCount(kl, &count));
    CHECK(count == 1);
    OK(IWbemPathKeyList_GetInfo(kl, 0, &info));
    CHECK(info & WBEMPATH_INFO_IS_SINGLETON);
    len = ARRAYSIZE(buf);
    OK(IWbemPath_GetText(path, WBEMPATH_GET_RELATIVE_ONLY, &len, buf));
    CHECK(!wcscmp(buf, L"Win32_LogicalDisk=@"));
    OK(IWbemPathKeyList_RemoveAllKeys(kl, 0));
    OK(IWbemPathKeyList_GetCount(kl, &count));
    CHECK(count == 0);
    IWbemPathKeyList_Release(kl);

    /* scopes */
    OK(IWbemPath_GetScopeCount(path, &count));
    CHECK(count == 0);
    OK(IWbemPath_SetScopeFromText(path, 0, (WCHAR *)L"Win32_Parent.Name=\"p\""));
    OK(IWbemPath_SetScope(path, 1, (WCHAR *)L"Win32_Child"));
    CHECK(IWbemPath_SetScope(path, 5, (WCHAR *)L"X") == WBEM_E_INVALID_PARAMETER);
    OK(IWbemPath_GetScopeCount(path, &count));
    CHECK(count == 2);
    len = ARRAYSIZE(buf);
    OK(IWbemPath_GetScope(path, 0, &len, buf, &sk));
    CHECK(!wcscmp(buf, L"Win32_Parent") && len == 13);
    OK(IWbemPathKeyList_GetCount(sk, &count));
    CHECK(count == 1);
    lval = 9;
    OK(IWbemPathKeyList_SetKey(sk, L"Id", 0, CIM_SINT32, &lval));
    IWbemPathKeyList_Release(sk);
    len = ARRAYSIZE(buf);
    OK(IWbemPath_GetScopeAsText(path, 0, &len, buf));
    CHECK(!wcscmp(buf, L"Win32_Parent.Name=\"p\",Id=9"));
    OK(IWbemPath_RemoveScope(path, 0));
    OK(IWbemPath_GetScopeCount(path, &count));
    CHECK(count == 1);
    OK(IWbemPath_RemoveAllScopes(path));
    OK(IWbemPath_GetScopeCount(path, &count));
    CHECK(count == 0);

    /* predicates */
    OK(IWbemPath_SetText(path, WBEMPATH_CREATE_ACCEPT_ALL, L"\\\\.\\root\\cimv2:Win32_Process"));
    CHECK(IWbemPath_IsLocal(path, L".") == TRUE);
    CHECK(IWbemPath_IsSameClassName(path, L"win32_process") == TRUE);
    CHECK(IWbemPath_IsSameClassName(path, L"Win32_Other") == FALSE);
    CHECK(IWbemPath_IsRelative(path, (WCHAR *)L".", (WCHAR *)L"root\\cimv2") == TRUE);
    CHECK(IWbemPath_IsRelative(path, (WCHAR *)L".", (WCHAR *)L"root\\wmi") == FALSE);
    CHECK(IWbemPath_IsRelativeOrChild(path, (WCHAR *)L".", (WCHAR *)L"root", 0) == TRUE);
    CHECK(IWbemPath_IsRelativeOrChild(path, (WCHAR *)L".", (WCHAR *)L"root\\wmi", 0) == FALSE);
    OK(IWbemPath_SetText(path, WBEMPATH_CREATE_ACCEPT_ALL, L"\\\\remote\\root\\cimv2:Win32_Process"));
    CHECK(IWbemPath_IsLocal(path, L"remote") == TRUE);
    CHECK(IWbemPath_IsLocal(path, L"other") == FALSE);

    /* class part */
    OK(IWbemPath_DeleteClassPart(path, 0));
    len = ARRAYSIZE(buf);
    CHECK(IWbemPath_GetClassName(path, &len, buf) == WBEM_E_INVALID_OBJECT_PATH);
    OK(IWbemPath_CreateClassPart(path, 0, L"Win32_Bios"));
    len = ARRAYSIZE(buf);
    OK(IWbemPath_GetClassName(path, &len, buf));
    CHECK(!wcscmp(buf, L"Win32_Bios"));
    CHECK(IWbemPath_DeleteClassPart(path, 4) == WBEM_E_INVALID_PARAMETER);

    /* SetText mode checks, empty path text */
    CHECK(IWbemPath_SetText(path, WBEMPATH_TREAT_SINGLE_IDENT_AS_NS, L"a:b") == WBEM_E_INVALID_PARAMETER);
    CHECK(IWbemPath_SetText(path, WBEMPATH_CREATE_ACCEPT_ABSOLUTE, L"/root/cimv2:Win32_OperatingSystem") == WBEM_E_INVALID_PARAMETER);
    OK(IWbemPath_SetText(path, WBEMPATH_CREATE_ACCEPT_ALL, L""));
    len = ARRAYSIZE(buf);
    OK(IWbemPath_GetText(path, WBEMPATH_GET_SERVER_TOO, &len, buf));
    CHECK(!buf[0] && len == 1);
    IWbemPath_Release(path);

    /* status code text */
    OK(CoCreateInstance(&my_CLSID_StatusCode, NULL, CLSCTX_INPROC_SERVER, &my_IID_StatusCodeText, (void **)&sct));
    OK(IWbemStatusCodeText_GetErrorCodeText(sct, WBEM_E_NOT_FOUND, 0, 0, &text));
    CHECK(text && wcsstr(text, L"not found") == NULL && wcsstr(text, L"Not found"));
    SysFreeString(text);
    OK(IWbemStatusCodeText_GetErrorCodeText(sct, WBEM_E_INVALID_OBJECT_PATH, 0, 0, &text));
    CHECK(text && wcsstr(text, L"Invalid object path"));
    SysFreeString(text);
    OK(IWbemStatusCodeText_GetErrorCodeText(sct, HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND), 0, 0, &text));
    CHECK(text && wcslen(text) > 3);
    SysFreeString(text);
    IWbemStatusCodeText_Release(sct);

    printf("%d checks, %d failures\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
