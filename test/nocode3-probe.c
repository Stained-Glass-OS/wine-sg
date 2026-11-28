/* test/nocode3-gate.sh: wlanapi profiles and connections (patches/sg/8017), against the gate's fake
 * sg-netctl (one adapter wlan0; saved networks MyNet and Cafe). */
#include <windows.h>
#include <stdio.h>
#include <wlanapi.h>

static int failures;
#define CHECK(c, ...) do { if (c) { printf("ok    "); printf(__VA_ARGS__); printf("\n"); } \
                           else { failures++; printf("FAIL  "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static HANDLE done;
static DWORD codes[16], ncodes, last_reason;
static WCHAR last_profile[WLAN_MAX_NAME_LENGTH];

static void WINAPI callback(WLAN_NOTIFICATION_DATA *data, void *context)
{
    if (data->NotificationSource != WLAN_NOTIFICATION_SOURCE_ACM) return;
    if (ncodes < 16) codes[ncodes++] = data->NotificationCode;
    if (data->NotificationCode == wlan_notification_acm_connection_complete ||
        data->NotificationCode == wlan_notification_acm_connection_attempt_fail)
    {
        WLAN_CONNECTION_NOTIFICATION_DATA *c = data->pData;
        last_reason = c->wlanReasonCode;
        wcscpy(last_profile, c->strProfileName);
    }
    if (data->NotificationCode == wlan_notification_acm_connection_complete) SetEvent(done);
}

static DWORD connect_and_wait(HANDLE h, GUID *g, WLAN_CONNECTION_PARAMETERS *p)
{
    DWORD ret;
    ncodes = 0; last_reason = 0xdeadbeef; last_profile[0] = 0;
    ResetEvent(done);
    ret = WlanConnect(h, g, p, NULL);
    if (!ret && WaitForSingleObject(done, 20000)) printf("FAIL  no connection_complete\n"), failures++;
    return ret;
}

int main(void)
{
    WLAN_INTERFACE_INFO_LIST *ifs = NULL;
    WLAN_PROFILE_INFO_LIST *profiles = NULL;
    WLAN_CONNECTION_PARAMETERS p;
    DOT11_SSID ssid;
    GUID g, bad = {0x12345678};
    HANDLE h;
    DWORD ver, ret, i;
    WCHAR text[256];
    BOOL mynet = FALSE, cafe = FALSE;

    done = CreateEventW(NULL, TRUE, FALSE, NULL);
    ret = WlanOpenHandle(2, NULL, &ver, &h);
    CHECK(ret == 0, "WlanOpenHandle %lu", ret);
    ret = WlanEnumInterfaces(h, NULL, &ifs);
    CHECK(ret == 0 && ifs->dwNumberOfItems == 1, "one adapter (%lu)", ret);
    if (ret || !ifs->dwNumberOfItems) return 1;
    g = ifs->InterfaceInfo[0].InterfaceGuid;
    WlanRegisterNotification(h, WLAN_NOTIFICATION_SOURCE_ACM, FALSE, callback, NULL, NULL, NULL);

    ret = WlanGetProfileList(h, &g, NULL, &profiles);
    for (i = 0; !ret && i < profiles->dwNumberOfItems; i++)
    {
        if (!wcscmp(profiles->ProfileInfo[i].strProfileName, L"MyNet")) mynet = TRUE;
        if (!wcscmp(profiles->ProfileInfo[i].strProfileName, L"Cafe")) cafe = TRUE;
    }
    CHECK(ret == 0 && profiles->dwNumberOfItems == 2 && mynet && cafe, "WlanGetProfileList: MyNet and Cafe (%lu)", ret);
    if (!ret) WlanFreeMemory(profiles);
    CHECK(WlanGetProfileList(h, &bad, NULL, &profiles) == ERROR_NOT_FOUND, "another interface");
    CHECK(WlanGetProfileList(h, &g, NULL, NULL) == ERROR_INVALID_PARAMETER, "no list");

    memset(&p, 0, sizeof(p));
    p.wlanConnectionMode = wlan_connection_mode_profile;
    p.strProfile = L"MyNet";
    p.dot11BssType = dot11_BSS_type_infrastructure;
    ret = connect_and_wait(h, &g, &p);
    CHECK(ret == 0 && last_reason == 0 && !wcscmp(last_profile, L"MyNet"), "WlanConnect MyNet: %lu, reason %#lx", ret, last_reason);
    CHECK(ncodes >= 2 && codes[0] == wlan_notification_acm_connection_start &&
          codes[ncodes - 1] == wlan_notification_acm_connection_complete, "start ... complete (%lu notifications)", ncodes);

    p.strProfile = L"Nowhere";
    CHECK(WlanConnect(h, &g, &p, NULL) == ERROR_NOT_FOUND, "a profile that does not exist");
    p.dot11BssType = dot11_BSS_type_independent;
    CHECK(WlanConnect(h, &g, &p, NULL) == ERROR_NOT_SUPPORTED, "an ad hoc network");
    p.dot11BssType = dot11_BSS_type_infrastructure;
    CHECK(WlanConnect(h, &g, NULL, NULL) == ERROR_INVALID_PARAMETER, "no parameters");

    /* a temporary profile with a key: the key goes on sg-netctl's standard input */
    p.wlanConnectionMode = wlan_connection_mode_temporary_profile;
    p.strProfile = L"<?xml version=\"1.0\"?><WLANProfile xmlns=\"http://www.microsoft.com/networking/WLAN/profile/v1\">"
                   L"<name>Guest &amp; Co</name><SSIDConfig><SSID><name>Guest &amp; Co</name></SSID></SSIDConfig>"
                   L"<connectionType>ESS</connectionType><connectionMode>manual</connectionMode><MSM><security>"
                   L"<authEncryption><authentication>WPA2PSK</authentication><encryption>AES</encryption>"
                   L"<useOneX>false</useOneX></authEncryption><sharedKey><keyType>passPhrase</keyType>"
                   L"<protected>false</protected><keyMaterial>s3cret pass</keyMaterial></sharedKey></security></MSM></WLANProfile>";
    ret = connect_and_wait(h, &g, &p);
    CHECK(ret == 0 && last_reason == 0 && !wcscmp(last_profile, L"Guest & Co"), "a temporary WPA2 profile: %lu, reason %#lx, %ls", ret, last_reason, last_profile);
    p.strProfile = L"<WLANProfile><name>W</name><SSIDConfig><SSID><name>W</name></SSID></SSIDConfig><MSM><security>"
                   L"<authEncryption><authentication>WPA2</authentication><encryption>AES</encryption></authEncryption>"
                   L"</security></MSM></WLANProfile>";
    CHECK(WlanConnect(h, &g, &p, NULL) == ERROR_NOT_SUPPORTED, "an enterprise (802.1X) profile");
    p.strProfile = L"not a profile";
    CHECK(WlanConnect(h, &g, &p, NULL) == ERROR_BAD_PROFILE, "text that is not a profile");

    /* a network the fake says needs another key: the attempt fails in the notifications */
    p.wlanConnectionMode = wlan_connection_mode_profile;
    p.strProfile = L"Cafe";
    ret = connect_and_wait(h, &g, &p);
    CHECK(ret == 0 && last_reason == WLAN_REASON_CODE_MSMSEC_PSK_MISMATCH_SUSPECTED, "a refused key: reason %#lx", last_reason);
    for (i = 0; i < ncodes; i++) if (codes[i] == wlan_notification_acm_connection_attempt_fail) break;
    CHECK(i < ncodes, "connection_attempt_fail was sent");

    p.wlanConnectionMode = wlan_connection_mode_discovery_unsecure;
    memset(&ssid, 0, sizeof(ssid));
    ssid.uSSIDLength = 4; memcpy(ssid.ucSSID, "Open", 4);
    p.pDot11Ssid = &ssid;
    p.strProfile = NULL;
    ret = connect_and_wait(h, &g, &p);
    CHECK(ret == 0 && last_reason == 0, "an open network by its SSID: %lu %#lx", ret, last_reason);

    ret = WlanDisconnect(h, &g, NULL);
    CHECK(ret == 0, "WlanDisconnect %lu", ret);
    CHECK(WlanDisconnect(h, &bad, NULL) == ERROR_NOT_FOUND, "WlanDisconnect, another interface");

    CHECK(WlanDeleteProfile(h, &g, L"Nowhere", NULL) == ERROR_NOT_FOUND, "WlanDeleteProfile of no profile");
    ret = WlanDeleteProfile(h, &g, L"cafe", NULL);
    CHECK(ret == 0, "WlanDeleteProfile cafe %lu", ret);

    ret = WlanReasonCodeToString(WLAN_REASON_CODE_NETWORK_NOT_AVAILABLE, ARRAYSIZE(text), text, NULL);
    CHECK(ret == 0 && text[0], "WlanReasonCodeToString: %ls", text);
    CHECK(WlanReasonCodeToString(0, 0, text, NULL) == ERROR_INVALID_PARAMETER, "no buffer size");

    WlanCloseHandle(h, NULL);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
