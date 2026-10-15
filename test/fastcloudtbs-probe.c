#include <windows.h>
#include <stdio.h>
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
typedef UINT32 (WINAPI *create_t)(const void *, void **);
typedef UINT32 (WINAPI *info_t)(UINT32, void *);
int main(void)
{
    HMODULE m = LoadLibraryA("tbs.dll");
    create_t create; info_t info;
    UINT32 p[2], r, dev[4];
    void *h = (void *)1;
    CHECK(m != NULL, "tbs.dll load");
    if (!m) { printf("RESULT: FAIL\n"); return 1; }
    create = (create_t)GetProcAddress(m, "Tbsi_Context_Create");
    info = (info_t)GetProcAddress(m, "Tbsi_GetDeviceInfo");
    p[0] = 1; p[1] = 0;
    r = create(NULL, &h); CHECK(r == 0x80284002, "NULL params: %08x", r);
    r = create(p, NULL); CHECK(r == 0x80284003, "NULL out: %08x", r);
    p[0] = 0; r = create(p, &h); CHECK(r == 0x80284007, "version 0: %08x", r);
    p[0] = 7; r = create(p, &h); CHECK(r == 0x80284007, "version 7: %08x", r);
    p[0] = 1; h = (void *)1; r = create(p, &h); CHECK(r == 0x8028400f && h == NULL, "no TPM v1: %08x %p", r, h);
    p[0] = 2; p[1] = 4; r = create(p, &h); CHECK(r == 0x8028400f, "no TPM v2: %08x", r);
    r = info(16, NULL); CHECK(r == 0x80284002, "info NULL: %08x", r);
    r = info(8, dev); CHECK(r == 0x80284002, "info small: %08x", r);
    r = info(16, dev); CHECK(r == 0x8028400f, "info no TPM: %08x", r);
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
