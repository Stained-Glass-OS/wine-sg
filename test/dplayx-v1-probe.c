/* Native probe for the IDirectPlay interface (the first version) of dplayx
 * that used to be E_NOTIMPL stubs (patch 2987): AddPlayerToGroup, Close,
 * CreatePlayer, CreateGroup, DeletePlayerFromGroup, DestroyPlayer,
 * DestroyGroup, EnableNewPlayers, EnumGroupPlayers, EnumGroups,
 * EnumPlayers, EnumSessions, GetCaps, GetMessageCount, GetPlayerCaps,
 * GetPlayerName, Initialize, Open, Receive, SaveSession, Send,
 * SetPlayerName. It runs on the service provider of test/dplayx-fakesp.c (no
 * network). Table-driven, values checked. */
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

static const GUID zero_guid;
static IDirectPlay *dp;
static IDirectPlay4 *dp4;

/* ---- the calls that need a service provider ---- */

static DWORD gdw;
static char gbuf[256];

static BOOL CALLBACK pcb(DPID id, LPSTR name, LPSTR fullname, DWORD flags, void *ctx) { (*(int *)ctx)++; return TRUE; }
static BOOL CALLBACK scb(LPDPSESSIONDESC sd, void *ctx, DWORD *timeout, DWORD flags) { (*(int *)ctx)++; return FALSE; }

static HRESULT c_addplayer(void)      { return IDirectPlay_AddPlayerToGroup(dp, 1, 2); }
static HRESULT c_createplayer(void)   { DPID id; return IDirectPlay_CreatePlayer(dp, &id, (char *)"a", (char *)"b", NULL); }
static HRESULT c_creategroup(void)    { DPID id; return IDirectPlay_CreateGroup(dp, &id, (char *)"a", (char *)"b"); }
static HRESULT c_destroyplayer(void)  { return IDirectPlay_DestroyPlayer(dp, 2); }
static HRESULT c_destroygroup(void)   { return IDirectPlay_DestroyGroup(dp, 1); }
static HRESULT c_enable(void)         { return IDirectPlay_EnableNewPlayers(dp, TRUE); }
static HRESULT c_enumgroupplayers(void) { int n = 0; return IDirectPlay_EnumGroupPlayers(dp, 1, pcb, &n, 0); }
static HRESULT c_enumgroups(void)     { int n = 0; return IDirectPlay_EnumGroups(dp, 0, pcb, &n, 0); }
static HRESULT c_enumplayers(void)    { int n = 0; return IDirectPlay_EnumPlayers(dp, 0, pcb, &n, 0); }
static HRESULT c_enumsessions(void)   { int n = 0; DPSESSIONDESC sd; memset(&sd, 0, sizeof(sd)); sd.dwSize = sizeof(sd); return IDirectPlay_EnumSessions(dp, &sd, 10, scb, &n, 0); }
static HRESULT c_getmessagecount(void) { return IDirectPlay_GetMessageCount(dp, 1, &gdw); }
static HRESULT c_open(void)           { DPSESSIONDESC sd; memset(&sd, 0, sizeof(sd)); sd.dwSize = sizeof(sd); return IDirectPlay_Open(dp, &sd); }
static HRESULT c_saveSession(void)    { return IDirectPlay_SaveSession(dp, NULL); }

static const struct { const char *name; HRESULT (*call)(void); } uninit_calls[] =
{
    { "AddPlayerToGroup", c_addplayer }, { "CreatePlayer", c_createplayer }, { "CreateGroup", c_creategroup },
{ "DestroyPlayer", c_destroyplayer }, { "DestroyGroup", c_destroygroup },
    { "EnableNewPlayers", c_enable }, { "EnumGroupPlayers", c_enumgroupplayers }, { "EnumGroups", c_enumgroups },
    { "EnumPlayers", c_enumplayers }, { "EnumSessions", c_enumsessions }, { "GetMessageCount", c_getmessagecount },
    { "Open", c_open }, { "SaveSession", c_saveSession },
};

