/* Native probe for the DirectPlayLobby methods of dplayx that used to be
 * stubs (patch 2988): RegisterApplication, UnregisterApplication,
 * EnumLocalApplications (Unicode), RunApplication (Unicode),
 * Send/ReceiveLobbyMessage, SetLobbyMessageEvent and ConnectEx. It runs on
 * the service provider of test/dplayx-fakesp.c (no network). The probe starts
 * itself as the lobbied application ("--app") to check the messages across
 * processes. Table-driven; values are checked, not only success. */
#define COBJMACROS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <initguid.h>
#include <windows.h>
#include <ole2.h>
#include "dplayx-fakesp.h"

static int checks, failures;

#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL  " __VA_ARGS__); printf("\n"); fflush(stdout); } } while (0)
#define CHECKHR(exp, got, what) CHECK((HRESULT)(got) == (HRESULT)(exp), "%s: got 0x%08lx, expected 0x%08lx", what, (unsigned long)(got), (unsigned long)(exp))

static IDirectPlayLobby3A *lobbyA;
static IDirectPlayLobby3 *lobbyW;

static const GUID APP1 = { 0x5e551001, 0x2222, 0x4333, { 0x94, 0x44, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 } };
static const GUID APP2 = { 0x5e551002, 0x2222, 0x4333, { 0x94, 0x44, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 } };
static const GUID APPRUN = { 0x5e551003, 0x2222, 0x4333, { 0x94, 0x44, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 } };
static const GUID APPNONE = { 0x5e5510ff, 0x2222, 0x4333, { 0x94, 0x44, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 } };

/* ---- the two flavours of every call ---- */

static HRESULT l_send(int w, DWORD flags, DWORD app, void *data, DWORD size)
{
    return w ? IDirectPlayLobby_SendLobbyMessage(lobbyW, flags, app, data, size)
             : IDirectPlayLobby_SendLobbyMessage(lobbyA, flags, app, data, size);
}
static HRESULT l_receive(int w, DWORD flags, DWORD app, DWORD *msgflags, void *data, DWORD *size)
{
    return w ? IDirectPlayLobby_ReceiveLobbyMessage(lobbyW, flags, app, msgflags, data, size)
             : IDirectPlayLobby_ReceiveLobbyMessage(lobbyA, flags, app, msgflags, data, size);
}
static HRESULT l_setevent(int w, DWORD flags, DWORD app, HANDLE event)
{
    return w ? IDirectPlayLobby_SetLobbyMessageEvent(lobbyW, flags, app, event)
             : IDirectPlayLobby_SetLobbyMessageEvent(lobbyA, flags, app, event);
}

/* ---- registered applications ---- */

struct enum_ctx { int count; int found1, found2; WCHAR name1[64]; BOOL ansi; int stop_after; char names[8][64]; };

static BOOL CALLBACK enum_appsA(LPCDPLAPPINFO info, void *ctx, DWORD flags)
{
    struct enum_ctx *c = ctx;

    CHECK(info->dwSize == sizeof(DPLAPPINFO), "DPLAPPINFO size %lu", info->dwSize);
    CHECK(flags == 0, "enum flags 0x%lx", flags);
    if (IsEqualGUID(&info->guidApplication, &APP1))
    {
        c->found1++;
        CHECK(!strcmp(info->lpszAppNameA, "SG Probe App One"), "A name '%s'", info->lpszAppNameA);
    }
    if (IsEqualGUID(&info->guidApplication, &APP2)) c->found2++;
    c->count++;
    return c->stop_after ? c->count < c->stop_after : TRUE;
}

static BOOL CALLBACK enum_appsW(LPCDPLAPPINFO info, void *ctx, DWORD flags)
{
    struct enum_ctx *c = ctx;

    CHECK(info->dwSize == sizeof(DPLAPPINFO), "DPLAPPINFO size %lu", info->dwSize);
    if (IsEqualGUID(&info->guidApplication, &APP1))
    {
        c->found1++;
        CHECK(!lstrcmpW(info->lpszAppName, L"SG Probe App One"), "W name %ls", info->lpszAppName);
    }
    if (IsEqualGUID(&info->guidApplication, &APP2))
    {
        c->found2++;
        CHECK(!lstrcmpW(info->lpszAppName, L"SG Probe App Two"), "W name %ls", info->lpszAppName);
    }
    c->count++;
    return c->stop_after ? c->count < c->stop_after : TRUE;
}

static HRESULT l_enumapps(int w, LPDPLENUMLOCALAPPLICATIONSCALLBACK cb, void *ctx, DWORD flags)
{
    return w ? IDirectPlayLobby_EnumLocalApplications(lobbyW, cb, ctx, flags)
             : IDirectPlayLobby_EnumLocalApplications(lobbyA, cb, ctx, flags);
}

