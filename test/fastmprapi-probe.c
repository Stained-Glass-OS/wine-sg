#include <windows.h>
#include <stdio.h>
#include <string.h>
#ifndef ERROR_NO_SUCH_INTERFACE
#define ERROR_NO_SUCH_INTERFACE 905
#endif

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef DWORD (WINAPI *f1)(void *);
typedef DWORD (WINAPI *f2)(void *, void *);

int main(void)
{
    HMODULE m = LoadLibraryA("mprapi.dll");
#define GET(n) void *n = (void *)GetProcAddress(m, #n)
    GET(MprInfoCreate); GET(MprInfoDelete); GET(MprInfoBlockAdd); GET(MprInfoBlockSet); GET(MprInfoBlockFind);
    GET(MprInfoBlockRemove); GET(MprInfoBlockQuerySize); GET(MprInfoDuplicate); GET(MprInfoRemoveAll);
    GET(MprAdminGetErrorString); GET(MprAdminBufferFree); GET(MprAdminIsServiceRunning); GET(MprAdminServerConnect);
    GET(MprConfigServerConnect); GET(MprConfigServerDisconnect); GET(MprConfigInterfaceEnum); GET(MprConfigInterfaceGetHandle);
    GET(MprConfigTransportGetHandle); GET(MprAdminInterfaceEnum); GET(MprConfigBufferFree);
    void *hdr = NULL, *h2 = NULL;
    DWORD r, size, count, i1 = 11, i2 = 22;
    BYTE *data;
    WCHAR *text = NULL;
    HANDLE cfg = NULL;

    CHECK(MprInfoCreate && MprAdminGetErrorString && MprConfigServerConnect && MprAdminInterfaceEnum, "exports");

    /* information blocks */
    r = ((DWORD (WINAPI *)(DWORD, void **))MprInfoCreate)(1, &hdr);
    CHECK(r == 0 && hdr, "MprInfoCreate %lu", r);
    CHECK(((DWORD (WINAPI *)(DWORD, void **))MprInfoCreate)(7, &h2) == ERROR_INVALID_PARAMETER, "bad version");
    {
        DWORD (WINAPI *add)(void *, DWORD, DWORD, DWORD, BYTE *, void **) = MprInfoBlockAdd;
        DWORD (WINAPI *set)(void *, DWORD, DWORD, DWORD, BYTE *, void **) = MprInfoBlockSet;
        DWORD (WINAPI *find)(void *, DWORD, DWORD *, DWORD *, BYTE **) = MprInfoBlockFind;
        DWORD (WINAPI *rem)(void *, DWORD, void **) = MprInfoBlockRemove;
        DWORD (WINAPI *qsize)(void *) = MprInfoBlockQuerySize;
        DWORD (WINAPI *dup)(void *, void **) = MprInfoDuplicate;
        DWORD (WINAPI *rall)(void *, void **) = MprInfoRemoveAll;
        void *hdr3 = NULL;

        r = add(hdr, 100, 4, 1, (BYTE *)&i1, &h2);
        CHECK(r == 0 && h2, "add block %lu", r);
        MprInfoDelete ? ((f1)MprInfoDelete)(hdr) : 0;
        hdr = h2; h2 = NULL;
        r = add(hdr, 100, 4, 1, (BYTE *)&i1, &h2);
        CHECK(r == ERROR_ALREADY_EXISTS, "duplicate block %lu", r);
        r = add(hdr, 200, 4, 1, (BYTE *)&i2, &h2);
        CHECK(r == 0, "second block %lu", r);
        ((f1)MprInfoDelete)(hdr);
        hdr = h2; h2 = NULL;
        r = find(hdr, 200, &size, &count, &data);
        CHECK(r == 0 && size == 4 && count == 1 && data && *(DWORD *)data == 22, "find 200 %lu", r);
        r = find(hdr, 100, &size, &count, &data);
        CHECK(r == 0 && *(DWORD *)data == 11, "find 100");
        CHECK(find(hdr, 300, &size, &count, &data) == ERROR_NOT_FOUND, "find missing");
        {
            DWORD arr[3] = { 1, 2, 3 };
            r = set(hdr, 100, 4, 3, (BYTE *)arr, &h2);
            CHECK(r == 0, "set replaces %lu", r);
            ((f1)MprInfoDelete)(hdr);
            hdr = h2; h2 = NULL;
            r = find(hdr, 100, &size, &count, &data);
            CHECK(r == 0 && count == 3 && ((DWORD *)data)[2] == 3, "replaced block");
            r = find(hdr, 200, &size, &count, &data);
            CHECK(r == 0 && *(DWORD *)data == 22, "other block kept");
        }
        r = dup(hdr, &hdr3);
        CHECK(r == 0 && qsize(hdr3) == qsize(hdr), "duplicate");
        ((f1)MprInfoDelete)(hdr3);
        r = rem(hdr, 100, &h2);
        CHECK(r == 0, "remove %lu", r);
        ((f1)MprInfoDelete)(hdr);
        hdr = h2; h2 = NULL;
        CHECK(find(hdr, 100, NULL, NULL, NULL) == ERROR_NOT_FOUND, "removed");
        CHECK(rem(hdr, 100, &h2) == ERROR_NOT_FOUND, "remove missing");
        r = rall(hdr, &h2);
        CHECK(r == 0 && find(h2, 200, NULL, NULL, NULL) == ERROR_NOT_FOUND, "remove all");
        ((f1)MprInfoDelete)(h2);
        ((f1)MprInfoDelete)(hdr);
    }

    /* error strings */
    r = ((DWORD (WINAPI *)(DWORD, WCHAR **))MprAdminGetErrorString)(905, &text);
    CHECK(r == 0 && text && text[0], "error string 905");
    ((f1)MprAdminBufferFree)(text);
    r = ((DWORD (WINAPI *)(DWORD, WCHAR **))MprAdminGetErrorString)(0x7ffff, &text);
    CHECK(r == 317 && !text, "unknown error code %lu", r);

    /* servers */
    CHECK(((BOOL (WINAPI *)(WCHAR *))MprAdminIsServiceRunning)(NULL) == FALSE, "service not running");
    {
        HANDLE hs = (HANDLE)1;
        r = ((DWORD (WINAPI *)(WCHAR *, HANDLE *))MprAdminServerConnect)(NULL, &hs);
        CHECK(r == RPC_S_SERVER_UNAVAILABLE && !hs, "MprAdminServerConnect %lu", r);
    }
    r = ((DWORD (WINAPI *)(HANDLE, DWORD, BYTE **, DWORD, DWORD *, DWORD *, DWORD *))MprAdminInterfaceEnum)(NULL, 0, NULL, 0, NULL, NULL, NULL);
    CHECK(r == ERROR_INVALID_HANDLE, "admin call without a server %lu", r);
    r = ((DWORD (WINAPI *)(WCHAR *, HANDLE *))MprConfigServerConnect)(NULL, &cfg);
    CHECK(r == 0 && cfg, "MprConfigServerConnect %lu", r);
    r = ((DWORD (WINAPI *)(WCHAR *, HANDLE *))MprConfigServerConnect)((WCHAR *)L"remote-host-xyz", &cfg);
    CHECK(r == RPC_S_SERVER_UNAVAILABLE, "remote config server %lu", r);
    r = ((DWORD (WINAPI *)(WCHAR *, HANDLE *))MprConfigServerConnect)(NULL, &cfg);
    {
        BYTE *buf = (BYTE *)1;
        DWORD read = 9, total = 9, resume = 9;
        HANDLE hi = (HANDLE)1;
        r = ((DWORD (WINAPI *)(HANDLE, DWORD, BYTE **, DWORD, DWORD *, DWORD *, DWORD *))MprConfigInterfaceEnum)(cfg, 0, &buf, 0xffffffff, &read, &total, &resume);
        CHECK(r == 0 && !buf && read == 0 && total == 0, "empty interface list %lu", r);
        r = ((DWORD (WINAPI *)(HANDLE, DWORD, BYTE **, DWORD, DWORD *, DWORD *, DWORD *))MprConfigInterfaceEnum)(cfg, 5, &buf, 0xffffffff, &read, &total, &resume);
        CHECK(r == ERROR_INVALID_LEVEL, "bad level %lu", r);
        r = ((DWORD (WINAPI *)(HANDLE, WCHAR *, HANDLE *))MprConfigInterfaceGetHandle)(cfg, (WCHAR *)L"Nope", &hi);
        CHECK(r == 905 && !hi, "no such interface %lu", r);
        r = ((DWORD (WINAPI *)(HANDLE, DWORD, HANDLE *))MprConfigTransportGetHandle)(cfg, 0x21, &hi);
        CHECK(r == 902, "no such transport %lu", r);
    }
    /* IP-in-IP interface friendly names */
    {
        typedef struct { WCHAR name[257]; GUID guid; } II0;
        II0 a, b, *list = NULL;
        DWORD n = 77, (WINAPI *create)(HANDLE, II0 *) = (void *)GetProcAddress(m, "MprSetupIpInIpInterfaceFriendlyNameCreate");
        DWORD (WINAPI *del)(HANDLE, GUID *) = (void *)GetProcAddress(m, "MprSetupIpInIpInterfaceFriendlyNameDelete");
        DWORD (WINAPI *enm)(HANDLE, BYTE **, DWORD *) = (void *)GetProcAddress(m, "MprSetupIpInIpInterfaceFriendlyNameEnum");
        DWORD (WINAPI *ffree)(void *) = (void *)GetProcAddress(m, "MprSetupIpInIpInterfaceFriendlyNameFree");
        memset(&a, 0, sizeof(a)); memset(&b, 0, sizeof(b));
        lstrcpyW(a.name, L"Tunnel A"); a.guid.Data1 = 0xa1a1a1a1; a.guid.Data4[7] = 1;
        lstrcpyW(b.name, L"Tunnel B"); b.guid.Data1 = 0xb2b2b2b2; b.guid.Data4[7] = 2;
        del(cfg, &a.guid); del(cfg, &b.guid);
        r = enm(cfg, (BYTE **)&list, &n); CHECK(r == 0 && n == 0, "ipinip enum empty %lu %lu", r, n);
        CHECK(create(NULL, &a) == ERROR_INVALID_HANDLE, "ipinip create bad handle");
        CHECK(create(cfg, NULL) == ERROR_INVALID_PARAMETER, "ipinip create NULL");
        r = create(cfg, &a); CHECK(r == 0, "ipinip create A %lu", r);
        r = create(cfg, &b); CHECK(r == 0, "ipinip create B %lu", r);
        b.guid.Data1 = 0xc3c3c3c3; r = create(cfg, &b); CHECK(r == ERROR_ALREADY_EXISTS, "ipinip duplicate name %lu", r);
        r = enm(cfg, (BYTE **)&list, &n); CHECK(r == 0 && n == 2 && list, "ipinip enum two %lu %lu", r, n);
        if (list && n == 2) {
            int ka = !lstrcmpW(list[0].name, L"Tunnel A") || !lstrcmpW(list[1].name, L"Tunnel A");
            int kg = list[0].guid.Data1 == 0xa1a1a1a1 || list[1].guid.Data1 == 0xa1a1a1a1;
            CHECK(ka && kg, "ipinip entries carry name and guid");
        }
        if (list) ffree(list);
        r = del(cfg, &a.guid); CHECK(r == 0, "ipinip delete A %lu", r);
        r = del(cfg, &a.guid); CHECK(r == ERROR_NO_SUCH_INTERFACE, "ipinip delete twice %lu", r);
        b.guid.Data1 = 0xb2b2b2b2; del(cfg, &b.guid);
        r = enm(cfg, (BYTE **)&list, &n); CHECK(r == 0 && n == 0, "ipinip enum empty again %lu %lu", r, n);
    }
    /* the calls that need the router service or a domain */
    {
        DWORD (WINAPI *ugi)(const WCHAR *, const WCHAR *, DWORD, BYTE *) = (void *)GetProcAddress(m, "MprAdminUserGetInfo");
        DWORD (WINAPI *gc)(WCHAR *, WCHAR *, WCHAR *, WCHAR *, WCHAR *) = (void *)GetProcAddress(m, "MprAdminInterfaceGetCredentials");
        DWORD (WINAPI *dom)(WCHAR *, WCHAR *, BOOL *) = (void *)GetProcAddress(m, "MprAdminIsDomainRasServer");
        DWORD (WINAPI *inst)(DWORD, void *) = (void *)GetProcAddress(m, "MprConfigServerInstall");
        BYTE buf[64]; BOOL isras = TRUE; WCHAR w[8] = {0};
        CHECK(ugi(NULL, NULL, 0, buf) == ERROR_INVALID_PARAMETER, "UserGetInfo NULL user");
        CHECK(ugi(NULL, L"bob", 9, buf) == ERROR_INVALID_LEVEL, "UserGetInfo level 9");
        CHECK(ugi(NULL, L"bob", 0, buf) == RPC_S_SERVER_UNAVAILABLE, "UserGetInfo without the service");
        CHECK(gc(NULL, NULL, w, w, w) == ERROR_INVALID_PARAMETER, "GetCredentials NULL interface");
        CHECK(gc(NULL, L"if", w, w, w) == RPC_S_SERVER_UNAVAILABLE, "GetCredentials without the service");
        CHECK(dom(NULL, L"m", &isras) == ERROR_INVALID_PARAMETER, "IsDomainRasServer NULL domain");
        r = dom(L"dom", L"m", &isras); CHECK(r == ERROR_NO_SUCH_DOMAIN && !isras, "IsDomainRasServer %lu", r);
        CHECK(inst(1, NULL) == ERROR_INVALID_PARAMETER && inst(7, buf) == ERROR_INVALID_LEVEL && inst(1, buf) == ERROR_NOT_SUPPORTED, "ServerInstall");
    }
    ((void (WINAPI *)(HANDLE))MprConfigServerDisconnect)(cfg);
    r = ((DWORD (WINAPI *)(HANDLE, DWORD, BYTE **, DWORD, DWORD *, DWORD *, DWORD *))MprConfigInterfaceEnum)(cfg, 0, NULL, 0, NULL, NULL, NULL);
    CHECK(r == ERROR_INVALID_HANDLE, "closed config handle %lu", r);

    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
