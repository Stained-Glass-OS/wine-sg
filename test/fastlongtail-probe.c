#define COBJMACROS
#include <winsock2.h>
#include <windows.h>
#include <ole2.h>
#include <oleauto.h>
#include <wincrypt.h>
#include <windns.h>
#include <stdio.h>

static int bad, total;
#define EXPECT(c) do { total++; if (!(c)) { bad++; printf("MISMATCH line %d: %s (err %lu)\n", __LINE__, #c, GetLastError()); } } while (0)

static const GUID CLSID_WordBreaker = {0x369647e0, 0x17b0, 0x11ce, {0x99,0x50,0x00,0xaa,0x00,0x4b,0xbb,0x1f}};
static const GUID CLSID_UserIdentityManager_ = {0xa9ae6c91, 0x1d1b, 0x11d2, {0xb2,0x1a,0x00,0xc0,0x4f,0xa3,0x57,0xfa}};
static const GUID CLSID_ScriptControl_ = {0x0e59f1d5, 0x1fbe, 0x11d0, {0x8f,0xf2,0x00,0xa0,0xd1,0x00,0x38,0xbc}};
static const GUID CLSID_WMP_ = {0x6bf52a52, 0x394a, 0x11d3, {0xb1,0x53,0x00,0xc0,0x4f,0x79,0xfa,0xa6}};

