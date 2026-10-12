/* wlanapi, fast mode: interfaces, scan with notification, available networks and interface queries, through a
 * canned sg-netctl (SG_NETCTL). */
#include <windows.h>
#include <wlanapi.h>
#include <stdio.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

static HANDLE scan_done;
static DWORD scan_code;
static void WINAPI notify(WLAN_NOTIFICATION_DATA *data, void *ctx)
{
    scan_code = data->NotificationCode;
    SetEvent(scan_done);
}

int main(void)
{
    HANDLE h;
    DWORD ver, size, *value, prev = 123, ret;
    WLAN_INTERFACE_INFO_LIST *ifaces;
    WLAN_AVAILABLE_NETWORK_LIST *nets;
    WLAN_OPCODE_VALUE_TYPE type;
    GUID guid, bad = { 1, 2, 3, { 4 } };
    void *data;

    scan_done = CreateEventA(NULL, FALSE, FALSE, NULL);
    check(WlanOpenHandle(2, NULL, &ver, &h) == ERROR_SUCCESS, "WlanOpenHandle");
    check(WlanEnumInterfaces(h, NULL, &ifaces) == ERROR_SUCCESS && ifaces->dwNumberOfItems == 1, "one wireless interface");
    guid = ifaces->InterfaceInfo[0].InterfaceGuid;
    check(!wcscmp(ifaces->InterfaceInfo[0].strInterfaceDescription, L"wlan0"), "described by its device");
    check(ifaces->InterfaceInfo[0].isState == wlan_interface_state_connected, "connected");
    WlanFreeMemory(ifaces);

    check(WlanRegisterNotification(h, WLAN_NOTIFICATION_SOURCE_ACM, TRUE, notify, NULL, NULL, &prev) == ERROR_SUCCESS && prev == 0,
          "register for ACM notifications");
    check(WlanScan(h, &bad, NULL, NULL, NULL) == ERROR_NOT_FOUND, "scan on an unknown interface");
    check(WlanScan(h, &guid, NULL, NULL, NULL) == ERROR_SUCCESS, "scan");
    check(WaitForSingleObject(scan_done, 10000) == WAIT_OBJECT_0 && scan_code == 7, "scan complete notification");

    ret = WlanGetAvailableNetworkList(h, &guid, 0, NULL, &nets);
    check(ret == ERROR_SUCCESS && nets->dwNumberOfItems == 2, "two networks");
    if (ret == ERROR_SUCCESS && nets->dwNumberOfItems == 2)
    {
        WLAN_AVAILABLE_NETWORK *n = &nets->Network[0];
        check(n->dot11Ssid.uSSIDLength == 5 && !memcmp(n->dot11Ssid.ucSSID, "MyNet", 5), "first SSID");
        check(n->wlanSignalQuality == 80 && n->bSecurityEnabled && n->dwFlags == 3, "first: quality, secured, connected with profile");
        check(!wcscmp(n->strProfileName, L"MyNet"), "profile name");
        n = &nets->Network[1];
        check(n->dot11Ssid.uSSIDLength == 4 && !memcmp(n->dot11Ssid.ucSSID, "Cafe", 4) && !n->bSecurityEnabled && n->dwFlags == 0,
              "second: open, not connected");
        WlanFreeMemory(nets);
    }
    check(WlanGetAvailableNetworkList(h, &bad, 0, NULL, &nets) == ERROR_NOT_FOUND, "networks of an unknown interface");

    ret = WlanQueryInterface(h, &guid, wlan_intf_opcode_interface_state, NULL, &size, &data, &type);
    value = data;
    check(ret == ERROR_SUCCESS && size == 4 && *value == wlan_interface_state_connected, "interface state");
    if (ret == ERROR_SUCCESS) WlanFreeMemory(data);
    ret = WlanQueryInterface(h, &guid, wlan_intf_opcode_autoconf_enabled, NULL, &size, &data, &type);
    check(ret == ERROR_SUCCESS && *(DWORD *)data == 1, "autoconf enabled");
    if (ret == ERROR_SUCCESS) WlanFreeMemory(data);

    check(WlanRegisterNotification(h, 0, TRUE, NULL, NULL, NULL, &prev) == ERROR_SUCCESS && prev == WLAN_NOTIFICATION_SOURCE_ACM,
          "unregister");
    check(WlanCloseHandle(h, NULL) == ERROR_SUCCESS, "close");
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
