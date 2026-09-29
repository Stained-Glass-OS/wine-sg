/* sppc-gate.sh's probe (0491): the Software Licensing client's store.
 * Prints one line per step: "NAME HR [ID]". */
#include <windows.h>
#include <stdio.h>

typedef GUID SLID;
typedef void *HSLC;
HRESULT WINAPI SLOpen(HSLC *);
HRESULT WINAPI SLClose(HSLC);
HRESULT WINAPI SLInstallLicense(HSLC, UINT, const BYTE *, SLID *);
HRESULT WINAPI SLUninstallLicense(HSLC, const SLID *);
HRESULT WINAPI SLInstallProofOfPurchase(HSLC, const WCHAR *, const WCHAR *, UINT, BYTE *, SLID *);
HRESULT WINAPI SLUninstallProofOfPurchase(HSLC, const SLID *);
HRESULT WINAPI SLGetLicensingStatusInformation(HSLC, const SLID *, const SLID *, const WCHAR *, UINT *, void **);
HRESULT WINAPI SLGetLicenseFileId(HSLC, UINT, const BYTE *, SLID *);
HRESULT WINAPI SLGetLicense(HSLC, const SLID *, UINT *, BYTE **);
HRESULT WINAPI SLGetPKeyId(HSLC, const WCHAR *, const WCHAR *, UINT, BYTE *, SLID *);
HRESULT WINAPI SLLoadApplicationPolicies(const SLID *, const SLID *, DWORD, void **);
HRESULT WINAPI SLGetApplicationPolicy(void *, const WCHAR *, UINT *, UINT *, BYTE **);
HRESULT WINAPI SLUnloadApplicationPolicies(void *, DWORD);
HRESULT WINAPI SLGetPolicyInformationDWORD(HSLC, const WCHAR *, DWORD *);
HRESULT WINAPI SLConsumeRight(HSLC, const SLID *, const SLID *, const WCHAR *, void *);

static void id(const char *name, HRESULT hr, const SLID *g)
{
    printf("%s %08lx {%08lX-%04X-%04X}\n", name, hr, g->Data1, g->Data2, g->Data3);
}

int main(void)
{
    static const char lic[] = "<?xml version=\"1.0\"?><rg:licenseGroup xmlns:rg=\"urn:test\"/>";
    static const char other[] = "<?xml version=\"1.0\"?><rg:licenseGroup xmlns:rg=\"urn:other\"/>";
    SLID a = {0}, b = {0}, c = {0}, k = {0}, app = {0};
    UINT n = 0;
    void *status = NULL;
    HSLC h;

    printf("open %08lx\n", SLOpen(&h));
    id("install", SLInstallLicense(h, sizeof(lic), (const BYTE *)lic, &a), &a);
    id("again", SLInstallLicense(h, sizeof(lic), (const BYTE *)lic, &b), &b);
    id("other", SLInstallLicense(h, sizeof(other), (const BYTE *)other, &c), &c);
    printf("same %d different %d\n", IsEqualGUID(&a, &b), !IsEqualGUID(&a, &c));
    {
        SLID f = {0};
        UINT size = 0;
        BYTE *blob = NULL;
        HRESULT hr = SLGetLicenseFileId(h, sizeof(lic), (const BYTE *)lic, &f);
        printf("fileid %08lx %d\n", hr, IsEqualGUID(&f, &a));
        hr = SLGetLicense(h, &a, &size, &blob);
        printf("getlicense %08lx %d\n", hr, size == sizeof(lic) && blob && !memcmp(blob, lic, size));
        if (blob) LocalFree(blob);
    }
    printf("uninstall %08lx\n", SLUninstallLicense(h, &a));
    printf("uninstall-again %08lx\n", SLUninstallLicense(h, &a));
    {
        SLID f = {0};
        printf("fileid-gone %08lx\n", SLGetLicenseFileId(h, sizeof(lic), (const BYTE *)lic, &f));
    }
    id("key", SLInstallProofOfPurchase(h, L"msft:rm/algorithm/pkey/2009", L"AAAAA-BBBBB-CCCCC-DDDDD-EEEEE", 0, NULL, &k), &k);
    {
        SLID p = {0};
        HRESULT hr = SLGetPKeyId(h, L"msft:rm/algorithm/pkey/2009", L"AAAAA-BBBBB-CCCCC-DDDDD-EEEEE", 0, NULL, &p);
        printf("pkeyid %08lx %d\n", hr, IsEqualGUID(&p, &k));
    }
    printf("unkey %08lx\n", SLUninstallProofOfPurchase(h, &k));
    printf("unkey-again %08lx\n", SLUninstallProofOfPurchase(h, &k));
    printf("empty %08lx\n", SLInstallLicense(h, 0, (const BYTE *)lic, &a));
    printf("status %08lx\n", SLGetLicensingStatusInformation(h, &app, NULL, NULL, &n, &status));
    {
        void *ctx = NULL;
        UINT type = 99, size = 99;
        BYTE *value = (BYTE *)1;
        DWORD d = 0;
        HRESULT hr = SLLoadApplicationPolicies(&app, NULL, 0, &ctx);
        printf("loadpolicies %08lx %d\n", hr, ctx != NULL);
        hr = SLGetApplicationPolicy(ctx, L"office-Something", &type, &size, &value);
        printf("policy %08lx %u %p\n", hr, size, value);
        printf("unloadpolicies %08lx\n", SLUnloadApplicationPolicies(ctx, 0));
        printf("policydword %08lx\n", SLGetPolicyInformationDWORD(h, L"Kernel-Something", &d));
        printf("consume %08lx\n", SLConsumeRight(h, &app, NULL, NULL, NULL));
    }
    SLUninstallLicense(h, &c);
    SLClose(h);
    return 0;
}
