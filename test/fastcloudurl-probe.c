#include <windows.h>
#include <wininet.h>
#include <stdio.h>
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
int main(void)
{
    HMODULE m = LoadLibraryA("url.dll");
    BOOL (WINAPI *off)(DWORD) = (void *)GetProcAddress(m, "InetIsOffline");
    DWORD st;
    CHECK(off && !off(0), "online by default");
    st = INTERNET_STATE_DISCONNECTED_BY_USER;
    CHECK(InternetSetOptionW(NULL, INTERNET_OPTION_CONNECTED_STATE, &st, sizeof(st)), "set offline %lu", GetLastError());
    CHECK(off(0), "offline after the user went offline");
    st = INTERNET_STATE_CONNECTED;
    CHECK(InternetSetOptionW(NULL, INTERNET_OPTION_CONNECTED_STATE, &st, sizeof(st)), "set online");
    CHECK(!off(0), "online again");
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
