/* msipriv-probe: for msipriv-gate.sh (wine-sg 9002, 9003).
 *   msipriv-probe.exe prop PACKAGE NAME     open PACKAGE, print "PROP NAME=value"
 *   msipriv-probe.exe inproc PACKAGE LEVEL  MsiSetInternalUI(LEVEL), MsiInstallProduct(PACKAGE): "INPROC rc"
 *   msipriv-probe.exe ctx PRODUCTCODE       the product's install context: "CTX machine|user|none" */
#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    WCHAR a[MAX_PATH], b[256], value[256];
    DWORD len = 256;
    MSIHANDLE pkg;
    UINT r;

    if (argc < 3) return 2;
    MultiByteToWideChar(CP_ACP, 0, argv[2], -1, a, MAX_PATH);
    if (!strcmp(argv[1], "prop") && argc > 3)
    {
        MultiByteToWideChar(CP_ACP, 0, argv[3], -1, b, 256);
        MsiSetInternalUI(INSTALLUILEVEL_NONE, NULL);
        if ((r = MsiOpenPackageW(a, &pkg))) { printf("PROP err %u\n", r); return 1; }
        value[0] = 0;
        MsiGetPropertyW(pkg, b, value, &len);
        printf("PROP %s=%ls\n", argv[3], value);
        MsiCloseHandle(pkg);
    }
    else if (!strcmp(argv[1], "inproc") && argc > 3)
    {
        MsiSetInternalUI((INSTALLUILEVEL)atoi(argv[3]), NULL);
        printf("INPROC %u\n", MsiInstallProductW(a, NULL));
    }
    else if (!strcmp(argv[1], "ctx"))
    {
        WCHAR state[16];
        len = 16;
        if (!MsiGetProductInfoExW(a, NULL, MSIINSTALLCONTEXT_MACHINE, L"State", state, &len)) printf("CTX machine\n");
        else if (len = 16, !MsiGetProductInfoExW(a, NULL, MSIINSTALLCONTEXT_USERUNMANAGED, L"State", state, &len)) printf("CTX user\n");
        else printf("CTX none\n");
    }
    return 0;
}