static BOOL regval(const char *app, const char *value, char *out, DWORD size)
{
    char path[256];
    HKEY key;
    LONG ret;

    sprintf(path, "SOFTWARE\\Microsoft\\DirectPlay\\Applications\\%s", app);
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &key)) return FALSE;
    ret = RegQueryValueExA(key, value, NULL, NULL, (BYTE *)out, &size);
    RegCloseKey(key);
    return ret == ERROR_SUCCESS;
}

static void test_applications(void)
{
    DPAPPLICATIONDESC desc;
    DPAPPLICATIONDESC bad;
    struct enum_ctx ctx;
    char value[256];
    HRESULT hr;
    int w;
    unsigned i;

    /* the arguments */
    for (w = 0; w < 2; w++)
    {
        const char *tag = w ? "W" : "A";
        char what[96];

        memset(&desc, 0, sizeof(desc));
        desc.dwSize = sizeof(desc);
        desc.guidApplication = APP1;
        if (w) { desc.lpszApplicationName = (WCHAR *)L"SG Probe App One"; desc.lpszFilename = (WCHAR *)L"probe.exe"; desc.lpszPath = (WCHAR *)L"C:\\"; }
        else   { desc.lpszApplicationNameA = (char *)"SG Probe App One"; desc.lpszFilenameA = (char *)"probe.exe"; desc.lpszPathA = (char *)"C:\\"; }

        sprintf(what, "%s RegisterApplication(flags 1)", tag);
        hr = w ? IDirectPlayLobby_RegisterApplication(lobbyW, 1, &desc) : IDirectPlayLobby_RegisterApplication(lobbyA, 1, &desc);
        CHECKHR(DPERR_INVALIDFLAGS, hr, what);
        sprintf(what, "%s RegisterApplication(NULL)", tag);
        hr = w ? IDirectPlayLobby_RegisterApplication(lobbyW, 0, NULL) : IDirectPlayLobby_RegisterApplication(lobbyA, 0, NULL);
        CHECKHR(DPERR_INVALIDPARAMS, hr, what);

        bad = desc; bad.dwSize = 20;
        sprintf(what, "%s RegisterApplication(bad size)", tag);
        hr = w ? IDirectPlayLobby_RegisterApplication(lobbyW, 0, &bad) : IDirectPlayLobby_RegisterApplication(lobbyA, 0, &bad);
        CHECKHR(DPERR_INVALIDPARAMS, hr, what);
        bad = desc; bad.lpszApplicationName = NULL;
        sprintf(what, "%s RegisterApplication(no name)", tag);
        hr = w ? IDirectPlayLobby_RegisterApplication(lobbyW, 0, &bad) : IDirectPlayLobby_RegisterApplication(lobbyA, 0, &bad);
        CHECKHR(DPERR_INVALIDPARAMS, hr, what);
        bad = desc; bad.lpszFilename = NULL;
        sprintf(what, "%s RegisterApplication(no file)", tag);
        hr = w ? IDirectPlayLobby_RegisterApplication(lobbyW, 0, &bad) : IDirectPlayLobby_RegisterApplication(lobbyA, 0, &bad);
        CHECKHR(DPERR_INVALIDPARAMS, hr, what);
        bad = desc; bad.lpszPath = NULL;
        sprintf(what, "%s RegisterApplication(no path)", tag);
        hr = w ? IDirectPlayLobby_RegisterApplication(lobbyW, 0, &bad) : IDirectPlayLobby_RegisterApplication(lobbyA, 0, &bad);
        CHECKHR(DPERR_INVALIDPARAMS, hr, what);
    }

    /* nothing is registered by now */
    memset(&ctx, 0, sizeof(ctx));
    hr = l_enumapps(0, enum_appsA, &ctx, 0);
    CHECKHR(S_OK, hr, "EnumLocalApplications before");
    CHECK(ctx.found1 == 0 && ctx.found2 == 0, "applications of the probe found before they were registered");

    /* register one with the ANSI interface, one with the Unicode one */
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.guidApplication = APP1;
    desc.lpszApplicationNameA = (char *)"SG Probe App One";
    desc.lpszFilenameA = (char *)"probe.exe";
    desc.lpszPathA = (char *)"C:\\";
    desc.lpszCommandLineA = (char *)"--app one";
    desc.lpszCurrentDirectoryA = (char *)"C:\\";
    desc.lpszDescriptionA = (char *)"first";
    hr = IDirectPlayLobby_RegisterApplication(lobbyA, 0, &desc);
    CHECKHR(S_OK, hr, "A RegisterApplication");

    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.guidApplication = APP2;
    desc.lpszApplicationName = (WCHAR *)L"SG Probe App Two";
    desc.lpszFilename = (WCHAR *)L"two.exe";
    desc.lpszPath = (WCHAR *)L"C:\\games";
    desc.lpszCommandLine = (WCHAR *)L"-two";
    desc.lpszCurrentDirectory = (WCHAR *)L"C:\\games";
    desc.lpszDescriptionW = (WCHAR *)L"second";
    hr = IDirectPlayLobby_RegisterApplication(lobbyW, 0, &desc);
    CHECKHR(S_OK, hr, "W RegisterApplication");

    {
        static const struct { const char *app, *value, *expect; } rows[] =
        {
            { "SG Probe App One", "Guid", "{5E551001-2222-4333-9444-010203040506}" },
            { "SG Probe App One", "File", "probe.exe" },
            { "SG Probe App One", "Path", "C:\\" },
            { "SG Probe App One", "CommandLine", "--app one" },
            { "SG Probe App One", "CurrentDirectory", "C:\\" },
            { "SG Probe App One", "DescriptionA", "first" },
            { "SG Probe App Two", "Guid", "{5E551002-2222-4333-9444-010203040506}" },
            { "SG Probe App Two", "File", "two.exe" },
            { "SG Probe App Two", "Path", "C:\\games" },
            { "SG Probe App Two", "CommandLine", "-two" },
        };
        for (i = 0; i < ARRAYSIZE(rows); i++)
        {
            BOOL ok = regval(rows[i].app, rows[i].value, value, sizeof(value));
            CHECK(ok && !lstrcmpiA(value, rows[i].expect), "registry %s/%s: '%s' expected '%s'", rows[i].app, rows[i].value, ok ? value : "(missing)", rows[i].expect);
        }
    }

    /* enumerate with both interfaces */
    for (w = 0; w < 2; w++)
    {
        char what[64];
        memset(&ctx, 0, sizeof(ctx));
        hr = l_enumapps(w, w ? enum_appsW : enum_appsA, &ctx, 0);
        sprintf(what, "%s EnumLocalApplications", w ? "W" : "A");
        CHECKHR(S_OK, hr, what);
        CHECK(ctx.found1 == 1, "%s: App One found %d times", what, ctx.found1);
        CHECK(ctx.found2 == 1, "%s: App Two found %d times", what, ctx.found2);

        /* the callback can stop it */
        memset(&ctx, 0, sizeof(ctx));
        ctx.stop_after = 1;
        hr = l_enumapps(w, w ? enum_appsW : enum_appsA, &ctx, 0);
        CHECKHR(S_OK, hr, what);
        CHECK(ctx.count == 1, "%s: stopped enumeration called back %d times", what, ctx.count);

        sprintf(what, "%s EnumLocalApplications(flags 1)", w ? "W" : "A");
        hr = l_enumapps(w, enum_appsA, &ctx, 1);
        CHECKHR(DPERR_INVALIDPARAMS, hr, what);
        sprintf(what, "%s EnumLocalApplications(NULL)", w ? "W" : "A");
        hr = l_enumapps(w, NULL, &ctx, 0);
        CHECKHR(DPERR_INVALIDPARAMS, hr, what);
    }

    /* registering again replaces, unregistering removes */
    hr = IDirectPlayLobby_RegisterApplication(lobbyA, 0, &desc);
    CHECKHR(S_OK, hr, "RegisterApplication again");

    hr = IDirectPlayLobby_UnregisterApplication(lobbyA, 1, &APP1);
    CHECKHR(DPERR_INVALIDFLAGS, hr, "A UnregisterApplication(flags 1)");
    hr = IDirectPlayLobby_UnregisterApplication(lobbyA, 0, &APPNONE);
    CHECKHR(DPERR_UNKNOWNAPPLICATION, hr, "A UnregisterApplication(unknown)");
    hr = IDirectPlayLobby_UnregisterApplication(lobbyA, 0, &APP1);
    CHECKHR(S_OK, hr, "A UnregisterApplication");
    hr = IDirectPlayLobby_UnregisterApplication(lobbyW, 0, &APP2);
    CHECKHR(S_OK, hr, "W UnregisterApplication");
    hr = IDirectPlayLobby_UnregisterApplication(lobbyW, 0, &APP1);
    CHECKHR(DPERR_UNKNOWNAPPLICATION, hr, "W UnregisterApplication(again)");

    memset(&ctx, 0, sizeof(ctx));
    hr = l_enumapps(1, enum_appsW, &ctx, 0);
    CHECKHR(S_OK, hr, "EnumLocalApplications after");
    CHECK(ctx.found1 == 0 && ctx.found2 == 0, "unregistered applications still listed (%d, %d)", ctx.found1, ctx.found2);
    CHECK(!regval("SG Probe App One", "Guid", value, sizeof(value)), "registry key of an unregistered application is still there");
}

