/* Type library writing and reading (patches/sg/1675), run by
 * test/tlbcreate-gate.sh: CreateTypeLib; the library's help string
 * context; module functions as DLL entries; custom data on functions,
 * parameters, variables and implemented interfaces; deleting variables,
 * functions by member ID and whole type infos; GetLibStatistics;
 * RegisterTypeLibForUser and UnRegisterTypeLibForUser; ITypeInfo::Invoke
 * of a constant and of an instance variable. These were stubs. */
#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <oleauto.h>
#include <stdio.h>

HRESULT WINAPI RegisterTypeLibForUser(ITypeLib *, OLECHAR *, OLECHAR *);
HRESULT WINAPI UnRegisterTypeLibForUser(REFGUID, WORD, WORD, LCID, SYSKIND);

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static const GUID libid = {0x5b1e3c10, 0x2a4f, 0x4c77, {0x9a, 0x01, 0x6e, 0x22, 0x31, 0x40, 0x50, 0x61}};
static const GUID iid_probe = {0x5b1e3c11, 0x2a4f, 0x4c77, {0x9a, 0x01, 0x6e, 0x22, 0x31, 0x40, 0x50, 0x61}};
static const GUID clsid_probe = {0x5b1e3c12, 0x2a4f, 0x4c77, {0x9a, 0x01, 0x6e, 0x22, 0x31, 0x40, 0x50, 0x61}};
static const GUID cd_guid = {0x5b1e3c13, 0x2a4f, 0x4c77, {0x9a, 0x01, 0x6e, 0x22, 0x31, 0x40, 0x50, 0x61}};
static const GUID iid_dispatch = {0x00020400, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};

static ITypeInfo *info_by_name(ITypeLib *lib, const WCHAR *name)
{
    ITypeInfo *info = NULL;
    MEMBERID memid;
    USHORT found = 1;
    WCHAR buf[64];

    lstrcpyW(buf, name);
    if (FAILED(ITypeLib_FindName(lib, buf, 0, &info, &memid, &found)) || !found) return NULL;
    return info;
}

static int custdata_is(VARIANT *v, LONG expect)
{
    int ok = V_VT(v) == VT_I4 && V_I4(v) == expect;
    VariantClear(v);
    return ok;
}

/* laid out as the type library lays a record out: 4 byte alignment */
#pragma pack(push, 4)
struct rec { LONG a; BSTR b; };
#pragma pack(pop)

