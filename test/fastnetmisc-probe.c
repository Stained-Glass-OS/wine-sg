#include <winsock2.h>
#include <ws2ipdef.h>
#include <windows.h>
#include <iphlpapi.h>
#include <lm.h>
#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static void set6(SOCKADDR_IN6 *a, const char *s)
{
    int len = sizeof(*a);
    memset(a, 0, sizeof(*a));
    if (WSAStringToAddressA((char *)s, AF_INET6, NULL, (SOCKADDR *)a, &len)) printf("bad address %s\n", s);
}

int main(void)
{
    WSADATA wsa;
    SOCKADDR_IN6 src[3], dst[4];
    SOCKADDR_IN6_PAIR *pairs = NULL;
    DWORD count = 0, rc, perr = 99;
    char buf[64];
    USER_MODALS_INFO_0 *m0; USER_MODALS_INFO_1 *m1; USER_MODALS_INFO_3 *m3; void *p = NULL;
    USE_INFO_1 ui;
    int i;

    WSAStartup(MAKEWORD(2, 2), &wsa);
    set6(&src[0], "::1"); set6(&src[1], "::ffff:127.0.0.1"); set6(&src[2], "2001:db8::5");
    set6(&dst[0], "fe80::1"); set6(&dst[1], "2001:db8::1"); set6(&dst[2], "::ffff:127.0.0.1"); set6(&dst[3], "::1");
    rc = CreateSortedAddressPairs(src, 3, dst, 4, 0, &pairs, &count);
    CHECK(rc == NO_ERROR && count == 4, "sorted pairs rc %lu count %lu", rc, count);
    if (rc == NO_ERROR)
    {
        const char *want[4] = { "::1", "fe80::1", "2001:db8::1", "::ffff:127.0.0.1" };
        for (i = 0; i < 4; i++)
        {
            DWORD len = sizeof(buf);
            SOCKADDR_IN6 a = *pairs[i].DestinationAddress;
            buf[0] = 0;
            WSAAddressToStringA((SOCKADDR *)&a, sizeof(a), NULL, buf, &len);
            CHECK(!strcmp(buf, want[i]), "order[%d] = %s want %s", i, buf, want[i]);
        }
        /* ::1 pairs with the ::1 source */
        CHECK(!memcmp(&pairs[0].SourceAddress->sin6_addr, &src[0].sin6_addr, 16), "source of ::1");
        /* the mapped destination pairs with the mapped source */
        CHECK(!memcmp(&pairs[3].SourceAddress->sin6_addr, &src[1].sin6_addr, 16), "source of the mapped destination");
        HeapFree(GetProcessHeap(), 0, pairs);
    }
    CHECK(CreateSortedAddressPairs(src, 0, dst, 4, 0, &pairs, &count) == ERROR_INVALID_PARAMETER, "src list without count");
    CHECK(CreateSortedAddressPairs(NULL, 0, dst, 0, 0, &pairs, &count) == ERROR_INVALID_PARAMETER, "no destinations");
    rc = CreateSortedAddressPairs(NULL, 0, dst, 4, 0, &pairs, &count);
    CHECK(rc == NO_ERROR && count == 4, "system sources rc %lu", rc);
    if (rc == NO_ERROR) HeapFree(GetProcessHeap(), 0, pairs);

    rc = NetUserModalsGet(NULL, 0, (BYTE **)&m0);
    CHECK(rc == NERR_Success, "level 0 rc %lu", rc);
    if (!rc) { CHECK(m0->usrmod0_max_passwd_age == 42 * 86400 && m0->usrmod0_force_logoff == TIMEQ_FOREVER && m0->usrmod0_min_passwd_len == 0, "level 0 values %lu %lu", m0->usrmod0_max_passwd_age, m0->usrmod0_force_logoff); NetApiBufferFree(m0); }
    rc = NetUserModalsGet(NULL, 1, (BYTE **)&m1);
    CHECK(rc == NERR_Success, "level 1 rc %lu", rc);
    if (!rc) { CHECK(m1->usrmod1_role == 0 && m1->usrmod1_primary && !m1->usrmod1_primary[0], "level 1 values %lu", m1->usrmod1_role); NetApiBufferFree(m1); }
    rc = NetUserModalsGet(NULL, 3, (BYTE **)&m3);
    CHECK(rc == NERR_Success, "level 3 rc %lu", rc);
    if (!rc) { CHECK(m3->usrmod3_lockout_duration == 1800 && m3->usrmod3_lockout_threshold == 0, "level 3 values"); NetApiBufferFree(m3); }
    CHECK(NetUserModalsGet(NULL, 4, (BYTE **)&p) == ERROR_INVALID_LEVEL, "level 4");

    memset(&ui, 0, sizeof(ui));
    ui.ui1_local = L"Q:";
    CHECK(NetUseAdd(NULL, 1, (BYTE *)&ui, &perr) == ERROR_INVALID_PARAMETER && perr == USE_REMOTE_PARMNUM, "NetUseAdd without remote: parm_err %lu", perr);
    CHECK(NetUseAdd(NULL, 7, (BYTE *)&ui, &perr) == ERROR_INVALID_LEVEL, "NetUseAdd level 7");
    ui.ui1_remote = L"\\\\server\\share"; ui.ui1_asg_type = 77;
    CHECK(NetUseAdd(NULL, 1, (BYTE *)&ui, &perr) == ERROR_INVALID_PARAMETER && perr == USE_ASGTYPE_PARMNUM, "NetUseAdd bad type: parm_err %lu", perr);
    CHECK(NetUseDel(NULL, L"Q:", 0) == NERR_UseNotFound, "NetUseDel of an unmapped drive");
    CHECK(NetUseDel(NULL, L"Q:", 9) == ERROR_INVALID_PARAMETER, "NetUseDel bad force");
    CHECK(NetUseDel(NULL, NULL, 0) == ERROR_INVALID_PARAMETER, "NetUseDel without name");

    printf(fails ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return fails != 0;
}