/* ---- lobby messages ---- */

static void test_messages(void)
{
    static const struct { const char *name; DWORD flags; DWORD app; BOOL nodata; DWORD size; HRESULT hr; } sends[] =
    {
        { "flags 1 (system)", 1, 0, FALSE, 4, DPERR_INVALIDFLAGS },
        { "flags 4", 4, 0, FALSE, 4, DPERR_INVALIDFLAGS },
        { "no data", 0, 0, TRUE, 4, DPERR_INVALIDPARAMS },
        { "no size", 0, 0, FALSE, 0, DPERR_INVALIDPARAMS },
        { "too big", 0, 0, FALSE, 100000, DPERR_BUFFERTOOLARGE },
        { "unknown application", 0, 0x7ff0, FALSE, 4, DPERR_NOTLOBBIED },
    };
    DWORD pid = GetCurrentProcessId();
    DPLCONNECTION conn;
    DPSESSIONDESC2 sd;
    HRESULT hr;
    DWORD msgflags, size;
    BYTE buf[3000], big[1500];
    HANDLE ev_app, ev_lobby;
    unsigned i;
    int w, n;

    /* this process is its own lobbied application: that is what settings
     * for an application that was not started by a lobby client create */
    memset(&sd, 0, sizeof(sd));
    sd.dwSize = sizeof(sd);
    sd.guidApplication = APPRUN;
    memset(&conn, 0, sizeof(conn));
    conn.dwSize = sizeof(conn);
    conn.lpSessionDesc = &sd;
    hr = IDirectPlayLobby_SetConnectionSettings(lobbyA, 0, 0, &conn);
    CHECKHR(S_OK, hr, "SetConnectionSettings");

    for (w = 0; w < 2; w++)
    {
        const char *tag = w ? "W" : "A";
        char what[96];

        for (i = 0; i < ARRAYSIZE(sends); i++)
        {
            char data[16] = "data";
            sprintf(what, "%s SendLobbyMessage %s", tag, sends[i].name);
            hr = l_send(w, sends[i].flags, sends[i].app ? sends[i].app : 0, sends[i].nodata ? NULL : (sends[i].size > 16 ? (void *)buf : (void *)data), sends[i].size);
            CHECKHR(sends[i].hr, hr, what);
        }

        /* receive arguments */
        sprintf(what, "%s ReceiveLobbyMessage(flags 1)", tag);
        size = sizeof(buf);
        hr = l_receive(w, 1, 0, &msgflags, buf, &size);
        CHECKHR(DPERR_INVALIDFLAGS, hr, what);
        sprintf(what, "%s ReceiveLobbyMessage(no flags pointer)", tag);
        hr = l_receive(w, 0, 0, NULL, buf, &size);
        CHECKHR(DPERR_INVALIDPARAMS, hr, what);
        sprintf(what, "%s ReceiveLobbyMessage(no size pointer)", tag);
        hr = l_receive(w, 0, 0, &msgflags, buf, NULL);
        CHECKHR(DPERR_INVALIDPARAMS, hr, what);
        sprintf(what, "%s ReceiveLobbyMessage(unknown application)", tag);
        hr = l_receive(w, 0, 0x7ff0, &msgflags, buf, &size);
        CHECKHR(DPERR_NOTLOBBIED, hr, what);
        sprintf(what, "%s ReceiveLobbyMessage(empty, application)", tag);
        size = sizeof(buf);
        hr = l_receive(w, 0, 0, &msgflags, buf, &size);
        CHECKHR(DPERR_NOMESSAGES, hr, what);
        sprintf(what, "%s ReceiveLobbyMessage(empty, client)", tag);
        hr = l_receive(w, 0, pid, &msgflags, buf, &size);
        CHECKHR(DPERR_NOMESSAGES, hr, what);

        /* client -> application, application -> client: two queues */
        sprintf(what, "%s client send", tag);
        hr = l_send(w, DPLMSG_STANDARD, pid, "to-app-1", 9);
        CHECKHR(S_OK, hr, what);
        hr = l_send(w, 0, pid, "to-app-2", 9);
        CHECKHR(S_OK, hr, what);
        sprintf(what, "%s application send", tag);
        hr = l_send(w, 0, 0, "to-client", 10);
        CHECKHR(S_OK, hr, what);

        /* a buffer that is too small leaves the message in the queue */
        size = 4; msgflags = 0;
        sprintf(what, "%s receive with a small buffer", tag);
        hr = l_receive(w, 0, 0, &msgflags, buf, &size);
        CHECKHR(DPERR_BUFFERTOOSMALL, hr, what);
        CHECK(size == 9, "%s: size %lu expected 9", what, size);
        size = 4;
        hr = l_receive(w, 0, 0, &msgflags, NULL, &size);
        CHECKHR(DPERR_BUFFERTOOSMALL, hr, what);
        CHECK(size == 9, "%s (no buffer): size %lu expected 9", what, size);

        /* in order, with the flags of the message */
        for (i = 1; i <= 2; i++)
        {
            char expect[16];
            sprintf(expect, "to-app-%u", i);
            memset(buf, 0, 16);
            size = sizeof(buf); msgflags = 0;
            sprintf(what, "%s application receive %u", tag, i);
            hr = l_receive(w, 0, 0, &msgflags, buf, &size);
            CHECKHR(S_OK, hr, what);
            CHECK(size == 9 && !memcmp(buf, expect, 9), "%s: size %lu data '%.9s' expected '%s'", what, size, buf, expect);
            CHECK(msgflags == DPLMSG_STANDARD, "%s: message flags 0x%lx", what, msgflags);
        }
        size = sizeof(buf);
        hr = l_receive(w, 0, 0, &msgflags, buf, &size);
        CHECKHR(DPERR_NOMESSAGES, hr, "application queue is empty again");

        sprintf(what, "%s client receive", tag);
        size = sizeof(buf); msgflags = 0;
        hr = l_receive(w, 0, pid, &msgflags, buf, &size);
        CHECKHR(S_OK, hr, what);
        CHECK(size == 10 && !memcmp(buf, "to-client", 10) && msgflags == DPLMSG_STANDARD, "%s: size %lu flags 0x%lx", what, size, msgflags);

        /* a message that takes several blocks of the shared storage */
        for (i = 0; i < sizeof(big); i++) big[i] = (BYTE)(i * 7 + w);
        sprintf(what, "%s long message", tag);
        hr = l_send(w, 0, pid, big, sizeof(big));
        CHECKHR(S_OK, hr, what);
        memset(buf, 0, sizeof(buf));
        size = sizeof(buf);
        hr = l_receive(w, 0, 0, &msgflags, buf, &size);
        CHECKHR(S_OK, hr, what);
        CHECK(size == sizeof(big) && !memcmp(buf, big, sizeof(big)), "%s: size %lu, data differs", what, size);

        /* a full queue says so, and empties again */
        n = 0;
        while (l_send(w, 0, pid, "fill", 5) == S_OK && n < 1000) n++;
        CHECK(n >= 16 && n < 1000, "%s: queue took %d messages", what, n);
        hr = l_send(w, 0, pid, "fill", 5);
        CHECKHR(DPERR_OUTOFMEMORY, hr, "send to a full queue");
        i = 0;
        for (;;)
        {
            size = sizeof(buf);
            if (l_receive(w, 0, 0, &msgflags, buf, &size) != S_OK) break;
            i++;
        }
        CHECK((int)i == n, "%s: %d messages went in, %u came out", what, n, i);
        hr = l_send(w, 0, pid, "after", 6);
        CHECKHR(S_OK, hr, "send after the queue emptied");
        size = sizeof(buf);
        hr = l_receive(w, 0, 0, &msgflags, buf, &size);
        CHECKHR(S_OK, hr, "receive after the queue emptied");
    }

    /* receive events: the side that sends signals what the other asked for */
    ev_app = CreateEventA(NULL, FALSE, FALSE, NULL);
    ev_lobby = CreateEventA(NULL, FALSE, FALSE, NULL);
    for (w = 0; w < 2; w++)
    {
        char what[96];
        sprintf(what, "%s SetLobbyMessageEvent(flags 1)", w ? "W" : "A");
        hr = l_setevent(w, 1, 0, ev_app);
        CHECKHR(DPERR_INVALIDFLAGS, hr, what);
        sprintf(what, "%s SetLobbyMessageEvent(unknown application)", w ? "W" : "A");
        hr = l_setevent(w, 0, 0x7ff0, ev_app);
        CHECKHR(DPERR_NOTLOBBIED, hr, what);
    }
    hr = l_setevent(0, 0, 0, ev_app);
    CHECKHR(S_OK, hr, "SetLobbyMessageEvent (application)");
    hr = l_setevent(1, 0, pid, ev_lobby);
    CHECKHR(S_OK, hr, "SetLobbyMessageEvent (client)");
    CHECK(WaitForSingleObject(ev_app, 0) == WAIT_TIMEOUT && WaitForSingleObject(ev_lobby, 0) == WAIT_TIMEOUT, "events signalled before any message");
    hr = l_send(0, 0, pid, "ping", 5);
    CHECKHR(S_OK, hr, "client sends");
    CHECK(WaitForSingleObject(ev_app, 0) == WAIT_OBJECT_0, "the application's event was not signalled by the client's message");
    CHECK(WaitForSingleObject(ev_lobby, 0) == WAIT_TIMEOUT, "the client's event was signalled by its own message");
    hr = l_send(1, 0, 0, "pong", 5);
    CHECKHR(S_OK, hr, "application sends");
    CHECK(WaitForSingleObject(ev_lobby, 0) == WAIT_OBJECT_0, "the client's event was not signalled by the application's message");
    CHECK(WaitForSingleObject(ev_app, 0) == WAIT_TIMEOUT, "the application's event was signalled by its own message");
    size = sizeof(buf);
    l_receive(0, 0, 0, &msgflags, buf, &size);
    size = sizeof(buf);
    l_receive(0, 0, pid, &msgflags, buf, &size);
    /* clearing the event */
    hr = l_setevent(0, 0, 0, NULL);
    CHECKHR(S_OK, hr, "SetLobbyMessageEvent(NULL)");
    l_send(0, 0, pid, "ping", 5);
    CHECK(WaitForSingleObject(ev_app, 0) == WAIT_TIMEOUT, "a cleared event was signalled");
    size = sizeof(buf);
    l_receive(0, 0, 0, &msgflags, buf, &size);
    CloseHandle(ev_app);
    CloseHandle(ev_lobby);
}

