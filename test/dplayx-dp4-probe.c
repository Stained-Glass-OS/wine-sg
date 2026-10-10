/* Native probe for the DirectPlay 4 methods of dplayx that used to be stubs
 * (patch 2985): GetPlayerFlags, GetPlayerAccount, GetPlayerAddress,
 * GetGroupFlags, Get/SetGroupOwner, Get/SetGroupConnectionSettings,
 * StartSession, SendChatMessage, CancelMessage/CancelPriority, SendComplete,
 * the system messages of DestroyPlayer/DestroyGroup, the enumeration and
 * CreateGroup state checks, SecureOpen parameter checks. It runs on the
 * service provider of test/dplayx-fakesp.c (no network): the gate registers
 * it. Table-driven: every row says what the call must return, the values
 * that come back are checked as well. */
#define COBJMACROS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <initguid.h>
#include <windows.h>
#include <ole2.h>
#include "dplayx-fakesp.h"

static int checks, failures;

/* the flags the provider is given for a request to cancel (dplaysp.h) */
#define DPCANCELSEND_PRIORITY 0x00000001
#define DPCANCELSEND_ALL      0x00000002

static DPID p_local, p_spec, p_server, p_other, group, hidden;

#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL  " __VA_ARGS__); printf("\n"); fflush(stdout); } } while (0)
#define CHECKHR(exp, got, what) CHECK((HRESULT)(got) == (HRESULT)(exp), "%s: got 0x%08lx, expected 0x%08lx", what, (unsigned long)(got), (unsigned long)(exp))

static SGSP_GETLOG sp_getlog;
static SGSP_COMPLETE sp_complete;
static SGLSP_GETLOG lsp_getlog;
static HMODULE spmod;

static IDirectPlay4 *make_dp(const GUID *provider, BOOL init)
{
    IDirectPlay4 *dp = NULL;
    IDirectPlayLobby3A *lobby;
    DPCOMPOUNDADDRESSELEMENT elem;
    DWORD size = 0;
    void *address;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_DirectPlay, NULL, CLSCTX_ALL, &IID_IDirectPlay4A, (void **)&dp);
    CHECKHR(S_OK, hr, "CoCreateInstance DirectPlay");
    if (FAILED(hr) || !init) return dp;

    hr = CoCreateInstance(&CLSID_DirectPlayLobby, NULL, CLSCTX_ALL, &IID_IDirectPlayLobby3A, (void **)&lobby);
    CHECKHR(S_OK, hr, "CoCreateInstance DirectPlayLobby");
    elem.guidDataType = provider == &SGLSP_GUID ? DPAID_LobbyProvider : DPAID_ServiceProvider;
    elem.dwDataSize = sizeof(GUID);
    elem.lpData = (void *)provider;
    hr = IDirectPlayLobby_CreateCompoundAddress(lobby, &elem, 1, NULL, &size);
    CHECKHR(DPERR_BUFFERTOOSMALL, hr, "CreateCompoundAddress size");
    address = calloc(1, size);
    hr = IDirectPlayLobby_CreateCompoundAddress(lobby, &elem, 1, address, &size);
    CHECKHR(S_OK, hr, "CreateCompoundAddress");
    hr = IDirectPlayX_InitializeConnection(dp, address, 0);
    CHECKHR(S_OK, hr, "InitializeConnection");
    free(address);
    IDirectPlayLobby_Release(lobby);
    return dp;
}

/* ---- calls that need a service provider: DPERR_UNINITIALIZED without ---- */

static DPID gid;
static DWORD gdw;
static char gbuf[256];
static DWORD gsize;

static HRESULT call_getplayerflags(IDirectPlay4 *dp)   { return IDirectPlayX_GetPlayerFlags(dp, 0, &gdw); }
static HRESULT call_getplayeraccount(IDirectPlay4 *dp) { gsize = sizeof(gbuf); return IDirectPlayX_GetPlayerAccount(dp, 0, 0, gbuf, &gsize); }
static HRESULT call_getplayeraddress(IDirectPlay4 *dp) { gsize = sizeof(gbuf); return IDirectPlayX_GetPlayerAddress(dp, 0, gbuf, &gsize); }
static HRESULT call_getgroupflags(IDirectPlay4 *dp)    { return IDirectPlayX_GetGroupFlags(dp, 0, &gdw); }
static HRESULT call_getgroupowner(IDirectPlay4 *dp)    { return IDirectPlayX_GetGroupOwner(dp, 0, &gid); }
static HRESULT call_setgroupowner(IDirectPlay4 *dp)    { return IDirectPlayX_SetGroupOwner(dp, 0, 0); }
static HRESULT call_getgroupconn(IDirectPlay4 *dp)     { gsize = sizeof(gbuf); return IDirectPlayX_GetGroupConnectionSettings(dp, 0, 0, gbuf, &gsize); }
static HRESULT call_setgroupconn(IDirectPlay4 *dp)     { DPLCONNECTION c = { sizeof(c) }; return IDirectPlayX_SetGroupConnectionSettings(dp, 0, 0, &c); }
static HRESULT call_startsession(IDirectPlay4 *dp)     { return IDirectPlayX_StartSession(dp, 0, 0); }
static HRESULT call_chat(IDirectPlay4 *dp)             { DPCHAT c = { sizeof(c), 0 }; c.lpszMessageA = (char *)"x"; return IDirectPlayX_SendChatMessage(dp, 1, 0, 0, &c); }
static HRESULT call_cancelmsg(IDirectPlay4 *dp)        { return IDirectPlayX_CancelMessage(dp, 1, 0); }
static HRESULT call_cancelprio(IDirectPlay4 *dp)       { return IDirectPlayX_CancelPriority(dp, 0, 10, 0); }
static HRESULT call_enumplayers(IDirectPlay4 *dp);
static BOOL CALLBACK enum_cb(DPID id, DWORD type, LPCDPNAME name, DWORD flags, void *ctx) { (*(int *)ctx)++; return TRUE; }
static HRESULT call_enumplayers(IDirectPlay4 *dp)      { int n = 0; return IDirectPlayX_EnumPlayers(dp, NULL, enum_cb, &n, 0); }
static HRESULT call_enumgroups(IDirectPlay4 *dp)       { int n = 0; return IDirectPlayX_EnumGroups(dp, NULL, enum_cb, &n, 0); }
static HRESULT call_enumgroupplayers(IDirectPlay4 *dp) { int n = 0; return IDirectPlayX_EnumGroupPlayers(dp, 0, NULL, enum_cb, &n, 0); }
static HRESULT call_destroyplayer(IDirectPlay4 *dp)    { return IDirectPlayX_DestroyPlayer(dp, 1); }
static HRESULT call_destroygroup(IDirectPlay4 *dp)     { return IDirectPlayX_DestroyGroup(dp, 1); }
static HRESULT call_creategroup(IDirectPlay4 *dp)      { DPID g; return IDirectPlayX_CreateGroup(dp, &g, NULL, NULL, 0, 0); }

