/* Probe for patches/sg/5210: a device event reaching an ANSI window through
 * RegisterDeviceNotificationA is converted from the wide structures the plug-and-play service
 * sends: DBT_DEVTYP_PORT's dbcp_name becomes an ANSI string, the structures without strings
 * (OEM, DEVNODE, VOLUME, NET) arrive as sent.  The events are sent with the plug-and-play RPC
 * interface (plugplay_send_event), as ntoskrnl does.
 * Table driven; last line "RESULT: PASS|FAIL". */
#include <windows.h>
#include <dbt.h>
#include <stdio.h>
#include <stdlib.h>
#include "plugplay.h"

void *__RPC_USER MIDL_user_allocate(size_t n) { return malloc(n); }
void __RPC_USER MIDL_user_free(void *p) { free(p); }

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static BYTE got[512];
static DWORD got_size, got_type;
static WPARAM got_wparam;
static int got_count;

static LRESULT CALLBACK proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (msg == WM_DEVICECHANGE && lp)
    {
        const DEV_BROADCAST_HDR *h = (const DEV_BROADCAST_HDR *)lp;
        got_size = h->dbch_size;
        got_type = h->dbch_devicetype;
        got_wparam = wp;
        memcpy(got, h, min(h->dbch_size, sizeof(got)));
        got_count++;
        return TRUE;
    }
    return DefWindowProcA(hwnd, msg, wp, lp);
}

static BOOL wait_event(int want)
{
    DWORD end = GetTickCount() + 5000;
    MSG msg;
    while (got_count < want && GetTickCount() < end)
    {
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
        MsgWaitForMultipleObjects(0, NULL, FALSE, 50, QS_ALLINPUT);
    }
    return got_count >= want;
}