/* ---- ConnectEx ---- */

static void make_address(void **address, DWORD *size, const GUID *provider)
{
    DPCOMPOUNDADDRESSELEMENT elem;
    HRESULT hr;

    elem.guidDataType = DPAID_ServiceProvider;
    elem.dwDataSize = sizeof(GUID);
    elem.lpData = (void *)provider;
    *size = 0;
    hr = IDirectPlayLobby_CreateCompoundAddress(lobbyA, &elem, 1, NULL, size);
    CHECKHR(DPERR_BUFFERTOOSMALL, hr, "CreateCompoundAddress size");
    *address = calloc(1, *size);
    hr = IDirectPlayLobby_CreateCompoundAddress(lobbyA, &elem, 1, *address, size);
    CHECKHR(S_OK, hr, "CreateCompoundAddress");
}

static BOOL CALLBACK count_players(DPID id, DWORD type, LPCDPNAME name, DWORD flags, void *ctx)
{
    (*(int *)ctx)++;
    return TRUE;
}

static void test_connect(void)
{
    static const struct { const char *name; REFIID riid; } ifaces[] =
    {
        { "IDirectPlay2A", &IID_IDirectPlay2A }, { "IDirectPlay3A", &IID_IDirectPlay3A },
        { "IDirectPlay4A", &IID_IDirectPlay4A }, { "IDirectPlay4", &IID_IDirectPlay4 },
    };
    DPSESSIONDESC2 sd;
    DPLCONNECTION conn;
    IUnknown *unk;
    IDirectPlay4 *dp4;
    void *address;
    DWORD addrsize;
    HRESULT hr;
    unsigned i;
    int n;
    char buf[512];
    DWORD size;

    /* no settings for this process yet (the test of the messages made them
     * and they are made for this process id: a second lobby object, here,
     * would see them too) */
    unk = (IUnknown *)0xdeadbeef;
    hr = IDirectPlayLobby_ConnectEx(lobbyA, 0, &IID_IDirectPlay4A, (void **)&unk, (IUnknown *)lobbyA);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "ConnectEx(aggregation)");
    hr = IDirectPlayLobby_ConnectEx(lobbyA, 0, &IID_IDirectPlay4A, NULL, NULL);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "ConnectEx(NULL)");
    hr = IDirectPlayLobby_ConnectEx(lobbyA, 0x10, &IID_IDirectPlay4A, (void **)&unk, NULL);
    CHECKHR(DPERR_INVALIDFLAGS, hr, "ConnectEx(flags 0x10)");

    memset(&sd, 0, sizeof(sd));
    sd.dwSize = sizeof(sd);
    sd.guidApplication = APPRUN;
    sd.dwMaxPlayers = 6;
    sd.lpszSessionNameA = (char *)"connect-ex";
    memset(&conn, 0, sizeof(conn));
    conn.dwSize = sizeof(conn);
    conn.dwFlags = DPLCONNECTION_CREATESESSION;
    conn.lpSessionDesc = &sd;

    /* an address of a provider that is not there: no object, the error of the provider */
    make_address(&address, &addrsize, &APPNONE);
    conn.lpAddress = address;
    conn.dwAddressSize = addrsize;
    hr = IDirectPlayLobby_SetConnectionSettings(lobbyA, 0, 0, &conn);
    CHECKHR(S_OK, hr, "SetConnectionSettings (unknown provider)");
    unk = (IUnknown *)0xdeadbeef;
    hr = IDirectPlayLobby_ConnectEx(lobbyA, 0, &IID_IDirectPlay4A, (void **)&unk, NULL);
    CHECKHR(DPERR_UNAVAILABLE, hr, "ConnectEx(unknown provider)");
    CHECK(unk == NULL, "ConnectEx left an interface after failing: %p", unk);
    free(address);

    make_address(&address, &addrsize, &SGSP_GUID);
    conn.lpAddress = address;
    conn.dwAddressSize = addrsize;
    hr = IDirectPlayLobby_SetConnectionSettings(lobbyA, 0, 0, &conn);
    CHECKHR(S_OK, hr, "SetConnectionSettings");

    for (i = 0; i < ARRAYSIZE(ifaces); i++)
    {
        char what[96];

        unk = NULL;
        sprintf(what, "ConnectEx(%s)", ifaces[i].name);
        hr = IDirectPlayLobby_ConnectEx(lobbyA, 0, ifaces[i].riid, (void **)&unk, NULL);
        CHECKHR(S_OK, hr, what);
        if (FAILED(hr) || !unk) continue;

        hr = IUnknown_QueryInterface(unk, &IID_IDirectPlay4A, (void **)&dp4);
        CHECKHR(S_OK, hr, "QueryInterface IDirectPlay4A of the connected object");
        if (hr == S_OK)
        {
            /* the session of the settings is open: players can be listed and made */
            n = 0;
            hr = IDirectPlayX_EnumPlayers(dp4, NULL, count_players, &n, 0);
            sprintf(what, "%s EnumPlayers in the session", ifaces[i].name);
            CHECKHR(S_OK, hr, what);
            size = sizeof(buf);
            hr = IDirectPlayX_GetSessionDesc(dp4, buf, &size);
            CHECKHR(S_OK, hr, "GetSessionDesc of the connected object");
            if (hr == S_OK)
            {
                DPSESSIONDESC2 *got = (DPSESSIONDESC2 *)buf;
                CHECK(IsEqualGUID(&got->guidApplication, &APPRUN) && got->dwMaxPlayers == 6 && got->lpszSessionNameA && !strcmp(got->lpszSessionNameA, "connect-ex"),
                      "%s: session of the connection settings is not the open one (max %lu name '%s')", ifaces[i].name, got->dwMaxPlayers, got->lpszSessionNameA ? got->lpszSessionNameA : "(null)");
            }
            IDirectPlayX_Close(dp4);
            IDirectPlayX_Release(dp4);
        }
        IUnknown_Release(unk);
    }
    free(address);
}

