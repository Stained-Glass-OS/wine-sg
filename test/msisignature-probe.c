/* Probe for patches/sg/2457: MsiGetFileSignatureInformation's errors for a missing file and for a file with no signature. */
#include <windows.h>
#include <wincrypt.h>
#include <msi.h>
#include <stdio.h>
#include <string.h>

#ifndef CRYPT_E_FILE_ERROR
#define CRYPT_E_FILE_ERROR ((HRESULT)0x80092003)
#endif

static int fails;
static void checkhr(const char *name, HRESULT got, HRESULT want)
{
    printf("      %s  %s (%08lx, want %08lx)\n", got == want ? "PASS" : "FAIL", name, (unsigned long)got, (unsigned long)want);
    if (got != want) fails++;
}

int main(void)
{
    static HRESULT (WINAPI *pSig)(LPCSTR, DWORD, PCCERT_CONTEXT *, BYTE *, DWORD *);
    const CERT_CONTEXT *cert;
    DWORD len = 0;
    HANDLE f;
    DWORD written;
    HRESULT hr;

    pSig = (void *)GetProcAddress(LoadLibraryA("msi.dll"), "MsiGetFileSignatureInformationA");
    if (!pSig) { puts("      FAIL  no MsiGetFileSignatureInformationA"); puts("RESULT: FAIL"); return 1; }

    DeleteFileA("signature.bin");
    checkhr("no path", pSig(NULL, 0, &cert, NULL, &len), E_INVALIDARG);
    checkhr("no certificate pointer", pSig("signature.bin", 0, NULL, NULL, &len), E_INVALIDARG);
    cert = (const CERT_CONTEXT *)0xdeadbeef;
    hr = pSig("signature.bin", 0, &cert, NULL, &len);
    checkhr("a file that is not there: a file error", hr, CRYPT_E_FILE_ERROR);
    printf("      %s  the certificate is cleared\n", cert == NULL ? "PASS" : "FAIL");
    if (cert != NULL) fails++;

    f = CreateFileA("signature.bin", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, "signature", 10, &written, NULL);
    CloseHandle(f);
    cert = (const CERT_CONTEXT *)0xdeadbeef;
    hr = pSig("signature.bin", 0, &cert, NULL, &len);
    checkhr("a file with no signature: function failed", hr, HRESULT_FROM_WIN32(ERROR_FUNCTION_FAILED));
    printf("      %s  the certificate is cleared\n", cert == NULL ? "PASS" : "FAIL");
    if (cert != NULL) fails++;
    DeleteFileA("signature.bin");
    puts(fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