int main(void)
{
    WNDCLASSA wc = {0};
    HWND hwnd;
    HDEVNOTIFY notify;
    RPC_WSTR str = NULL;
    RPC_STATUS st;
    BYTE buf[256];
    DEV_BROADCAST_PORT_W *port = (DEV_BROADCAST_PORT_W *)buf;
    DEV_BROADCAST_VOLUME *vol = (DEV_BROADCAST_VOLUME *)buf;
    DEV_BROADCAST_NET *net = (DEV_BROADCAST_NET *)buf;
    DEV_BROADCAST_OEM *oem = (DEV_BROADCAST_OEM *)buf;
    int n = 0;
    const char *ansi;
    static const WCHAR com1[] = L"COM1", acute[] = L"COM\x00e9x", empty[] = L"";

    wc.lpfnWndProc = proc; wc.hInstance = GetModuleHandleA(NULL); wc.lpszClassName = "SgDevAnsi";
    RegisterClassA(&wc);
    hwnd = CreateWindowA("SgDevAnsi", "t", WS_OVERLAPPED, 0, 0, 10, 10, NULL, NULL, wc.hInstance, NULL);
    check(hwnd && !IsWindowUnicode(hwnd), "ANSI window");
    notify = RegisterDeviceNotificationA(hwnd, NULL, DEVICE_NOTIFY_WINDOW_HANDLE);
    if (!notify) { check(0, "RegisterDeviceNotificationA failed (%lu)", GetLastError()); goto out; }

    st = RpcStringBindingComposeW(NULL, (RPC_WSTR)L"ncacn_np", NULL, (RPC_WSTR)L"\\pipe\\wine_plugplay", NULL, &str);
    check(st == RPC_S_OK, "binding string");
    st = RpcBindingFromStringBindingW(str, &plugplay_binding_handle);
    check(st == RPC_S_OK, "binding");
    if (st) goto out;

#define SEND(code, sz) do { memset(&got, 0, sizeof(got)); got_size = got_type = 0xdead; \
    plugplay_send_event((WCHAR *)L"x", (code), buf, (sz)); \
    n++; check(wait_event(n), "event %d delivered", n); } while (0)

    /* PORT with an ASCII name: wide size 12 + 5 WCHARs */
    memset(buf, 0, sizeof(buf));
    port->dbcp_size = FIELD_OFFSET(DEV_BROADCAST_PORT_W, dbcp_name) + sizeof(com1);
    port->dbcp_devicetype = DBT_DEVTYP_PORT;
    port->dbcp_reserved = 0x1234;
    memcpy(port->dbcp_name, com1, sizeof(com1));
    SEND(DBT_DEVICEARRIVAL, port->dbcp_size);
    ansi = (const char *)got + FIELD_OFFSET(DEV_BROADCAST_PORT_A, dbcp_name);
    check(got_type == DBT_DEVTYP_PORT && got_wparam == DBT_DEVICEARRIVAL, "port: type %lu code %#Ix", got_type, got_wparam);
    check(!strcmp(ansi, "COM1"), "port name is ANSI: '%s'", ansi);
    check(got_size == FIELD_OFFSET(DEV_BROADCAST_PORT_A, dbcp_name) + 5, "port size %lu", got_size);
    check(((DEV_BROADCAST_PORT_A *)got)->dbcp_reserved == 0x1234, "port reserved kept");

    /* a name with a non-ASCII character */
    memset(buf, 0, sizeof(buf));
    port->dbcp_size = FIELD_OFFSET(DEV_BROADCAST_PORT_W, dbcp_name) + sizeof(acute);
    port->dbcp_devicetype = DBT_DEVTYP_PORT;
    memcpy(port->dbcp_name, acute, sizeof(acute));
    SEND(DBT_DEVICEREMOVECOMPLETE, port->dbcp_size);
    ansi = (const char *)got + FIELD_OFFSET(DEV_BROADCAST_PORT_A, dbcp_name);
    check(got_type == DBT_DEVTYP_PORT && got_wparam == DBT_DEVICEREMOVECOMPLETE, "port 2: type %lu code %#Ix", got_type, got_wparam);
    check(strlen(ansi) == 5 && !memcmp(ansi, "COM", 3) && ansi[4] == 'x', "port name with accent: %d chars", (int)strlen(ansi));

    /* an empty name */
    memset(buf, 0, sizeof(buf));
    port->dbcp_size = FIELD_OFFSET(DEV_BROADCAST_PORT_W, dbcp_name) + sizeof(empty);
    port->dbcp_devicetype = DBT_DEVTYP_PORT;
    SEND(DBT_DEVICEARRIVAL, port->dbcp_size);
    check(got_size == FIELD_OFFSET(DEV_BROADCAST_PORT_A, dbcp_name) + 1 && !got[FIELD_OFFSET(DEV_BROADCAST_PORT_A, dbcp_name)],
          "empty port name: size %lu", got_size);

    /* the structures without strings arrive as they were sent */
    memset(buf, 0, sizeof(buf));
    vol->dbcv_size = sizeof(*vol); vol->dbcv_devicetype = DBT_DEVTYP_VOLUME; vol->dbcv_unitmask = 0x4; vol->dbcv_flags = DBTF_MEDIA;
    SEND(DBT_DEVICEARRIVAL, vol->dbcv_size);
    check(got_type == DBT_DEVTYP_VOLUME && got_size == sizeof(*vol) && !memcmp(got, buf, sizeof(*vol)), "volume unchanged (type %lu size %lu)", got_type, got_size);

    memset(buf, 0, sizeof(buf));
    net->dbcn_size = sizeof(*net); net->dbcn_devicetype = DBT_DEVTYP_NET; net->dbcn_resource = 7; net->dbcn_flags = DBTF_NET;
    SEND(DBT_DEVICEARRIVAL, net->dbcn_size);
    check(got_type == DBT_DEVTYP_NET && got_size == sizeof(*net) && !memcmp(got, buf, sizeof(*net)), "net unchanged (type %lu size %lu)", got_type, got_size);

    memset(buf, 0, sizeof(buf));
    oem->dbco_size = sizeof(*oem); oem->dbco_devicetype = DBT_DEVTYP_OEM; oem->dbco_identifier = 0x55;
    SEND(DBT_DEVICEARRIVAL, oem->dbco_size);
    check(got_type == DBT_DEVTYP_OEM && got_size == sizeof(*oem) && !memcmp(got, buf, sizeof(*oem)), "oem unchanged (type %lu size %lu)", got_type, got_size);

    memset(buf, 0, sizeof(buf));
    ((DWORD *)buf)[0] = 16; ((DWORD *)buf)[1] = DBT_DEVTYP_DEVNODE; ((DWORD *)buf)[3] = 9;
    SEND(DBT_DEVICEARRIVAL, 16);
    check(got_type == DBT_DEVTYP_DEVNODE && got_size == 16 && !memcmp(got, buf, 16), "devnode unchanged (type %lu size %lu)", got_type, got_size);

out:
    if (notify) UnregisterDeviceNotification(notify);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