static void test_uninitialized(void)
{
    unsigned i;
    DWORD a = 10, b = 10;
    char n[10], f[10];
    DPCAPS caps;
    DPID from, to;
    DWORD size = 4;
    HRESULT hr;

    for (i = 0; i < ARRAYSIZE(uninit_calls); i++)
    {
        hr = uninit_calls[i].call();
        CHECKHR(DPERR_UNINITIALIZED, hr, uninit_calls[i].name);
    }
    hr = IDirectPlay_GetPlayerName(dp, 1, n, &a, f, &b);
    CHECKHR(DPERR_UNINITIALIZED, hr, "GetPlayerName");
    memset(&caps, 0, sizeof(caps)); caps.dwSize = sizeof(caps);
    hr = IDirectPlay_GetCaps(dp, &caps);
    CHECKHR(DPERR_UNINITIALIZED, hr, "GetCaps");
    hr = IDirectPlay_GetPlayerCaps(dp, 1, &caps);
    CHECKHR(DPERR_UNINITIALIZED, hr, "GetPlayerCaps");
    hr = IDirectPlay_Receive(dp, &from, &to, DPRECEIVE_ALL, gbuf, &size);
    CHECKHR(DPERR_UNINITIALIZED, hr, "Receive");
    hr = IDirectPlay_Send(dp, 1, 2, 0, gbuf, 4);
    CHECKHR(DPERR_UNINITIALIZED, hr, "Send");
    hr = IDirectPlay_SetPlayerName(dp, 1, (char *)"x", (char *)"y");
    CHECKHR(DPERR_UNINITIALIZED, hr, "SetPlayerName");
}

/* ---- Initialize ---- */

static void test_initialize(void)
{
    static const GUID unknown = { 0x12345678, 1, 2, { 3, 4, 5, 6, 7, 8, 9, 10 } };
    HRESULT hr;
    int n = 0;

    hr = IDirectPlay_Initialize(dp, NULL);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "Initialize(NULL)");
    hr = IDirectPlay_Initialize(dp, (GUID *)&unknown);
    CHECKHR(DPERR_UNAVAILABLE, hr, "Initialize(unknown provider)");
    hr = IDirectPlay_Initialize(dp, (GUID *)&zero_guid);
    CHECKHR(S_OK, hr, "Initialize(GUID_NULL)");
    hr = IDirectPlay_EnumPlayers(dp, 0, pcb, &n, 0);
    CHECKHR(DPERR_UNINITIALIZED, hr, "EnumPlayers after Initialize(GUID_NULL)");
    hr = IDirectPlay_Initialize(dp, (GUID *)&SGSP_GUID);
    CHECKHR(S_OK, hr, "Initialize(provider)");
    hr = IDirectPlay_Initialize(dp, (GUID *)&SGSP_GUID);
    CHECKHR(DPERR_ALREADYINITIALIZED, hr, "Initialize(again)");
    hr = IDirectPlay_Initialize(dp, (GUID *)&unknown);
    CHECKHR(DPERR_ALREADYINITIALIZED, hr, "Initialize(unknown, again)");
}

/* ---- sessions ---- */

struct sessions { int count; int timedout; DPSESSIONDESC sd; DWORD timeout_seen; };

static BOOL CALLBACK enum_sessions(LPDPSESSIONDESC sd, void *ctx, DWORD *timeout, DWORD flags)
{
    struct sessions *s = ctx;

    if (flags & DPESC_TIMEDOUT)
    {
        s->timedout++;
        CHECK(sd == NULL, "timeout callback came with a session");
        return FALSE;
    }
    s->count++;
    if (sd) s->sd = *sd;
    s->timeout_seen = *timeout;
    return TRUE;
}

