/* Native probe for the interfaces dplayx gives to service providers (patch
 * 2986): IDirectPlaySP (AddMRUEntry, EnumMRUEntries, CreateAddress,
 * CreateCompoundAddress, GetPlayerFlags, SendComplete's plumbing is in the
 * dp4 probe) and IDPLobbySP (AddGroupToGroup, AddPlayerToGroup, CreateGroup,
 * CreateGroupInGroup, DeleteGroupFromGroup, DeletePlayerFromGroup,
 * DestroyGroup, EnumSessionsResponse, Get/SetSPDataPointer, HandleMessage,
 * SendChatMessage, SetGroupName, SetPlayerName, SetSessionDesc,
 * StartSession). The service provider of test/dplayx-fakesp.c hands both
 * interfaces out; this program calls them the way a provider does. */
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

/* --- the interface a lobby provider is given --- */
typedef struct SGLSP SGLSP;
typedef struct { DWORD dwSize; SGLSP *lpISP; DWORD dwAnchorID, dwGroupID, dwParentID; DPNAME *lpName; DWORD dwGroupFlags; } ADDGROUPTOGROUP;
typedef struct { DWORD dwSize; SGLSP *lpISP; DWORD dwGroupID, dwPlayerID, dwPlayerFlags; DPNAME *lpName; } ADDPLAYERTOGROUP;
typedef struct { DWORD dwSize; SGLSP *lpISP; DWORD dwGroupID; DPNAME *lpName; void *lpData; DWORD dwDataSize, dwFlags; } CREATEGROUP;
typedef struct { DWORD dwSize; SGLSP *lpISP; DWORD dwParentID, dwGroupID; DPNAME *lpName; DWORD dwFlags; } CREATEGROUPINGROUP;
typedef struct { DWORD dwSize; SGLSP *lpISP; DWORD dwParentID, dwGroupID; } DELETEGROUPFROMGROUP;
typedef struct { DWORD dwSize; SGLSP *lpISP; DWORD dwGroupID, dwPlayerID; } DELETEPLAYERFROMGROUP;
typedef struct { DWORD dwSize; SGLSP *lpISP; DWORD dwGroupID; } DESTROYGROUP;
typedef struct { DWORD dwSize; DPSESSIONDESC2 *lpsd; } ENUMSESSIONSRESPONSE;
typedef struct { DWORD dwSize; DWORD dwFromID, dwToID; void *lpBuffer; DWORD dwBufSize; } HANDLEMESSAGE;
typedef struct { DWORD dwSize; SGLSP *lpISP; DWORD dwFromID, dwToID, dwFlags; DPCHAT *lpChat; } CHATMESSAGE;
typedef struct { DWORD dwSize; SGLSP *lpISP; DWORD dwGroupID; DPNAME *lpName; DWORD dwFlags; } SETGROUPNAME;
typedef struct { DWORD dwSize; SGLSP *lpISP; DWORD dwPlayerID; DPNAME *lpName; DWORD dwFlags; } SETPLAYERNAME;
typedef struct { DWORD dwSize; DPSESSIONDESC2 *lpsd; } SETSESSIONDESC;
typedef struct { DWORD dwFlags, dwGroupID, dwHostID; DPLCONNECTION *lpConn; } STARTSESSIONCOMMAND;
struct SGLSP
{
    const struct
    {
        HRESULT (WINAPI *QueryInterface)(SGLSP *, REFIID, void **);
        ULONG (WINAPI *AddRef)(SGLSP *);
        ULONG (WINAPI *Release)(SGLSP *);
        HRESULT (WINAPI *AddGroupToGroup)(SGLSP *, ADDGROUPTOGROUP *);
        HRESULT (WINAPI *AddPlayerToGroup)(SGLSP *, ADDPLAYERTOGROUP *);
        HRESULT (WINAPI *CreateGroup)(SGLSP *, CREATEGROUP *);
        HRESULT (WINAPI *CreateGroupInGroup)(SGLSP *, CREATEGROUPINGROUP *);
        HRESULT (WINAPI *DeleteGroupFromGroup)(SGLSP *, DELETEGROUPFROMGROUP *);
        HRESULT (WINAPI *DeletePlayerFromGroup)(SGLSP *, DELETEPLAYERFROMGROUP *);
        HRESULT (WINAPI *DestroyGroup)(SGLSP *, DESTROYGROUP *);
        HRESULT (WINAPI *EnumSessionsResponse)(SGLSP *, ENUMSESSIONSRESPONSE *);
        HRESULT (WINAPI *GetSPDataPointer)(SGLSP *, void **);
        HRESULT (WINAPI *HandleMessage)(SGLSP *, HANDLEMESSAGE *);
        HRESULT (WINAPI *SendChatMessage)(SGLSP *, CHATMESSAGE *);
        HRESULT (WINAPI *SetGroupName)(SGLSP *, SETGROUPNAME *);
        HRESULT (WINAPI *SetPlayerName)(SGLSP *, SETPLAYERNAME *);
        HRESULT (WINAPI *SetSessionDesc)(SGLSP *, SETSESSIONDESC *);
        HRESULT (WINAPI *SetSPDataPointer)(SGLSP *, void *);
        HRESULT (WINAPI *StartSession)(SGLSP *, STARTSESSIONCOMMAND *);
    } *lpVtbl;
};

