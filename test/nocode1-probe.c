/* test/nocode1-gate.sh: exports that had no code (spec stubs) and now do, batch 1 (patches/sg/8009-8014).
 *   8009 ktmw32 GetTransactionId, GetTransactionInformation, Commit/RollbackTransactionAsync
 *   8010 advapi32/secur32 LsaRegister/UnregisterPolicyChangeNotification
 *   8011 netapi32 the global group "None", NetGroup* writes, NetUserSetGroups, messenger calls, wksta transports
 *   8012 setupapi/cfgmgr32 resource descriptors
 *   8013 msports the COM port database
 *   8014 query BindIFilterFromStorage/Stream
 * argv[1] = the test filter DLL (test/nocode1-filter.c). Run by the prefix's owner (an administrator). */
#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <stdio.h>
#include <ktmw32.h>
#include <ntsecapi.h>
#include <lm.h>
#include <lmmsg.h>
#include <cfgmgr32.h>
#include <ole2.h>
#include <shlwapi.h>
#include <filter.h>

static int failures;
#define CHECK(c, ...) do { if (c) { printf("ok    "); printf(__VA_ARGS__); printf("\n"); } \
                           else { failures++; printf("FAIL  "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void *get(const char *dll, const char *name)
{
    HMODULE m = LoadLibraryA(dll);
    void *p = m ? (void *)GetProcAddress(m, name) : NULL;
    if (!p) { failures++; printf("FAIL  %s!%s not found\n", dll, name); }
    return p;
}

static void test_ktm(void)
{
    BOOL (WINAPI *pGetTransactionId)(HANDLE, GUID *) = get("ktmw32.dll", "GetTransactionId");
    BOOL (WINAPI *pGetTransactionInformation)(HANDLE, DWORD *, DWORD *, DWORD *, DWORD *, DWORD, WCHAR *) =
        get("ktmw32.dll", "GetTransactionInformation");
    BOOL (WINAPI *pCommitTransactionAsync)(HANDLE) = get("ktmw32.dll", "CommitTransactionAsync");
    BOOL (WINAPI *pRollbackTransactionAsync)(HANDLE) = get("ktmw32.dll", "RollbackTransactionAsync");
    DWORD outcome = 99, level = 99, flags = 99, timeout = 99;
    WCHAR desc[64];
    GUID id, id2, zero = {0};
    HANDLE t, t2;
    BOOL ret;

    if (!pGetTransactionId || !pGetTransactionInformation || !pCommitTransactionAsync || !pRollbackTransactionAsync) return;

    t = CreateTransaction(NULL, NULL, 0, 0, 0, 0, (WCHAR *)L"sg nocode1");
    CHECK(t != INVALID_HANDLE_VALUE, "CreateTransaction %lu", GetLastError());
    ret = pGetTransactionId(t, &id);
    CHECK(ret && memcmp(&id, &zero, sizeof(id)), "GetTransactionId gives a GUID");
    t2 = OpenTransaction(TRANSACTION_ALL_ACCESS, &id);
    CHECK(t2 != INVALID_HANDLE_VALUE, "OpenTransaction by that GUID %lu", GetLastError());
    ret = pGetTransactionId(t2, &id2);
    CHECK(ret && !memcmp(&id, &id2, sizeof(id)), "the opened transaction has the same id");
    CloseHandle(t2);

    memset(desc, 0xcc, sizeof(desc));
    ret = pGetTransactionInformation(t, &outcome, &level, &flags, &timeout, sizeof(desc), desc);
    CHECK(ret && outcome == TransactionOutcomeUndetermined && timeout == 0 && !wcscmp(desc, L"sg nocode1"),
          "GetTransactionInformation: ret %d outcome %lu timeout %lu desc %ls", ret, outcome, timeout, ret ? desc : L"");
    ret = pGetTransactionInformation(t, &outcome, NULL, NULL, NULL, 0, NULL);
    CHECK(ret, "GetTransactionInformation with only the outcome");
    SetLastError(0xdeadbeef);
    ret = pGetTransactionInformation(t, NULL, NULL, NULL, NULL, 4, desc);
    CHECK(!ret && GetLastError() == ERROR_BUFFER_OVERFLOW, "a short description buffer: %d %lu", ret, GetLastError());

    ret = pCommitTransactionAsync(t);
    CHECK(ret, "CommitTransactionAsync %lu", GetLastError());
    ret = pGetTransactionInformation(t, &outcome, NULL, NULL, NULL, 0, NULL);
    CHECK(ret && outcome == TransactionOutcomeCommitted, "committed: outcome %lu", outcome);
    CloseHandle(t);

    t = CreateTransaction(NULL, NULL, 0, 0, 0, 60000, NULL);
    ret = pGetTransactionInformation(t, NULL, NULL, NULL, &timeout, sizeof(desc), desc);
    CHECK(ret && timeout > 50000 && timeout <= 60000 && !desc[0], "a 60 s transaction: timeout %lu", timeout);
    ret = pRollbackTransactionAsync(t);
    CHECK(ret, "RollbackTransactionAsync %lu", GetLastError());
    ret = pGetTransactionInformation(t, &outcome, NULL, NULL, NULL, 0, NULL);
    CHECK(ret && outcome == TransactionOutcomeAborted, "aborted: outcome %lu", outcome);
    CloseHandle(t);

    SetLastError(0xdeadbeef);
    ret = pGetTransactionId(GetCurrentProcess(), &id);
    CHECK(!ret && GetLastError() == ERROR_INVALID_HANDLE, "GetTransactionId of a process handle: %lu", GetLastError());
}

static void test_lsa(void)
{
    NTSTATUS (WINAPI *pReg)(POLICY_NOTIFICATION_INFORMATION_CLASS, HANDLE) = get("secur32.dll", "LsaRegisterPolicyChangeNotification");
    NTSTATUS (WINAPI *pUnreg)(POLICY_NOTIFICATION_INFORMATION_CLASS, HANDLE) = get("secur32.dll", "LsaUnregisterPolicyChangeNotification");
    HANDLE ev = CreateEventW(NULL, FALSE, FALSE, NULL), mutex = CreateMutexW(NULL, FALSE, NULL);
    NTSTATUS st;

    if (!pReg || !pUnreg) return;
    st = pReg(PolicyNotifyAuditEventsInformation, ev);
    CHECK(st == 0, "secur32 LsaRegisterPolicyChangeNotification %#lx", st);
    st = pReg(PolicyNotifyDnsDomainInformation, ev);
    CHECK(st == 0, "registered for a second class %#lx", st);
    CHECK(WaitForSingleObject(ev, 0) == WAIT_TIMEOUT, "not signalled (no policy changed)");
    st = pReg(0, ev);
    CHECK(st == STATUS_INVALID_PARAMETER, "class 0: %#lx", st);
    st = pReg(PolicyNotifyAuditEventsInformation, NULL);
    CHECK(st == STATUS_INVALID_HANDLE, "no event: %#lx", st);
    st = pReg(PolicyNotifyAuditEventsInformation, mutex);
    CHECK(st == STATUS_INVALID_HANDLE, "a mutex: %#lx", st);
    st = pUnreg(PolicyNotifyAuditEventsInformation, ev);
    CHECK(st == 0, "LsaUnregisterPolicyChangeNotification %#lx", st);
    st = pUnreg(PolicyNotifyAuditEventsInformation, ev);
    CHECK(st == STATUS_INVALID_HANDLE, "unregistered twice: %#lx", st);
    st = pUnreg(PolicyNotifyDnsDomainInformation, ev);
    CHECK(st == 0, "the second class %#lx", st);
    CloseHandle(ev);
    CloseHandle(mutex);
}

static void test_netapi(void)
{
    GROUP_INFO_2 *g2;
    GROUP_INFO_3 *g3;
    GROUP_INFO_0 *g0;
    GROUP_USERS_INFO_0 *u0;
    GROUP_INFO_1 add = { (WCHAR *)L"Testers", (WCHAR *)L"x" };
    WCHAR me[UNLEN + 1];
    DWORD len = UNLEN + 1, read, total, i, err;
    DWORD_PTR resume = 0;
    NET_API_STATUS ret;
    BOOL found = FALSE;

    GetUserNameW(me, &len);
    ret = NetGroupEnum(NULL, 0, (BYTE **)&g0, MAX_PREFERRED_LENGTH, &read, &total, NULL);
    CHECK(ret == NERR_Success && read == 1 && total == 1 && !wcscmp(g0->grpi0_name, L"None"), "NetGroupEnum: the group None (%lu)", ret);
    if (!ret) NetApiBufferFree(g0);
    ret = NetGroupGetInfo(NULL, L"none", 2, (BYTE **)&g2);
    CHECK(ret == NERR_Success && g2->grpi2_group_id == 513 && !wcscmp(g2->grpi2_comment, L"Ordinary users") && g2->grpi2_attributes == 7,
          "NetGroupGetInfo level 2 (%lu)", ret);
    if (!ret) NetApiBufferFree(g2);
    ret = NetGroupGetInfo(NULL, L"None", 3, (BYTE **)&g3);
    CHECK(ret == NERR_Success && IsValidSid(g3->grpi3_group_sid) && *GetSidSubAuthority(g3->grpi3_group_sid,
          *GetSidSubAuthorityCount(g3->grpi3_group_sid) - 1) == 513, "NetGroupGetInfo level 3: a SID ending in 513 (%lu)", ret);
    if (!ret) NetApiBufferFree(g3);
    CHECK(NetGroupGetInfo(NULL, L"Nobody", 0, (BYTE **)&g0) == NERR_GroupNotFound, "NetGroupGetInfo of another group");

    ret = NetGroupGetUsers(NULL, L"None", 0, (BYTE **)&u0, MAX_PREFERRED_LENGTH, &read, &total, &resume);
    for (i = 0; !ret && i < read; i++) if (!wcsicmp(u0[i].grui0_name, me)) found = TRUE;
    CHECK(ret == NERR_Success && read >= 1 && read == total && found, "NetGroupGetUsers None: %lu users, %ls among them (%lu)", read, me, ret);
    if (!ret) NetApiBufferFree(u0);
    CHECK(NetGroupGetUsers(NULL, L"Nobody", 0, (BYTE **)&u0, MAX_PREFERRED_LENGTH, &read, &total, NULL) == NERR_GroupNotFound,
          "NetGroupGetUsers of another group");

    ret = NetGroupAdd(NULL, 1, (BYTE *)&add, &err);
    CHECK(ret == NERR_NotPrimary, "NetGroupAdd: %lu", ret);
    add.grpi1_name = (WCHAR *)L"None";
    CHECK(NetGroupAdd(NULL, 1, (BYTE *)&add, &err) == NERR_GroupExists, "NetGroupAdd None: exists");
    CHECK(NetGroupDel(NULL, L"Nobody") == NERR_GroupNotFound, "NetGroupDel of no group");
    CHECK(NetGroupDel(NULL, L"None") == NERR_NotPrimary, "NetGroupDel None");
    CHECK(NetGroupDelUser(NULL, L"None", me) == NERR_NotPrimary, "NetGroupDelUser");
    CHECK(NetGroupDelUser(NULL, L"None", L"no such user sg") == NERR_UserNotFound, "NetGroupDelUser of no user");
    CHECK(NetGroupSetInfo(NULL, L"None", 1002, (BYTE *)&add.grpi1_comment, &err) == NERR_NotPrimary, "NetGroupSetInfo");
    CHECK(NetGroupSetUsers(NULL, L"None", 0, NULL, 0) == NERR_NotPrimary, "NetGroupSetUsers");
    CHECK(NetUserSetGroups(NULL, me, 0, NULL, 0) == NERR_NotPrimary, "NetUserSetGroups");
    CHECK(NetGroupAdd(L"\\\\nosuchhost", 1, (BYTE *)&add, &err) == ERROR_BAD_NETPATH, "NetGroupAdd, another computer");

    CHECK(NetMessageBufferSend(NULL, L"someone", NULL, (BYTE *)L"hi", 4) == ERROR_NOT_SUPPORTED, "NetMessageBufferSend: no messenger service");
    CHECK(NetMessageNameAdd(NULL, L"sgname") == ERROR_NOT_SUPPORTED, "NetMessageNameAdd");
    CHECK(NetMessageNameDel(NULL, L"sgname") == ERROR_NOT_SUPPORTED, "NetMessageNameDel");
    CHECK(NetMessageNameEnum(NULL, 0, (BYTE **)&u0, MAX_PREFERRED_LENGTH, &read, &total, NULL) == ERROR_NOT_SUPPORTED, "NetMessageNameEnum");
    CHECK(NetMessageNameGetInfo(NULL, L"sgname", 0, (BYTE **)&u0) == ERROR_NOT_SUPPORTED, "NetMessageNameGetInfo");
    CHECK(NetMessageNameEnum(NULL, 2, (BYTE **)&u0, MAX_PREFERRED_LENGTH, &read, &total, NULL) == ERROR_INVALID_LEVEL, "NetMessageNameEnum level 2");
    {
        WKSTA_TRANSPORT_INFO_0 ti = {0};
        ti.wkti0_transport_name = (WCHAR *)L"\\Device\\NetBT_Tcpip_sg";
        CHECK(NetWkstaTransportAdd(NULL, 0, (BYTE *)&ti, &err) == ERROR_NOT_SUPPORTED, "NetWkstaTransportAdd");
        CHECK(NetWkstaTransportAdd(NULL, 1, (BYTE *)&ti, &err) == ERROR_INVALID_LEVEL, "NetWkstaTransportAdd level 1");
        CHECK(NetWkstaTransportDel(NULL, ti.wkti0_transport_name, USE_NOFORCE) == ERROR_NOT_SUPPORTED, "NetWkstaTransportDel");
    }
}

static void test_cfgmgr(void)
{
    CONFIGRET (WINAPI *pNext)(RES_DES *, RES_DES, RESOURCEID, RESOURCEID *, ULONG) = get("setupapi.dll", "CM_Get_Next_Res_Des");
    CONFIGRET (WINAPI *pData)(RES_DES, void *, ULONG, ULONG) = get("setupapi.dll", "CM_Get_Res_Des_Data");
    CONFIGRET (WINAPI *pSize)(ULONG *, RES_DES, ULONG) = get("cfgmgr32.dll", "CM_Get_Res_Des_Data_Size");
    CONFIGRET (WINAPI *pFree)(RES_DES) = get("setupapi.dll", "CM_Free_Res_Des_Handle");
    CONFIGRET (WINAPI *pAdd)(RES_DES *, LOG_CONF, RESOURCEID, const void *, ULONG, ULONG) = get("cfgmgr32.dll", "CM_Add_Res_Des");
    CONFIGRET (WINAPI *pFreeConf)(LOG_CONF, ULONG) = get("cfgmgr32.dll", "CM_Free_Log_Conf");
    RES_DES rd = 0xdead;
    RESOURCEID type;
    ULONG size = 77;
    DEVINST root;
    LOG_CONF conf = 0x1234;
    char data[16] = {0};
    CONFIGRET cr;

    if (!pNext || !pData || !pSize || !pFree || !pAdd || !pFreeConf) return;
    CM_Locate_DevNodeW(&root, NULL, CM_LOCATE_DEVNODE_NORMAL);
    cr = CM_Get_First_Log_Conf(&conf, root, ALLOC_LOG_CONF);
    CHECK(cr == CR_NO_MORE_LOG_CONF, "the root device has no configuration (%#lx)", cr);
    cr = pNext(&rd, conf, ResType_All, &type, 0);
    CHECK(cr == CR_INVALID_RES_DES && rd == 0, "CM_Get_Next_Res_Des of no configuration: %#lx", cr);
    CHECK(pNext(NULL, conf, ResType_All, &type, 0) == CR_INVALID_POINTER, "CM_Get_Next_Res_Des without a result");
    CHECK(pNext(&rd, conf, ResType_All, NULL, 0) == CR_INVALID_POINTER, "CM_Get_Next_Res_Des ResType_All without a type");
    CHECK(pNext(&rd, conf, ResType_Mem, NULL, 1) == CR_INVALID_FLAG, "CM_Get_Next_Res_Des flags");
    CHECK(pNext(&rd, conf, 0x42, NULL, 0) == CR_INVALID_RESOURCEID, "CM_Get_Next_Res_Des type 0x42");
    CHECK(pData(rd, data, sizeof(data), 0) == CR_INVALID_RES_DES, "CM_Get_Res_Des_Data");
    CHECK(pData(rd, NULL, sizeof(data), 0) == CR_INVALID_POINTER, "CM_Get_Res_Des_Data without a buffer");
    cr = pSize(&size, rd, 0);
    CHECK(cr == CR_INVALID_RES_DES && size == 0, "cfgmgr32 CM_Get_Res_Des_Data_Size: %#lx", cr);
    CHECK(pFree(rd) == CR_INVALID_RES_DES, "CM_Free_Res_Des_Handle");
    CHECK(pAdd(&rd, conf, ResType_IRQ, data, sizeof(data), 0) == CR_INVALID_LOG_CONF, "CM_Add_Res_Des to no configuration");
    CHECK(pAdd(&rd, conf, ResType_All, data, sizeof(data), 0) == CR_INVALID_RESOURCEID, "CM_Add_Res_Des ResType_All");
    CHECK(pFreeConf(conf, 0) == CR_INVALID_LOG_CONF, "CM_Free_Log_Conf");
}

#define HCOMDB HANDLE
#define CDB_REPORT_BITS 0
#define CDB_REPORT_BYTES 1
static void test_comdb(void)
{
    LONG (WINAPI *pOpen)(HCOMDB *) = get("msports.dll", "ComDBOpen");
    LONG (WINAPI *pClose)(HCOMDB) = get("msports.dll", "ComDBClose");
    LONG (WINAPI *pUsage)(HCOMDB, BYTE *, DWORD, ULONG, DWORD *) = get("msports.dll", "ComDBGetCurrentPortUsage");
    LONG (WINAPI *pNextFree)(HCOMDB, DWORD *) = get("msports.dll", "ComDBClaimNextFreePort");
    LONG (WINAPI *pClaim)(HCOMDB, DWORD, BOOL, BOOL *) = get("msports.dll", "ComDBClaimPort");
    LONG (WINAPI *pRelease)(HCOMDB, DWORD) = get("msports.dll", "ComDBReleasePort");
    LONG (WINAPI *pResize)(HCOMDB, DWORD) = get("msports.dll", "ComDBResizeDatabase");
    HCOMDB db, db2;
    DWORD ports = 0, n = 0, n2 = 0;
    BYTE bytes[8], bits[1];
    BOOL forced;
    LONG ret;

    if (!pOpen || !pClose || !pUsage || !pNextFree || !pClaim || !pRelease || !pResize) return;
    ret = pOpen(&db);
    CHECK(ret == ERROR_SUCCESS, "ComDBOpen %ld", ret);
    if (ret) return;
    ret = pUsage(db, NULL, 0, 0, &ports);
    CHECK(ret == ERROR_SUCCESS && ports == 256, "ComDBGetCurrentPortUsage: %lu ports (%ld)", ports, ret);
    ret = pNextFree(db, &n);
    CHECK(ret == ERROR_SUCCESS && n == 1, "ComDBClaimNextFreePort: COM%lu (%ld)", n, ret);
    pOpen(&db2);
    ret = pNextFree(db2, &n2);
    CHECK(ret == ERROR_SUCCESS && n2 == 2, "a second handle sees the claim: COM%lu", n2);
    pClose(db2);
    CHECK(pClaim(db, 4, FALSE, &forced) == ERROR_SUCCESS, "ComDBClaimPort COM4");
    ret = pClaim(db, 4, FALSE, &forced);
    CHECK(ret == ERROR_SHARING_VIOLATION, "COM4 again: %ld", ret);
    ret = pClaim(db, 4, TRUE, &forced);
    CHECK(ret == ERROR_SUCCESS && forced, "COM4 forced: %ld forced %d", ret, forced);
    ret = pUsage(db, bytes, sizeof(bytes), CDB_REPORT_BYTES, &ports);
    CHECK(ret == ERROR_SUCCESS && ports == 8 && bytes[0] == 1 && bytes[1] == 1 && bytes[2] == 0 && bytes[3] == 1 && bytes[4] == 0,
          "usage as bytes: %u %u %u %u %u (%lu)", bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], ports);
    ret = pUsage(db, bits, sizeof(bits), CDB_REPORT_BITS, &ports);
    CHECK(ret == ERROR_SUCCESS && ports == 8 && bits[0] == 0x0b, "usage as bits: %#x (%lu)", bits[0], ports);
    CHECK(pRelease(db, 2) == ERROR_SUCCESS, "ComDBReleasePort COM2");
    ret = pNextFree(db, &n);
    CHECK(ret == ERROR_SUCCESS && n == 2, "the next free port is COM2 again: COM%lu", n);
    CHECK(pClaim(db, 0, FALSE, &forced) == ERROR_INVALID_PARAMETER, "COM0");
    CHECK(pClaim(db, 300, FALSE, &forced) == ERROR_INVALID_PARAMETER, "COM300 is beyond the database");
    CHECK(pResize(db, 1000) == ERROR_BAD_LENGTH, "resizing to 1000");
    CHECK(pResize(db, 1024) == ERROR_SUCCESS, "resizing to 1024");
    pUsage(db, NULL, 0, 0, &ports);
    CHECK(ports == 1024, "now %lu ports", ports);
    CHECK(pClaim(db, 300, FALSE, &forced) == ERROR_SUCCESS, "COM300 now");
    CHECK(pClose(db) == ERROR_SUCCESS, "ComDBClose");
    CHECK(pClose(db) == ERROR_INVALID_PARAMETER, "closed twice");
}

static const CLSID CLSID_SgFilter = {0x5d6a8a1e,0x2b8e,0x4d7c,{0x9a,0x51,0x3e,0x6f,0x10,0x22,0x33,0x01}};

static void reg(const WCHAR *key, const WCHAR *name, const WCHAR *value)
{
    RegSetKeyValueW(HKEY_CLASSES_ROOT, key, name, REG_SZ, value, (wcslen(value) + 1) * sizeof(WCHAR));
}

static void test_bind(const char *dll)
{
    HRESULT (WINAPI *pBindStream)(IStream *, IUnknown *, void **) = get("query.dll", "BindIFilterFromStream");
    HRESULT (WINAPI *pBindStorage)(IStorage *, IUnknown *, void **) = get("query.dll", "BindIFilterFromStorage");
    static const CLSID CLSID_Doc = {0x5d6a8a1e,0x2b8e,0x4d7c,{0x9a,0x51,0x3e,0x6f,0x10,0x22,0x33,0x02}};
    WCHAR dllw[MAX_PATH], dir[MAX_PATH], path[MAX_PATH];
    IStream *stm;
    IStorage *stg;
    IFilter *filter;
    STAT_CHUNK chunk;
    ULONG attr;
    HANDLE file;
    DWORD written;
    HRESULT hr;

    if (!pBindStream || !pBindStorage) return;
    MultiByteToWideChar(CP_ACP, 0, dll, -1, dllw, MAX_PATH);
    GetFullPathNameW(dllw, MAX_PATH, path, NULL);
    reg(L"CLSID\\{5D6A8A1E-2B8E-4D7C-9A51-3E6F10223301}\\InprocServer32", NULL, path);
    reg(L"CLSID\\{5D6A8A1E-2B8E-4D7C-9A51-3E6F10223301}\\InprocServer32", L"ThreadingModel", L"Both");
    reg(L"CLSID\\{5D6A8A1E-2B8E-4D7C-9A51-3E6F10223300}\\PersistentAddinsRegistered\\{89BCB740-6119-101A-BCB7-00DD010655AF}",
        NULL, L"{5D6A8A1E-2B8E-4D7C-9A51-3E6F10223301}");
    reg(L".sgflt\\PersistentHandler", NULL, L"{5D6A8A1E-2B8E-4D7C-9A51-3E6F10223300}");
    reg(L"CLSID\\{5D6A8A1E-2B8E-4D7C-9A51-3E6F10223302}\\PersistentHandler", NULL, L"{5D6A8A1E-2B8E-4D7C-9A51-3E6F10223300}");
    CoInitialize(NULL);

    GetTempPathW(MAX_PATH, dir);
    swprintf(path, MAX_PATH, L"%lsnocode1.sgflt", dir);
    file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(file, "twelve bytes", 12, &written, NULL);
    CloseHandle(file);

    hr = SHCreateStreamOnFileEx(path, STGM_READ, 0, FALSE, NULL, &stm);
    CHECK(hr == S_OK, "a stream on %ls (%#lx)", path, hr);
    if (hr == S_OK)
    {
        filter = NULL;
        hr = pBindStream(stm, NULL, (void **)&filter);
        CHECK(hr == S_OK && filter, "BindIFilterFromStream: the .sgflt filter (%#lx)", hr);
        if (filter)
        {
            IFilter_Init(filter, 0, 0, NULL, &attr);
            IFilter_GetChunk(filter, &chunk);
            CHECK(chunk.idChunk == 12, "it read the stream: %lu bytes", chunk.idChunk);
            IFilter_Release(filter);
        }
        IStream_Release(stm);
    }
    CreateStreamOnHGlobal(NULL, TRUE, &stm);
    hr = pBindStream(stm, NULL, (void **)&filter);
    CHECK(hr == E_FAIL, "a memory stream has no filter (%#lx)", hr);
    IStream_Release(stm);
    CHECK(pBindStream(NULL, NULL, (void **)&filter) == E_INVALIDARG, "no stream");

    /* a compound file of the document class (its CLSID's persistent handler), named without an extension */
    swprintf(path, MAX_PATH, L"%lsnocode1doc", dir);
    hr = StgCreateDocfile(path, STGM_CREATE | STGM_READWRITE | STGM_SHARE_EXCLUSIVE, 0, &stg);
    CHECK(hr == S_OK, "a compound file (%#lx)", hr);
    if (hr == S_OK)
    {
        WriteClassStg(stg, &CLSID_Doc);
        if (SUCCEEDED(IStorage_CreateStream(stg, L"Contents", STGM_CREATE | STGM_WRITE | STGM_SHARE_EXCLUSIVE, 0, 0, &stm)))
        {
            IStream_Write(stm, "abc", 3, NULL);
            IStream_Release(stm);
        }
        IStorage_Commit(stg, 0);
        filter = NULL;
        hr = pBindStorage(stg, NULL, (void **)&filter);
        CHECK(hr == S_OK && filter, "BindIFilterFromStorage: the class's filter (%#lx)", hr);
        if (filter)
        {
            IFilter_Init(filter, 0, 0, NULL, &attr);
            IFilter_GetChunk(filter, &chunk);
            CHECK(chunk.idChunk == 1003, "it read the storage: %lu", chunk.idChunk);
            IFilter_Release(filter);
        }
        { static const CLSID none; WriteClassStg(stg, &none); }
        hr = pBindStorage(stg, NULL, (void **)&filter);
        CHECK(hr == E_FAIL, "no class and no extension: no filter (%#lx)", hr);
        IStorage_Release(stg);
    }
    CHECK(pBindStorage(NULL, NULL, (void **)&filter) == E_INVALIDARG, "no storage");
    CoUninitialize();
}

int main(int argc, char **argv)
{
    test_ktm();
    test_lsa();
    test_netapi();
    test_cfgmgr();
    test_comdb();
    if (argc > 1) test_bind(argv[1]);
    else { failures++; printf("FAIL  no filter DLL given\n"); }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
