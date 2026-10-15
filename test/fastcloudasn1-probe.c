#include <windows.h>
#include <psapi.h>
#include <stdio.h>
#include <msasn1.h>
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static SIZE_T priv(void)
{
    PROCESS_MEMORY_COUNTERS_EX pmc = { sizeof(pmc) };
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS *)&pmc, sizeof(pmc));
    return pmc.PagefileUsage;
}
int main(void)
{
    const ASN1GenericFun_t encfn[] = { NULL }, decfn[] = { NULL };
    const ASN1FreeFun_t freefn[] = { NULL };
    const ASN1uint32_t size[] = { 0 };
    ASN1module_t mod = ASN1_CreateModule(ASN1_THIS_VERSION, ASN1_BER_RULE_DER, 0, 1, encfn, decfn, freefn, size, 1);
    ASN1encoding_t enc;
    ASN1decoding_t dec;
    SIZE_T before, after;
    int i;

    CHECK(mod != NULL, "module");
    /* closing NULL is harmless */
    ASN1_CloseEncoder(NULL);
    ASN1_CloseDecoder(NULL);
    /* warm up, then 20000 create/close cycles must not grow the heap */
    for (i = 0; i < 100; i++)
    {
        ASN1_CreateEncoder(mod, &enc, NULL, 0, NULL); ASN1_CloseEncoder(enc);
        ASN1_CreateDecoder(mod, &dec, NULL, 0, NULL); ASN1_CloseDecoder(dec);
    }
    before = priv();
    for (i = 0; i < 20000; i++)
    {
        CHECK(ASN1_CreateEncoder(mod, &enc, NULL, 0, NULL) == ASN1_SUCCESS, "enc");
        ASN1_CloseEncoder(enc);
    }
    after = priv();
    CHECK(after - before < 400000, "encoders leak: %lu bytes", (unsigned long)(after - before));
    before = priv();
    for (i = 0; i < 20000; i++)
    {
        CHECK(ASN1_CreateDecoder(mod, &dec, NULL, 0, NULL) == ASN1_SUCCESS, "dec");
        ASN1_CloseDecoder(dec);
    }
    after = priv();
    CHECK(after - before < 400000, "decoders leak: %lu bytes", (unsigned long)(after - before));
    ASN1_CloseModule(mod);
    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