int main(void)
{
    HMODULE m;
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);

    /* apphelp: ShimFlushCache (an administrator flushes; the prefix user is one) */
    m = LoadLibraryA("apphelp.dll");
    {
        BOOL (WINAPI *flush)(HWND, HINSTANCE, LPCSTR, int) = (void *)GetProcAddress(m, "ShimFlushCache");
        EXPECT(flush && flush(NULL, NULL, "", 0));
    }

    /* newdev: InstallNewDevice leaves no restart pending */
    m = LoadLibraryA("newdev.dll");
    {
        BOOL (WINAPI *install)(HWND, GUID *, DWORD *) = (void *)GetProcAddress(m, "InstallNewDevice");
        DWORD reboot = 7;
        EXPECT(install && install(NULL, NULL, &reboot) && reboot == 0);
    }

    /* bluetoothapis: authentication registrations */
    m = LoadLibraryA("bluetoothapis.dll");
    {
        DWORD (WINAPI *reg)(const void *, HANDLE *, void *, void *) = (void *)GetProcAddress(m, "BluetoothRegisterForAuthenticationEx");
        BOOL (WINAPI *unreg)(HANDLE) = (void *)GetProcAddress(m, "BluetoothUnregisterAuthentication");
        HANDLE h1 = NULL, h2 = NULL;

        EXPECT(reg && unreg);
        if (reg && unreg)
        {
            EXPECT(reg(NULL, &h1, (void *)1, NULL) == ERROR_SUCCESS && h1);
            EXPECT(reg(NULL, &h2, (void *)1, NULL) == ERROR_SUCCESS && h2 && h2 != h1);
            EXPECT(reg(NULL, NULL, (void *)1, NULL) == ERROR_INVALID_PARAMETER);
            EXPECT(unreg(h1));
            SetLastError(0xdeadbeef);
            EXPECT(!unreg(h1) && GetLastError() == ERROR_INVALID_HANDLE);
            SetLastError(0xdeadbeef);
            EXPECT(!unreg(NULL) && GetLastError() == ERROR_INVALID_HANDLE);
            EXPECT(unreg(h2));
        }
    }

    /* infosoft: LockServer of the word breaker factory */
    LoadLibraryA("infosoft.dll");
    {
        IClassFactory *cf = NULL;
        hr = CoGetClassObject(&CLSID_WordBreaker, CLSCTX_INPROC_SERVER, NULL, &IID_IClassFactory, (void **)&cf);
        if (SUCCEEDED(hr) && cf)
        {
            EXPECT(IClassFactory_LockServer(cf, TRUE) == S_OK);
            EXPECT(IClassFactory_LockServer(cf, FALSE) == S_OK);
            IClassFactory_Release(cf);
        }
        else EXPECT(!"word breaker factory");
    }

    /* msident: the identity UI is not there, the user cancels */
    {
        IUnknown *mgr = NULL;
        static const GUID iid_mgr = {0xa9ae6c90, 0x1d1b, 0x11d2, {0xb2,0x1a,0x00,0xc0,0x4f,0xa3,0x57,0xfa}};
        hr = CoCreateInstance(&CLSID_UserIdentityManager_, NULL, CLSCTX_INPROC_SERVER, &iid_mgr, (void **)&mgr);
        if (SUCCEEDED(hr))
        {
            typedef HRESULT (WINAPI *manage_t)(void *, HWND, DWORD);
            struct { void **vtbl; } *obj = (void *)mgr;
            /* IUnknown (3 methods), EnumIdentities, ManageIdentities */
            manage_t manage = (manage_t)obj->vtbl[4];
            EXPECT(manage(mgr, NULL, 0) == (HRESULT)0x80007112);
            IUnknown_Release(mgr);
        }
        else EXPECT(!"IUserIdentityManager");
    }

    /* msscript: the control is not a link source */
    {
        IOleObject *ole = NULL;
        IMoniker *mk = (IMoniker *)1;
        hr = CoCreateInstance(&CLSID_ScriptControl_, NULL, CLSCTX_INPROC_SERVER, &IID_IOleObject, (void **)&ole);
        if (SUCCEEDED(hr))
        {
            EXPECT(IOleObject_SetMoniker(ole, OLEWHICHMK_CONTAINER, NULL) == S_OK);
            EXPECT(IOleObject_GetMoniker(ole, OLEGETMONIKER_ONLYIFTHERE, OLEWHICHMK_OBJFULL, &mk) == E_UNEXPECTED);
            EXPECT(mk == NULL);
            IOleObject_Release(ole);
        }
        else EXPECT(!"ScriptControl");
    }

    /* wmp: no help mode, a name that is accepted */
    {
        IOleObject *ole = NULL;
        IOleInPlaceObject *ip = NULL;
        hr = CoCreateInstance(&CLSID_WMP_, NULL, CLSCTX_INPROC_SERVER, &IID_IOleObject, (void **)&ole);
        if (SUCCEEDED(hr))
        {
            EXPECT(IOleObject_SetMoniker(ole, OLEWHICHMK_CONTAINER, NULL) == S_OK);
            if (SUCCEEDED(IOleObject_QueryInterface(ole, &IID_IOleInPlaceObject, (void **)&ip)))
            {
                EXPECT(IOleInPlaceObject_ContextSensitiveHelp(ip, TRUE) == S_OK);
                IOleInPlaceObject_Release(ip);
            }
            else EXPECT(!"IOleInPlaceObject");
            IOleObject_Release(ole);
        }
        else EXPECT(!"WMP control");
    }

    /* msvcrt: a counted source ends at its first null */
    m = LoadLibraryA("msvcrt.dll");
    {
        int (__cdecl *lcmap)(LCID, DWORD, const WCHAR *, int, WCHAR *, int, unsigned int, int) =
            (void *)GetProcAddress(m, "__crtLCMapStringW");
        WCHAR src[8] = { 'a', 'b', 'c', 0, 'd', 'e', 0, 0 }, dst[16] = {0};
        EXPECT(lcmap && lcmap(LOCALE_USER_DEFAULT, LCMAP_UPPERCASE, src, 8, dst, 16, 0, 0) == 3);
        EXPECT(dst[0] == 'A' && dst[2] == 'C' && dst[3] == 0);
    }

    /* wsock32: GetService finds nothing */
    m = LoadLibraryA("wsock32.dll");
    {
        INT (WINAPI *get)(DWORD, GUID *, char *, DWORD, void *, DWORD *, void *) = (void *)GetProcAddress(m, "GetServiceA");
        char buf[64];
        DWORD size = sizeof(buf);
        SetLastError(0xdeadbeef);
        EXPECT(get && get(0, NULL, (char *)"x", 0, buf, &size, NULL) == 0 && GetLastError() == ERROR_SERVICE_NOT_FOUND);
    }

    /* crypt32: a control type the provider does not know */
    {
        HCERTSTORE store;
        HKEY key;
        WCHAR path[MAX_PATH];
        HANDLE file;

        GetTempPathW(MAX_PATH, path);
        lstrcatW(path, L"fastlongtail.sst");
        file = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        store = CertOpenStore(CERT_STORE_PROV_FILE, X509_ASN_ENCODING, 0, CERT_STORE_CREATE_NEW_FLAG, file);
        EXPECT(store != NULL);
        if (store)
        {
            SetLastError(0xdeadbeef);
            EXPECT(!CertControlStore(store, 0, 9999, NULL) && GetLastError() == ERROR_NOT_SUPPORTED);
            CertCloseStore(store, 0);
        }
        CloseHandle(file);
        DeleteFileW(path);

        if (!RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\FastLongTail", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL))
        {
            store = CertOpenStore(CERT_STORE_PROV_REG, 0, 0, 0, key);
            EXPECT(store != NULL);
            if (store)
            {
                SetLastError(0xdeadbeef);
                EXPECT(!CertControlStore(store, 0, 9999, NULL) && GetLastError() == ERROR_NOT_SUPPORTED);
                CertCloseStore(store, 0);
            }
            RegCloseKey(key);
            RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\FastLongTail");
        }
    }

    /* dnsapi: the registration settings are DWORDs */
    {
        DWORD len = 0, value = 0xdead;
        DNS_STATUS st;
        st = DnsQueryConfig(DnsConfigPrimaryHostNameRegistrationEnabled, 0, NULL, NULL, NULL, &len);
        EXPECT(st == ERROR_INSUFFICIENT_BUFFER && len == sizeof(DWORD));
        len = sizeof(value);
        st = DnsQueryConfig(DnsConfigPrimaryHostNameRegistrationEnabled, 0, NULL, NULL, &value, &len);
        EXPECT(st == ERROR_SUCCESS && len == sizeof(DWORD) && value == 1);
        value = 0xdead; len = sizeof(value);
        st = DnsQueryConfig(DnsConfigAddressRegistrationMaxCount, 0, NULL, NULL, &value, &len);
        EXPECT(st == ERROR_SUCCESS && value == 1);
        st = DnsQueryConfig(DnsConfigAdapterInfo, 0, NULL, NULL, &value, &len);
        EXPECT(st != ERROR_SUCCESS);
        /* the values come from the TCP/IP parameters */
        {
            HKEY k, ik; DWORD one = 1, zero = 0, three = 3;
            RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"System\\CurrentControlSet\\Services\\Tcpip\\Parameters", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &k, NULL);
            RegCreateKeyExW(k, L"Interfaces\\{FASTLT00-0000-0000-0000-000000000001}", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &ik, NULL);
            RegSetValueExW(k, L"DisableDynamicUpdate", 0, REG_DWORD, (BYTE *)&one, sizeof(one));
            RegSetValueExW(k, L"DefaultRegistrationMaxAddressCount", 0, REG_DWORD, (BYTE *)&three, sizeof(three));
            value = 0xdead; len = sizeof(value);
            st = DnsQueryConfig(DnsConfigPrimaryHostNameRegistrationEnabled, 0, NULL, NULL, &value, &len);
            EXPECT(st == ERROR_SUCCESS && value == 0);
            value = 0xdead; len = sizeof(value);
            st = DnsQueryConfig(DnsConfigAddressRegistrationMaxCount, 0, NULL, NULL, &value, &len);
            EXPECT(st == ERROR_SUCCESS && value == 3);
            RegSetValueExW(k, L"DisableDynamicUpdate", 0, REG_DWORD, (BYTE *)&zero, sizeof(zero));
            RegSetValueExW(ik, L"RegistrationEnabled", 0, REG_DWORD, (BYTE *)&zero, sizeof(zero));
            value = 0xdead; len = sizeof(value);
            st = DnsQueryConfig(DnsConfigAdapterHostNameRegistrationEnabled, 0, L"{FASTLT00-0000-0000-0000-000000000001}", NULL, &value, &len);
            EXPECT(st == ERROR_SUCCESS && value == 0);
            RegSetValueExW(ik, L"RegistrationEnabled", 0, REG_DWORD, (BYTE *)&one, sizeof(one));
            value = 0xdead; len = sizeof(value);
            st = DnsQueryConfig(DnsConfigAdapterHostNameRegistrationEnabled, 0, L"{FASTLT00-0000-0000-0000-000000000001}", NULL, &value, &len);
            EXPECT(st == ERROR_SUCCESS && value == 1);
            st = DnsQueryConfig(DnsConfigAdapterHostNameRegistrationEnabled, 0, NULL, NULL, &value, &len);
            EXPECT(st == ERROR_INVALID_PARAMETER);
            RegDeleteValueW(k, L"DefaultRegistrationMaxAddressCount");
            RegDeleteValueW(k, L"DisableDynamicUpdate");
            RegCloseKey(ik);
            RegDeleteKeyW(k, L"Interfaces\\{FASTLT00-0000-0000-0000-000000000001}");
            RegCloseKey(k);
        }
    }

    /* propsys: a GUID from sixteen bytes */
    m = LoadLibraryA("propsys.dll");
    {
        HRESULT (WINAPI *to_guid)(const PROPVARIANT *, GUID *) = (void *)GetProcAddress(m, "PropVariantToGUID");
        static const GUID want = {0x01020304, 0x0506, 0x0708, {9,10,11,12,13,14,15,16}};
        SAFEARRAY *sa = SafeArrayCreateVector(VT_UI1, 0, 16), *sa2 = SafeArrayCreateVector(VT_UI1, 0, 4);
        PROPVARIANT pv;
        GUID got = {0};
        void *data;

        EXPECT(to_guid && sa);
        SafeArrayAccessData(sa, &data);
        memcpy(data, &want, 16);
        SafeArrayUnaccessData(sa);
        memset(&pv, 0, sizeof(pv));
        pv.vt = VT_ARRAY | VT_UI1;
        pv.parray = sa;
        EXPECT(to_guid(&pv, &got) == S_OK && !memcmp(&got, &want, 16));
        pv.parray = sa2;
        EXPECT(FAILED(to_guid(&pv, &got)));
        memset(&pv, 0, sizeof(pv));
        EXPECT(FAILED(to_guid(&pv, &got)));
        SafeArrayDestroy(sa);
        SafeArrayDestroy(sa2);
    }

    /* mscms: no per-user association list is kept */
    m = LoadLibraryA("mscms.dll");
    {
        BOOL (WINAPI *peruser)(const WCHAR *, DWORD, BOOL *) = (void *)GetProcAddress(m, "WcsGetUsePerUserProfiles");
        BOOL use = TRUE;
        EXPECT(peruser && peruser(L"display", 0x6D6E7472, &use) && use == FALSE);
        SetLastError(0xdeadbeef);
        EXPECT(!peruser(L"display", 0x12345678, &use) && GetLastError() == ERROR_INVALID_PARAMETER);
        SetLastError(0xdeadbeef);
        EXPECT(!peruser(NULL, 0x6D6E7472, &use) && GetLastError() == ERROR_INVALID_PARAMETER);
    }

    /* ws2_32: QOS templates */
    {
        WSADATA wsa;
        SOCKET s;
        WSABUF name = { 0, NULL };
        QOS qos;
        BOOL (WINAPI *byname)(SOCKET, WSABUF *, QOS *);
        char buf[8] = "x";

        WSAStartup(MAKEWORD(2, 2), &wsa);
        byname = (void *)GetProcAddress(GetModuleHandleA("ws2_32.dll"), "WSAGetQOSByName");
        s = socket(AF_INET, SOCK_STREAM, 0);
        EXPECT(byname && s != INVALID_SOCKET);
        SetLastError(0xdeadbeef);
        EXPECT(!byname((SOCKET)0x1234, &name, &qos) && WSAGetLastError() == WSAENOTSOCK);
        SetLastError(0xdeadbeef);
        EXPECT(!byname(s, NULL, &qos) && WSAGetLastError() == WSAEFAULT);
        /* the enumeration with no buffer ({0, NULL} succeeded): replaced at
         * integration, superseded by 3715 (documented: the enumeration is asked
         * with buf pointing to an empty string, and a buffer that is not valid
         * is WSAEFAULT); the enumeration with an empty string still succeeds */
        {
            char empty[8] = "";
            WSABUF list = { sizeof(empty), empty };
            EXPECT(byname(s, &list, &qos));
        }
        name.len = 1;
        name.buf = buf;
        EXPECT(!byname(s, &name, &qos));
        closesocket(s);
        WSACleanup();
    }

    printf("checks %d, mismatches %d\nRESULT: %s\n", total, bad, bad ? "FAIL" : "PASS");
    return bad != 0;
}