static const struct { const char *name; HRESULT (*call)(IDirectPlay4 *); } uninit_calls[] =
{
    { "GetPlayerFlags", call_getplayerflags }, { "GetPlayerAccount", call_getplayeraccount },
    { "GetPlayerAddress", call_getplayeraddress }, { "GetGroupFlags", call_getgroupflags },
    { "GetGroupOwner", call_getgroupowner }, { "SetGroupOwner", call_setgroupowner },
    { "GetGroupConnectionSettings", call_getgroupconn }, { "SetGroupConnectionSettings", call_setgroupconn },
    { "StartSession", call_startsession }, { "SendChatMessage", call_chat },
    { "CancelMessage", call_cancelmsg }, { "CancelPriority", call_cancelprio },
    { "EnumPlayers", call_enumplayers }, { "EnumGroups", call_enumgroups },
    { "EnumGroupPlayers", call_enumgroupplayers }, { "DestroyPlayer", call_destroyplayer },
    { "DestroyGroup", call_destroygroup }, { "CreateGroup", call_creategroup },
};

/* ---- the message queue of a player ---- */

static BYTE rbuf[2048];

static HRESULT receive(IDirectPlay4 *dp, DPID *from, DPID *to, DWORD *size)
{
    *from = 0; *to = 0; *size = sizeof(rbuf);
    return IDirectPlayX_Receive(dp, from, to, DPRECEIVE_ALL, rbuf, size);
}

static int count_messages(IDirectPlay4 *dp, DWORD type, DPID *last_from)
{
    int n = 0;
    DPID from, to;
    DWORD size;

    while (receive(dp, &from, &to, &size) == DP_OK)
        if (from == DPID_SYSMSG && size >= sizeof(DWORD) && *(DWORD *)rbuf == type)
        {
            n++;
            if (last_from) *last_from = from;
        }
    return n;
}

static void drain(IDirectPlay4 *dp)
{
    DPID from, to;
    DWORD size;
    while (receive(dp, &from, &to, &size) == DP_OK) ;
}

/* ---- the session ---- */

static void test_uninitialized(void)
{
    IDirectPlay4 *dp = make_dp(NULL, FALSE);
    unsigned i;

    if (!dp) return;
    for (i = 0; i < ARRAYSIZE(uninit_calls); i++)
    {
        HRESULT hr = uninit_calls[i].call(dp);
        CHECKHR(DPERR_UNINITIALIZED, hr, uninit_calls[i].name);
    }
    IDirectPlayX_Release(dp);
}

static void test_before_session(IDirectPlay4 *dp)
{
    HRESULT hr;
    int n = 0;
    DPID g;

    /* no session yet: no players, no groups, nothing to enumerate */
    gdw = 0x1234;
    hr = IDirectPlayX_GetPlayerFlags(dp, 0, &gdw);
    CHECKHR(DPERR_INVALIDPLAYER, hr, "GetPlayerFlags(0) without session");
    hr = IDirectPlayX_GetPlayerFlags(dp, 1, &gdw);
    CHECKHR(DPERR_INVALIDPLAYER, hr, "GetPlayerFlags(1) without session");

    gsize = 1024;
    hr = IDirectPlayX_GetPlayerAccount(dp, 0, 0, gbuf, &gsize);
    CHECKHR(DPERR_NOSESSIONS, hr, "GetPlayerAccount without session");
    CHECK(gsize == 1024, "GetPlayerAccount touched the size: %lu", gsize);

    gsize = 1024;
    hr = IDirectPlayX_GetPlayerAddress(dp, 0, gbuf, &gsize);
    CHECKHR(DPERR_UNSUPPORTED, hr, "GetPlayerAddress(0) without session");
    hr = IDirectPlayX_GetPlayerAddress(dp, 1, gbuf, &gsize);
    CHECKHR(DPERR_INVALIDPLAYER, hr, "GetPlayerAddress(1) without session");
    CHECK(gsize == 1024, "GetPlayerAddress touched the size: %lu", gsize);

    hr = IDirectPlayX_GetGroupFlags(dp, 5, &gdw);
    CHECKHR(DPERR_INVALIDGROUP, hr, "GetGroupFlags without session");
    hr = IDirectPlayX_GetGroupOwner(dp, 5, &gid);
    CHECKHR(DPERR_INVALIDGROUP, hr, "GetGroupOwner without session");

    hr = IDirectPlayX_CreateGroup(dp, &g, NULL, NULL, 0, 0);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "CreateGroup without session");

    hr = IDirectPlayX_EnumPlayers(dp, NULL, enum_cb, &n, 0);
    CHECKHR(DPERR_NOSESSIONS, hr, "EnumPlayers without session");
    hr = IDirectPlayX_EnumGroups(dp, NULL, enum_cb, &n, 0);
    CHECKHR(DPERR_NOSESSIONS, hr, "EnumGroups without session");
    hr = IDirectPlayX_EnumPlayers(dp, (GUID *)&SGSP_GUID, enum_cb, &n, DPENUMPLAYERS_SESSION);
    CHECKHR(DPERR_NOSESSIONS, hr, "EnumPlayers(SESSION, unknown guid) without session");
    hr = IDirectPlayX_EnumGroupPlayers(dp, 3, NULL, enum_cb, &n, 0);
    CHECKHR(DPERR_NOSESSIONS, hr, "EnumGroupPlayers without session");
    CHECK(n == 0, "callback called %d times without session", n);

    hr = IDirectPlayX_EnumPlayers(dp, NULL, NULL, &n, 0);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "EnumPlayers(NULL callback)");

    /* cancelling without the provider having anything: its answer is passed on */
    hr = IDirectPlayX_CancelMessage(dp, 7, 1);
    CHECKHR(DPERR_INVALIDFLAGS, hr, "CancelMessage(flags 1)");
    hr = IDirectPlayX_CancelPriority(dp, 0, 10, 1);
    CHECKHR(DPERR_INVALIDFLAGS, hr, "CancelPriority(flags 1)");
}

