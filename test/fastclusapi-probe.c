#include <windows.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    HMODULE m = LoadLibraryA("clusapi.dll");
    DWORD (WINAPI *state)(LPCWSTR, DWORD *) = (void *)GetProcAddress(m, "GetNodeClusterState");
    HANDLE (WINAPI *open)(LPCWSTR) = (void *)GetProcAddress(m, "OpenCluster");
    BOOL (WINAPI *close)(HANDLE) = (void *)GetProcAddress(m, "CloseCluster");
    HANDLE (WINAPI *opennode)(HANDLE, LPCWSTR) = (void *)GetProcAddress(m, "OpenClusterNode");
    DWORD (WINAPI *nodestate)(HANDLE) = (void *)GetProcAddress(m, "GetClusterNodeState");
    DWORD (WINAPI *info)(HANDLE, LPWSTR, LPDWORD, void *) = (void *)GetProcAddress(m, "GetClusterInformation");
    LONG (WINAPI *regopen)(HANDLE, LPCWSTR, DWORD, HANDLE *) = (void *)GetProcAddress(m, "ClusterRegOpenKey");
    HANDLE (WINAPI *openenum)(HANDLE, DWORD) = (void *)GetProcAddress(m, "ClusterOpenEnum");
    DWORD (WINAPI *getenumcount)(HANDLE) = (void *)GetProcAddress(m, "ClusterGetEnumCount");
    DWORD st = 99, r;
    HANDLE h;

    CHECK(state && open && close && opennode && nodestate && info && regopen && openenum && getenumcount, "exports");
    r = state(NULL, &st);
    CHECK(r == ERROR_SUCCESS && st == 0, "GetNodeClusterState %lu %lu", r, st);
    CHECK(state(NULL, NULL) == ERROR_INVALID_PARAMETER, "null state pointer");
    SetLastError(0);
    h = open(NULL);
    CHECK(h == NULL && GetLastError() == RPC_S_SERVER_UNAVAILABLE, "OpenCluster: %p %lu", h, GetLastError());
    SetLastError(0);
    CHECK(!close(NULL) && GetLastError() == ERROR_INVALID_HANDLE, "CloseCluster");
    SetLastError(0);
    CHECK(opennode(NULL, L"node") == NULL && GetLastError() == ERROR_INVALID_HANDLE, "OpenClusterNode");
    SetLastError(0);
    CHECK(nodestate(NULL) == (DWORD)-1 && GetLastError() == ERROR_INVALID_HANDLE, "GetClusterNodeState");
    CHECK(info(NULL, NULL, NULL, NULL) == ERROR_INVALID_HANDLE, "GetClusterInformation");
    CHECK(regopen(NULL, L"x", 0, &h) == ERROR_INVALID_HANDLE, "ClusterRegOpenKey");
    SetLastError(0);
    CHECK(openenum(NULL, 1) == NULL && GetLastError() == ERROR_INVALID_HANDLE, "ClusterOpenEnum");
    CHECK(getenumcount(NULL) == ERROR_INVALID_HANDLE, "ClusterGetEnumCount");

    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