/* ---- the application that is started by RunApplication ---- */

static int run_as_application(void)
{
    IDirectPlayLobby3A *lobby;
    DPLCONNECTION *conn;
    DWORD size = 0, msgflags, mlen;
    char buf[256];
    HRESULT hr;
    int result = 0;
    int i;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_DirectPlayLobby, NULL, CLSCTX_ALL, &IID_IDirectPlayLobby3A, (void **)&lobby);
    if (FAILED(hr)) return 10;

    hr = IDirectPlayLobby_GetConnectionSettings(lobby, 0, NULL, &size);
    if (hr != DPERR_BUFFERTOOSMALL) return 11;
    conn = calloc(1, size);
    hr = IDirectPlayLobby_GetConnectionSettings(lobby, 0, conn, &size);
    if (hr != S_OK) return 12;
    if (!conn->lpSessionDesc || !IsEqualGUID(&conn->lpSessionDesc->guidApplication, &APPRUN)) return 13;

    /* wait for the message of the client, answer it */
    for (i = 0; i < 100; i++)
    {
        mlen = sizeof(buf);
        hr = IDirectPlayLobby_ReceiveLobbyMessage(lobby, 0, 0, &msgflags, buf, &mlen);
        if (hr == S_OK) break;
        Sleep(100);
    }
    if (hr != S_OK) return 14;
    if (mlen != 11 || memcmp(buf, "from-client", 11)) return 15;
    hr = IDirectPlayLobby_SendLobbyMessage(lobby, 0, 0, "from-app", 9);
    if (hr != S_OK) return 16;
    return result;
}

