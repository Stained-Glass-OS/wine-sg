/* cryptui/crypt32: CryptUIDlgViewSignerInfoA/W, the Location column of the certificate chooser, the Location of certificates in system stores (patch 3722) */
#include <windows.h>
#include <wincrypt.h>
#include <cryptuiapi.h>
#include <commctrl.h>
#include <stdio.h>
#include <string.h>

static int f;
#define CHECK(c, ...) do { if (!(c)) { f++; printf("FAIL line %d: %s: ", __LINE__, #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { DWORD dwSize; HWND hwndParent; DWORD dwFlags; LPCWSTR szTitle; CMSG_SIGNER_INFO *pSignerInfo; HCRYPTMSG hMsg;
    LPCSTR pszOID; DWORD_PTR dwReserved; DWORD cStores; HCERTSTORE *rghStores; DWORD cPropSheetPages; void *rgPropSheetPages; } VIEWSIGNERW;
typedef struct { DWORD dwSize; HWND hwndParent; DWORD dwFlags; LPCSTR szTitle; CMSG_SIGNER_INFO *pSignerInfo; HCRYPTMSG hMsg;
    LPCSTR pszOID; DWORD_PTR dwReserved; DWORD cStores; HCERTSTORE *rghStores; DWORD cPropSheetPages; void *rgPropSheetPages; } VIEWSIGNERA;
#define CRYPTUI_VIEWSIGNERINFO_STRUCTW VIEWSIGNERW
#define CRYPTUI_VIEWSIGNERINFO_STRUCTA VIEWSIGNERA
static BOOL (WINAPI *CryptUIDlgViewSignerInfoW)(VIEWSIGNERW *);
static BOOL (WINAPI *CryptUIDlgViewSignerInfoA)(VIEWSIGNERA *);
static volatile LONG closed_dialogs;
static WCHAR loc_text[256];
static volatile LONG want_location;

static BOOL CALLBACK enum_proc(HWND hwnd, LPARAM lp)
{
    DWORD pid;
    char cls[32];
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != GetCurrentProcessId() || !IsWindowVisible(hwnd)) return TRUE;
    GetClassNameA(hwnd, cls, sizeof(cls));
    if (strcmp(cls, "#32770") && strcmp(cls, "ComboLBox") && strcmp(cls, "ThunderRT6FormDC") && !strstr(cls, "Prop")) { /* property sheets are #32770 */ }
    if (!strcmp(cls, "#32770"))
    {
        HWND lv = FindWindowExA(hwnd, NULL, "SysListView32", NULL);
        if (want_location && lv)
        {
            LVITEMW item = {0};
            item.iSubItem = 5; item.pszText = loc_text; item.cchTextMax = 256;
            SendMessageW(lv, LVM_GETITEMTEXTW, 0, (LPARAM)&item);
            want_location = 0;
        }
        PostMessageA(hwnd, WM_CLOSE, 0, 0);
        if (!lv) PostMessageA(hwnd, WM_COMMAND, IDCANCEL, 0);
        else PostMessageA(hwnd, WM_COMMAND, IDCANCEL, 0);
        InterlockedIncrement(&closed_dialogs);
    }
    return TRUE;
}

static DWORD WINAPI closer(void *arg)
{
    DWORD end = GetTickCount() + 20000;
    while (GetTickCount() < end && !closed_dialogs) { Sleep(300); EnumWindows(enum_proc, 0); }
    return 0;
}

static PCCERT_CONTEXT make_cert(void)
{
    HCRYPTPROV prov = 0;
    HCRYPTKEY key = 0;
    CERT_NAME_BLOB name = {0};
    CRYPT_KEY_PROV_INFO kpi = {0};
    PCCERT_CONTEXT cert;
    BYTE *buf;
    DWORD len = 0;

    CryptAcquireContextW(&prov, L"fastbcont", NULL, PROV_RSA_FULL, CRYPT_DELETEKEYSET);
    if (!CryptAcquireContextW(&prov, L"fastbcont", NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET)) { printf("FAIL no key set %lu\n", GetLastError()); f++; return NULL; }
    CryptGenKey(prov, AT_KEYEXCHANGE, 1024 << 16, &key);
    CertStrToNameW(X509_ASN_ENCODING, L"CN=fastb signer", 0, NULL, NULL, &len, NULL);
    buf = malloc(len);
    CertStrToNameW(X509_ASN_ENCODING, L"CN=fastb signer", 0, NULL, buf, &len, NULL);
    name.pbData = buf; name.cbData = len;
    kpi.pwszContainerName = (WCHAR *)L"fastbcont"; kpi.dwProvType = PROV_RSA_FULL; kpi.dwKeySpec = AT_KEYEXCHANGE;
    cert = CertCreateSelfSignCertificate(prov, &name, 0, &kpi, NULL, NULL, NULL, NULL);
    CryptDestroyKey(key);
    CryptReleaseContext(prov, 0);
    free(buf);
    return cert;
}