static void test_secure_open_params(IDirectPlay4 *dp)
{
    DPSESSIONDESC2 sd;
    DPSECURITYDESC sec;
    DPCREDENTIALS cred;
    HRESULT hr;

    memset(&sd, 0, sizeof(sd));
    sd.dwSize = sizeof(sd);
    sd.dwMaxPlayers = 8;
    memset(&sec, 0, sizeof(sec));
    memset(&cred, 0, sizeof(cred));

    hr = IDirectPlayX_SecureOpen(dp, NULL, DPOPEN_CREATE, NULL, NULL);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "SecureOpen(NULL session)");
    sec.dwSize = 3;
    hr = IDirectPlayX_SecureOpen(dp, &sd, DPOPEN_CREATE, &sec, NULL);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "SecureOpen(bad security size)");
    sec.dwSize = sizeof(sec); sec.dwFlags = 1;
    hr = IDirectPlayX_SecureOpen(dp, &sd, DPOPEN_CREATE, &sec, NULL);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "SecureOpen(security flags)");
    cred.dwSize = 5;
    hr = IDirectPlayX_SecureOpen(dp, &sd, DPOPEN_CREATE, NULL, &cred);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "SecureOpen(bad credentials size)");
    cred.dwSize = sizeof(cred); cred.dwFlags = 2;
    hr = IDirectPlayX_SecureOpen(dp, &sd, DPOPEN_CREATE, NULL, &cred);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "SecureOpen(credentials flags)");
}