int main(void)
{
    ICreateTypeLib *old = NULL;
    ICreateTypeLib2 *ctl;
    ICreateTypeInfo *cti, *module, *iface, *record, *coclass;
    ICreateTypeInfo2 *cti2;
    ITypeLib *lib, *disp_lib;
    ITypeInfo *info, *dispatch;
    ITypeInfo2 *info2;
    ITypeLib2 *lib2;
    FUNCDESC func = { 0 };
    ELEMDESC param = {{ 0 }};
    VARDESC var = { 0 };
    VARIANT v, cv;
    WCHAR path[MAX_PATH], tmp[MAX_PATH];
    LPOLESTR names[2] = { (LPOLESTR)L"Method", (LPOLESTR)L"arg" };
    HREFTYPE href;
    HRESULT hr;
    BSTR dll = NULL, entry = NULL, str = NULL;
    WORD ordinal;
    DWORD ctx = 0, help = 0;
    ULONG count = 0, chars = 0;
    UINT i;
    HKEY key;
    WCHAR keyname[128], guid_str[40];

    CoInitialize(NULL);
    GetTempPathW(MAX_PATH, tmp);
    GetTempFileNameW(tmp, L"tlb", 0, path);

    hr = CreateTypeLib(SYS_WIN64, path, &old);
    check(hr == S_OK && old, "CreateTypeLib");
    if (old) ICreateTypeLib_Release(old);

    CreateTypeLib2(sizeof(void *) == 8 ? SYS_WIN64 : SYS_WIN32, path, &ctl);
    ICreateTypeLib2_SetGuid(ctl, &libid);
    ICreateTypeLib2_SetName(ctl, (LPOLESTR)L"ProbeLib");
    ICreateTypeLib2_SetVersion(ctl, 1, 0);
    ICreateTypeLib2_SetLcid(ctl, LOCALE_NEUTRAL);
    ICreateTypeLib2_SetHelpContext(ctl, 0x11);
    check(ICreateTypeLib2_SetHelpStringContext(ctl, 0x22) == S_OK, "SetHelpStringContext");

    /* a module with a DLL function and a constant */
    ICreateTypeLib2_CreateTypeInfo(ctl, (LPOLESTR)L"Mod", TKIND_MODULE, &module);
    func.memid = 0x60000000;
    func.funckind = FUNC_STATIC;
    func.invkind = INVOKE_FUNC;
    func.callconv = CC_STDCALL;
    func.elemdescFunc.tdesc.vt = VT_UI4;
    ICreateTypeInfo_AddFuncDesc(module, 0, &func);
    ICreateTypeInfo_SetFuncAndParamNames(module, 0, (LPOLESTR *)&(const WCHAR *){ L"Ticks" }, 1);
    hr = ICreateTypeInfo_DefineFuncAsDllEntry(module, 0, (LPOLESTR)L"kernel32.dll", (LPOLESTR)L"GetTickCount");
    check(hr == S_OK, "DefineFuncAsDllEntry");
    var.memid = 0x60000010;
    var.varkind = VAR_CONST;
    var.elemdescVar.tdesc.vt = VT_I4;
    V_VT(&cv) = VT_I4;
    V_I4(&cv) = 42;
    var.lpvarValue = &cv;
    ICreateTypeInfo_AddVarDesc(module, 0, &var);
    ICreateTypeInfo_SetVarName(module, 0, (LPOLESTR)L"Answer");

    /* an interface with custom data on a method and its parameter */
    ICreateTypeLib2_CreateTypeInfo(ctl, (LPOLESTR)L"IProbe", TKIND_INTERFACE, &iface);
    ICreateTypeInfo_SetGuid(iface, &iid_probe);
    LoadRegTypeLib(&(GUID){0x00020430, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}}, 2, 0, LOCALE_NEUTRAL, &disp_lib);
    ITypeLib_GetTypeInfoOfGuid(disp_lib, &iid_dispatch, &dispatch);
    ICreateTypeInfo_AddRefTypeInfo(iface, dispatch, &href);
    ICreateTypeInfo_AddImplType(iface, 0, href);
    memset(&func, 0, sizeof(func));
    func.memid = 1;
    func.funckind = FUNC_PUREVIRTUAL;
    func.invkind = INVOKE_FUNC;
    func.callconv = CC_STDCALL;
    func.cParams = 1;
    param.tdesc.vt = VT_I4;
    func.lprgelemdescParam = &param;
    func.elemdescFunc.tdesc.vt = VT_HRESULT;
    ICreateTypeInfo_AddFuncDesc(iface, 0, &func);
    func.memid = 2;
    func.cParams = 0;
    ICreateTypeInfo_AddFuncDesc(iface, 1, &func);
    ICreateTypeInfo_SetFuncAndParamNames(iface, 0, names, 2);
    ICreateTypeInfo_QueryInterface(iface, &IID_ICreateTypeInfo2, (void **)&cti2);
    V_VT(&v) = VT_I4;
    V_I4(&v) = 7;
    check(ICreateTypeInfo2_SetFuncCustData(cti2, 0, &cd_guid, &v) == S_OK, "SetFuncCustData");
    V_I4(&v) = 8;
    check(ICreateTypeInfo2_SetParamCustData(cti2, 0, 0, &cd_guid, &v) == S_OK, "SetParamCustData");
    check(ICreateTypeInfo2_SetFuncHelpStringContext(cti2, 0, 0x33) == S_OK, "SetFuncHelpStringContext");
    check(ICreateTypeInfo2_DeleteFuncDescByMemId(cti2, 2, INVOKE_FUNC) == S_OK, "DeleteFuncDescByMemId");
    ICreateTypeInfo2_Release(cti2);

    /* a record: a variable deleted, custom data on the other */
    ICreateTypeLib2_CreateTypeInfo(ctl, (LPOLESTR)L"Rec", TKIND_RECORD, &record);
    memset(&var, 0, sizeof(var));
    var.memid = 0x40000000;
    var.varkind = VAR_PERINSTANCE;
    var.elemdescVar.tdesc.vt = VT_I4;
    ICreateTypeInfo_AddVarDesc(record, 0, &var);
    ICreateTypeInfo_SetVarName(record, 0, (LPOLESTR)L"a");
    var.memid = 0x40000001;
    var.elemdescVar.tdesc.vt = VT_BSTR;
    ICreateTypeInfo_AddVarDesc(record, 1, &var);
    ICreateTypeInfo_SetVarName(record, 1, (LPOLESTR)L"b");
    var.memid = 0x40000002;
    var.elemdescVar.tdesc.vt = VT_R8;
    ICreateTypeInfo_AddVarDesc(record, 2, &var);
    ICreateTypeInfo_SetVarName(record, 2, (LPOLESTR)L"c");
    ICreateTypeInfo_QueryInterface(record, &IID_ICreateTypeInfo2, (void **)&cti2);
    check(ICreateTypeInfo2_DeleteVarDesc(cti2, 2) == S_OK, "DeleteVarDesc");
    V_I4(&v) = 9;
    check(ICreateTypeInfo2_SetVarCustData(cti2, 0, &cd_guid, &v) == S_OK, "SetVarCustData");
    ICreateTypeInfo2_Release(cti2);

    /* a type info deleted again */
    ICreateTypeLib2_CreateTypeInfo(ctl, (LPOLESTR)L"Gone", TKIND_RECORD, &cti);
    ICreateTypeInfo_Release(cti);
    check(ICreateTypeLib2_DeleteTypeInfo(ctl, (LPOLESTR)L"Gone") == S_OK, "DeleteTypeInfo");

    /* a coclass with custom data on its interface */
    ICreateTypeLib2_CreateTypeInfo(ctl, (LPOLESTR)L"Probe", TKIND_COCLASS, &coclass);
    ICreateTypeInfo_SetGuid(coclass, &clsid_probe);
    ICreateTypeInfo_QueryInterface(iface, &IID_ITypeInfo, (void **)&info);
    ICreateTypeInfo_AddRefTypeInfo(coclass, info, &href);
    ITypeInfo_Release(info);
    ICreateTypeInfo_AddImplType(coclass, 0, href);
    ICreateTypeInfo_QueryInterface(coclass, &IID_ICreateTypeInfo2, (void **)&cti2);
    V_I4(&v) = 10;
    check(ICreateTypeInfo2_SetImplTypeCustData(cti2, 0, &cd_guid, &v) == S_OK, "SetImplTypeCustData");
    ICreateTypeInfo2_Release(cti2);

    ICreateTypeInfo_Release(module);
    ICreateTypeInfo_Release(iface);
    ICreateTypeInfo_Release(record);
    ICreateTypeInfo_Release(coclass);
    hr = ICreateTypeLib2_SaveAllChanges(ctl);
    check(hr == S_OK, "SaveAllChanges");
    ICreateTypeLib2_Release(ctl);
    ITypeInfo_Release(dispatch);
    ITypeLib_Release(disp_lib);

    /* read back */
    hr = LoadTypeLibEx(path, REGKIND_NONE, &lib);
    check(hr == S_OK, "loaded again");
    if (FAILED(hr)) goto done;
    printf("      %u type infos\n", ITypeLib_GetTypeInfoCount(lib));
    check(ITypeLib_GetTypeInfoCount(lib) == 4, "the deleted type info is gone");
    ITypeLib_GetDocumentation(lib, -1, NULL, NULL, &help, NULL);
    ITypeLib_QueryInterface(lib, &IID_ITypeLib2, (void **)&lib2);
    ITypeLib2_GetDocumentation2(lib2, -1, 0, NULL, &ctx, NULL);
    printf("      help context %#lx, help string context %#lx\n", help, ctx);
    check(help == 0x11 && ctx == 0x22, "the library's help context and help string context");
    ITypeLib2_GetLibStatistics(lib2, &count, &chars);
    printf("      %lu names, %lu characters\n", count, chars);
    check(count >= 10 && chars > count, "GetLibStatistics: the library's names");
    ITypeLib2_Release(lib2);

    if ((info = info_by_name(lib, L"Mod")))
    {
        hr = ITypeInfo_GetDllEntry(info, 0x60000000, INVOKE_FUNC, &dll, &entry, &ordinal);
        printf("      %08lx %ls %ls\n", hr, dll, entry);
        check(hr == S_OK && dll && !lstrcmpiW(dll, L"kernel32.dll") && entry && !lstrcmpW(entry, L"GetTickCount"),
              "GetDllEntry: the DLL and the function");
        SysFreeString(dll);
        SysFreeString(entry);
        {
            DISPPARAMS dp = { NULL, NULL, 0, 0 };
            VariantInit(&v);
            hr = ITypeInfo_Invoke(info, NULL, 0x60000010, DISPATCH_PROPERTYGET, &dp, &v, NULL, NULL);
            printf("      Invoke const %08lx vt %d\n", hr, V_VT(&v));
            check(hr == S_OK && V_VT(&v) == VT_I4 && V_I4(&v) == 42, "Invoke of a constant: its value");
        }
        ITypeInfo_Release(info);
    }
    else check(0, "the module");

    if ((info = info_by_name(lib, L"IProbe")))
    {
        TYPEATTR *attr;
        ITypeInfo_QueryInterface(info, &IID_ITypeInfo2, (void **)&info2);
        ITypeInfo_GetTypeAttr(info, &attr);
        check(attr->cFuncs == 1, "the method deleted by member ID is gone");
        ITypeInfo_ReleaseTypeAttr(info, attr);
        VariantInit(&v);
        check(ITypeInfo2_GetFuncCustData(info2, 0, &cd_guid, &v) == S_OK && custdata_is(&v, 7),
              "the method's custom data saved");
        VariantInit(&v);
        check(ITypeInfo2_GetParamCustData(info2, 0, 0, &cd_guid, &v) == S_OK && custdata_is(&v, 8),
              "the parameter's custom data saved");
        ctx = 0;
        ITypeInfo2_GetDocumentation2(info2, 1, 0, NULL, &ctx, NULL);
        check(ctx == 0x33, "the method's help string context saved");
        ITypeInfo2_Release(info2);
        ITypeInfo_Release(info);
    }
    else check(0, "the interface");

    if ((info = info_by_name(lib, L"Rec")))
    {
        TYPEATTR *attr;
        struct rec r = { 5, NULL };
        DISPPARAMS dp = { NULL, NULL, 0, 0 };
        DISPID put = DISPID_PROPERTYPUT;
        VARIANT arg;

        ITypeInfo_QueryInterface(info, &IID_ITypeInfo2, (void **)&info2);
        ITypeInfo_GetTypeAttr(info, &attr);
        check(attr->cVars == 2, "the deleted variable is gone");
        ITypeInfo_ReleaseTypeAttr(info, attr);
        VariantInit(&v);
        check(ITypeInfo2_GetVarCustData(info2, 0, &cd_guid, &v) == S_OK && custdata_is(&v, 9),
              "the variable's custom data saved");
        VariantInit(&v);
        hr = ITypeInfo_Invoke(info, &r, 0x40000000, DISPATCH_PROPERTYGET, &dp, &v, NULL, NULL);
        printf("      Invoke get %08lx\n", hr);
        check(hr == S_OK && V_VT(&v) == VT_I4 && V_I4(&v) == 5, "Invoke reads an instance variable");
        V_VT(&arg) = VT_BSTR;
        V_BSTR(&arg) = SysAllocString(L"set");
        dp.rgvarg = &arg;
        dp.cArgs = 1;
        dp.rgdispidNamedArgs = &put;
        dp.cNamedArgs = 1;
        hr = ITypeInfo_Invoke(info, &r, 0x40000001, DISPATCH_PROPERTYPUT, &dp, NULL, NULL, NULL);
        printf("      Invoke put %08lx %p\n", hr, r.b);
        check(hr == S_OK && r.b && !lstrcmpW(r.b, L"set"), "and writes one");
        VariantClear(&arg);
        SysFreeString(r.b);
        ITypeInfo2_Release(info2);
        ITypeInfo_Release(info);
    }
    else check(0, "the record");

    if ((info = info_by_name(lib, L"Probe")))
    {
        ITypeInfo_QueryInterface(info, &IID_ITypeInfo2, (void **)&info2);
        VariantInit(&v);
        check(ITypeInfo2_GetImplTypeCustData(info2, 0, &cd_guid, &v) == S_OK && custdata_is(&v, 10),
              "the implemented interface's custom data saved");
        ITypeInfo2_Release(info2);
        ITypeInfo_Release(info);
    }
    else check(0, "the coclass");

    /* registration for this user only */
    StringFromGUID2(&libid, guid_str, 40);
    swprintf(keyname, 128, L"Software\\Classes\\TypeLib\\%ls", guid_str);
    hr = RegisterTypeLibForUser(lib, path, NULL);
    check(hr == S_OK, "RegisterTypeLibForUser");
    check(!RegOpenKeyExW(HKEY_CURRENT_USER, keyname, 0, KEY_READ, &key), "under the user's classes") ;
    RegCloseKey(key);
    swprintf(keyname, 128, L"TypeLib\\%ls", guid_str);
    {
        HKEY machine;
        LONG err = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"Software\\Classes", 0, KEY_READ, &machine);
        if (!err)
        {
            err = RegOpenKeyExW(machine, keyname, 0, KEY_READ, &key);
            if (!err) RegCloseKey(key);
            RegCloseKey(machine);
        }
        check(err != ERROR_SUCCESS, "and not the machine's");
    }
    {
        ITypeLib *reg = NULL;
        hr = LoadRegTypeLib(&libid, 1, 0, LOCALE_NEUTRAL, &reg);
        check(hr == S_OK && reg, "LoadRegTypeLib finds it");
        if (reg) ITypeLib_Release(reg);
        hr = QueryPathOfRegTypeLib(&libid, 1, 0, LOCALE_NEUTRAL, &str);
        check(hr == S_OK && str && !lstrcmpiW(str, path), "QueryPathOfRegTypeLib gives its path");
        SysFreeString(str);
    }
    hr = UnRegisterTypeLibForUser(&libid, 1, 0, LOCALE_NEUTRAL, sizeof(void *) == 8 ? SYS_WIN64 : SYS_WIN32);
    swprintf(keyname, 128, L"Software\\Classes\\TypeLib\\%ls", guid_str);
    check(hr == S_OK && RegOpenKeyExW(HKEY_CURRENT_USER, keyname, 0, KEY_READ, &key), "UnRegisterTypeLibForUser");

    ITypeLib_Release(lib);
done:
    DeleteFileW(path);
    CoUninitialize();
    (void)i;
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