static void test_enum_sessions(void)
{
    struct sessions s;
    DPSESSIONDESC sd;
    HRESULT hr;

    memset(&sd, 0, sizeof(sd));
    sd.dwSize = sizeof(sd);

    hr = IDirectPlay_EnumSessions(dp, NULL, 10, enum_sessions, &s, 0);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "EnumSessions(NULL)");
    sd.dwSize = 20;
    hr = IDirectPlay_EnumSessions(dp, &sd, 10, enum_sessions, &s, 0);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "EnumSessions(bad size)");
    sd.dwSize = sizeof(sd);
    hr = IDirectPlay_EnumSessions(dp, &sd, 10, NULL, &s, 0);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "EnumSessions(NULL callback)");

    /* the sessions that were played before: none were saved */
    memset(&s, 0, sizeof(s));
    hr = IDirectPlay_EnumSessions(dp, &sd, 10, enum_sessions, &s, 4);
    CHECKHR(S_OK, hr, "EnumSessions(PREVIOUS)");
    CHECK(s.count == 0 && s.timedout == 1, "EnumSessions(PREVIOUS): %d sessions, %d timeouts", s.count, s.timedout);

    memset(&s, 0, sizeof(s));
    hr = IDirectPlay_EnumSessions(dp, &sd, 100, enum_sessions, &s, DPENUMSESSIONS_AVAILABLE);
    CHECKHR(S_OK, hr, "EnumSessions");
    CHECK(s.timedout == 1, "EnumSessions: %d timeout callbacks", s.timedout);
    CHECK(s.count >= 1, "EnumSessions: %d sessions", s.count);
    if (s.count >= 1)
    {
        CHECK(s.sd.dwSize == sizeof(DPSESSIONDESC), "dwSize %lu", s.sd.dwSize);
        CHECK(IsEqualGUID(&s.sd.guidSession, &SGSESSION_APPLICATION), "guidSession is not the application guid");
        CHECK(s.sd.dwSession == SGSESSION_INSTANCE.Data1, "dwSession 0x%lx expected 0x%lx", s.sd.dwSession, (unsigned long)SGSESSION_INSTANCE.Data1);
        CHECK(s.sd.dwMaxPlayers == 5 && s.sd.dwCurrentPlayers == 2, "players %lu/%lu", s.sd.dwCurrentPlayers, s.sd.dwMaxPlayers);
        CHECK(!strcmp(s.sd.szSessionName, "sg-session"), "name '%s'", s.sd.szSessionName);
        CHECK(s.sd.dwUser1 == 11 && s.sd.dwUser2 == 22 && s.sd.dwUser3 == 33 && s.sd.dwUser4 == 44,
              "user values %lu %lu %lu %lu", s.sd.dwUser1, s.sd.dwUser2, s.sd.dwUser3, s.sd.dwUser4);
    }

    /* the application guid selects */
    sd.guidSession = SGSESSION_INSTANCE; /* not an application guid of the one session */
    memset(&s, 0, sizeof(s));
    hr = IDirectPlay_EnumSessions(dp, &sd, 100, enum_sessions, &s, DPENUMSESSIONS_AVAILABLE);
    CHECKHR(S_OK, hr, "EnumSessions(other application)");
}

/* ---- the session ---- */

struct players { int count; DPID ids[8]; char names[8][32]; char fullnames[8][32]; DWORD flags[8]; };

static BOOL CALLBACK enum_players(DPID id, LPSTR name, LPSTR fullname, DWORD flags, void *ctx)
{
    struct players *p = ctx;

    if (p->count < 8)
    {
        p->ids[p->count] = id;
        lstrcpynA(p->names[p->count], name ? name : "(null)", 32);
        lstrcpynA(p->fullnames[p->count], fullname ? fullname : "(null)", 32);
        p->flags[p->count] = flags;
    }
    p->count++;
    return TRUE;
}

static BOOL has_player(const struct players *p, DPID id, const char *name, const char *fullname)
{
    int i;
    for (i = 0; i < p->count && i < 8; i++)
        if (p->ids[i] == id)
            return !strcmp(p->names[i], name) && !strcmp(p->fullnames[i], fullname);
    return FALSE;
}

static DWORD session_flags(void)
{
    char buf[512];
    DWORD size = sizeof(buf);

    if (IDirectPlayX_GetSessionDesc(dp4, buf, &size) != S_OK) return 0xdeadbeef;
    return ((DPSESSIONDESC2 *)buf)->dwFlags;
}