static void test_session(void)
{
    IDirectPlay4 *dp = make_dp(&SGSP_GUID, TRUE);
    IDirectPlay4 *dpw = NULL;
    DPSESSIONDESC2 sd;
    DPID from, to;
    DPNAME name;
    SGSPLog *log = sp_getlog();
    DWORD flags, size;
    HRESULT hr;
    int n;
    unsigned i;

    if (!dp) return;

    test_before_session(dp);
    test_secure_open_params(dp);

    memset(&sd, 0, sizeof(sd));
    sd.dwSize = sizeof(sd);
    sd.dwMaxPlayers = 8;
    hr = IDirectPlayX_Open(dp, &sd, DPOPEN_CREATE);
    CHECKHR(S_OK, hr, "Open(CREATE)");
    if (FAILED(hr)) { IDirectPlayX_Release(dp); return; }

    /* players: a normal one, a spectator, the server player, and a group */
    hr = IDirectPlayX_CreatePlayer(dp, &p_local, NULL, NULL, NULL, 0, 0);
    CHECKHR(S_OK, hr, "CreatePlayer local");
    hr = IDirectPlayX_CreatePlayer(dp, &p_spec, NULL, NULL, NULL, 0, DPPLAYER_SPECTATOR);
    CHECKHR(S_OK, hr, "CreatePlayer spectator");
    hr = IDirectPlayX_CreatePlayer(dp, &p_server, NULL, NULL, NULL, 0, DPPLAYER_SERVERPLAYER);
    CHECKHR(S_OK, hr, "CreatePlayer server");
    memset(&name, 0, sizeof(name));
    name.dwSize = sizeof(name);
    name.lpszShortNameA = (char *)"destroy-me";
    hr = IDirectPlayX_CreatePlayer(dp, &p_other, &name, NULL, NULL, 0, 0);
    CHECKHR(S_OK, hr, "CreatePlayer other");
    hr = IDirectPlayX_CreateGroup(dp, &group, NULL, NULL, 0, 0);
    CHECKHR(S_OK, hr, "CreateGroup");
    hr = IDirectPlayX_CreateGroup(dp, &hidden, NULL, NULL, 0, DPGROUP_HIDDEN);
    CHECKHR(S_OK, hr, "CreateGroup hidden");

    /* GetPlayerFlags */
    {
        static const struct { const char *name; DPID *id; DWORD flags; } rows[] =
        {
            { "local", &p_local, DPPLAYER_LOCAL },
            { "spectator", &p_spec, DPPLAYER_SPECTATOR | DPPLAYER_LOCAL },
            { "server", &p_server, DPPLAYER_SERVERPLAYER | DPPLAYER_LOCAL },
        };
        for (i = 0; i < ARRAYSIZE(rows); i++)
        {
            char what[64];
            sprintf(what, "GetPlayerFlags %s", rows[i].name);
            flags = 0xdeadbeef;
            hr = IDirectPlayX_GetPlayerFlags(dp, *rows[i].id, &flags);
            CHECKHR(S_OK, hr, what);
            CHECK(flags == rows[i].flags, "%s: flags 0x%08lx expected 0x%08lx", what, flags, rows[i].flags);
        }
        hr = IDirectPlayX_GetPlayerFlags(dp, 0, &flags);
        CHECKHR(DPERR_INVALIDPLAYER, hr, "GetPlayerFlags(0)");
        hr = IDirectPlayX_GetPlayerFlags(dp, 0x7777, &flags);
        CHECKHR(DPERR_INVALIDPLAYER, hr, "GetPlayerFlags(unknown)");
        hr = IDirectPlayX_GetPlayerFlags(dp, group, &flags);
        CHECKHR(DPERR_INVALIDPLAYER, hr, "GetPlayerFlags(group)");
        hr = IDirectPlayX_GetPlayerFlags(dp, p_local, NULL);
        CHECKHR(DPERR_INVALIDPARAMS, hr, "GetPlayerFlags(NULL)");
    }

    /* GetGroupFlags */
    flags = 0xdeadbeef;
    hr = IDirectPlayX_GetGroupFlags(dp, group, &flags);
    CHECKHR(S_OK, hr, "GetGroupFlags");
    CHECK((flags & DPGROUP_HIDDEN) == 0, "GetGroupFlags visible group has hidden flag: 0x%08lx", flags);
    flags = 0;
    hr = IDirectPlayX_GetGroupFlags(dp, hidden, &flags);
    CHECKHR(S_OK, hr, "GetGroupFlags hidden");
    CHECK((flags & DPGROUP_HIDDEN) != 0, "GetGroupFlags hidden group: 0x%08lx", flags);
    CHECK((flags & ~(DPGROUP_LOCAL | DPGROUP_HIDDEN | DPGROUP_STAGINGAREA)) == 0, "GetGroupFlags other bits: 0x%08lx", flags);
    hr = IDirectPlayX_GetGroupFlags(dp, 0, &flags);
    CHECKHR(DPERR_INVALIDGROUP, hr, "GetGroupFlags(0)");
    hr = IDirectPlayX_GetGroupFlags(dp, p_local, &flags);
    CHECKHR(DPERR_INVALIDGROUP, hr, "GetGroupFlags(player)");
    hr = IDirectPlayX_GetGroupFlags(dp, group, NULL);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "GetGroupFlags(NULL)");

    /* GetPlayerAccount: the session is not secure */
    gsize = 1024;
    hr = IDirectPlayX_GetPlayerAccount(dp, p_local, 0, gbuf, &gsize);
    CHECKHR(DPERR_UNSUPPORTED, hr, "GetPlayerAccount non secure");
    CHECK(gsize == 1024, "GetPlayerAccount non secure touched the size: %lu", gsize);

    /* GetPlayerAddress: the provider answers */
    memset(log, 0, sizeof(*log));
    gsize = 1024;
    hr = IDirectPlayX_GetPlayerAddress(dp, 0, gbuf, &gsize);
    CHECKHR(DPERR_UNSUPPORTED, hr, "GetPlayerAddress(0)");
    hr = IDirectPlayX_GetPlayerAddress(dp, group, gbuf, &gsize);
    CHECKHR(DPERR_UNSUPPORTED, hr, "GetPlayerAddress(group)");
    hr = IDirectPlayX_GetPlayerAddress(dp, 0x7777, gbuf, &gsize);
    CHECKHR(DPERR_INVALIDPLAYER, hr, "GetPlayerAddress(unknown)");
    CHECK(gsize == 1024 && log->getaddress_calls == 0, "GetPlayerAddress for non players reached the provider (%lu calls)", log->getaddress_calls);
    hr = IDirectPlayX_GetPlayerAddress(dp, p_local, gbuf, NULL);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "GetPlayerAddress(NULL size)");
    gsize = 1024;
    hr = IDirectPlayX_GetPlayerAddress(dp, p_local, NULL, &gsize);
    CHECKHR(DPERR_BUFFERTOOSMALL, hr, "GetPlayerAddress(NULL buffer)");
    CHECK(gsize == 24, "GetPlayerAddress size %lu expected 24", gsize);
    CHECK(log->getaddress_calls == 1 && log->getaddress_player == p_local && (log->getaddress_flags & 0x8) != 0,
          "provider saw %lu calls, player %lu flags 0x%lx", log->getaddress_calls, log->getaddress_player, log->getaddress_flags);
    memset(gbuf, 0, sizeof(gbuf));
    gsize = 1024;
    hr = IDirectPlayX_GetPlayerAddress(dp, p_local, gbuf, &gsize);
    CHECKHR(S_OK, hr, "GetPlayerAddress");
    CHECK(gsize == 24, "GetPlayerAddress size %lu expected 24", gsize);
    CHECK(IsEqualGUID(&((DPADDRESS *)gbuf)->guidDataType, &DPAID_TotalSize) && *(DWORD *)(gbuf + sizeof(DPADDRESS)) == 24,
          "GetPlayerAddress data not the provider's");

    /* the group staging area features need a lobby session */
    hr = IDirectPlayX_GetGroupOwner(dp, group, &gid);
    CHECKHR(DPERR_UNSUPPORTED, hr, "GetGroupOwner");
    hr = IDirectPlayX_GetGroupOwner(dp, 0, &gid);
    CHECKHR(DPERR_INVALIDGROUP, hr, "GetGroupOwner(0)");
    hr = IDirectPlayX_GetGroupOwner(dp, group, NULL);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "GetGroupOwner(NULL)");
    hr = IDirectPlayX_SetGroupOwner(dp, group, p_local);
    CHECKHR(DPERR_UNSUPPORTED, hr, "SetGroupOwner");
    hr = IDirectPlayX_SetGroupOwner(dp, 0x7777, p_local);
    CHECKHR(DPERR_INVALIDGROUP, hr, "SetGroupOwner(unknown group)");
    {
        DPLCONNECTION conn;
        memset(&conn, 0, sizeof(conn));
        conn.dwSize = sizeof(conn);
        gsize = 1024;
        hr = IDirectPlayX_GetGroupConnectionSettings(dp, 0, group, gbuf, &gsize);
        CHECKHR(DPERR_UNSUPPORTED, hr, "GetGroupConnectionSettings");
        hr = IDirectPlayX_GetGroupConnectionSettings(dp, 1, group, gbuf, &gsize);
        CHECKHR(DPERR_INVALIDFLAGS, hr, "GetGroupConnectionSettings(flags 1)");
        hr = IDirectPlayX_GetGroupConnectionSettings(dp, 0, 0x7777, gbuf, &gsize);
        CHECKHR(DPERR_INVALIDGROUP, hr, "GetGroupConnectionSettings(unknown group)");
        hr = IDirectPlayX_SetGroupConnectionSettings(dp, 0, group, &conn);
        CHECKHR(DPERR_UNSUPPORTED, hr, "SetGroupConnectionSettings");
        hr = IDirectPlayX_SetGroupConnectionSettings(dp, 0, group, NULL);
        CHECKHR(DPERR_INVALIDPARAMS, hr, "SetGroupConnectionSettings(NULL)");
        hr = IDirectPlayX_SetGroupConnectionSettings(dp, 2, group, &conn);
        CHECKHR(DPERR_INVALIDFLAGS, hr, "SetGroupConnectionSettings(flags 2)");
    }
    hr = IDirectPlayX_StartSession(dp, 0, group);
    CHECKHR(DPERR_UNSUPPORTED, hr, "StartSession");
    hr = IDirectPlayX_StartSession(dp, 1, group);
    CHECKHR(DPERR_INVALIDFLAGS, hr, "StartSession(flags 1)");
    hr = IDirectPlayX_StartSession(dp, 0, 0x7777);
    CHECKHR(DPERR_INVALIDGROUP, hr, "StartSession(unknown group)");

    /* chat: delivered to the local players it is for, as DPSYS_CHAT */
    drain(dp);
    {
        DPCHAT chat;
        DPMSG_CHAT *msg;
        memset(&chat, 0, sizeof(chat));
        chat.dwSize = sizeof(chat);
        chat.lpszMessageA = (char *)"hello chat";
        hr = IDirectPlayX_SendChatMessage(dp, p_local, p_spec, DPSEND_GUARANTEED, &chat);
        CHECKHR(S_OK, hr, "SendChatMessage");
        hr = receive(dp, &from, &to, &size);
        CHECKHR(S_OK, hr, "Receive chat");
        msg = (DPMSG_CHAT *)rbuf;
        CHECK(from == DPID_SYSMSG && to == p_spec, "chat from 0x%lx to 0x%lx", from, to);
        CHECK(size >= sizeof(DPMSG_CHAT) && msg->dwType == DPSYS_CHAT, "chat type 0x%lx size %lu", msg->dwType, size);
        if (size >= sizeof(DPMSG_CHAT))
        {
            CHECK(msg->idFromPlayer == p_local && msg->idToPlayer == p_spec && msg->idToGroup == 0 && msg->dwFlags == DPSEND_GUARANTEED,
                  "chat ids from 0x%lx to 0x%lx group 0x%lx flags 0x%lx", msg->idFromPlayer, msg->idToPlayer, msg->idToGroup, msg->dwFlags);
            CHECK(msg->lpChat && msg->lpChat->dwSize == sizeof(DPCHAT) && !strcmp(msg->lpChat->lpszMessageA, "hello chat"),
                  "chat text '%s'", msg->lpChat && msg->lpChat->lpszMessageA ? msg->lpChat->lpszMessageA : "(null)");
        }
        hr = receive(dp, &from, &to, &size);
        CHECKHR(DPERR_NOMESSAGES, hr, "Receive after chat");

        /* a chat to a group: its members (none) get nothing, the call works */
        hr = IDirectPlayX_SendChatMessage(dp, p_local, group, 0, &chat);
        CHECKHR(S_OK, hr, "SendChatMessage to a group");
        hr = IDirectPlayX_SendChatMessage(dp, p_local, 0x7777, 0, &chat);
        CHECKHR(DPERR_INVALIDPLAYER, hr, "SendChatMessage to unknown");
        hr = IDirectPlayX_SendChatMessage(dp, 0x7777, p_spec, 0, &chat);
        CHECKHR(DPERR_INVALIDPLAYER, hr, "SendChatMessage from unknown");
        hr = IDirectPlayX_SendChatMessage(dp, p_local, p_spec, 4, &chat);
        CHECKHR(DPERR_INVALIDFLAGS, hr, "SendChatMessage(flags 4)");
        hr = IDirectPlayX_SendChatMessage(dp, p_local, p_spec, 0, NULL);
        CHECKHR(DPERR_INVALIDPARAMS, hr, "SendChatMessage(NULL)");
        chat.dwSize = 5;
        hr = IDirectPlayX_SendChatMessage(dp, p_local, p_spec, 0, &chat);
        CHECKHR(DPERR_INVALIDPARAMS, hr, "SendChatMessage(bad size)");
    }

    /* cancelling: the provider is asked */
    memset(log, 0, sizeof(*log));
    hr = IDirectPlayX_CancelMessage(dp, 0x5001, 0);
    CHECKHR(S_OK, hr, "CancelMessage");
    CHECK(log->cancel_calls == 1 && log->cancel_count == 1 && log->cancel_first == 0x5001 && !(log->cancel_flags & (DPCANCELSEND_ALL | DPCANCELSEND_PRIORITY)),
          "CancelMessage: calls %lu count %lu first 0x%lx flags 0x%lx", log->cancel_calls, log->cancel_count, (unsigned long)log->cancel_first, log->cancel_flags);
    hr = IDirectPlayX_CancelMessage(dp, 0, 0);
    CHECKHR(S_OK, hr, "CancelMessage(all)");
    CHECK(log->cancel_calls == 2 && log->cancel_count == 0 && (log->cancel_flags & DPCANCELSEND_ALL),
          "CancelMessage(all): count %lu flags 0x%lx", log->cancel_count, log->cancel_flags);
    hr = IDirectPlayX_CancelPriority(dp, 3, 9, 0);
    CHECKHR(S_OK, hr, "CancelPriority");
    CHECK(log->cancel_calls == 3 && log->cancel_count == 0 && (log->cancel_flags & DPCANCELSEND_PRIORITY) && log->cancel_minprio == 3 && log->cancel_maxprio == 9,
          "CancelPriority: flags 0x%lx min %lu max %lu", log->cancel_flags, log->cancel_minprio, log->cancel_maxprio);

    /* an asynchronous send the provider keeps: SendComplete tells the sender */
    drain(dp);
    {
        DWORD msgid = 0;
        char data[8] = "async";
        DPMSG_SENDCOMPLETE *msg;

        hr = IDirectPlayX_SendEx(dp, p_local, p_other, DPSEND_ASYNC, data, sizeof(data), 4, 100, (void *)0x1234, &msgid);
        CHECKHR(DPERR_PENDING, hr, "SendEx(ASYNC)");
        CHECK(msgid == log->sendex_msgid && msgid != 0, "SendEx msgid 0x%lx, provider 0x%lx", msgid, log->sendex_msgid);
        n = count_messages(dp, DPSYS_SENDCOMPLETE, NULL);
        CHECK(n == 0, "SENDCOMPLETE before the provider is done: %d", n);
        drain(dp);
        sp_complete((void *)(ULONG_PTR)msgid, S_OK);
        hr = IDirectPlayX_Receive(dp, &from, &to, DPRECEIVE_ALL | DPRECEIVE_TOPLAYER, rbuf, (size = sizeof(rbuf), &size)) ;
        to = p_local; from = 0;
        size = sizeof(rbuf);
        hr = IDirectPlayX_Receive(dp, &from, &to, DPRECEIVE_TOPLAYER, rbuf, &size);
        CHECKHR(S_OK, hr, "Receive SENDCOMPLETE");
        msg = (DPMSG_SENDCOMPLETE *)rbuf;
        if (hr == S_OK)
        {
            CHECK(from == DPID_SYSMSG && msg->dwType == DPSYS_SENDCOMPLETE, "message from 0x%lx type 0x%lx", from, msg->dwType);
            CHECK(msg->idFrom == p_local && msg->idTo == p_other && msg->dwFlags == DPSEND_ASYNC && msg->dwPriority == 4 && msg->dwTimeout == 100,
                  "SENDCOMPLETE from 0x%lx to 0x%lx flags 0x%lx prio %lu timeout %lu", msg->idFrom, msg->idTo, msg->dwFlags, msg->dwPriority, msg->dwTimeout);
            CHECK(msg->lpvContext == (void *)0x1234 && msg->dwMsgID == msgid && msg->hr == S_OK,
                  "SENDCOMPLETE context %p msgid 0x%lx hr 0x%lx", msg->lpvContext, msg->dwMsgID, (unsigned long)msg->hr);
        }
        /* a second completion of the same send, or one of a send nobody waits for, is ignored */
        sp_complete((void *)(ULONG_PTR)msgid, S_OK);
        sp_complete((void *)0x99999, S_OK);
        drain(dp);
    }

    /* destroying players and groups tells the other local players */
    drain(dp);
    hr = IDirectPlayX_GetSessionDesc(dp, gbuf, (gsize = sizeof(gbuf), &gsize));
    CHECKHR(S_OK, hr, "GetSessionDesc");
    n = (int)((DPSESSIONDESC2 *)gbuf)->dwCurrentPlayers;
    CHECK(n == 4, "current players %d expected 4", n);

    memset(log, 0, sizeof(*log));
    hr = IDirectPlayX_DestroyPlayer(dp, p_other);
    CHECKHR(S_OK, hr, "DestroyPlayer");
    CHECK(log->deleteplayer_calls == 1 && log->deleteplayer_id == p_other && (log->deleteplayer_flags & 0x8),
          "provider DeletePlayer: calls %lu id 0x%lx flags 0x%lx", log->deleteplayer_calls, log->deleteplayer_id, log->deleteplayer_flags);
    hr = IDirectPlayX_GetSessionDesc(dp, gbuf, (gsize = sizeof(gbuf), &gsize));
    n = (int)((DPSESSIONDESC2 *)gbuf)->dwCurrentPlayers;
    CHECK(n == 3, "current players after DestroyPlayer %d expected 3", n);
    hr = IDirectPlayX_GetPlayerFlags(dp, p_other, &flags);
    CHECKHR(DPERR_INVALIDPLAYER, hr, "GetPlayerFlags(destroyed)");
    {
        DPMSG_DESTROYPLAYERORGROUP *msg;
        BOOL found = FALSE;
        int seen = 0;

        while (receive(dp, &from, &to, &size) == DP_OK)
        {
            msg = (DPMSG_DESTROYPLAYERORGROUP *)rbuf;
            if (from == DPID_SYSMSG && msg->dwType == DPSYS_DESTROYPLAYERORGROUP && msg->dpId == p_other)
            {
                seen++;
                if (to == p_local)
                {
                    found = TRUE;
                    CHECK(msg->dwPlayerType == DPPLAYERTYPE_PLAYER && msg->dwFlags == DPPLAYER_LOCAL && msg->dpIdParent == 0,
                          "DESTROYPLAYER type %lu flags 0x%lx parent 0x%lx", msg->dwPlayerType, msg->dwFlags, msg->dpIdParent);
                    CHECK(msg->dpnName.lpszShortNameA && !strcmp(msg->dpnName.lpszShortNameA, "destroy-me"),
                          "DESTROYPLAYER name '%s'", msg->dpnName.lpszShortNameA ? msg->dpnName.lpszShortNameA : "(null)");
                }
            }
        }
        CHECK(found, "no DPSYS_DESTROYPLAYERORGROUP for the destroyed player (%d seen)", seen);
        /* the destroyed player does not get its own message */
    }

    memset(log, 0, sizeof(*log));
    hr = IDirectPlayX_DestroyGroup(dp, hidden);
    CHECKHR(S_OK, hr, "DestroyGroup");
    CHECK(log->deletegroup_calls == 1 && log->deletegroup_id == hidden && (log->deletegroup_flags & 0x400),
          "provider DeleteGroup: calls %lu id 0x%lx flags 0x%lx", log->deletegroup_calls, log->deletegroup_id, log->deletegroup_flags);
    {
        DPMSG_DESTROYPLAYERORGROUP *msg;
        BOOL found = FALSE;
        while (receive(dp, &from, &to, &size) == DP_OK)
        {
            msg = (DPMSG_DESTROYPLAYERORGROUP *)rbuf;
            if (from == DPID_SYSMSG && msg->dwType == DPSYS_DESTROYPLAYERORGROUP && msg->dpId == hidden)
            {
                found = TRUE;
                CHECK(msg->dwPlayerType == DPPLAYERTYPE_GROUP && (msg->dwFlags & DPGROUP_HIDDEN), "DESTROYGROUP type %lu flags 0x%lx", msg->dwPlayerType, msg->dwFlags);
            }
        }
        CHECK(found, "no DPSYS_DESTROYPLAYERORGROUP for the destroyed group");
    }
    hr = IDirectPlayX_GetGroupFlags(dp, hidden, &flags);
    CHECKHR(DPERR_INVALIDGROUP, hr, "GetGroupFlags(destroyed)");
    hr = IDirectPlayX_DestroyGroup(dp, hidden);
    CHECKHR(DPERR_INVALIDPLAYER, hr, "DestroyGroup(destroyed)");

    /* the enumerations work on the open session */
    n = 0;
    hr = IDirectPlayX_EnumPlayers(dp, NULL, enum_cb, &n, 0);
    CHECKHR(S_OK, hr, "EnumPlayers");
    CHECK(n == 3, "EnumPlayers listed %d players expected 3", n);
    n = 0;
    hr = IDirectPlayX_EnumPlayers(dp, NULL, enum_cb, &n, DPENUMPLAYERS_SESSION);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "EnumPlayers(SESSION, NULL guid)");
    CHECK(n == 0, "callback called with invalid parameters");
    n = 0;
    hr = IDirectPlayX_EnumGroups(dp, NULL, enum_cb, &n, 0);
    CHECKHR(S_OK, hr, "EnumGroups");
    CHECK(n == 1, "EnumGroups listed %d groups expected 1", n);
    hr = IDirectPlayX_EnumGroups(dp, NULL, NULL, &n, 0);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "EnumGroups(NULL callback)");
    hr = IDirectPlayX_EnumPlayers(dp, &(GUID){ 1 }, enum_cb, &n, DPENUMPLAYERS_SESSION);
    CHECKHR(DPERR_GENERIC, hr, "EnumPlayers(SESSION, other session)");
    {
        GUID instance;
        size = sizeof(gbuf);
        IDirectPlayX_GetSessionDesc(dp, gbuf, &size);
        instance = ((DPSESSIONDESC2 *)gbuf)->guidInstance;
        n = 0;
        hr = IDirectPlayX_EnumPlayers(dp, &instance, enum_cb, &n, DPENUMPLAYERS_SESSION);
        CHECKHR(S_OK, hr, "EnumPlayers(SESSION, the open session)");
        CHECK(n == 3, "EnumPlayers(SESSION, the open session) listed %d players expected 3", n);
        n = 0;
        hr = IDirectPlayX_EnumGroups(dp, &instance, enum_cb, &n, DPENUMGROUPS_SESSION);
        CHECKHR(S_OK, hr, "EnumGroups(SESSION, the open session)");
        CHECK(n == 1, "EnumGroups(SESSION, the open session) listed %d groups expected 1", n);
    }

    /* after Close the session is gone again: no session to enumerate */
    hr = IDirectPlayX_Close(dp);
    CHECKHR(S_OK, hr, "Close");
    n = 0;
    hr = IDirectPlayX_EnumPlayers(dp, NULL, enum_cb, &n, 0);
    CHECKHR(DPERR_NOSESSIONS, hr, "EnumPlayers after Close");
    hr = IDirectPlayX_CreateGroup(dp, &group, NULL, NULL, 0, 0);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "CreateGroup after Close");

    /* a secure session: accounts exist but nobody has one */
    sd.dwFlags = DPSESSION_SECURESERVER;
    hr = IDirectPlayX_SecureOpen(dp, &sd, DPOPEN_CREATE, NULL, NULL);
    CHECKHR(S_OK, hr, "SecureOpen(CREATE, secure)");
    hr = IDirectPlayX_CreatePlayer(dp, &p_local, NULL, NULL, NULL, 0, 0);
    CHECKHR(S_OK, hr, "CreatePlayer in the secure session");
    {
        static const struct { const char *name; DWORD flags; DPID *id; BOOL nodata; HRESULT hr; DWORD size_after; } rows[] =
        {
            { "invalid player", 0, NULL, FALSE, DPERR_INVALIDPLAYER, 1024 },
            { "flags -1", (DWORD)-1, &p_local, FALSE, DPERR_INVALIDFLAGS, 1024 },
            { "flags 1", 1, &p_local, FALSE, DPERR_INVALIDFLAGS, 1024 },
            { "NULL buffer", 0, &p_local, TRUE, DPERR_INVALIDPLAYER, 0 },
            { "no account", 0, &p_local, FALSE, DPERR_INVALIDPLAYER, 1024 },
        };
        for (i = 0; i < ARRAYSIZE(rows); i++)
        {
            char what[64];
            sprintf(what, "GetPlayerAccount secure, %s", rows[i].name);
            gsize = 1024;
            hr = IDirectPlayX_GetPlayerAccount(dp, rows[i].id ? *rows[i].id : 0, rows[i].flags, rows[i].nodata ? NULL : gbuf, &gsize);
            CHECKHR(rows[i].hr, hr, what);
            CHECK(gsize == rows[i].size_after, "%s: size %lu expected %lu", what, gsize, rows[i].size_after);
        }
    }
    hr = IDirectPlayX_Close(dp);
    CHECKHR(S_OK, hr, "Close secure session");
    IDirectPlayX_Release(dp);
    (void)dpw;
}

