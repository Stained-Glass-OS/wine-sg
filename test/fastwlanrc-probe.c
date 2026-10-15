#include <windows.h>
#include <stdio.h>
static int bad, total;
#define EXPECT_ERR(e, c) do { DWORD r_ = (c); total++; if (r_ != (e)) { bad++; printf("MISMATCH line %d: %s = %lu, expected %lu\n", __LINE__, #c, r_, (DWORD)(e)); } } while (0)

int main(void)
{
    HMODULE m = LoadLibraryA("wlanapi.dll");
    DWORD (WINAPI *open)(DWORD, void *, DWORD *, HANDLE *) = (void *)GetProcAddress(m, "WlanOpenHandle");
    DWORD (WINAPI *close)(HANDLE, void *) = (void *)GetProcAddress(m, "WlanCloseHandle");
    DWORD (WINAPI *prop)(HANDLE, int, DWORD *, void **, int *, void *) = (void *)GetProcAddress(m, "WlanHostedNetworkQueryProperty");
    DWORD (WINAPI *key)(HANDLE, DWORD *, unsigned char *, BOOL *, BOOL *, int *, void *) = (void *)GetProcAddress(m, "WlanHostedNetworkQuerySecondaryKey");
    DWORD (WINAPI *status)(HANDLE, void *, void *) = (void *)GetProcAddress(m, "WlanHostedNetworkQueryStatus");
    DWORD ver, size = 0;
    HANDLE h = NULL;
    void *data = NULL;
    int type = 0, reason = 0;
    unsigned char keybuf[64];
    BOOL pass, persist;
    char status_buf[256];

    setvbuf(stdout, NULL, _IONBF, 0);
    total++; if (!open || !close || !prop || !key || !status) { printf("MISMATCH missing exports\n"); return 1; }
    EXPECT_ERR(ERROR_SUCCESS, open(2, NULL, &ver, &h));

    /* bad arguments come first, then the handle, then the service */
    EXPECT_ERR(ERROR_INVALID_PARAMETER, prop(NULL, 0, &size, &data, &type, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER, prop(h, 0, NULL, &data, &type, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER, prop(h, 0, &size, NULL, &type, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER, prop(h, 0, &size, &data, NULL, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER, prop(h, 0, &size, &data, &type, (void *)1));
    EXPECT_ERR(ERROR_INVALID_HANDLE, prop((HANDLE)0x7777, 0, &size, &data, &type, NULL));
    EXPECT_ERR(ERROR_SERVICE_NOT_ACTIVE, prop(h, 0, &size, &data, &type, NULL));

    size = sizeof(keybuf);
    EXPECT_ERR(ERROR_INVALID_PARAMETER, key(NULL, &size, keybuf, &pass, &persist, &reason, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER, key(h, NULL, keybuf, &pass, &persist, &reason, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER, key(h, &size, NULL, &pass, &persist, &reason, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER, key(h, &size, keybuf, NULL, &persist, &reason, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER, key(h, &size, keybuf, &pass, NULL, &reason, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER, key(h, &size, keybuf, &pass, &persist, &reason, (void *)1));
    EXPECT_ERR(ERROR_INVALID_HANDLE, key((HANDLE)0x7777, &size, keybuf, &pass, &persist, &reason, NULL));
    EXPECT_ERR(ERROR_SERVICE_NOT_ACTIVE, key(h, &size, keybuf, &pass, &persist, &reason, NULL));

    EXPECT_ERR(ERROR_INVALID_PARAMETER, status(NULL, status_buf, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER, status(h, NULL, NULL));
    EXPECT_ERR(ERROR_INVALID_PARAMETER, status(h, status_buf, (void *)1));
    EXPECT_ERR(ERROR_INVALID_HANDLE, status((HANDLE)0x7777, status_buf, NULL));
    EXPECT_ERR(ERROR_SERVICE_NOT_ACTIVE, status(h, status_buf, NULL));

    EXPECT_ERR(ERROR_SUCCESS, close(h, NULL));
    /* a closed handle is unknown */
    EXPECT_ERR(ERROR_INVALID_HANDLE, status(h, status_buf, NULL));
    printf("checks %d, mismatches %d\nRESULT: %s\n", total, bad, bad ? "FAIL" : "PASS");
    return bad != 0;
}