static SGSPUnknown *spisp;
static SGLSP *lspisp;
static SGSP_GETISP sp_getisp;
static SGLSP_GETISP lsp_getisp;

static IDirectPlay4 *make_dp(const GUID *provider)
{
    IDirectPlay4 *dp = NULL;
    IDirectPlayLobby3A *lobby;
    DPCOMPOUNDADDRESSELEMENT elem;
    DWORD size = 0;
    void *address;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_DirectPlay, NULL, CLSCTX_ALL, &IID_IDirectPlay4A, (void **)&dp);
    CHECKHR(S_OK, hr, "CoCreateInstance DirectPlay");
    hr = CoCreateInstance(&CLSID_DirectPlayLobby, NULL, CLSCTX_ALL, &IID_IDirectPlayLobby3A, (void **)&lobby);
    CHECKHR(S_OK, hr, "CoCreateInstance DirectPlayLobby");
    elem.guidDataType = provider == &SGLSP_GUID ? DPAID_LobbyProvider : DPAID_ServiceProvider;
    elem.dwDataSize = sizeof(GUID);
    elem.lpData = (void *)provider;
    IDirectPlayLobby_CreateCompoundAddress(lobby, &elem, 1, NULL, &size);
    address = calloc(1, size);
    IDirectPlayLobby_CreateCompoundAddress(lobby, &elem, 1, address, &size);
    hr = IDirectPlayX_InitializeConnection(dp, address, 0);
    CHECKHR(S_OK, hr, "InitializeConnection");
    free(address);
    IDirectPlayLobby_Release(lobby);
    return dp;
}

/* ---- addresses ---- */

struct elements { int count; GUID types[8]; DWORD sizes[8]; BYTE data[8][64]; };

static BOOL CALLBACK enum_address(REFGUID type, DWORD size, const void *data, void *ctx)
{
    struct elements *e = ctx;
    if (e->count < 8)
    {
        e->types[e->count] = *type;
        e->sizes[e->count] = size;
        memcpy(e->data[e->count], data, size < 64 ? size : 64);
    }
    e->count++;
    return TRUE;
}

static void test_addresses(void)
{
    DPCOMPOUNDADDRESSELEMENT elems[3];
    DWORD size, size2;
    BYTE a1[256], a2[256];
    struct elements e;
    IDirectPlayLobby3A *lobby;
    HRESULT hr;
    WORD port = 2300;

    CoCreateInstance(&CLSID_DirectPlayLobby, NULL, CLSCTX_ALL, &IID_IDirectPlayLobby3A, (void **)&lobby);

    /* CreateAddress: the provider and one element */
    size = 0;
    hr = spisp->lpVtbl->CreateAddress(spisp, &SGSP_GUID, &DPAID_INet, "10.1.2.3", 9, NULL, &size);
    CHECKHR(DPERR_BUFFERTOOSMALL, hr, "CreateAddress(size)");
    CHECK(size == (sizeof(DPADDRESS) + 4) + (sizeof(DPADDRESS) + sizeof(GUID)) + (sizeof(DPADDRESS) + 9), "CreateAddress size %lu", size);
    hr = spisp->lpVtbl->CreateAddress(spisp, &SGSP_GUID, &DPAID_INet, "10.1.2.3", 9, a1, &size);
    CHECKHR(S_OK, hr, "CreateAddress");
    memset(&e, 0, sizeof(e));
    hr = IDirectPlayLobby_EnumAddress(lobby, enum_address, a1, size, &e);
    CHECKHR(S_OK, hr, "EnumAddress of the created address");
    CHECK(e.count == 3 && IsEqualGUID(&e.types[0], &DPAID_TotalSize) && e.sizes[0] == 4 && *(DWORD *)e.data[0] == size,
          "address: %d elements, first size %lu total %lu", e.count, e.sizes[0], *(DWORD *)e.data[0]);
    CHECK(e.count == 3 && IsEqualGUID(&e.types[1], &DPAID_ServiceProvider) && IsEqualGUID((GUID *)e.data[1], &SGSP_GUID), "address: provider element");
    CHECK(e.count == 3 && IsEqualGUID(&e.types[2], &DPAID_INet) && e.sizes[2] == 9 && !strcmp((char *)e.data[2], "10.1.2.3"), "address: INet element");

    hr = spisp->lpVtbl->CreateAddress(spisp, NULL, &DPAID_INet, "x", 2, a1, &size);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "CreateAddress(NULL provider)");
    hr = spisp->lpVtbl->CreateAddress(spisp, &SGSP_GUID, &DPAID_INet, "x", 2, a1, NULL);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "CreateAddress(NULL size)");

    /* CreateCompoundAddress is the lobby's one */
    elems[0].guidDataType = DPAID_ServiceProvider; elems[0].dwDataSize = sizeof(GUID); elems[0].lpData = (void *)&SGSP_GUID;
    elems[1].guidDataType = DPAID_INet; elems[1].dwDataSize = 8; elems[1].lpData = (void *)"1.2.3.4";
    elems[2].guidDataType = DPAID_INetPort; elems[2].dwDataSize = sizeof(WORD); elems[2].lpData = &port;
    size = size2 = 0;
    hr = spisp->lpVtbl->CreateCompoundAddress(spisp, elems, 3, NULL, &size);
    CHECKHR(DPERR_BUFFERTOOSMALL, hr, "CreateCompoundAddress(size)");
    hr = IDirectPlayLobby_CreateCompoundAddress(lobby, elems, 3, NULL, &size2);
    CHECK(size == size2 && size != 0, "CreateCompoundAddress size %lu, the lobby's %lu", size, size2);
    memset(a1, 0, sizeof(a1)); memset(a2, 0, sizeof(a2));
    hr = spisp->lpVtbl->CreateCompoundAddress(spisp, elems, 3, a1, &size);
    CHECKHR(S_OK, hr, "CreateCompoundAddress");
    hr = IDirectPlayLobby_CreateCompoundAddress(lobby, elems, 3, a2, &size2);
    CHECK(!memcmp(a1, a2, size), "CreateCompoundAddress is not the lobby's address");
    hr = spisp->lpVtbl->CreateCompoundAddress(spisp, elems, 0, a1, &size);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "CreateCompoundAddress(no elements)");
    hr = spisp->lpVtbl->CreateCompoundAddress(spisp, NULL, 3, a1, &size);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "CreateCompoundAddress(NULL)");
    hr = spisp->lpVtbl->CreateCompoundAddress(spisp, elems, 3, a1, NULL);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "CreateCompoundAddress(NULL size)");

    /* EnumAddress works on the same address */
    memset(&e, 0, sizeof(e));
    hr = spisp->lpVtbl->EnumAddress(spisp, enum_address, a1, size, &e);
    CHECKHR(S_OK, hr, "SP EnumAddress");
    CHECK(e.count == 4, "SP EnumAddress: %d elements", e.count);
    IDirectPlayLobby_Release(lobby);
}