/* ---- joining a session: the host hands out the ids ---- */

struct join_ctx { GUID instance; int found; };

static BOOL CALLBACK enum_join(const DPSESSIONDESC2 *sd, DWORD *timeout, DWORD flags, void *ctx)
{
    struct join_ctx *c = ctx;

    if (flags & DPESC_TIMEDOUT) return FALSE;
    c->instance = sd->guidInstance;
    c->found++;
    return TRUE;
}

static void test_join(void)
{
    IDirectPlay4 *dp = make_dp(&SGSP_GUID, TRUE);
    SGSPLog *log = sp_getlog();
    struct join_ctx ctx;
    DPSESSIONDESC2 sd;
    DPID p, g1, g2, from, to;
    char buf[512];
    DWORD size;
    HRESULT hr;

    if (!dp) return;
    memset(log, 0, sizeof(*log));
    memset(&ctx, 0, sizeof(ctx));
    memset(&sd, 0, sizeof(sd));
    sd.dwSize = sizeof(sd);
    sd.guidApplication = SGSESSION_APPLICATION;
    hr = IDirectPlayX_EnumSessions(dp, &sd, 100, enum_join, &ctx, 0);
    CHECKHR(S_OK, hr, "EnumSessions");
    CHECK(ctx.found == 1 && IsEqualGUID(&ctx.instance, &SGSESSION_INSTANCE), "EnumSessions found %d sessions", ctx.found);

    sd.guidInstance = ctx.instance;
    hr = IDirectPlayX_Open(dp, &sd, DPOPEN_JOIN);
    CHECKHR(S_OK, hr, "Open(JOIN)");
    if (FAILED(hr)) { IDirectPlayX_Release(dp); return; }
    CHECK(log->join_requests == 1 && log->newid_requests == 1, "join: %lu join requests, %lu id requests", log->join_requests, log->newid_requests);
    CHECK((log->newid_flags & 0x09) == 0x09, "the id of the system player was asked with flags 0x%lx", log->newid_flags);

    size = sizeof(buf);
    hr = IDirectPlayX_GetSessionDesc(dp, buf, &size);
    CHECKHR(S_OK, hr, "GetSessionDesc");
    CHECK(IsEqualGUID(&((DPSESSIONDESC2 *)buf)->guidInstance, &SGSESSION_INSTANCE) && ((DPSESSIONDESC2 *)buf)->dwMaxPlayers == 5 &&
          ((DPSESSIONDESC2 *)buf)->lpszSessionNameA && !strcmp(((DPSESSIONDESC2 *)buf)->lpszSessionNameA, "sg-session"),
          "the joined session is not the one that was found");

    /* a peer that is not the host asks the host for the id of every group and player */
    hr = IDirectPlayX_CreatePlayer(dp, &p, NULL, NULL, NULL, 0, 0);
    CHECKHR(S_OK, hr, "CreatePlayer in the joined session");
    CHECK(log->newid_requests == 2 && p > 0x7000 && p <= 0x7000 + 2, "player id 0x%lx after %lu id requests", p, log->newid_requests);
    hr = IDirectPlayX_CreateGroup(dp, &g1, NULL, NULL, 0, 0);
    CHECKHR(S_OK, hr, "CreateGroup in the joined session");
    CHECK(log->newid_requests == 3 && g1 > p && g1 <= 0x7000 + 3, "group id 0x%lx after %lu id requests", g1, log->newid_requests);
    CHECK((log->newid_flags & 0x08) != 0, "the id of the group was asked with flags 0x%lx", log->newid_flags);
    hr = IDirectPlayX_CreateGroup(dp, &g2, NULL, NULL, 0, DPGROUP_HIDDEN);
    CHECKHR(S_OK, hr, "CreateGroup (2)");
    CHECK(log->newid_requests == 4 && g2 > g1 && g2 != p, "second group id 0x%lx", g2);
    hr = IDirectPlayX_GetGroupFlags(dp, g2, &gdw);
    CHECK(hr == S_OK && (gdw & DPGROUP_HIDDEN), "GetGroupFlags of the group with the host's id: 0x%lx 0x%lx", (unsigned long)hr, gdw);

    /* the player is not the host's: the session's owner is not asked about enumerating */
    {
        int n = 0;
        hr = IDirectPlayX_EnumPlayers(dp, NULL, enum_cb, &n, 0);
        CHECK(hr == S_OK && n == 1, "EnumPlayers in the joined session: 0x%lx, %d players", (unsigned long)hr, n);
    }
    drain(dp);
    (void)from; (void)to;
    IDirectPlayX_Close(dp);
    IDirectPlayX_Release(dp);
}

