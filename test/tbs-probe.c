/* tbs-gate.sh's probe (5517): the TBS entry points without a TPM. */
#include <windows.h>
#include <stdio.h>

typedef UINT32 TBS_RESULT;
typedef struct { UINT32 version; } PARAMS;
TBS_RESULT WINAPI Tbsi_Context_Create(const PARAMS *, void **);
TBS_RESULT WINAPI Tbsi_GetDeviceInfo(UINT32, void *);

static int bad;
static void check(const char *name, TBS_RESULT got, TBS_RESULT want)
{
    if (got == want) printf("ok %s\n", name);
    else { printf("FAIL %s got %08lx want %08lx\n", name, (unsigned long)got, (unsigned long)want); bad = 1; }
}

int main(void)
{
    HMODULE tbs = LoadLibraryA("tbs.dll");
    TBS_RESULT (WINAPI *create)(const PARAMS *, void **) = (void *)GetProcAddress(tbs, "Tbsi_Context_Create");
    TBS_RESULT (WINAPI *info)(UINT32, void *) = (void *)GetProcAddress(tbs, "Tbsi_GetDeviceInfo");
    PARAMS p = {2};
    void *ctx = NULL;
    BYTE buf[16];

    if (!create || !info) { printf("FAIL tbs.dll missing\n"); return 1; }
    check("create-null-output", create(&p, NULL), 0x80284003);
    /* NULL params: TBS_E_BAD_PARAMETER, superseded by fast-a 3237 at integration (both codes are
     * documented for Tbsi_Context_Create, the order is a guess; RECONCILE-CLOUD kept 3237's) */
    check("create-null-params", create(NULL, &ctx), 0x80284002);
    p.version = 0; check("create-version-0", create(&p, &ctx), 0x80284007);
    p.version = 3; check("create-version-3", create(&p, &ctx), 0x80284007);
    p.version = 1; check("create-tpm12-no-tpm", create(&p, &ctx), 0x8028400F);
    p.version = 2; check("create-tpm20-no-tpm", create(&p, &ctx), 0x8028400F);
    check("info-null", info(sizeof(buf), NULL), 0x80284002);
    check("info-small", info(8, buf), 0x80284002);
    check("info-no-tpm", info(sizeof(buf), buf), 0x8028400F);
    printf(bad ? "tbs-probe: FAILED\n" : "tbs-probe: all ok\n");
    return bad;
}
