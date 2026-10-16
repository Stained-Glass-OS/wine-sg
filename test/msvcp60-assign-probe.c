/* Smoke probe for patches/sg/4565 (fast mode): msvcp60's _Lockit and
 * locale::id assignment operators (_Lockit has no data in VC 6;
 * locale::id copies the id). Lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <stdio.h>

#ifdef _WIN64
#define THISCALL
#define N(a, b) a
#else
#define THISCALL __attribute__((thiscall))
#define N(a, b) b
#endif

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

int main(void)
{
    static const size_t ids[] = {0, 1, 42, 0x7fffffff};
    HMODULE mod = LoadLibraryA("msvcp60.dll");
    void *(THISCALL *lockit_assign)(void *, const void *);
    size_t *(THISCALL *id_assign)(size_t *, const size_t *);
    unsigned int i;

    if (!mod) { printf("FAIL load\nRESULT: FAIL\n"); return 1; }
    lockit_assign = (void *)GetProcAddress(mod, N("??4_Lockit@std@@QEAAAEAV01@AEBV01@@Z", "??4_Lockit@std@@QAEAAV01@ABV01@@Z"));
    id_assign = (void *)GetProcAddress(mod, N("??4id@locale@std@@QEAAAEAV012@AEBV012@@Z", "??4id@locale@std@@QAEAAV012@ABV012@@Z"));
    check(lockit_assign && id_assign, "exports");
    if (failures) { printf("RESULT: FAIL\n"); return 1; }
    {
        char a = 0x11, b = 0x22;

        check(lockit_assign(&a, &b) == &a && a == 0x11, "_Lockit::operator= returns this, changes nothing");
    }
    for (i = 0; i < ARRAYSIZE(ids); i++)
    {
        size_t a = 99, b = ids[i];

        check(id_assign(&a, &b) == &a && a == ids[i] && b == ids[i], "locale::id = %Iu", ids[i]);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