/* ---- most recently used lists ---- */

struct mru { int count; DWORD sizes[8]; char data[8][16]; int stop; };

static BOOL CALLBACK enum_mru(const void *data, DWORD size, void *ctx)
{
    struct mru *m = ctx;

    if (m->count < 8)
    {
        m->sizes[m->count] = size;
        memcpy(m->data[m->count], data, size < 16 ? size : 15);
        m->data[m->count][size < 16 ? size : 15] = 0;
    }
    m->count++;
    return !(m->stop && m->count >= m->stop);
}

static void check_mru(const WCHAR *section, const WCHAR *key, const char *expect, const char *what)
{
    struct mru m;
    char got[64] = "";
    HRESULT hr;
    int i;

    memset(&m, 0, sizeof(m));
    hr = spisp->lpVtbl->EnumMRUEntries(spisp, section, key, enum_mru, &m);
    CHECKHR(S_OK, hr, what);
    for (i = 0; i < m.count && i < 8; i++)
    {
        if (i) strcat(got, ",");
        strcat(got, m.data[i]);
    }
    CHECK(!strcmp(got, expect), "%s: list is '%s' expected '%s'", what, got, expect);
}

static void test_mru(void)
{
    static const WCHAR section[] = L"Software\\SG Probe\\MRU";
    static const WCHAR key[] = L"list";
    struct mru m;
    HRESULT hr;
    HKEY hkey;
    char value[32];
    DWORD size;

    /* a list that was never made is empty */
    check_mru(section, key, "", "EnumMRUEntries of a list that does not exist");

    hr = spisp->lpVtbl->AddMRUEntry(spisp, section, key, "alpha", 6, 3);
    CHECKHR(S_OK, hr, "AddMRUEntry alpha");
    hr = spisp->lpVtbl->AddMRUEntry(spisp, section, key, "beta", 5, 3);
    CHECKHR(S_OK, hr, "AddMRUEntry beta");
    hr = spisp->lpVtbl->AddMRUEntry(spisp, section, key, "gamma", 6, 3);
    CHECKHR(S_OK, hr, "AddMRUEntry gamma");
    check_mru(section, key, "gamma,beta,alpha", "newest first");

    hr = spisp->lpVtbl->AddMRUEntry(spisp, section, key, "beta", 5, 3);
    CHECKHR(S_OK, hr, "AddMRUEntry beta again");
    check_mru(section, key, "beta,gamma,alpha", "an entry that is added again moves to the front");

    hr = spisp->lpVtbl->AddMRUEntry(spisp, section, key, "delta", 6, 3);
    CHECKHR(S_OK, hr, "AddMRUEntry delta");
    check_mru(section, key, "delta,beta,gamma", "the oldest entry falls off the list");

    hr = spisp->lpVtbl->AddMRUEntry(spisp, section, key, "eps", 4, 1);
    CHECKHR(S_OK, hr, "AddMRUEntry with a maximum of one");
    check_mru(section, key, "eps", "the list is cut to the maximum");

    /* the list is stored for the user in the registry */
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\SG Probe\\MRU\\list", 0, KEY_READ, &hkey) == ERROR_SUCCESS)
    {
        size = sizeof(value);
        CHECK(RegQueryValueExA(hkey, "0", NULL, NULL, (BYTE *)value, &size) == ERROR_SUCCESS && size == 4 && !strcmp(value, "eps"), "registry entry 0");
        CHECK(RegQueryValueExA(hkey, "1", NULL, NULL, NULL, NULL) != ERROR_SUCCESS, "registry still has an entry 1");
        RegCloseKey(hkey);
    }
    else
        CHECK(0, "the list is not in the registry of the user");

    /* the callback stops the enumeration */
    hr = spisp->lpVtbl->AddMRUEntry(spisp, section, key, "zeta", 5, 4);
    hr = spisp->lpVtbl->AddMRUEntry(spisp, section, key, "eta", 4, 4);
    memset(&m, 0, sizeof(m));
    m.stop = 1;
    hr = spisp->lpVtbl->EnumMRUEntries(spisp, section, key, enum_mru, &m);
    CHECKHR(S_OK, hr, "EnumMRUEntries(stopped)");
    CHECK(m.count == 1, "stopped enumeration called back %d times", m.count);

    /* arguments */
    hr = spisp->lpVtbl->AddMRUEntry(spisp, NULL, key, "x", 2, 3);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "AddMRUEntry(no section)");
    hr = spisp->lpVtbl->AddMRUEntry(spisp, section, key, NULL, 2, 3);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "AddMRUEntry(no data)");
    hr = spisp->lpVtbl->AddMRUEntry(spisp, section, key, "x", 0, 3);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "AddMRUEntry(no size)");
    hr = spisp->lpVtbl->AddMRUEntry(spisp, section, key, "x", 2, 0);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "AddMRUEntry(no maximum)");
    hr = spisp->lpVtbl->EnumMRUEntries(spisp, NULL, key, enum_mru, &m);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "EnumMRUEntries(no section)");
    hr = spisp->lpVtbl->EnumMRUEntries(spisp, section, key, NULL, &m);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "EnumMRUEntries(no callback)");

    /* a list under the section itself */
    hr = spisp->lpVtbl->AddMRUEntry(spisp, section, NULL, "solo", 5, 2);
    CHECKHR(S_OK, hr, "AddMRUEntry(no key)");
    check_mru(section, NULL, "solo", "a list without a key");

    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\SG Probe\\MRU\\list");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\SG Probe\\MRU");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\SG Probe");
}

