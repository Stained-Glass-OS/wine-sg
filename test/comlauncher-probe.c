/* A COM local server started through a launcher (patches/sg/0628): the
 * program a class names starts the real server and exits, as Omaha's
 * ...UpdateOnDemand.exe does; the class is registered by its child.
 *   comlauncher-probe register   the two classes, in HKLM (this view)
 *   comlauncher-probe launcher   (as the class's server) starts "server", exits 0
 *   comlauncher-probe broken     (as the other class's server) exits 3
 *   comlauncher-probe server     registers the class, serves it 20 s
 *   comlauncher-probe client     prints: launched=<hr> <ms>  broken=<hr> <ms>
 */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <stdio.h>
#include <string.h>

static const CLSID clsid_launched = {0x5a3c1e10,0x2b6d,0x4f0e,{0x9a,0x51,0x6e,0x2d,0x7c,0x11,0x40,0x01}};
static const CLSID clsid_broken = {0x5a3c1e10,0x2b6d,0x4f0e,{0x9a,0x51,0x6e,0x2d,0x7c,0x11,0x40,0x02}};

static HRESULT WINAPI cf_QueryInterface(IClassFactory *iface, REFIID riid, void **out)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IClassFactory))
    {
        *out = iface;
        return S_OK;
    }
    *out = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI cf_AddRef(IClassFactory *iface) { return 2; }
static ULONG WINAPI cf_Release(IClassFactory *iface) { return 1; }
static HRESULT WINAPI cf_CreateInstance(IClassFactory *iface, IUnknown *outer, REFIID riid, void **out)
{
    return cf_QueryInterface(iface, riid, out);
}
static HRESULT WINAPI cf_LockServer(IClassFactory *iface, BOOL lock) { return S_OK; }
static IClassFactoryVtbl cf_vtbl = { cf_QueryInterface, cf_AddRef, cf_Release, cf_CreateInstance, cf_LockServer };
static IClassFactory cf = { &cf_vtbl };

static void set_server(const CLSID *clsid, const char *mode)
{
    char key[128], self[MAX_PATH], cmd[MAX_PATH + 32];
    WCHAR guid[40];
    HKEY hkey;

    StringFromGUID2(clsid, guid, 40);
    snprintf(key, sizeof(key), "Software\\Classes\\CLSID\\%ls\\LocalServer32", guid);
    GetModuleFileNameA(NULL, self, MAX_PATH);
    snprintf(cmd, sizeof(cmd), "\"%s\" %s", self, mode);
    if (!RegCreateKeyExA(HKEY_LOCAL_MACHINE, key, 0, NULL, 0, KEY_WRITE, NULL, &hkey, NULL))
    {
        RegSetValueExA(hkey, NULL, 0, REG_SZ, (BYTE *)cmd, strlen(cmd) + 1);
        RegCloseKey(hkey);
    }
}

static void create(const char *name, const CLSID *clsid)
{
    DWORD start = GetTickCount();
    IUnknown *unk = NULL;
    HRESULT hr = CoCreateInstance(clsid, NULL, CLSCTX_LOCAL_SERVER, &IID_IUnknown, (void **)&unk);
    printf("%s=%08lx %lu\n", name, hr, GetTickCount() - start);
    if (unk) IUnknown_Release(unk);
}

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "";

    if (!strcmp(mode, "register"))
    {
        set_server(&clsid_launched, "launcher");
        set_server(&clsid_broken, "broken");
    }
    else if (!strcmp(mode, "launcher"))
    {
        char self[MAX_PATH], cmd[MAX_PATH + 16];
        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;

        GetModuleFileNameA(NULL, self, MAX_PATH);
        snprintf(cmd, sizeof(cmd), "\"%s\" server", self);
        /* the real server is not up yet when this leaves */
        if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) return 2;
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return 0;
    }
    else if (!strcmp(mode, "broken"))
        return 3;
    else if (!strcmp(mode, "server"))
    {
        DWORD cookie, start;
        MSG msg;

        Sleep(1500);   /* later than the launcher's exit, as a real server's start-up is */
        CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        CoRegisterClassObject(&clsid_launched, (IUnknown *)&cf, CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &cookie);
        for (start = GetTickCount(); GetTickCount() - start < 20000;)
        {
            MsgWaitForMultipleObjects(0, NULL, FALSE, 100, QS_ALLINPUT);
            while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
        }
        CoRevokeClassObject(cookie);
        CoUninitialize();
    }
    else if (!strcmp(mode, "client"))
    {
        CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
        create("launched", &clsid_launched);
        create("broken", &clsid_broken);
        CoUninitialize();
    }
    return 0;
}
