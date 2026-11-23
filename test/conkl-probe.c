/* wine-sg 9711: GetConsoleKeyboardLayoutNameA/W give the console's input locale identifier (eight hex
 * digits, the active layout); the stub said TRUE and wrote nothing. */
#include <windows.h>
#include <stdio.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("PASS  "); else { printf("FAIL  "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)
typedef BOOL (WINAPI *klw_t)(LPWSTR);
typedef BOOL (WINAPI *kla_t)(LPSTR);

int main(void)
{
    HMODULE k32 = GetModuleHandleA("kernel32");
    klw_t klw = (klw_t)GetProcAddress(k32, "GetConsoleKeyboardLayoutNameW");
    kla_t kla = (kla_t)GetProcAddress(k32, "GetConsoleKeyboardLayoutNameA");
    WCHAR name[KL_NAMELENGTH] = L"XXXXXXXX", user[KL_NAMELENGTH] = L"";
    char nameA[KL_NAMELENGTH] = "XXXXXXXX";
    BOOL ret;
    int i, hex = 1;

    ret = klw(name);
    for (i = 0; i < 8; i++) if (!iswxdigit(name[i])) hex = 0;
    CHECK(ret && hex && !name[8], "GetConsoleKeyboardLayoutNameW: eight hex digits (%ls)", name);
    GetKeyboardLayoutNameW(user);
    CHECK(!lstrcmpiW(name, user), "the active layout (GetKeyboardLayoutName: %ls)", user);
    ret = kla(nameA);
    CHECK(ret && !lstrcmpiA(nameA, "XXXXXXXX") == 0 && strlen(nameA) == 8, "GetConsoleKeyboardLayoutNameA (%s)", nameA);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
