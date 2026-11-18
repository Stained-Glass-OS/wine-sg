/* sweepfix1-probe.c: the first fixes from the unimplemented-import sweep (wine-sg 7853,
 * outbox/import-sweep.md). Each function was an "unimplemented function" import or a `@ stub` that
 * aborted the calling program; each now answers as documented. Resolved with GetProcAddress (the
 * export exists), 64- and 32-bit. Prints PASS/FAIL and RESULT. */
#define SECURITY_WIN32
#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2ipdef.h>
#include <windows.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <sspi.h>
#include <schannel.h>
#include <ncrypt.h>
#include <wininet.h>
#include <wlanapi.h>
#include <rpc.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  " __VA_ARGS__); printf("\n"); } \
                           else { printf("PASS  " __VA_ARGS__); printf("\n"); } } while (0)
#define GET(mod, name) GetProcAddress(LoadLibraryA(mod), name)

int main(void)
{
    /* ncrypt: no attestation claims here */
    {
        SECURITY_STATUS (WINAPI *p)(NCRYPT_KEY_HANDLE, NCRYPT_KEY_HANDLE, DWORD, void *, BYTE *, DWORD, DWORD *, DWORD) =
            (void *)GET("ncrypt.dll", "NCryptCreateClaim");
        DWORD result = 0xdead;
        SECURITY_STATUS st = p ? p(1, 2, 0, NULL, NULL, 0, &result, 0) : -1;
        CHECK(p && st == NTE_NOT_SUPPORTED && result == 0, "NCryptCreateClaim: NTE_NOT_SUPPORTED, no claim (%#lx)", st);
    }
    /* secur32/sspicli SetCredentialsAttributes */
    {
        SECURITY_STATUS (WINAPI *p)(PCredHandle, ULONG, void *, ULONG) = (void *)GET("secur32.dll", "SetCredentialsAttributesW");
        SECURITY_STATUS (WINAPI *pa)(PCredHandle, ULONG, void *, ULONG) = (void *)GET("secur32.dll", "SetCredentialsAttributesA");
        void *ps = GET("sspicli.dll", "SetCredentialsAttributesW");
        SCHANNEL_CRED sc = { SCHANNEL_CRED_VERSION };
        CredHandle cred;
        TimeStamp ts;
        ULONG value = 0;
        SECURITY_STATUS st;
        sc.dwFlags = SCH_CRED_NO_DEFAULT_CREDS | SCH_CRED_MANUAL_CRED_VALIDATION;
        st = AcquireCredentialsHandleW(NULL, (WCHAR *)UNISP_NAME_W, SECPKG_CRED_OUTBOUND, NULL, &sc, NULL, NULL, &cred, &ts);
        CHECK(p && pa && ps, "SetCredentialsAttributesW/A exported (secur32 and sspicli)");
        if (p && st == SEC_E_OK)
        {
            st = p(&cred, 1 /* SECPKG_CRED_ATTR_NAMES */, &value, sizeof(value));
            CHECK(st == SEC_E_UNSUPPORTED_FUNCTION, "SetCredentialsAttributesW on credentials: SEC_E_UNSUPPORTED_FUNCTION (%#lx)", st);
            FreeCredentialsHandle(&cred);
        }
        st = p ? p(NULL, 1, &value, sizeof(value)) : -1;
        CHECK(st == SEC_E_INVALID_HANDLE, "SetCredentialsAttributesW without credentials: SEC_E_INVALID_HANDLE (%#lx)", st);
    }
    /* iphlpapi ResolveIpNetEntry2 */
    {
        DWORD (WINAPI *p)(MIB_IPNET_ROW2 *, const SOCKADDR_INET *) = (void *)GET("iphlpapi.dll", "ResolveIpNetEntry2");
        MIB_IPNET_ROW2 row;
        MIB_IPNET_TABLE2 *table = NULL;
        DWORD err, i;
        CHECK(p != NULL, "ResolveIpNetEntry2 exported");
        if (p)
        {
            CHECK(p(NULL, NULL) == ERROR_INVALID_PARAMETER, "ResolveIpNetEntry2(NULL): ERROR_INVALID_PARAMETER");
            memset(&row, 0, sizeof(row));
            row.Address.Ipv4.sin_family = AF_INET;
            row.Address.Ipv4.sin_addr.s_addr = htonl(0xc0000201);   /* 192.0.2.1, TEST-NET-1 */
            err = p(&row, NULL);
            CHECK(err == ERROR_BAD_NET_NAME, "ResolveIpNetEntry2 of an address no neighbor has: ERROR_BAD_NET_NAME (%lu)", err);
            if (!GetIpNetTable2(AF_INET, &table))
            {
                for (i = 0; i < table->NumEntries; i++)
                    if (table->Table[i].State >= NlnsReachable && table->Table[i].PhysicalAddressLength) break;
                if (i < table->NumEntries)
                {
                    memset(&row, 0, sizeof(row));
                    row.Address = table->Table[i].Address;
                    err = p(&row, NULL);
                    CHECK(!err && row.PhysicalAddressLength == table->Table[i].PhysicalAddressLength &&
                          !memcmp(row.PhysicalAddress, table->Table[i].PhysicalAddress, row.PhysicalAddressLength),
                          "ResolveIpNetEntry2 of a neighbor gives its link-layer address (%lu)", err);
                }
                else printf("NOTE  no reachable IPv4 neighbor to resolve\n");
                FreeMibTable(table);
            }
        }
    }
    /* rpcrt4 I_RpcOpenClientProcess */
    {
        RPC_STATUS (RPC_ENTRY *p)(RPC_BINDING_HANDLE, ULONG, void **) = (void *)GET("rpcrt4.dll", "I_RpcOpenClientProcess");
        void *process = (void *)1;
        CHECK(p != NULL, "I_RpcOpenClientProcess exported");
        if (p)
        {
            RPC_STATUS st = p(NULL, PROCESS_QUERY_LIMITED_INFORMATION, &process);
            CHECK(st == RPC_S_NO_CALL_ACTIVE && !process, "I_RpcOpenClientProcess outside a call: RPC_S_NO_CALL_ACTIVE (%ld)", st);
            CHECK(p(NULL, 0, NULL) == RPC_S_INVALID_ARG, "I_RpcOpenClientProcess without an out pointer: RPC_S_INVALID_ARG");
        }
    }
    /* mf MFCreateTranscodeTopology */
    {
        HRESULT (WINAPI *p)(void *, const WCHAR *, void *, void **) = (void *)GET("mf.dll", "MFCreateTranscodeTopology");
        void *topo = (void *)1;
        CHECK(p != NULL, "MFCreateTranscodeTopology exported");
        if (p)
        {
            CHECK(p(NULL, NULL, NULL, NULL) == E_POINTER, "MFCreateTranscodeTopology without an out pointer: E_POINTER");
            CHECK(p(NULL, L"x.mp4", NULL, &topo) == E_INVALIDARG && !topo, "MFCreateTranscodeTopology without a source: E_INVALIDARG, no topology");
        }
    }
    /* fwpuclnt: no engine, nothing added (fail closed) */
    {
        static const char *names[] = { "FwpmFilterDeleteById0", "FwpmProviderAdd0", "FwpmTransactionBegin0",
                                       "FwpmTransactionCommit0", "FwpmTransactionAbort0", "FwpmSubLayerDestroyEnumHandle0" };
        DWORD (WINAPI *create)(HANDLE, const void *, HANDLE *) = (void *)GET("fwpuclnt.dll", "FwpmSubLayerCreateEnumHandle0");
        DWORD (WINAPI *appid)(const WCHAR *, void **) = (void *)GET("fwpuclnt.dll", "FwpmGetAppIdFromFileName0");
        DWORD (WINAPI *begin)(HANDLE, UINT32) = (void *)GET("fwpuclnt.dll", "FwpmTransactionBegin0");
        DWORD (WINAPI *provider)(HANDLE, const void *, void *) = (void *)GET("fwpuclnt.dll", "FwpmProviderAdd0");
        HANDLE h = (HANDLE)1;
        void *blob = (void *)1;
        unsigned int i, n = 0;
        for (i = 0; i < ARRAYSIZE(names); i++) n += GET("fwpuclnt.dll", names[i]) != NULL;
        CHECK(n == ARRAYSIZE(names) && create && appid && GET("fwpuclnt.dll", "FwpmSubLayerEnum0"), "the Fwpm filter/provider/transaction/sublayer functions are exported");
        CHECK(begin && begin(NULL, 0) == RPC_S_CANNOT_SUPPORT, "FwpmTransactionBegin0: RPC_S_CANNOT_SUPPORT, as FwpmEngineOpen0");
        CHECK(provider && provider(NULL, NULL, NULL) == RPC_S_CANNOT_SUPPORT, "FwpmProviderAdd0 adds nothing: RPC_S_CANNOT_SUPPORT");
        CHECK(create && create(NULL, NULL, &h) == RPC_S_CANNOT_SUPPORT && !h, "FwpmSubLayerCreateEnumHandle0: no handle");
        CHECK(appid && appid(L"C:\\\\windows\\\\notepad.exe", &blob) == RPC_S_CANNOT_SUPPORT && !blob, "FwpmGetAppIdFromFileName0: no id");
    }
    /* wlanapi WlanGetNetworkBssList */
    {
        DWORD (WINAPI *p)(HANDLE, const GUID *, const DOT11_SSID *, DOT11_BSS_TYPE, BOOL, void *, WLAN_BSS_LIST **) =
            (void *)GET("wlanapi.dll", "WlanGetNetworkBssList");
        GUID none = {0x12345678, 1, 2, {3}};
        WLAN_BSS_LIST *list = NULL;
        HANDLE h = NULL;
        DWORD ver, err;
        CHECK(p != NULL, "WlanGetNetworkBssList exported");
        if (p)
        {
            CHECK(p(NULL, &none, NULL, dot11_BSS_type_any, FALSE, NULL, &list) == ERROR_INVALID_PARAMETER,
                  "WlanGetNetworkBssList without a handle: ERROR_INVALID_PARAMETER");
            if (!WlanOpenHandle(2, NULL, &ver, &h))
            {
                CHECK(p(h, &none, NULL, dot11_BSS_type_any, FALSE, NULL, NULL) == ERROR_INVALID_PARAMETER,
                      "WlanGetNetworkBssList without an out pointer: ERROR_INVALID_PARAMETER");
                err = p(h, &none, NULL, dot11_BSS_type_any, FALSE, NULL, &list);
                CHECK(err == ERROR_NOT_FOUND || err == ERROR_SERVICE_NOT_ACTIVE, "WlanGetNetworkBssList of an unknown adapter: not found (%lu)", err);
                WlanCloseHandle(h, NULL);
            }
            else printf("NOTE  WlanOpenHandle failed\n");
        }
    }
    /* wininet InternetSetCookieEx2 */
    {
        DWORD (WINAPI *p)(const WCHAR *, const INTERNET_COOKIE2 *, const WCHAR *, DWORD, DWORD *) =
            (void *)GET("wininet.dll", "InternetSetCookieEx2");
        INTERNET_COOKIE2 c;
        WCHAR buf[256];
        DWORD size = ARRAYSIZE(buf), state = 99, err;
        SYSTEMTIME st;
        CHECK(p != NULL, "InternetSetCookieEx2 exported");
        if (p)
        {
            memset(&c, 0, sizeof(c));
            c.pwszName = (WCHAR *)L"sgsweep";
            c.pwszValue = (WCHAR *)L"v1";
            c.pwszPath = (WCHAR *)L"/";
            GetSystemTime(&st);
            st.wYear += 1;
            SystemTimeToFileTime(&st, &c.ftExpires);
            c.fExpiresSet = TRUE;
            err = p(L"http://sweep.example.test/", &c, NULL, 0, &state);
            CHECK(!err && state == COOKIE_STATE_ACCEPT, "InternetSetCookieEx2 sets a cookie (%lu, state %lu)", err, state);
            buf[0] = 0;
            InternetGetCookieW(L"http://sweep.example.test/page", NULL, buf, &size);
            CHECK(wcsstr(buf, L"sgsweep=v1") != NULL, "and InternetGetCookie reads it back (%ls)", buf);
            c.pwszName = NULL;
            CHECK(p(L"http://sweep.example.test/", &c, NULL, 0, &state) == ERROR_INVALID_PARAMETER,
                  "InternetSetCookieEx2 without a name: ERROR_INVALID_PARAMETER");
        }
    }
    puts(failures ? "RESULT: FAIL" : "RESULT: PASS");
    return failures != 0;
}