int main(void)
{
    PCCERT_CONTEXT cert = make_cert();
    HCERTSTORE mem, my;
    CMSG_SIGNER_INFO si = {0};
    CRYPTUI_VIEWSIGNERINFO_STRUCTW vw = { sizeof(vw) };
    CRYPTUI_VIEWSIGNERINFO_STRUCTA va = { sizeof(va) };
    PCCERT_CONTEXT chosen;
    HANDLE th;
    BOOL ret;

    HMODULE ui = LoadLibraryA("cryptui.dll");
    if (!cert) { printf("RESULT: FAIL\n"); return 1; }
    CryptUIDlgViewSignerInfoW = (void *)GetProcAddress(ui, "CryptUIDlgViewSignerInfoW");
    CryptUIDlgViewSignerInfoA = (void *)GetProcAddress(ui, "CryptUIDlgViewSignerInfoA");
    CHECK(CryptUIDlgViewSignerInfoW != NULL, "CryptUIDlgViewSignerInfoW is exported");
    if (!CryptUIDlgViewSignerInfoW) { printf("RESULT: FAIL\n"); return 1; }
    InitCommonControls();

    mem = CertOpenStore(CERT_STORE_PROV_MEMORY, 0, 0, 0, NULL);

    /* view signer info: argument checks */
    si.Issuer = cert->pCertInfo->Issuer;
    si.SerialNumber = cert->pCertInfo->SerialNumber;
    SetLastError(0); ret = CryptUIDlgViewSignerInfoW(NULL);
    CHECK(!ret && GetLastError() == E_INVALIDARG, "NULL: %d %#lx", ret, GetLastError());
    vw.pSignerInfo = NULL;
    SetLastError(0); ret = CryptUIDlgViewSignerInfoW(&vw);
    CHECK(!ret && GetLastError() == E_INVALIDARG, "no signer info: %d %#lx", ret, GetLastError());
    vw.dwSize = 4; vw.pSignerInfo = &si;
    SetLastError(0); ret = CryptUIDlgViewSignerInfoW(&vw);
    CHECK(!ret && GetLastError() == E_INVALIDARG, "short struct: %d %#lx", ret, GetLastError());
    vw.dwSize = sizeof(vw);
    SetLastError(0); ret = CryptUIDlgViewSignerInfoW(&vw);
    CHECK(!ret && GetLastError() == CRYPT_E_NOT_FOUND, "signer certificate nowhere: %d %#lx", ret, GetLastError());
    va.pSignerInfo = &si;
    SetLastError(0); ret = CryptUIDlgViewSignerInfoA(&va);
    CHECK(!ret && GetLastError() == CRYPT_E_NOT_FOUND, "A: signer certificate nowhere: %d %#lx", ret, GetLastError());

    /* the certificate is in a store the caller names: the certificate dialog opens (and is closed by the helper) */
    CertAddCertificateContextToStore(mem, cert, CERT_STORE_ADD_ALWAYS, NULL);
    vw.cStores = 1; vw.rghStores = &mem; vw.szTitle = L"fastb signer";
    closed_dialogs = 0; want_location = 0;
    th = CreateThread(NULL, 0, closer, NULL, 0, NULL);
    ret = CryptUIDlgViewSignerInfoW(&vw);
    WaitForSingleObject(th, 25000); CloseHandle(th);
    CHECK(GetLastError() != CRYPT_E_NOT_FOUND, "the signer's certificate was found (%lu)", GetLastError());
    CHECK(closed_dialogs >= 1, "a dialog appeared and was closed (%ld)", closed_dialogs);
    va.cStores = 1; va.rghStores = &mem; va.szTitle = "fastb signer A";
    closed_dialogs = 0;
    th = CreateThread(NULL, 0, closer, NULL, 0, NULL);
    ret = CryptUIDlgViewSignerInfoA(&va);
    WaitForSingleObject(th, 25000); CloseHandle(th);
    CHECK(closed_dialogs >= 1 && GetLastError() != CRYPT_E_NOT_FOUND, "A: shown (%d, %ld dialogs)", ret, closed_dialogs);

    /* the certificate chooser lists the store's name in its Location column */
    my = CertOpenStore(CERT_STORE_PROV_SYSTEM_W, 0, 0, CERT_SYSTEM_STORE_CURRENT_USER, L"My");
    CertAddCertificateContextToStore(my, cert, CERT_STORE_ADD_REPLACE_EXISTING, NULL);
    closed_dialogs = 0; want_location = 1; memset(loc_text, 0, sizeof(loc_text));
    th = CreateThread(NULL, 0, closer, NULL, 0, NULL);
    chosen = CryptUIDlgSelectCertificateFromStore(my, NULL, L"fastb choose", L"pick", 0, 0, NULL);
    WaitForSingleObject(th, 25000); CloseHandle(th);
    if (chosen) CertFreeCertificateContext(chosen);
    CHECK(closed_dialogs >= 1, "chooser appeared");
    CHECK(!wcscmp(loc_text, L"Personal"), "Location column: '%ls'", loc_text);
    { PCCERT_CONTEXT c = CertFindCertificateInStore(my, X509_ASN_ENCODING, 0, CERT_FIND_EXISTING, cert, NULL);
      if (c) CertDeleteCertificateFromStore(c); }
    CertCloseStore(my, 0);
    CertCloseStore(mem, 0);
    CertFreeCertificateContext(cert);
    { HCRYPTPROV p; CryptAcquireContextW(&p, L"fastbcont", NULL, PROV_RSA_FULL, CRYPT_DELETEKEYSET); }
    printf("%s\n", f ? "RESULT: FAIL" : "RESULT: PASS");
    return f != 0;
}