/* ---- the provider's view of players ---- */

static DPID local, spec, server;

static void test_player_flags(void)
{
    IDirectPlay4 *dp = make_dp(&SGSP_GUID);
    DPSESSIONDESC2 sd;
    DWORD flags;
    HRESULT hr;

    spisp = sp_getisp(); /* every object that loads the provider hands it its own interface */
    memset(&sd, 0, sizeof(sd));
    sd.dwSize = sizeof(sd);
    hr = IDirectPlayX_Open(dp, &sd, DPOPEN_CREATE);
    CHECKHR(S_OK, hr, "Open");
    IDirectPlayX_CreatePlayer(dp, &local, NULL, NULL, NULL, 0, 0);
    IDirectPlayX_CreatePlayer(dp, &spec, NULL, NULL, NULL, 0, DPPLAYER_SPECTATOR);
    IDirectPlayX_CreatePlayer(dp, &server, NULL, NULL, NULL, 0, DPPLAYER_SERVERPLAYER);

    {
        static const struct { const char *name; DPID *id; DWORD flags; } rows[] =
        {
            { "player", &local, 0x08 },
            { "spectator", &spec, 0x08 | DPPLAYER_SPECTATOR },
            { "server player", &server, 0x08 | DPPLAYER_SERVERPLAYER },
        };
        unsigned i;
        for (i = 0; i < ARRAYSIZE(rows); i++)
        {
            char what[64];
            sprintf(what, "SP GetPlayerFlags %s", rows[i].name);
            flags = 0xdeadbeef;
            hr = spisp->lpVtbl->GetPlayerFlags(spisp, *rows[i].id, &flags);
            CHECKHR(S_OK, hr, what);
            CHECK(flags == rows[i].flags, "%s: 0x%08lx expected 0x%08lx", what, flags, rows[i].flags);
        }
    }
    hr = spisp->lpVtbl->GetPlayerFlags(spisp, 0x7777, &flags);
    CHECKHR(DPERR_INVALIDPLAYER, hr, "SP GetPlayerFlags(unknown)");
    hr = spisp->lpVtbl->GetPlayerFlags(spisp, local, NULL);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "SP GetPlayerFlags(NULL)");

    IDirectPlayX_Close(dp);
    IDirectPlayX_Release(dp);
}

/* ---- the lobby provider ---- */

static char rbuf[2048];

static HRESULT receive(IDirectPlay4 *dp, DPID *from, DPID *to, DWORD *size)
{
    *from = 0; *to = 0; *size = sizeof(rbuf);
    return IDirectPlayX_Receive(dp, from, to, DPRECEIVE_ALL, rbuf, size);
}

static void drain(IDirectPlay4 *dp)
{
    DPID from, to;
    DWORD size;
    while (receive(dp, &from, &to, &size) == DP_OK) ;
}

struct list { int count; DPID ids[8]; char names[8][32]; DWORD flags[8]; };

static BOOL CALLBACK list_cb(DPID id, DWORD type, LPCDPNAME name, DWORD flags, void *ctx)
{
    struct list *l = ctx;
    if (l->count < 8)
    {
        l->ids[l->count] = id;
        lstrcpynA(l->names[l->count], name && name->lpszShortNameA ? name->lpszShortNameA : "", 32);
        l->flags[l->count] = flags;
    }
    l->count++;
    return TRUE;
}

static BOOL in_list(const struct list *l, DPID id, const char *name)
{
    int i;
    for (i = 0; i < l->count && i < 8; i++)
        if (l->ids[i] == id) return !name || !strcmp(l->names[i], name);
    return FALSE;
}