static void test_session(void)
{
    DPSESSIONDESC sd, join;
    DPID p1, p2, g1;
    HANDLE ev1 = NULL, ev2 = NULL;
    struct players pl;
    DWORD a, b, count, size, flags;
    char n[40], f[40];
    HRESULT hr;
    DPCAPS caps, caps4;
    DPID from, to;
    char buf[512];
    char whatbuf[96];

    /* no session yet */
    hr = IDirectPlay_EnableNewPlayers(dp, FALSE);
    CHECKHR(DPERR_NOSESSIONS, hr, "EnableNewPlayers without session");
    hr = IDirectPlay_CreatePlayer(dp, &p1, (char *)"a", NULL, NULL);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "CreatePlayer without session");
    memset(&pl, 0, sizeof(pl));
    hr = IDirectPlay_EnumPlayers(dp, 0, enum_players, &pl, 0);
    CHECKHR(DPERR_NOSESSIONS, hr, "EnumPlayers without session");
    hr = IDirectPlay_EnumGroups(dp, 0, enum_players, &pl, 0);
    CHECKHR(DPERR_NOSESSIONS, hr, "EnumGroups without session");

    /* Open */
    memset(&sd, 0, sizeof(sd));
    hr = IDirectPlay_Open(dp, NULL);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "Open(NULL)");
    sd.dwSize = 33;
    hr = IDirectPlay_Open(dp, &sd);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "Open(bad size)");
    sd.dwSize = sizeof(sd);
    sd.dwSession = 0x00abcdef;
    hr = IDirectPlay_Open(dp, &sd);
    CHECKHR(DPERR_NOSESSIONS, hr, "Open(unknown session number)");

    memset(&sd, 0, sizeof(sd));
    sd.dwSize = sizeof(sd);
    sd.guidSession = SGSESSION_APPLICATION;
    sd.dwMaxPlayers = 3;
    sd.dwUser1 = 77;
    strcpy(sd.szSessionName, "v1 game");
    hr = IDirectPlay_Open(dp, &sd);
    CHECKHR(S_OK, hr, "Open(create)");
    if (FAILED(hr)) return;
    hr = IDirectPlay_Open(dp, &sd);
    CHECKHR(DPERR_ALREADYINITIALIZED, hr, "Open(again)");
    {
        char buf2[512];
        DPSESSIONDESC2 *sd2 = (DPSESSIONDESC2 *)buf2;
        size = sizeof(buf2);
        hr = IDirectPlayX_GetSessionDesc(dp4, buf2, &size);
        CHECKHR(S_OK, hr, "GetSessionDesc");
        CHECK(IsEqualGUID(&sd2->guidApplication, &SGSESSION_APPLICATION) && sd2->dwMaxPlayers == 3 && sd2->dwUser1 == 77 &&
              sd2->lpszSessionNameA && !strcmp(sd2->lpszSessionNameA, "v1 game"),
              "the session that was opened: max %lu user %lu name '%s'", sd2->dwMaxPlayers, sd2->dwUser1, sd2->lpszSessionNameA ? sd2->lpszSessionNameA : "(null)");
        join = sd; /* kept for later */
    }

    /* EnableNewPlayers is the session flag */
    hr = IDirectPlay_EnableNewPlayers(dp, FALSE);
    CHECKHR(S_OK, hr, "EnableNewPlayers(FALSE)");
    flags = session_flags();
    CHECK(flags & DPSESSION_NEWPLAYERSDISABLED, "EnableNewPlayers(FALSE): flags 0x%lx", flags);
    hr = IDirectPlay_EnableNewPlayers(dp, TRUE);
    CHECKHR(S_OK, hr, "EnableNewPlayers(TRUE)");
    flags = session_flags();
    CHECK(!(flags & DPSESSION_NEWPLAYERSDISABLED), "EnableNewPlayers(TRUE): flags 0x%lx", flags);

    /* players and groups */
    hr = IDirectPlay_CreatePlayer(dp, &p1, (char *)"alice", (char *)"Alice Liddell", &ev1);
    CHECKHR(S_OK, hr, "CreatePlayer(names, event)");
    CHECK(ev1 != NULL, "CreatePlayer gave no event");
    hr = IDirectPlay_CreatePlayer(dp, &p2, (char *)"bob", NULL, &ev2);
    CHECKHR(S_OK, hr, "CreatePlayer(name, event)");
    hr = IDirectPlay_CreatePlayer(dp, &g1, (char *)"x", (char *)"y", NULL);
    CHECKHR(S_OK, hr, "CreatePlayer(no event)");
    hr = IDirectPlay_DestroyPlayer(dp, g1);
    CHECKHR(S_OK, hr, "DestroyPlayer");
    hr = IDirectPlay_CreateGroup(dp, &g1, (char *)"team", (char *)"The Team");
    CHECKHR(S_OK, hr, "CreateGroup");
    hr = IDirectPlay_CreateGroup(dp, NULL, (char *)"team", NULL);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "CreateGroup(NULL)");

    /* names: two buffers */
    a = sizeof(n); b = sizeof(f);
    memset(n, 'x', sizeof(n)); memset(f, 'x', sizeof(f));
    hr = IDirectPlay_GetPlayerName(dp, p1, n, &a, f, &b);
    CHECKHR(S_OK, hr, "GetPlayerName");
    CHECK(a == 6 && b == 14 && !strcmp(n, "alice") && !strcmp(f, "Alice Liddell"), "GetPlayerName: '%s' (%lu) '%s' (%lu)", n, a, f, b);
    a = 3; b = 3;
    hr = IDirectPlay_GetPlayerName(dp, p1, n, &a, f, &b);
    CHECKHR(DPERR_BUFFERTOOSMALL, hr, "GetPlayerName(small buffers)");
    CHECK(a == 6 && b == 14, "GetPlayerName(small buffers): sizes %lu %lu", a, b);
    a = 0; b = 0;
    hr = IDirectPlay_GetPlayerName(dp, p1, NULL, &a, NULL, &b);
    CHECKHR(DPERR_BUFFERTOOSMALL, hr, "GetPlayerName(no buffers)");
    CHECK(a == 6 && b == 14, "GetPlayerName(no buffers): sizes %lu %lu", a, b);
    a = sizeof(n); b = sizeof(f);
    hr = IDirectPlay_GetPlayerName(dp, p2, n, &a, f, &b);
    CHECKHR(S_OK, hr, "GetPlayerName(only a short name)");
    CHECK(a == 4 && b == 0 && !strcmp(n, "bob"), "GetPlayerName(short only): '%s' (%lu) (%lu)", n, a, b);
    hr = IDirectPlay_GetPlayerName(dp, 0x7777, n, &a, f, &b);
    CHECKHR(DPERR_INVALIDPLAYER, hr, "GetPlayerName(unknown)");
    hr = IDirectPlay_GetPlayerName(dp, p1, n, NULL, f, &b);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "GetPlayerName(no size)");

    hr = IDirectPlay_SetPlayerName(dp, p2, (char *)"robert", (char *)"Robert Smith");
    CHECKHR(S_OK, hr, "SetPlayerName");
    a = sizeof(n); b = sizeof(f);
    hr = IDirectPlay_GetPlayerName(dp, p2, n, &a, f, &b);
    CHECK(hr == S_OK && !strcmp(n, "robert") && !strcmp(f, "Robert Smith"), "name after SetPlayerName: '%s' '%s'", n, f);

    /* groups */
    hr = IDirectPlay_AddPlayerToGroup(dp, g1, p1);
    CHECKHR(S_OK, hr, "AddPlayerToGroup");
    hr = IDirectPlay_AddPlayerToGroup(dp, 0x7777, p1);
    CHECKHR(DPERR_INVALIDGROUP, hr, "AddPlayerToGroup(unknown group)");
    hr = IDirectPlay_AddPlayerToGroup(dp, g1, 0x7777);
    CHECKHR(DPERR_INVALIDPLAYER, hr, "AddPlayerToGroup(unknown player)");

    memset(&pl, 0, sizeof(pl));
    hr = IDirectPlay_EnumPlayers(dp, 0, enum_players, &pl, 0);
    CHECKHR(S_OK, hr, "EnumPlayers");
    CHECK(pl.count == 2 && has_player(&pl, p1, "alice", "Alice Liddell") && has_player(&pl, p2, "robert", "Robert Smith"),
          "EnumPlayers: %d players", pl.count);
    memset(&pl, 0, sizeof(pl));
    hr = IDirectPlay_EnumGroups(dp, 0, enum_players, &pl, 0);
    CHECKHR(S_OK, hr, "EnumGroups");
    CHECK(pl.count == 1 && has_player(&pl, g1, "team", "The Team"), "EnumGroups: %d groups", pl.count);
    memset(&pl, 0, sizeof(pl));
    hr = IDirectPlay_EnumGroupPlayers(dp, g1, enum_players, &pl, 0);
    CHECKHR(S_OK, hr, "EnumGroupPlayers");
    CHECK(pl.count == 1 && has_player(&pl, p1, "alice", "Alice Liddell"), "EnumGroupPlayers: %d players", pl.count);
    hr = IDirectPlay_EnumGroupPlayers(dp, 0x7777, enum_players, &pl, 0);
    CHECKHR(DPERR_INVALIDGROUP, hr, "EnumGroupPlayers(unknown group)");
    hr = IDirectPlay_EnumPlayers(dp, 0, NULL, &pl, 0);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "EnumPlayers(NULL callback)");
    /* the session number of the open session is accepted, another one is not there */
    memset(&pl, 0, sizeof(pl));
    {
        char buf2[512];
        DWORD number;
        size = sizeof(buf2);
        IDirectPlayX_GetSessionDesc(dp4, buf2, &size);
        number = ((DPSESSIONDESC2 *)buf2)->guidInstance.Data1;
        hr = IDirectPlay_EnumPlayers(dp, number, enum_players, &pl, 0);
        CHECKHR(S_OK, hr, "EnumPlayers(session number of the open session)");
        CHECK(pl.count == 2, "EnumPlayers(session number): %d players", pl.count);
        hr = IDirectPlay_EnumPlayers(dp, number + 1, enum_players, &pl, 0);
        CHECKHR(DPERR_NOSESSIONS, hr, "EnumPlayers(another session number)");
    }

    hr = IDirectPlay_DeletePlayerFromGroup(dp, g1, p1);
    CHECKHR(S_OK, hr, "DeletePlayerFromGroup");
    memset(&pl, 0, sizeof(pl));
    IDirectPlay_EnumGroupPlayers(dp, g1, enum_players, &pl, 0);
    CHECK(pl.count == 0, "EnumGroupPlayers after DeletePlayerFromGroup: %d", pl.count);

    /* caps */
    memset(&caps, 0, sizeof(caps)); caps.dwSize = sizeof(caps);
    memset(&caps4, 0, sizeof(caps4)); caps4.dwSize = sizeof(caps4);
    hr = IDirectPlay_GetCaps(dp, &caps);
    CHECKHR(S_OK, hr, "GetCaps");
    IDirectPlayX_GetCaps(dp4, &caps4, 0);
    CHECK(!memcmp(&caps, &caps4, sizeof(caps)) && caps.dwMaxBufferSize == 1024 && caps.dwMaxPlayers == 64,
          "GetCaps: max buffer %lu max players %lu", caps.dwMaxBufferSize, caps.dwMaxPlayers);
    memset(&caps, 0, sizeof(caps)); caps.dwSize = sizeof(caps);
    hr = IDirectPlay_GetPlayerCaps(dp, p1, &caps);
    CHECKHR(S_OK, hr, "GetPlayerCaps");
    CHECK(caps.dwMaxBufferSize == 1024, "GetPlayerCaps: max buffer %lu", caps.dwMaxBufferSize);

    /* messages */
    for (;;)
    {
        size = sizeof(buf);
        if (IDirectPlay_Receive(dp, &from, &to, DPRECEIVE_ALL, buf, &size) != S_OK) break;
    }
    hr = IDirectPlay_GetMessageCount(dp, p2, &count);
    CHECKHR(S_OK, hr, "GetMessageCount");
    CHECK(count == 0, "GetMessageCount of an empty queue: %lu", count);
    hr = IDirectPlay_GetMessageCount(dp, 0x7777, &count);
    CHECKHR(DPERR_INVALIDPLAYER, hr, "GetMessageCount(unknown)");
    hr = IDirectPlay_GetMessageCount(dp, p2, NULL);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "GetMessageCount(NULL)");
    if (ev1) WaitForSingleObject(ev1, 0); /* auto reset events: the system messages signalled them */
    if (ev2) WaitForSingleObject(ev2, 0);

    hr = IDirectPlay_Send(dp, p1, p2, 0, (void *)"hello v1", 9);
    CHECKHR(S_OK, hr, "Send");
    hr = IDirectPlay_Send(dp, p1, p2, 0, (void *)"hello v1", 9);
    CHECKHR(S_OK, hr, "Send (2)");
    hr = IDirectPlay_GetMessageCount(dp, p2, &count);
    CHECK(hr == S_OK && count == 2, "GetMessageCount after two sends: 0x%lx, %lu", (unsigned long)hr, count);
    CHECK(ev2 && WaitForSingleObject(ev2, 0) == WAIT_OBJECT_0, "the event of the player was not signalled by a message");
    CHECK(!ev1 || WaitForSingleObject(ev1, 0) == WAIT_TIMEOUT, "the event of the other player was signalled");
    from = to = 0; size = sizeof(buf); memset(buf, 0, sizeof(buf));
    hr = IDirectPlay_Receive(dp, &from, &to, DPRECEIVE_ALL, buf, &size);
    CHECKHR(S_OK, hr, "Receive");
    CHECK(from == p1 && to == p2 && size == 9 && !strcmp(buf, "hello v1"), "Receive: from 0x%lx to 0x%lx size %lu '%s'", from, to, size, buf);
    hr = IDirectPlay_GetMessageCount(dp, p2, &count);
    CHECK(hr == S_OK && count == 1, "GetMessageCount after a receive: %lu", count);
    size = 2;
    hr = IDirectPlay_Receive(dp, &from, &to, DPRECEIVE_ALL | DPRECEIVE_PEEK, buf, &size);
    CHECKHR(DPERR_BUFFERTOOSMALL, hr, "Receive(PEEK, small buffer)");
    CHECK(size == 9, "Receive(small buffer): size %lu", size);
    size = sizeof(buf);
    hr = IDirectPlay_Receive(dp, &from, &to, DPRECEIVE_ALL, buf, &size);
    CHECKHR(S_OK, hr, "Receive (2)");
    size = sizeof(buf);
    hr = IDirectPlay_Receive(dp, &from, &to, DPRECEIVE_ALL, buf, &size);
    CHECKHR(DPERR_NOMESSAGES, hr, "Receive (empty)");

    hr = IDirectPlay_SaveSession(dp, NULL);
    CHECKHR(DPERR_UNSUPPORTED, hr, "SaveSession");

    sprintf(whatbuf, "DestroyGroup");
    hr = IDirectPlay_DestroyGroup(dp, g1);
    CHECKHR(S_OK, hr, whatbuf);
    hr = IDirectPlay_DestroyPlayer(dp, p1);
    CHECKHR(S_OK, hr, "DestroyPlayer(p1)");
    hr = IDirectPlay_DestroyPlayer(dp, p1);
    CHECKHR(DPERR_INVALIDPLAYER, hr, "DestroyPlayer(again)");
    memset(&pl, 0, sizeof(pl));
    IDirectPlay_EnumPlayers(dp, 0, enum_players, &pl, 0);
    CHECK(pl.count == 1, "EnumPlayers after DestroyPlayer: %d", pl.count);

    hr = IDirectPlay_Close(dp);
    CHECKHR(S_OK, hr, "Close");
    memset(&pl, 0, sizeof(pl));
    hr = IDirectPlay_EnumPlayers(dp, 0, enum_players, &pl, 0);
    CHECKHR(DPERR_NOSESSIONS, hr, "EnumPlayers after Close");
    if (ev1) CloseHandle(ev1);
    if (ev2) CloseHandle(ev2);
    (void)join;
}

