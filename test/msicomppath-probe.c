/* Probe for patches/sg/2455: what MsiGetComponentPathEx leaves in the length it is given (and MsiGetComponentPath does not). */
#include <windows.h>
#include <msi.h>
#include <stdio.h>
#include <string.h>

static INSTALLSTATE (WINAPI *pEx)(LPCSTR, LPCSTR, LPCSTR, MSIINSTALLCONTEXT, LPSTR, LPDWORD);
#define MsiGetComponentPathExA pEx
static int fails;
static void checku(const char *name, UINT got, UINT want)
{
    printf("      %s  %s (%u, want %u)\n", got == want ? "PASS" : "FAIL", name, got, want);
    if (got != want) fails++;
}

int main(void)
{
    /* test GUIDs: product {379B1C47-40C1-42FA-A9BB-BEBB6F1B0172}, component {1F1F1F1F-...} */
    static const char prod[] = "{379B1C47-40C1-42FA-A9BB-BEBB6F1B0172}", comp[] = "{8A2D9B5F-7B0F-4E3B-9E6D-1F6A0C3B2D11}";
    char path[260];
    DWORD size;
    INSTALLSTATE st;

    pEx = (void *)GetProcAddress(LoadLibraryA("msi.dll"), "MsiGetComponentPathExA");
    if (!pEx) { puts("      FAIL  no MsiGetComponentPathExA"); puts("RESULT: FAIL"); return 1; }

    size = 260;
    st = MsiGetComponentPathExA(NULL, comp, NULL, MSIINSTALLCONTEXT_USERMANAGED, path, &size);
    checku("no product: invalid argument", st, INSTALLSTATE_INVALIDARG);
    checku("   and a length of 0", size, 0);
    size = 260;
    st = MsiGetComponentPathExA(prod, NULL, NULL, MSIINSTALLCONTEXT_USERMANAGED, path, &size);
    checku("no component: invalid argument", st, INSTALLSTATE_INVALIDARG);
    checku("   and a length of 0", size, 0);
    size = 260;
    st = MsiGetComponentPathExA(prod, comp, "S-1-5-18", MSIINSTALLCONTEXT_MACHINE, path, &size);
    checku("a user for the machine context: invalid argument", st, INSTALLSTATE_INVALIDARG);
    checku("   and a length of 0", size, 0);
    size = 260;
    st = MsiGetComponentPathExA(prod, comp, NULL, MSIINSTALLCONTEXT_MACHINE, path, NULL);
    checku("no length: invalid argument", st, INSTALLSTATE_INVALIDARG);

    size = 260;
    st = MsiGetComponentPathExA(prod, comp, NULL, MSIINSTALLCONTEXT_MACHINE, path, &size);
    checku("unknown component", st, INSTALLSTATE_UNKNOWN);
    checku("   and a length of 0", size, 0);
    size = 260;
    st = MsiGetComponentPathExA(prod, comp, NULL, MSIINSTALLCONTEXT_MACHINE, NULL, &size);
    checku("unknown, no buffer", st, INSTALLSTATE_UNKNOWN);
    checku("   and twice the length given", size, 520);

    /* the older function leaves the length alone */
    size = 260;
    st = MsiGetComponentPathA(prod, comp, path, &size);
    checku("MsiGetComponentPath: unknown", st, INSTALLSTATE_UNKNOWN);
    checku("   and the length untouched", size, 260);
    size = 260;
    st = MsiGetComponentPathA(NULL, comp, path, &size);
    checku("MsiGetComponentPath, no product", st, INSTALLSTATE_INVALIDARG);
    checku("   and the length untouched", size, 260);

    /* a registered component whose file is there, with room for nothing: the bytes of the wide path */
    {
        char key_path[300], sq_prod[33] = "74C1B9730C0A2F24AA9BBBEB6B1F2710", sq_comp[33] = "F5B9D2A8F0B7B3E4E9D6F1A6C0B3D211";
        HKEY key_comp = NULL, key_prop = NULL;
        DWORD one = 1;
        HANDLE f;

        /* the squashed forms of our GUIDs */
        {
            const char *g[2] = { prod, comp };
            char *out[2] = { sq_prod, sq_comp };
            int k, i;
            for (k = 0; k < 2; k++)
            {
                char h[33]; int n = 0;
                for (i = 0; g[k][i]; i++) if (g[k][i] != '{' && g[k][i] != '}' && g[k][i] != '-') h[n++] = g[k][i];
                for (i = 0; i < 8; i++) out[k][i] = h[7 - i];
                for (i = 0; i < 4; i++) out[k][8 + i] = h[11 - i];
                for (i = 0; i < 4; i++) out[k][12 + i] = h[15 - i];
                for (i = 0; i < 8; i++) { out[k][16 + i * 2] = h[16 + i * 2 + 1]; out[k][16 + i * 2 + 1] = h[16 + i * 2]; }
                out[k][32] = 0;
            }
        }
        snprintf(key_path, sizeof(key_path), "Software\\Microsoft\\Windows\\CurrentVersion\\Installer\\UserData\\S-1-5-18\\Components\\%s", sq_comp);
        if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, key_path, 0, NULL, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &key_comp, NULL)) { puts("      (no registry rights: the rest is skipped)"); goto done; }
        RegSetValueExA(key_comp, sq_prod, 0, REG_SZ, (BYTE *)"c:\\testcomponentpath", 20);
        snprintf(key_path, sizeof(key_path), "Software\\Microsoft\\Windows\\CurrentVersion\\Installer\\UserData\\S-1-5-18\\Products\\%s\\InstallProperties", sq_prod);
        RegCreateKeyExA(HKEY_LOCAL_MACHINE, key_path, 0, NULL, 0, KEY_ALL_ACCESS | KEY_WOW64_64KEY, NULL, &key_prop, NULL);
        RegSetValueExA(key_prop, "WindowsInstaller", 0, REG_DWORD, (BYTE *)&one, sizeof(one));
        f = CreateFileA("c:\\testcomponentpath", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (f != INVALID_HANDLE_VALUE) CloseHandle(f);

        path[0] = 0; size = 0;
        st = MsiGetComponentPathExA(prod, comp, NULL, MSIINSTALLCONTEXT_MACHINE, path, &size);
        checku("a file that is there, no room: more data", st, INSTALLSTATE_MOREDATA);
        checku("   and the bytes of the wide path (2 x 20)", size, 40);
        path[0] = 0; size = 260;
        st = MsiGetComponentPathExA(prod, comp, NULL, MSIINSTALLCONTEXT_MACHINE, path, &size);
        checku("with room: local", st, INSTALLSTATE_LOCAL);
        checku("   the length in characters", size, 20);

        DeleteFileA("c:\\testcomponentpath");
        RegDeleteValueA(key_comp, sq_prod);
        { LONG (WINAPI *pDel)(HKEY, LPCSTR, REGSAM, DWORD) = (void *)GetProcAddress(GetModuleHandleA("advapi32.dll"), "RegDeleteKeyExA"); if (pDel) pDel(HKEY_LOCAL_MACHINE, key_path, KEY_WOW64_64KEY, 0); }
        RegCloseKey(key_prop);
        RegCloseKey(key_comp);
    }
done:
    puts(fails ? "RESULT: FAIL" : "RESULT: PASS");
    return fails != 0;
}