static DPNAME wname(const WCHAR *s)
{
    DPNAME n;
    memset(&n, 0, sizeof(n));
    n.dwSize = sizeof(n);
    n.lpszShortName = (WCHAR *)s;
    return n;
}

static void test_lobby_sp(void)
{
    IDirectPlay4 *dp;
    DPSESSIONDESC2 sd, remote_sd;
    DPID from, to;
    DPNAME nm;
    struct list l;
    HRESULT hr;
    DWORD size;
    void *ptr;
    BYTE buf[256];
    ADDGROUPTOGROUP agg;
    ADDPLAYERTOGROUP apg;
    CREATEGROUP cg;
    CREATEGROUPINGROUP cgg;
    DELETEGROUPFROMGROUP dgg;
    DELETEPLAYERFROMGROUP dpg;
    DESTROYGROUP dg;
    ENUMSESSIONSRESPONSE esr;
    HANDLEMESSAGE hm;
    CHATMESSAGE cm;
    SETGROUPNAME sgn;
    SETPLAYERNAME spn;
    SETSESSIONDESC ssd;
    STARTSESSIONCOMMAND ssc;
    DPCHAT chat;
    DPLCONNECTION conn;
    DPSESSIONDESC2 conn_sd;

    dp = make_dp(&SGLSP_GUID);
    lspisp = (SGLSP *)lsp_getisp();
    CHECK(lspisp != NULL, "no lobby provider interface");
    if (!lspisp) return;

    /* the provider's own pointer */
    ptr = (void *)0x1;
    hr = lspisp->lpVtbl->GetSPDataPointer(lspisp, &ptr);
    CHECKHR(DPERR_GENERIC, hr, "GetSPDataPointer (nothing set)");
    CHECK(ptr == NULL, "GetSPDataPointer (nothing set): %p", ptr);
    hr = lspisp->lpVtbl->GetSPDataPointer(lspisp, NULL);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "GetSPDataPointer(NULL)");
    hr = lspisp->lpVtbl->SetSPDataPointer(lspisp, (void *)0x1234);
    CHECKHR(S_OK, hr, "SetSPDataPointer");
    hr = lspisp->lpVtbl->GetSPDataPointer(lspisp, &ptr);
    CHECKHR(S_OK, hr, "GetSPDataPointer");
    CHECK(ptr == (void *)0x1234, "GetSPDataPointer: %p", ptr);

    /* without a session there is nothing to tell DirectPlay about */
    nm = wname(L"early");
    memset(&cg, 0, sizeof(cg));
    cg.dwSize = sizeof(cg); cg.lpISP = lspisp; cg.dwGroupID = 0x500; cg.lpName = &nm;
    hr = lspisp->lpVtbl->CreateGroup(lspisp, &cg);
    CHECKHR(DPERR_NOSESSIONS, hr, "CreateGroup without session");
    memset(&sgn, 0, sizeof(sgn)); sgn.dwSize = sizeof(sgn); sgn.dwGroupID = 1; sgn.lpName = &nm;
    hr = lspisp->lpVtbl->SetGroupName(lspisp, &sgn);
    CHECKHR(DPERR_NOSESSIONS, hr, "SetGroupName without session");
    hm.dwSize = sizeof(hm); hm.dwFromID = 1; hm.dwToID = 2; hm.lpBuffer = "data"; hm.dwBufSize = 5;
    hr = lspisp->lpVtbl->HandleMessage(lspisp, &hm);
    CHECKHR(DPERR_NOSESSIONS, hr, "HandleMessage without session");

    memset(&sd, 0, sizeof(sd));
    sd.dwSize = sizeof(sd);
    sd.dwMaxPlayers = 4;
    sd.lpszSessionNameA = (char *)"lobby game";
    hr = IDirectPlayX_Open(dp, &sd, DPOPEN_CREATE);
    CHECKHR(S_OK, hr, "lobby Open");
    hr = IDirectPlayX_CreatePlayer(dp, &local, NULL, NULL, NULL, 0, 0);
    CHECKHR(S_OK, hr, "lobby CreatePlayer");
    drain(dp);

    /* arguments */
    hr = lspisp->lpVtbl->CreateGroup(lspisp, NULL);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "CreateGroup(NULL)");
    cg.dwSize = 4;
    hr = lspisp->lpVtbl->CreateGroup(lspisp, &cg);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "CreateGroup(bad size)");
    hr = lspisp->lpVtbl->HandleMessage(lspisp, NULL);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "HandleMessage(NULL)");
    hm.lpBuffer = NULL;
    hr = lspisp->lpVtbl->HandleMessage(lspisp, &hm);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "HandleMessage(no data)");

    /* a group made on the lobby server */
    nm = wname(L"remote group");
    memset(&cg, 0, sizeof(cg));
    cg.dwSize = sizeof(cg); cg.lpISP = lspisp; cg.dwGroupID = 0x500; cg.lpName = &nm; cg.dwFlags = DPGROUP_HIDDEN;
    hr = lspisp->lpVtbl->CreateGroup(lspisp, &cg);
    CHECKHR(S_OK, hr, "CreateGroup");
    memset(&l, 0, sizeof(l));
    hr = IDirectPlayX_EnumGroups(dp, NULL, list_cb, &l, DPENUMGROUPS_ALL);
    CHECKHR(S_OK, hr, "EnumGroups");
    CHECK(in_list(&l, 0x500, "remote group"), "the group of the lobby is not listed (%d groups)", l.count);
    hr = IDirectPlayX_GetGroupFlags(dp, 0x500, &size);
    CHECK(hr == S_OK && (size & DPGROUP_HIDDEN), "GetGroupFlags of the lobby's group: 0x%lx 0x%lx", (unsigned long)hr, size);

    nm = wname(L"inner");
    memset(&cgg, 0, sizeof(cgg));
    cgg.dwSize = sizeof(cgg); cgg.lpISP = lspisp; cgg.dwParentID = 0x500; cgg.dwGroupID = 0x501; cgg.lpName = &nm;
    hr = lspisp->lpVtbl->CreateGroupInGroup(lspisp, &cgg);
    CHECKHR(S_OK, hr, "CreateGroupInGroup");
    cgg.dwParentID = 0x7777; cgg.dwGroupID = 0x502;
    hr = lspisp->lpVtbl->CreateGroupInGroup(lspisp, &cgg);
    CHECKHR(DPERR_INVALIDGROUP, hr, "CreateGroupInGroup(unknown parent)");
    memset(&l, 0, sizeof(l));
    hr = IDirectPlayX_EnumGroupsInGroup(dp, 0x500, NULL, list_cb, &l, DPENUMGROUPS_ALL);
    CHECK(hr == S_OK && l.count == 1 && in_list(&l, 0x501, "inner"), "EnumGroupsInGroup: 0x%lx, %d groups", (unsigned long)hr, l.count);

    nm = wname(L"added");
    memset(&agg, 0, sizeof(agg));
    agg.dwSize = sizeof(agg); agg.lpISP = lspisp; agg.dwAnchorID = 0x500; agg.dwGroupID = 0x503; agg.dwParentID = 0x500; agg.lpName = &nm;
    hr = lspisp->lpVtbl->AddGroupToGroup(lspisp, &agg);
    CHECKHR(S_OK, hr, "AddGroupToGroup");
    memset(&l, 0, sizeof(l));
    IDirectPlayX_EnumGroupsInGroup(dp, 0x500, NULL, list_cb, &l, DPENUMGROUPS_ALL);
    CHECK(l.count == 2 && in_list(&l, 0x503, "added"), "EnumGroupsInGroup after AddGroupToGroup: %d groups", l.count);

    /* a player of the lobby joined the group: unknown here until now */
    nm = wname(L"remote player");
    memset(&apg, 0, sizeof(apg));
    apg.dwSize = sizeof(apg); apg.lpISP = lspisp; apg.dwGroupID = 0x500; apg.dwPlayerID = 0x600; apg.lpName = &nm;
    hr = lspisp->lpVtbl->AddPlayerToGroup(lspisp, &apg);
    CHECKHR(S_OK, hr, "AddPlayerToGroup");
    memset(&l, 0, sizeof(l));
    hr = IDirectPlayX_EnumGroupPlayers(dp, 0x500, NULL, list_cb, &l, DPENUMPLAYERS_ALL);
    CHECK(hr == S_OK && l.count == 1 && in_list(&l, 0x600, "remote player"), "EnumGroupPlayers: 0x%lx, %d players", (unsigned long)hr, l.count);
    hr = IDirectPlayX_GetPlayerFlags(dp, 0x600, &size);
    CHECK(hr == S_OK && !(size & DPPLAYER_LOCAL), "the remote player is local: 0x%lx 0x%lx", (unsigned long)hr, size);
    apg.dwGroupID = 0x7777;
    hr = lspisp->lpVtbl->AddPlayerToGroup(lspisp, &apg);
    CHECKHR(DPERR_INVALIDGROUP, hr, "AddPlayerToGroup(unknown group)");

    /* names */
    nm = wname(L"renamed player");
    memset(&spn, 0, sizeof(spn));
    spn.dwSize = sizeof(spn); spn.lpISP = lspisp; spn.dwPlayerID = 0x600; spn.lpName = &nm;
    hr = lspisp->lpVtbl->SetPlayerName(lspisp, &spn);
    CHECKHR(S_OK, hr, "SetPlayerName");
    size = sizeof(buf);
    hr = IDirectPlayX_GetPlayerName(dp, 0x600, buf, &size);
    CHECK(hr == S_OK && ((DPNAME *)buf)->lpszShortNameA && !strcmp(((DPNAME *)buf)->lpszShortNameA, "renamed player"), "player name after SetPlayerName: 0x%lx", (unsigned long)hr);
    spn.lpName = NULL;
    hr = lspisp->lpVtbl->SetPlayerName(lspisp, &spn);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "SetPlayerName(no name)");
    nm = wname(L"renamed group");
    memset(&sgn, 0, sizeof(sgn));
    sgn.dwSize = sizeof(sgn); sgn.lpISP = lspisp; sgn.dwGroupID = 0x500; sgn.lpName = &nm;
    hr = lspisp->lpVtbl->SetGroupName(lspisp, &sgn);
    CHECKHR(S_OK, hr, "SetGroupName");
    memset(&l, 0, sizeof(l));
    IDirectPlayX_EnumGroups(dp, NULL, list_cb, &l, DPENUMGROUPS_ALL);
    CHECK(in_list(&l, 0x500, "renamed group"), "group name after SetGroupName");

    /* a message of the lobby for the local player */
    drain(dp);
    hm.dwSize = sizeof(hm); hm.dwFromID = 0x600; hm.dwToID = local; hm.lpBuffer = "from the lobby"; hm.dwBufSize = 15;
    hr = lspisp->lpVtbl->HandleMessage(lspisp, &hm);
    CHECKHR(S_OK, hr, "HandleMessage");
    hr = receive(dp, &from, &to, &size);
    CHECKHR(S_OK, hr, "Receive the lobby's message");
    CHECK(from == 0x600 && to == local && size == 15 && !strcmp(rbuf, "from the lobby"), "message from 0x%lx to 0x%lx size %lu '%s'", from, to, size, rbuf);

    /* chat */
    memset(&chat, 0, sizeof(chat));
    chat.dwSize = sizeof(chat);
    chat.lpszMessage = (WCHAR *)L"chat from the lobby";
    memset(&cm, 0, sizeof(cm));
    cm.dwSize = sizeof(cm); cm.lpISP = lspisp; cm.dwFromID = 0x600; cm.dwToID = local; cm.lpChat = &chat;
    hr = lspisp->lpVtbl->SendChatMessage(lspisp, &cm);
    CHECKHR(S_OK, hr, "SendChatMessage");
    hr = receive(dp, &from, &to, &size);
    CHECKHR(S_OK, hr, "Receive the chat");
    {
        DPMSG_CHAT *msg = (DPMSG_CHAT *)rbuf;
        CHECK(from == DPID_SYSMSG && to == local && msg->dwType == DPSYS_CHAT && msg->idFromPlayer == 0x600 && msg->idToPlayer == local &&
              msg->lpChat && !strcmp(msg->lpChat->lpszMessageA, "chat from the lobby"), "chat message type 0x%lx text '%s'", msg->dwType,
              msg->lpChat && msg->lpChat->lpszMessageA ? msg->lpChat->lpszMessageA : "(none)");
    }
    cm.lpChat = NULL;
    hr = lspisp->lpVtbl->SendChatMessage(lspisp, &cm);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "SendChatMessage(no chat)");
    cm.lpChat = &chat; cm.dwToID = 0x7777;
    hr = lspisp->lpVtbl->SendChatMessage(lspisp, &cm);
    CHECKHR(DPERR_INVALIDPLAYER, hr, "SendChatMessage(unknown receiver)");

    /* the session description changed on the server */
    memset(&remote_sd, 0, sizeof(remote_sd));
    remote_sd.dwSize = sizeof(remote_sd);
    remote_sd.dwMaxPlayers = 9;
    remote_sd.lpszSessionName = (WCHAR *)L"renamed session";
    ssd.dwSize = sizeof(ssd); ssd.lpsd = &remote_sd;
    hr = lspisp->lpVtbl->SetSessionDesc(lspisp, &ssd);
    CHECKHR(S_OK, hr, "SetSessionDesc");
    size = sizeof(buf);
    hr = IDirectPlayX_GetSessionDesc(dp, buf, &size);
    CHECK(hr == S_OK && ((DPSESSIONDESC2 *)buf)->dwMaxPlayers == 9 && ((DPSESSIONDESC2 *)buf)->lpszSessionNameA && !strcmp(((DPSESSIONDESC2 *)buf)->lpszSessionNameA, "renamed session"),
          "session after SetSessionDesc: 0x%lx max %lu", (unsigned long)hr, ((DPSESSIONDESC2 *)buf)->dwMaxPlayers);
    remote_sd.dwSize = 8;
    hr = lspisp->lpVtbl->SetSessionDesc(lspisp, &ssd);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "SetSessionDesc(bad size)");

    /* the lobby starts the session: the players are told */
    drain(dp);
    memset(&conn_sd, 0, sizeof(conn_sd));
    conn_sd.dwSize = sizeof(conn_sd);
    conn_sd.guidApplication = SGSESSION_APPLICATION;
    conn_sd.dwMaxPlayers = 7;
    conn_sd.lpszSessionName = (WCHAR *)L"started";
    memset(&conn, 0, sizeof(conn));
    conn.dwSize = sizeof(conn);
    conn.dwFlags = DPLCONNECTION_JOINSESSION;
    conn.lpSessionDesc = &conn_sd;
    memset(&ssc, 0, sizeof(ssc));
    ssc.dwGroupID = 0; ssc.lpConn = &conn;
    hr = lspisp->lpVtbl->StartSession(lspisp, &ssc);
    CHECKHR(S_OK, hr, "StartSession");
    hr = receive(dp, &from, &to, &size);
    CHECKHR(S_OK, hr, "Receive DPSYS_STARTSESSION");
    {
        DPMSG_STARTSESSION *msg = (DPMSG_STARTSESSION *)rbuf;
        CHECK(from == DPID_SYSMSG && to == local && msg->dwType == DPSYS_STARTSESSION && msg->lpConn && msg->lpConn->dwSize == sizeof(DPLCONNECTION) &&
              msg->lpConn->dwFlags == DPLCONNECTION_JOINSESSION && msg->lpConn->lpSessionDesc &&
              IsEqualGUID(&msg->lpConn->lpSessionDesc->guidApplication, &SGSESSION_APPLICATION) && msg->lpConn->lpSessionDesc->dwMaxPlayers == 7 &&
              msg->lpConn->lpSessionDesc->lpszSessionNameA && !strcmp(msg->lpConn->lpSessionDesc->lpszSessionNameA, "started"),
              "STARTSESSION message type 0x%lx", msg->dwType);
    }
    ssc.dwGroupID = 0x7777;
    hr = lspisp->lpVtbl->StartSession(lspisp, &ssc);
    CHECKHR(DPERR_INVALIDGROUP, hr, "StartSession(unknown group)");
    ssc.lpConn = NULL; ssc.dwGroupID = 0;
    hr = lspisp->lpVtbl->StartSession(lspisp, &ssc);
    CHECKHR(DPERR_INVALIDPARAMS, hr, "StartSession(no connection)");

    /* removals */
    memset(&dpg, 0, sizeof(dpg));
    dpg.dwSize = sizeof(dpg); dpg.lpISP = lspisp; dpg.dwGroupID = 0x500; dpg.dwPlayerID = 0x600;
    hr = lspisp->lpVtbl->DeletePlayerFromGroup(lspisp, &dpg);
    CHECKHR(S_OK, hr, "DeletePlayerFromGroup");
    memset(&l, 0, sizeof(l));
    IDirectPlayX_EnumGroupPlayers(dp, 0x500, NULL, list_cb, &l, DPENUMPLAYERS_ALL);
    CHECK(l.count == 0, "EnumGroupPlayers after DeletePlayerFromGroup: %d", l.count);
    memset(&dgg, 0, sizeof(dgg));
    dgg.dwSize = sizeof(dgg); dgg.lpISP = lspisp; dgg.dwParentID = 0x500; dgg.dwGroupID = 0x501;
    hr = lspisp->lpVtbl->DeleteGroupFromGroup(lspisp, &dgg);
    CHECKHR(S_OK, hr, "DeleteGroupFromGroup");
    memset(&l, 0, sizeof(l));
    IDirectPlayX_EnumGroupsInGroup(dp, 0x500, NULL, list_cb, &l, DPENUMGROUPS_ALL);
    CHECK(l.count == 1 && !in_list(&l, 0x501, NULL), "EnumGroupsInGroup after DeleteGroupFromGroup: %d", l.count);
    memset(&dg, 0, sizeof(dg));
    dg.dwSize = sizeof(dg); dg.lpISP = lspisp; dg.dwGroupID = 0x500;
    hr = lspisp->lpVtbl->DestroyGroup(lspisp, &dg);
    CHECKHR(S_OK, hr, "DestroyGroup");
    hr = IDirectPlayX_GetGroupFlags(dp, 0x500, &size);
    CHECKHR(DPERR_INVALIDGROUP, hr, "the destroyed group is still there");
    hr = lspisp->lpVtbl->DestroyGroup(lspisp, &dg);
    CHECK(FAILED(hr), "DestroyGroup(again) succeeded");

    /* sessions the lobby knows of can be told: they are known to the enumerations */
    IDirectPlayX_Close(dp);
    {
        GUID instance = SGSESSION_INSTANCE;
        int n = 0;
        hr = IDirectPlayX_EnumPlayers(dp, &instance, list_cb, &n, DPENUMPLAYERS_SESSION);
        CHECKHR(DPERR_NOSESSIONS, hr, "EnumPlayers(session not known yet)");
        memset(&remote_sd, 0, sizeof(remote_sd));
        remote_sd.dwSize = sizeof(remote_sd);
        remote_sd.guidInstance = SGSESSION_INSTANCE;
        remote_sd.guidApplication = SGSESSION_APPLICATION;
        remote_sd.dwMaxPlayers = 3;
        remote_sd.lpszSessionName = (WCHAR *)L"lobby session";
        esr.dwSize = sizeof(esr); esr.lpsd = &remote_sd;
        hr = lspisp->lpVtbl->EnumSessionsResponse(lspisp, &esr);
        CHECKHR(S_OK, hr, "EnumSessionsResponse");
        hr = IDirectPlayX_EnumPlayers(dp, &instance, list_cb, &n, DPENUMPLAYERS_SESSION);
        CHECKHR(DPERR_GENERIC, hr, "EnumPlayers(session reported by the lobby)");
        esr.lpsd = NULL;
        hr = lspisp->lpVtbl->EnumSessionsResponse(lspisp, &esr);
        CHECKHR(DPERR_INVALIDPARAMS, hr, "EnumSessionsResponse(no session)");
    }
    IDirectPlayX_Release(dp);
}

int main(int argc, char **argv)
{
    char path[MAX_PATH];
    HMODULE spmod;
    IDirectPlay4 *dp;

    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);

    GetFullPathNameA("sgfakesp.dll", sizeof(path), path, NULL);
    spmod = LoadLibraryA(path);
    if (!spmod) { printf("FAIL  cannot load %s\n", path); return 1; }
    sp_getisp = (SGSP_GETISP)GetProcAddress(spmod, "SGSP_GetISP");
    lsp_getisp = (SGLSP_GETISP)GetProcAddress(spmod, "SGLSP_GetISP");

    /* the provider interface exists once an object has loaded the provider */
    dp = make_dp(&SGSP_GUID);
    spisp = sp_getisp();
    CHECK(spisp != NULL, "no service provider interface");
    if (spisp)
    {
        test_addresses();
        test_mru();
        test_player_flags();
    }
    IDirectPlayX_Release(dp);
    test_lobby_sp();

    printf("%d checks, %d failures\n", checks, failures);
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