static void test_run_application(const char *self)
{
    DPAPPLICATIONDESC desc;
    DPSESSIONDESC2 sd;
    DPLCONNECTION conn;
    char cmd[MAX_PATH + 32], path[MAX_PATH], *slash;
    DWORD appid = 0, size, msgflags, exitcode = 99;
    HANDLE event, process;
    HRESULT hr;
    char buf[64];
    int i, w;

    strcpy(path, self);
    slash = strrchr(path, '\\');
    *slash = 0;
    sprintf(cmd, "--app");

    memset(&sd, 0, sizeof(sd));
    sd.dwSize = sizeof(sd);
    sd.guidApplication = APPRUN;
    memset(&conn, 0, sizeof(conn));
    conn.dwSize = sizeof(conn);
    conn.lpSessionDesc = &sd;

    /* not registered: nothing to run */
    hr = IDirectPlayLobby_RunApplication(lobbyA, 0, &appid, &conn, NULL);
    CHECKHR(DPERR_UNKNOWNAPPLICATION, hr, "A RunApplication(unregistered)");
    hr = IDirectPlayLobby_RunApplication(lobbyW, 0, &appid, &conn, NULL);
    CHECKHR(DPERR_UNKNOWNAPPLICATION, hr, "W RunApplication(unregistered)");
    hr = IDirectPlayLobby_RunApplication(lobbyA, 1, &appid, &conn, NULL);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "RunApplication(flags 1)");
    hr = IDirectPlayLobby_RunApplication(lobbyA, 0, NULL, &conn, NULL);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "RunApplication(no id)");
    hr = IDirectPlayLobby_RunApplication(lobbyA, 0, &appid, NULL, NULL);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "RunApplication(no connection)");

    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.guidApplication = APPRUN;
    desc.lpszApplicationNameA = (char *)"SG Probe Run";
    desc.lpszFilenameA = strrchr(self, '\\') + 1;
    desc.lpszPathA = path;
    desc.lpszCommandLineA = cmd;
    desc.lpszCurrentDirectoryA = path;
    hr = IDirectPlayLobby_RegisterApplication(lobbyA, 0, &desc);
    CHECKHR(S_OK, hr, "RegisterApplication (the application)");

    for (w = 0; w < 2; w++)
    {
        const char *tag = w ? "W" : "A";
        char what[96];

        event = CreateEventA(NULL, FALSE, FALSE, NULL);
        appid = 0;
        sprintf(what, "%s RunApplication", tag);
        hr = w ? IDirectPlayLobby_RunApplication(lobbyW, 0, &appid, &conn, event)
               : IDirectPlayLobby_RunApplication(lobbyA, 0, &appid, &conn, event);
        CHECKHR(S_OK, hr, what);
        if (FAILED(hr)) { CloseHandle(event); continue; }
        CHECK(appid != 0 && appid != GetCurrentProcessId(), "%s: application id %lu", what, appid);

        /* the settings of the new application are the ones it was started with */
        size = 0;
        hr = IDirectPlayLobby_GetConnectionSettings(lobbyA, appid, NULL, &size);
        CHECKHR(DPERR_BUFFERTOOSMALL, hr, "GetConnectionSettings of the application");

        hr = l_send(w, 0, appid, "from-client", 11);
        CHECKHR(S_OK, hr, "RunApplication: client message to the application");

        /* the application answers: that signals the event we gave */
        size = sizeof(buf);
        hr = DPERR_NOMESSAGES;
        if (WaitForSingleObject(event, 20000) == WAIT_OBJECT_0)
            hr = l_receive(w, 0, appid, &msgflags, buf, &size);
        CHECKHR(S_OK, hr, "RunApplication: message of the application");
        CHECK(size == 9 && !memcmp(buf, "from-app", 9), "application message size %lu '%.9s'", size, buf);

        process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, appid);
        if (process)
        {
            if (WaitForSingleObject(process, 20000) == WAIT_OBJECT_0)
                GetExitCodeProcess(process, &exitcode);
            else
                TerminateProcess(process, 98);
            CHECK(exitcode == 0, "the application exited with %lu", exitcode);
            CloseHandle(process);
        }
        CloseHandle(event);
        (void)i;
    }

    IDirectPlayLobby_UnregisterApplication(lobbyA, 0, &APPRUN);
}

int main(int argc, char **argv)
{
    char self[MAX_PATH];
    char path[MAX_PATH];
    HMODULE spmod;

    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    GetModuleFileNameA(NULL, self, sizeof(self));

    if (argc > 1 && !strcmp(argv[1], "--app"))
        return run_as_application();

    GetFullPathNameA("sgfakesp.dll", sizeof(path), path, NULL);
    spmod = LoadLibraryA(path);
    if (!spmod) { printf("FAIL  cannot load %s\n", path); return 1; }

    if (FAILED(CoCreateInstance(&CLSID_DirectPlayLobby, NULL, CLSCTX_ALL, &IID_IDirectPlayLobby3A, (void **)&lobbyA)) ||
        FAILED(CoCreateInstance(&CLSID_DirectPlayLobby, NULL, CLSCTX_ALL, &IID_IDirectPlayLobby3, (void **)&lobbyW)))
    {
        printf("FAIL  cannot create the lobby objects\n");
        return 1;
    }

    test_applications();
    test_messages();
    test_connect();
    test_run_application(self);

    printf("%d checks, %d failures\n", checks, failures);
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