/* ---- a lobby session: the staging area features go to the lobby provider ---- */

static void test_lobby_session(void)
{
    IDirectPlay4 *dp = make_dp(&SGLSP_GUID, TRUE);
    SGLSPLog *log = lsp_getlog();
    DPSESSIONDESC2 sd;
    DPLCONNECTION conn;
    DPID group, owner;
    HRESULT hr;

    if (!dp) return;

    memset(&sd, 0, sizeof(sd));
    sd.dwSize = sizeof(sd);
    sd.dwMaxPlayers = 4;
    hr = IDirectPlayX_Open(dp, &sd, DPOPEN_CREATE);
    CHECKHR(S_OK, hr, "lobby Open(CREATE)");
    if (FAILED(hr)) { IDirectPlayX_Release(dp); return; }

    hr = IDirectPlayX_CreateGroup(dp, &group, NULL, NULL, 0, DPGROUP_STAGINGAREA);
    CHECKHR(S_OK, hr, "lobby CreateGroup");

    hr = IDirectPlayX_GetGroupFlags(dp, group, &gdw);
    CHECKHR(S_OK, hr, "lobby GetGroupFlags");
    CHECK(gdw & DPGROUP_STAGINGAREA, "lobby GetGroupFlags: 0x%08lx", gdw);

    hr = IDirectPlayX_GetGroupOwner(dp, group, &owner);
    CHECKHR(S_OK, hr, "lobby GetGroupOwner");
    CHECK(owner == 0, "owner 0x%lx expected none", owner);
    hr = IDirectPlayX_SetGroupOwner(dp, group, 0x7777);
    CHECKHR(DPERR_INVALIDPLAYER, hr, "lobby SetGroupOwner(unknown player)");

    memset(&conn, 0, sizeof(conn));
    conn.dwSize = sizeof(conn);
    memset(log, 0, sizeof(*log));
    hr = IDirectPlayX_SetGroupConnectionSettings(dp, 0, group, &conn);
    CHECKHR(S_OK, hr, "lobby SetGroupConnectionSettings");
    CHECK(log->setgroupconn_calls == 1 && log->setgroupconn_group == group && log->setgroupconn_conn == &conn,
          "SetGroupConnectionSettings reached the lobby provider: %lu calls", log->setgroupconn_calls);
    gsize = 100;
    hr = IDirectPlayX_GetGroupConnectionSettings(dp, 0, group, gbuf, &gsize);
    CHECKHR(DPERR_NOTLOBBIED, hr, "lobby GetGroupConnectionSettings (the provider's answer)");
    CHECK(log->getgroupconn_calls == 1 && log->getgroupconn_group == group, "GetGroupConnectionSettings reached the lobby provider: %lu calls", log->getgroupconn_calls);
    hr = IDirectPlayX_StartSession(dp, 0, group);
    CHECKHR(S_OK, hr, "lobby StartSession");
    CHECK(log->startsession_calls == 1 && log->startsession_group == group, "StartSession reached the lobby provider: %lu calls", log->startsession_calls);

    IDirectPlayX_Close(dp);
    IDirectPlayX_Release(dp);
}

int main(int argc, char **argv)
{
    char path[MAX_PATH];
    SGSP_GETISP unused;

    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);

    /* the provider is loaded by DirectPlay too: this is the same module */
    GetFullPathNameA("sgfakesp.dll", sizeof(path), path, NULL);
    spmod = LoadLibraryA(path);
    if (!spmod) { printf("FAIL  cannot load %s\n", path); return 1; }
    sp_getlog = (SGSP_GETLOG)GetProcAddress(spmod, "SGSP_GetLog");
    sp_complete = (SGSP_COMPLETE)GetProcAddress(spmod, "SGSP_Complete");
    lsp_getlog = (SGLSP_GETLOG)GetProcAddress(spmod, "SGLSP_GetLog");
    unused = (SGSP_GETISP)GetProcAddress(spmod, "SGSP_GetISP");
    (void)unused;

    test_uninitialized();
    test_session();
    test_join();
    test_lobby_session();

    printf("%d checks, %d failures\n", checks, failures);
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