/* ---- joining the session that EnumSessions found ---- */

static SGSP_GETLOG sp_getlog;

static void test_join(void)
{
    IDirectPlay *jdp;
    IDirectPlay4 *jdp4;
    SGSPLog *log = sp_getlog();
    struct sessions s;
    DPSESSIONDESC sd, wrong;
    DPID p, g;
    char buf[512];
    DWORD size;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_DirectPlay, NULL, CLSCTX_ALL, &IID_IDirectPlay, (void **)&jdp);
    CHECKHR(S_OK, hr, "CoCreateInstance IDirectPlay (join)");
    if (FAILED(hr)) return;
    IDirectPlay_QueryInterface(jdp, &IID_IDirectPlay4A, (void **)&jdp4);
    hr = IDirectPlay_Initialize(jdp, (GUID *)&SGSP_GUID);
    CHECKHR(S_OK, hr, "Initialize (join)");

    memset(log, 0, sizeof(*log));
    memset(&s, 0, sizeof(s));
    memset(&sd, 0, sizeof(sd));
    sd.dwSize = sizeof(sd);
    hr = IDirectPlay_EnumSessions(jdp, &sd, 100, enum_sessions, &s, DPENUMSESSIONS_AVAILABLE);
    CHECKHR(S_OK, hr, "EnumSessions (join)");
    CHECK(s.count >= 1, "EnumSessions (join) found %d sessions", s.count);

    /* the session description of the enumeration opens the session it names */
    wrong = s.sd;
    wrong.dwSession++;
    hr = IDirectPlay_Open(jdp, &wrong);
    CHECKHR(DPERR_NOSESSIONS, hr, "Open(session number that was not enumerated)");
    hr = IDirectPlay_Open(jdp, &s.sd);
    CHECKHR(S_OK, hr, "Open(join)");
    if (hr == S_OK)
    {
        CHECK(log->join_requests == 1, "Open(join): %lu join requests", log->join_requests);
        size = sizeof(buf);
        hr = IDirectPlayX_GetSessionDesc(jdp4, buf, &size);
        CHECK(hr == S_OK && IsEqualGUID(&((DPSESSIONDESC2 *)buf)->guidInstance, &SGSESSION_INSTANCE) && ((DPSESSIONDESC2 *)buf)->dwMaxPlayers == 5,
              "the session that was joined is not the enumerated one");

        /* only the host can allow or refuse new players */
        hr = IDirectPlay_EnableNewPlayers(jdp, FALSE);
        CHECKHR(DPERR_ACCESSDENIED, hr, "EnableNewPlayers on a session that was joined");

        /* the host gives the ids of the new players and groups */
        hr = IDirectPlay_CreatePlayer(jdp, &p, (char *)"joiner", NULL, NULL);
        CHECKHR(S_OK, hr, "CreatePlayer (join)");
        hr = IDirectPlay_CreateGroup(jdp, &g, (char *)"joined group", NULL);
        CHECKHR(S_OK, hr, "CreateGroup (join)");
        CHECK(log->newid_requests == 3 && p > 0x7000 && g > p && g <= 0x7003, "ids 0x%lx and 0x%lx after %lu id requests", p, g, log->newid_requests);
        IDirectPlay_Close(jdp);
    }
    IDirectPlayX_Release(jdp4);
    IDirectPlay_Release(jdp);
}

int main(int argc, char **argv)
{
    char path[MAX_PATH];
    HMODULE spmod;
    HRESULT hr;

    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);

    GetFullPathNameA("sgfakesp.dll", sizeof(path), path, NULL);
    spmod = LoadLibraryA(path);
    if (!spmod) { printf("FAIL  cannot load %s\n", path); return 1; }

    sp_getlog = (SGSP_GETLOG)GetProcAddress(spmod, "SGSP_GetLog");
    hr = CoCreateInstance(&CLSID_DirectPlay, NULL, CLSCTX_ALL, &IID_IDirectPlay, (void **)&dp);
    CHECKHR(S_OK, hr, "CoCreateInstance IDirectPlay");
    if (FAILED(hr)) return 1;
    hr = IDirectPlay_QueryInterface(dp, &IID_IDirectPlay4A, (void **)&dp4);
    CHECKHR(S_OK, hr, "QueryInterface IDirectPlay4A");

    test_uninitialized();
    test_initialize();
    test_enum_sessions();
    test_session();
    test_join();

    IDirectPlayX_Release(dp4);
    IDirectPlay_Release(dp);
    printf("%d checks, %d failures\n", checks, failures);
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
