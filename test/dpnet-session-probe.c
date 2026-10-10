/* dpnet's FIXME stubs (patches/sg/2980), run by test/dpnet-session-gate.sh:
 * the Peer / Client / Server / LobbiedApplication / LobbyClient / ThreadPool
 * objects after Initialize: state kept (application description, local
 * player info, players, groups, contexts, async operations, caps, thread
 * counts, registered programs) and the DPNERR_* result per state. There is no
 * network, so a connect gets no response and a host has no remote players.
 *
 *   dpnet-session-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <objbase.h>
#include <dplay8.h>
#include <dplobby8.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[300]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)
#define EXPECT(call, want) do { HRESULT _hr = (call); CHECKF(_hr == (want), "%s -> %#lx (want %#lx)", #call, (unsigned long)_hr, (unsigned long)(want)); } while (0)

/* ----------------------------------------------------------- message log */
static struct { DWORD id; DPNID dpnid, dpnid2; void *ctx, *ctx2; HRESULT hr; DPNHANDLE handle; DWORD size; BYTE data[16]; } msgs[64];
static int nmsgs;
static CRITICAL_SECTION cs;

static HRESULT WINAPI handler(void *context, DWORD id, void *buf)
{
    int i;

    EnterCriticalSection(&cs);
    i = nmsgs < 64 ? nmsgs++ : 63;
    memset(&msgs[i], 0, sizeof(msgs[i]));
    msgs[i].id = id;
    switch (id)
    {
    case DPN_MSGID_CREATE_PLAYER: { DPNMSG_CREATE_PLAYER *m = buf; msgs[i].dpnid = m->dpnidPlayer; msgs[i].ctx = m->pvPlayerContext; break; }
    case DPN_MSGID_DESTROY_PLAYER: { DPNMSG_DESTROY_PLAYER *m = buf; msgs[i].dpnid = m->dpnidPlayer; msgs[i].ctx = m->pvPlayerContext; msgs[i].hr = m->dwReason; break; }
    case DPN_MSGID_CREATE_GROUP: { DPNMSG_CREATE_GROUP *m = buf; msgs[i].dpnid = m->dpnidGroup; msgs[i].dpnid2 = m->dpnidOwner; msgs[i].ctx = m->pvGroupContext; break; }
    case DPN_MSGID_DESTROY_GROUP: { DPNMSG_DESTROY_GROUP *m = buf; msgs[i].dpnid = m->dpnidGroup; msgs[i].ctx = m->pvGroupContext; break; }
    case DPN_MSGID_ADD_PLAYER_TO_GROUP: { DPNMSG_ADD_PLAYER_TO_GROUP *m = buf; msgs[i].dpnid = m->dpnidGroup; msgs[i].dpnid2 = m->dpnidPlayer; break; }
    case DPN_MSGID_REMOVE_PLAYER_FROM_GROUP: { DPNMSG_REMOVE_PLAYER_FROM_GROUP *m = buf; msgs[i].dpnid = m->dpnidGroup; msgs[i].dpnid2 = m->dpnidPlayer; break; }
    case DPN_MSGID_ASYNC_OP_COMPLETE: { DPNMSG_ASYNC_OP_COMPLETE *m = buf; msgs[i].handle = m->hAsyncOp; msgs[i].ctx = m->pvUserContext; msgs[i].hr = m->hResultCode; break; }
    case DPN_MSGID_CONNECT_COMPLETE: { DPNMSG_CONNECT_COMPLETE *m = buf; msgs[i].handle = m->hAsyncOp; msgs[i].ctx = m->pvUserContext; msgs[i].hr = m->hResultCode; break; }
    case DPN_MSGID_RECEIVE: { DPNMSG_RECEIVE *m = buf; msgs[i].dpnid = m->dpnidSender; msgs[i].size = m->dwReceiveDataSize; memcpy(msgs[i].data, m->pReceiveData, m->dwReceiveDataSize < 16 ? m->dwReceiveDataSize : 16); break; }
    case DPN_MSGID_SEND_COMPLETE: { DPNMSG_SEND_COMPLETE *m = buf; msgs[i].handle = m->hAsyncOp; msgs[i].ctx = m->pvUserContext; msgs[i].hr = m->hResultCode; break; }
    case DPN_MSGID_CREATE_THREAD: { DPNMSG_CREATE_THREAD *m = buf; msgs[i].size = m->dwSize; break; }
    case DPN_MSGID_DESTROY_THREAD: { DPNMSG_DESTROY_THREAD *m = buf; msgs[i].size = m->dwSize; break; }
    }
    LeaveCriticalSection(&cs);
    return S_OK;
}

static int count_msgs(DWORD id)
{
    int i, n = 0;
    EnterCriticalSection(&cs);
    for (i = 0; i < nmsgs && i < 64; i++) if (msgs[i].id == id) n++;
    LeaveCriticalSection(&cs);
    return n;
}
static int find_msg(DWORD id, int nth)
{
    int i;
    for (i = 0; i < nmsgs && i < 64; i++) if (msgs[i].id == id && !nth--) return i;
    return -1;
}
static void clear_msgs(void) { EnterCriticalSection(&cs); nmsgs = 0; LeaveCriticalSection(&cs); }
static BOOL wait_msg(DWORD id, int count)
{
    int i;
    for (i = 0; i < 100; i++) { if (count_msgs(id) >= count) return TRUE; Sleep(20); }
    return FALSE;
}

static IDirectPlay8Address *make_addr(const WCHAR *host)
{
    IDirectPlay8Address *a = NULL;
    CoCreateInstance(&CLSID_DirectPlay8Address, NULL, CLSCTX_ALL, &IID_IDirectPlay8Address, (void **)&a);
    IDirectPlay8Address_SetSP(a, &CLSID_DP8SP_TCPIP);
    if (host) IDirectPlay8Address_AddComponent(a, DPNA_KEY_HOSTNAME, host, (lstrlenW(host) + 1) * 2, DPNA_DATATYPE_STRING);
    return a;
}

static WCHAR sessname[] = L"probe session";
static WCHAR password[] = L"secret";
static BYTE appdata[] = {1, 2, 3, 4, 5};

/* ------------------------------------------------------------------ peer */
static void test_peer(void)
{
    IDirectPlay8Peer *peer;
    IDirectPlay8Address *dev = make_addr(NULL), *host = make_addr(L"127.0.0.1"), *addrs[2], *out;
    DPN_APPLICATION_DESC desc, *buf;
    DPN_PLAYER_INFO info, *pinfo;
    DPN_GROUP_INFO ginfo, *gbuf;
    DPN_CAPS caps;
    DPN_CAPS_EX capsex;
    DPN_CONNECTION_INFO cinfo;
    DPNID ids[8], self, group, group2;
    DPNHANDLE h = 0, h2 = 0;
    DWORD size, count, i;
    char big[256];
    BYTE rbuf[64];
    void *ctx;
    int m;
    HRESULT hr;
    DPN_BUFFER_DESC bd;

    hr = CoCreateInstance(&CLSID_DirectPlay8Peer, NULL, CLSCTX_ALL, &IID_IDirectPlay8Peer, (void **)&peer);
    check(hr == S_OK, "create peer");
    if (hr != S_OK) return;

    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.guidApplication.Data1 = 0x1234;
    desc.pwszSessionName = sessname;
    desc.dwMaxPlayers = 8;

    /* uninitialized */
    addrs[0] = dev;
    size = 0;
    EXPECT(IDirectPlay8Peer_Close(peer, 0), DPNERR_UNINITIALIZED);
    EXPECT(IDirectPlay8Peer_Host(peer, &desc, addrs, 1, NULL, NULL, NULL, 0), DPNERR_UNINITIALIZED);
    EXPECT(IDirectPlay8Peer_GetApplicationDesc(peer, NULL, &size, 0), DPNERR_UNINITIALIZED);
    EXPECT(IDirectPlay8Peer_GetCaps(peer, &caps, 0), DPNERR_UNINITIALIZED);
    EXPECT(IDirectPlay8Peer_CancelAsyncOperation(peer, 1, 0), DPNERR_UNINITIALIZED);
    EXPECT(IDirectPlay8Peer_GetPlayerContext(peer, 1, &ctx, 0), DPNERR_UNINITIALIZED);
    EXPECT(IDirectPlay8Peer_EnumPlayersAndGroups(peer, ids, &count, DPNENUM_PLAYERS), DPNERR_UNINITIALIZED);

    EXPECT(IDirectPlay8Peer_Initialize(peer, (void *)0x77, handler, 0), S_OK);

    /* idle: no session */
    size = 0;
    EXPECT(IDirectPlay8Peer_GetApplicationDesc(peer, NULL, &size, 0), DPNERR_NOCONNECTION);
    EXPECT(IDirectPlay8Peer_GetApplicationDesc(peer, NULL, NULL, 0), DPNERR_INVALIDPOINTER);
    EXPECT(IDirectPlay8Peer_SetApplicationDesc(peer, &desc, 0), DPNERR_NOCONNECTION);
    EXPECT(IDirectPlay8Peer_SendTo(peer, 0, NULL, 0, 0, NULL, NULL, 0), DPNERR_INVALIDPARAM);
    bd.dwBufferSize = 4; bd.pBufferData = (BYTE *)"abcd";
    EXPECT(IDirectPlay8Peer_SendTo(peer, 0, &bd, 1, 0, NULL, NULL, DPNSEND_SYNC), DPNERR_NOCONNECTION);
    EXPECT(IDirectPlay8Peer_CreateGroup(peer, NULL, NULL, NULL, NULL, DPNCREATEGROUP_SYNC), DPNERR_INVALIDPOINTER);
    EXPECT(IDirectPlay8Peer_TerminateSession(peer, NULL, 0, 0), DPNERR_NOCONNECTION);
    EXPECT(IDirectPlay8Peer_GetLocalHostAddresses(peer, &out, NULL, 0), DPNERR_INVALIDPOINTER);
    count = 1;
    EXPECT(IDirectPlay8Peer_GetLocalHostAddresses(peer, &out, &count, 0), DPNERR_NOCONNECTION);
    EXPECT(IDirectPlay8Peer_EnumPlayersAndGroups(peer, ids, &count, DPNENUM_PLAYERS), DPNERR_NOCONNECTION);
    EXPECT(IDirectPlay8Peer_GetPlayerContext(peer, 5, &ctx, 0), DPNERR_INVALIDPLAYER);
    EXPECT(IDirectPlay8Peer_GetPlayerContext(peer, 5, NULL, 0), DPNERR_INVALIDPOINTER);
    EXPECT(IDirectPlay8Peer_GetGroupContext(peer, 5, &ctx, 0), DPNERR_INVALIDGROUP);
    EXPECT(IDirectPlay8Peer_ReturnBuffer(peer, 5, 0), DPNERR_INVALIDHANDLE);
    EXPECT(IDirectPlay8Peer_ReturnBuffer(peer, 5, 3), DPNERR_INVALIDFLAGS);

    /* caps */
    memset(&caps, 0, sizeof(caps));
    EXPECT(IDirectPlay8Peer_GetCaps(peer, &caps, 0), DPNERR_INVALIDPARAM);
    EXPECT(IDirectPlay8Peer_GetCaps(peer, NULL, 0), DPNERR_INVALIDPOINTER);
    caps.dwSize = sizeof(caps);
    EXPECT(IDirectPlay8Peer_GetCaps(peer, &caps, 0), S_OK);
    CHECKF(caps.dwConnectTimeout == 200 && caps.dwConnectRetries == 14 && caps.dwTimeoutUntilKeepAlive == 60000,
           "default caps %lu %lu %lu", caps.dwConnectTimeout, caps.dwConnectRetries, caps.dwTimeoutUntilKeepAlive);
    caps.dwConnectTimeout = 555; caps.dwConnectRetries = 3; caps.dwTimeoutUntilKeepAlive = 4444;
    EXPECT(IDirectPlay8Peer_SetCaps(peer, &caps, 0), S_OK);
    memset(&caps, 0, sizeof(caps)); caps.dwSize = sizeof(caps);
    EXPECT(IDirectPlay8Peer_GetCaps(peer, &caps, 0), S_OK);
    CHECKF(caps.dwConnectTimeout == 555 && caps.dwConnectRetries == 3 && caps.dwTimeoutUntilKeepAlive == 4444,
           "caps round trip %lu %lu %lu", caps.dwConnectTimeout, caps.dwConnectRetries, caps.dwTimeoutUntilKeepAlive);
    memset(&capsex, 0, sizeof(capsex)); capsex.dwSize = sizeof(capsex);
    EXPECT(IDirectPlay8Peer_GetCaps(peer, (DPN_CAPS *)&capsex, 0), S_OK);
    CHECKF(capsex.dwConnectTimeout == 555 && capsex.dwMaxRecvMsgSize == 65536, "extended caps %lu %lu", capsex.dwConnectTimeout, capsex.dwMaxRecvMsgSize);
    capsex.dwMaxRecvMsgSize = 4096;
    EXPECT(IDirectPlay8Peer_SetCaps(peer, (DPN_CAPS *)&capsex, 0), S_OK);
    capsex.dwMaxRecvMsgSize = 0;
    EXPECT(IDirectPlay8Peer_GetCaps(peer, (DPN_CAPS *)&capsex, 0), S_OK);
    CHECKF(capsex.dwMaxRecvMsgSize == 4096, "extended caps stored %lu", capsex.dwMaxRecvMsgSize);
    caps.dwSize = 7;
    EXPECT(IDirectPlay8Peer_SetCaps(peer, &caps, 0), DPNERR_INVALIDPARAM);
    caps.dwSize = sizeof(caps);
    EXPECT(IDirectPlay8Peer_SetCaps(peer, &caps, 1), DPNERR_INVALIDFLAGS);

    /* local info before hosting */
    memset(&info, 0, sizeof(info));
    info.dwSize = sizeof(info);
    info.dwInfoFlags = DPNINFO_NAME | DPNINFO_DATA;
    info.pwszName = L"peerone";
    info.pvData = appdata; info.dwDataSize = sizeof(appdata);
    EXPECT(IDirectPlay8Peer_SetPeerInfo(peer, &info, NULL, NULL, DPNSETPEERINFO_SYNC), S_OK);
    info.dwSize = 3;
    EXPECT(IDirectPlay8Peer_SetPeerInfo(peer, &info, NULL, NULL, DPNSETPEERINFO_SYNC), DPNERR_INVALIDPARAM);
    info.dwSize = sizeof(info);
    EXPECT(IDirectPlay8Peer_SetPeerInfo(peer, &info, NULL, &h, DPNSETPEERINFO_SYNC), DPNERR_INVALIDPARAM);
    EXPECT(IDirectPlay8Peer_SetPeerInfo(peer, &info, NULL, NULL, 0x10), DPNERR_INVALIDFLAGS);

    /* async set info: pending + completion message */
    clear_msgs();
    h = 0;
    EXPECT(IDirectPlay8Peer_SetPeerInfo(peer, &info, (void *)0x99, &h, 0), DPNSUCCESS_PENDING);
    CHECKF(h != 0, "async SetPeerInfo gave a handle");
    check(wait_msg(DPN_MSGID_ASYNC_OP_COMPLETE, 1), "async SetPeerInfo completes");
    m = find_msg(DPN_MSGID_ASYNC_OP_COMPLETE, 0);
    CHECKF(m >= 0 && msgs[m].handle == h && msgs[m].ctx == (void *)0x99 && msgs[m].hr == S_OK, "completion carries handle/context/S_OK");

    /* connect */
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.guidApplication.Data1 = 0x1234;
    EXPECT(IDirectPlay8Peer_Connect(peer, NULL, host, dev, NULL, NULL, NULL, 0, NULL, NULL, NULL, DPNCONNECT_SYNC), DPNERR_INVALIDPARAM);
    EXPECT(IDirectPlay8Peer_Connect(peer, &desc, NULL, dev, NULL, NULL, NULL, 0, NULL, NULL, NULL, DPNCONNECT_SYNC), DPNERR_INVALIDPARAM);
    EXPECT(IDirectPlay8Peer_Connect(peer, &desc, host, dev, NULL, NULL, NULL, 0, NULL, NULL, &h, DPNCONNECT_SYNC), DPNERR_INVALIDPARAM);
    EXPECT(IDirectPlay8Peer_Connect(peer, &desc, host, dev, NULL, NULL, NULL, 0, NULL, NULL, NULL, 0x4), DPNERR_INVALIDFLAGS);
    EXPECT(IDirectPlay8Peer_Connect(peer, &desc, dev, dev, NULL, NULL, NULL, 0, NULL, NULL, NULL, DPNCONNECT_SYNC), DPNERR_INCOMPLETEADDRESS);
    EXPECT(IDirectPlay8Peer_Connect(peer, &desc, host, dev, NULL, NULL, NULL, 0, NULL, NULL, NULL, DPNCONNECT_SYNC), DPNERR_NORESPONSE);
    clear_msgs();
    h = 0;
    EXPECT(IDirectPlay8Peer_Connect(peer, &desc, host, dev, NULL, NULL, NULL, 0, NULL, (void *)0x55, &h, 0), DPNSUCCESS_PENDING);
    EXPECT(IDirectPlay8Peer_Connect(peer, &desc, host, dev, NULL, NULL, NULL, 0, NULL, NULL, NULL, DPNCONNECT_SYNC), DPNERR_CONNECTING);
    EXPECT(IDirectPlay8Peer_Host(peer, &desc, addrs, 1, NULL, NULL, NULL, 0), DPNERR_CONNECTING);
    check(wait_msg(DPN_MSGID_CONNECT_COMPLETE, 1), "async Connect completes");
    m = find_msg(DPN_MSGID_CONNECT_COMPLETE, 0);
    CHECKF(m >= 0 && msgs[m].handle == h && msgs[m].ctx == (void *)0x55 && msgs[m].hr == DPNERR_NORESPONSE, "connect completion NORESPONSE with handle/context");
    h = 0;
    EXPECT(IDirectPlay8Peer_Connect(peer, &desc, host, dev, NULL, NULL, NULL, 0, NULL, (void *)0x56, &h, 0), DPNSUCCESS_PENDING);
    clear_msgs();
    EXPECT(IDirectPlay8Peer_CancelAsyncOperation(peer, h, 0), S_OK);
    m = find_msg(DPN_MSGID_CONNECT_COMPLETE, 0);
    CHECKF(m >= 0 && msgs[m].handle == h && msgs[m].hr == DPNERR_USERCANCEL, "cancelled connect completes with USERCANCEL");

    /* enum hosts + cancel */
    desc.dwSize = sizeof(desc);
    EXPECT(IDirectPlay8Peer_EnumHosts(peer, NULL, host, dev, NULL, 0, 1, 0, 1, NULL, &h, 0), DPNERR_INVALIDPARAM);
    EXPECT(IDirectPlay8Peer_EnumHosts(peer, &desc, host, dev, NULL, 0, 1, 0, 1, NULL, NULL, 0x4000), DPNERR_INVALIDFLAGS);
    h = h2 = 0;
    EXPECT(IDirectPlay8Peer_EnumHosts(peer, &desc, host, dev, NULL, 0, INFINITE, 0, INFINITE, (void *)0x31, &h, 0), DPNSUCCESS_PENDING);
    EXPECT(IDirectPlay8Peer_EnumHosts(peer, &desc, host, dev, NULL, 0, INFINITE, 0, INFINITE, (void *)0x32, &h2, 0), DPNSUCCESS_PENDING);
    CHECKF(h && h2 && h != h2, "distinct enum handles %lx %lx", h, h2);
    EXPECT(IDirectPlay8Peer_CancelAsyncOperation(peer, 0, 0), DPNERR_INVALIDHANDLE);
    EXPECT(IDirectPlay8Peer_CancelAsyncOperation(peer, h, 1), DPNERR_INVALIDFLAGS);
    EXPECT(IDirectPlay8Peer_CancelAsyncOperation(peer, 0x7fff0000, 0), DPNERR_INVALIDHANDLE);
    clear_msgs();
    EXPECT(IDirectPlay8Peer_CancelAsyncOperation(peer, h, 0), S_OK);
    m = find_msg(DPN_MSGID_ASYNC_OP_COMPLETE, 0);
    CHECKF(m >= 0 && msgs[m].handle == h && msgs[m].ctx == (void *)0x31 && msgs[m].hr == DPNERR_USERCANCEL, "enum cancel -> USERCANCEL completion");
    EXPECT(IDirectPlay8Peer_CancelAsyncOperation(peer, h, 0), DPNERR_INVALIDHANDLE);
    clear_msgs();
    EXPECT(IDirectPlay8Peer_CancelAsyncOperation(peer, 0, DPNCANCEL_ENUM), S_OK);
    m = find_msg(DPN_MSGID_ASYNC_OP_COMPLETE, 0);
    CHECKF(m >= 0 && msgs[m].handle == h2 && msgs[m].hr == DPNERR_USERCANCEL, "cancel-all-enums cancels the second");
    EXPECT(IDirectPlay8Peer_CancelAsyncOperation(peer, 0, 0x100), DPNERR_INVALIDFLAGS);

    /* host */
    desc.pwszSessionName = sessname;
    desc.pwszPassword = password;
    desc.dwMaxPlayers = 8;
    desc.dwFlags = DPNSESSION_REQUIREPASSWORD;
    desc.pvApplicationReservedData = appdata;
    desc.dwApplicationReservedDataSize = sizeof(appdata);
    EXPECT(IDirectPlay8Peer_Host(peer, NULL, addrs, 1, NULL, NULL, NULL, 0), DPNERR_INVALIDPARAM);
    EXPECT(IDirectPlay8Peer_Host(peer, &desc, NULL, 1, NULL, NULL, NULL, 0), DPNERR_INVALIDPARAM);
    EXPECT(IDirectPlay8Peer_Host(peer, &desc, addrs, 0, NULL, NULL, NULL, 0), DPNERR_INVALIDPARAM);
    EXPECT(IDirectPlay8Peer_Host(peer, &desc, addrs, 1, NULL, NULL, NULL, 8), DPNERR_INVALIDFLAGS);
    desc.dwFlags = 0x8000;
    EXPECT(IDirectPlay8Peer_Host(peer, &desc, addrs, 1, NULL, NULL, NULL, 0), DPNERR_INVALIDFLAGS);
    desc.dwFlags = DPNSESSION_REQUIREPASSWORD;
    desc.dwSize = 5;
    EXPECT(IDirectPlay8Peer_Host(peer, &desc, addrs, 1, NULL, NULL, NULL, 0), DPNERR_INVALIDPARAM);
    desc.dwSize = sizeof(desc);
    addrs[0] = (IDirectPlay8Address *)NULL;
    EXPECT(IDirectPlay8Peer_Host(peer, &desc, addrs, 1, NULL, NULL, NULL, 0), DPNERR_INVALIDDEVICEADDRESS);
    addrs[0] = dev;
    clear_msgs();
    EXPECT(IDirectPlay8Peer_Host(peer, &desc, addrs, 1, NULL, NULL, (void *)0xabc, 0), S_OK);
    EXPECT(IDirectPlay8Peer_Host(peer, &desc, addrs, 1, NULL, NULL, NULL, 0), DPNERR_HOSTING);
    EXPECT(IDirectPlay8Peer_Connect(peer, &desc, host, dev, NULL, NULL, NULL, 0, NULL, NULL, NULL, DPNCONNECT_SYNC), DPNERR_HOSTING);
    m = find_msg(DPN_MSGID_CREATE_PLAYER, 0);
    CHECKF(m >= 0, "Host sends CREATE_PLAYER");
    self = m >= 0 ? msgs[m].dpnid : 0;
    CHECKF(self != 0 && m >= 0 && msgs[m].ctx == (void *)0xabc, "local player id %#lx with the host's player context", self);

    /* application description */
    size = 0;
    EXPECT(IDirectPlay8Peer_GetApplicationDesc(peer, NULL, &size, 0), DPNERR_BUFFERTOOSMALL);
    CHECKF(size == sizeof(DPN_APPLICATION_DESC) + sizeof(sessname) + sizeof(password) + sizeof(appdata), "GetApplicationDesc size %lu", size);
    buf = calloc(1, size + 16);
    buf->dwSize = sizeof(*buf);
    size -= 1;
    EXPECT(IDirectPlay8Peer_GetApplicationDesc(peer, buf, &size, 0), DPNERR_BUFFERTOOSMALL);
    size += 1;
    EXPECT(IDirectPlay8Peer_GetApplicationDesc(peer, buf, &size, 0), S_OK);
    CHECKF(!lstrcmpW(buf->pwszSessionName, L"probe session") && !lstrcmpW(buf->pwszPassword, L"secret"), "names copied");
    CHECKF(buf->dwMaxPlayers == 8 && buf->dwCurrentPlayers == 1 && buf->guidApplication.Data1 == 0x1234, "max %lu current %lu app %#lx",
           buf->dwMaxPlayers, buf->dwCurrentPlayers, buf->guidApplication.Data1);
    CHECKF(buf->dwApplicationReservedDataSize == 5 && !memcmp(buf->pvApplicationReservedData, appdata, 5), "application data copied");
    CHECKF((BYTE *)buf->pwszSessionName >= (BYTE *)(buf + 1), "strings live inside the buffer");
    CHECKF(buf->guidInstance.Data1 || buf->guidInstance.Data2 || buf->guidInstance.Data3, "instance GUID generated");
    EXPECT(IDirectPlay8Peer_GetApplicationDesc(peer, buf, &size, 1), DPNERR_INVALIDFLAGS);
    desc.dwMaxPlayers = 12;
    desc.pwszSessionName = L"renamed";
    EXPECT(IDirectPlay8Peer_SetApplicationDesc(peer, &desc, 0), S_OK);
    size = 1000;
    EXPECT(IDirectPlay8Peer_GetApplicationDesc(peer, buf, &size, 0), S_OK);
    CHECKF(buf->dwMaxPlayers == 12 && !lstrcmpW(buf->pwszSessionName, L"renamed"), "SetApplicationDesc applied (%lu)", buf->dwMaxPlayers);
    free(buf);
    desc.dwSize = 2;
    EXPECT(IDirectPlay8Peer_SetApplicationDesc(peer, &desc, 0), DPNERR_INVALIDPARAM);
    EXPECT(IDirectPlay8Peer_SetApplicationDesc(peer, NULL, 0), DPNERR_INVALIDPARAM);
    desc.dwSize = sizeof(desc);

    /* local addresses and peer info of the local player */
    count = 0;
    EXPECT(IDirectPlay8Peer_GetLocalHostAddresses(peer, &out, &count, 0), DPNERR_BUFFERTOOSMALL);
    CHECKF(count == 1, "one local address (%lu)", count);
    EXPECT(IDirectPlay8Peer_GetLocalHostAddresses(peer, &out, &count, 0), S_OK);
    { GUID sp; CHECKF(IDirectPlay8Address_GetSP(out, &sp) == S_OK && IsEqualGUID(&sp, &CLSID_DP8SP_TCPIP), "local address has the SP"); IDirectPlay8Address_Release(out); }
    out = NULL;
    EXPECT(IDirectPlay8Peer_GetPeerAddress(peer, self, &out, 0), S_OK);
    if (out) IDirectPlay8Address_Release(out);
    EXPECT(IDirectPlay8Peer_GetPeerAddress(peer, 12345, &out, 0), DPNERR_INVALIDPLAYER);

    size = 0;
    EXPECT(IDirectPlay8Peer_GetPeerInfo(peer, self, NULL, &size, 0), DPNERR_BUFFERTOOSMALL);
    CHECKF(size == sizeof(DPN_PLAYER_INFO) + sizeof(L"peerone") + sizeof(appdata), "GetPeerInfo size %lu", size);
    pinfo = calloc(1, size);
    pinfo->dwSize = sizeof(*pinfo);
    EXPECT(IDirectPlay8Peer_GetPeerInfo(peer, self, pinfo, &size, 0), S_OK);
    CHECKF(pinfo->dwInfoFlags == (DPNINFO_NAME | DPNINFO_DATA) && !lstrcmpW(pinfo->pwszName, L"peerone") &&
           pinfo->dwDataSize == 5 && !memcmp(pinfo->pvData, appdata, 5), "peer info round trip (%#lx)", pinfo->dwInfoFlags);
    CHECKF((pinfo->dwPlayerFlags & (DPNPLAYER_LOCAL | DPNPLAYER_HOST)) == (DPNPLAYER_LOCAL | DPNPLAYER_HOST), "player flags %#lx", pinfo->dwPlayerFlags);
    free(pinfo);
    EXPECT(IDirectPlay8Peer_GetPeerInfo(peer, 99, NULL, &size, 0), DPNERR_INVALIDPLAYER);
    EXPECT(IDirectPlay8Peer_GetPlayerContext(peer, self, &ctx, 0), S_OK);
    CHECKF(ctx == (void *)0xabc, "player context %p", ctx);
    EXPECT(IDirectPlay8Peer_GetPlayerContext(peer, self, &ctx, 4), DPNERR_INVALIDFLAGS);
    memset(&cinfo, 0, sizeof(cinfo));
    cinfo.dwSize = sizeof(cinfo);
    EXPECT(IDirectPlay8Peer_GetConnectionInfo(peer, 99, &cinfo, 0), DPNERR_INVALIDPLAYER);
    EXPECT(IDirectPlay8Peer_GetConnectionInfo(peer, 99, NULL, 0), DPNERR_INVALIDPOINTER);
    cinfo.dwSize = 4;
    EXPECT(IDirectPlay8Peer_GetConnectionInfo(peer, 99, &cinfo, 0), DPNERR_INVALIDPARAM);
    EXPECT(IDirectPlay8Peer_DestroyPeer(peer, 99, NULL, 0, 0), DPNERR_INVALIDPLAYER);
    EXPECT(IDirectPlay8Peer_DestroyPeer(peer, self, NULL, 0, 0), DPNERR_INVALIDPLAYER);
    EXPECT(IDirectPlay8Peer_TerminateSession(peer, NULL, 0, 0), S_OK);
    EXPECT(IDirectPlay8Peer_TerminateSession(peer, NULL, 5, 0), DPNERR_INVALIDPOINTER);

    /* send queue */
    count = 7; size = 7;
    EXPECT(IDirectPlay8Peer_GetSendQueueInfo(peer, self, &count, &size, 0), S_OK);
    CHECKF(count == 0 && size == 0, "empty send queue (%lu/%lu)", count, size);
    EXPECT(IDirectPlay8Peer_GetSendQueueInfo(peer, 99, &count, &size, 0), DPNERR_INVALIDPLAYER);
    EXPECT(IDirectPlay8Peer_GetSendQueueInfo(peer, self, &count, &size, 0x40), DPNERR_INVALIDFLAGS);

    /* groups */
    memset(&ginfo, 0, sizeof(ginfo));
    ginfo.dwSize = sizeof(ginfo);
    ginfo.dwInfoFlags = DPNINFO_NAME | DPNINFO_DATA;
    ginfo.pwszName = L"team";
    ginfo.pvData = appdata; ginfo.dwDataSize = 3;
    ginfo.dwGroupFlags = DPNGROUP_AUTODESTRUCT;
    clear_msgs();
    EXPECT(IDirectPlay8Peer_CreateGroup(peer, &ginfo, (void *)0x501, NULL, NULL, DPNCREATEGROUP_SYNC), S_OK);
    m = find_msg(DPN_MSGID_CREATE_GROUP, 0);
    group = m >= 0 ? msgs[m].dpnid : 0;
    CHECKF(m >= 0 && group && msgs[m].ctx == (void *)0x501 && msgs[m].dpnid2 == self, "CREATE_GROUP id %#lx ctx %p owner %#lx", group, m >= 0 ? msgs[m].ctx : 0, m >= 0 ? msgs[m].dpnid2 : 0);
    ginfo.dwGroupFlags = 0x10;
    EXPECT(IDirectPlay8Peer_CreateGroup(peer, &ginfo, NULL, NULL, NULL, DPNCREATEGROUP_SYNC), DPNERR_INVALIDFLAGS);
    ginfo.dwGroupFlags = 0;
    h = 0;
    clear_msgs();
    EXPECT(IDirectPlay8Peer_CreateGroup(peer, &ginfo, (void *)0x502, (void *)0x77, &h, 0), DPNSUCCESS_PENDING);
    check(wait_msg(DPN_MSGID_ASYNC_OP_COMPLETE, 1), "async CreateGroup completes");
    m = find_msg(DPN_MSGID_ASYNC_OP_COMPLETE, 0);
    CHECKF(m >= 0 && msgs[m].handle == h && msgs[m].ctx == (void *)0x77 && msgs[m].hr == S_OK, "CreateGroup completion");
    m = find_msg(DPN_MSGID_CREATE_GROUP, 0);
    group2 = m >= 0 ? msgs[m].dpnid : 0;
    CHECKF(group2 && group2 != group, "second group id");
    EXPECT(IDirectPlay8Peer_GetGroupContext(peer, group, &ctx, 0), S_OK);
    CHECKF(ctx == (void *)0x501, "group context %p", ctx);
    EXPECT(IDirectPlay8Peer_GetGroupContext(peer, group, &ctx, 2), DPNERR_INVALIDFLAGS);

    size = 0;
    EXPECT(IDirectPlay8Peer_GetGroupInfo(peer, group, NULL, &size, 0), DPNERR_BUFFERTOOSMALL);
    CHECKF(size == sizeof(DPN_GROUP_INFO) + sizeof(L"team") + 3, "group info size %lu", size);
    gbuf = calloc(1, size);
    gbuf->dwSize = sizeof(*gbuf);
    EXPECT(IDirectPlay8Peer_GetGroupInfo(peer, group, gbuf, &size, 0), S_OK);
    CHECKF(!lstrcmpW(gbuf->pwszName, L"team") && gbuf->dwDataSize == 3 && gbuf->dwGroupFlags == DPNGROUP_AUTODESTRUCT, "group info round trip (flags %#lx)", gbuf->dwGroupFlags);
    free(gbuf);
    EXPECT(IDirectPlay8Peer_GetGroupInfo(peer, 99, NULL, &size, 0), DPNERR_INVALIDGROUP);
    ginfo.dwInfoFlags = DPNINFO_NAME;
    ginfo.pwszName = L"renamed team";
    EXPECT(IDirectPlay8Peer_SetGroupInfo(peer, group, &ginfo, NULL, NULL, DPNSETGROUPINFO_SYNC), S_OK);
    EXPECT(IDirectPlay8Peer_SetGroupInfo(peer, 99, &ginfo, NULL, NULL, DPNSETGROUPINFO_SYNC), DPNERR_INVALIDGROUP);
    size = 0;
    EXPECT(IDirectPlay8Peer_GetGroupInfo(peer, group, NULL, &size, 0), DPNERR_BUFFERTOOSMALL);
    CHECKF(size == sizeof(DPN_GROUP_INFO) + sizeof(L"renamed team") + 3, "renamed group info size %lu", size);

    count = 0;
    EXPECT(IDirectPlay8Peer_EnumPlayersAndGroups(peer, NULL, &count, 0), DPNERR_INVALIDFLAGS);
    EXPECT(IDirectPlay8Peer_EnumPlayersAndGroups(peer, NULL, &count, DPNENUM_PLAYERS | DPNENUM_GROUPS), DPNERR_BUFFERTOOSMALL);
    CHECKF(count == 3, "players+groups count %lu", count);
    count = 8;
    EXPECT(IDirectPlay8Peer_EnumPlayersAndGroups(peer, ids, &count, DPNENUM_PLAYERS), S_OK);
    CHECKF(count == 1 && ids[0] == self, "players: %lu", count);
    count = 8;
    EXPECT(IDirectPlay8Peer_EnumPlayersAndGroups(peer, ids, &count, DPNENUM_GROUPS), S_OK);
    CHECKF(count == 2 && ids[0] == group && ids[1] == group2, "groups: %lu", count);

    clear_msgs();
    EXPECT(IDirectPlay8Peer_AddPlayerToGroup(peer, group, self, NULL, NULL, DPNADDPLAYERTOGROUP_SYNC), S_OK);
    m = find_msg(DPN_MSGID_ADD_PLAYER_TO_GROUP, 0);
    CHECKF(m >= 0 && msgs[m].dpnid == group && msgs[m].dpnid2 == self, "ADD_PLAYER_TO_GROUP message");
    EXPECT(IDirectPlay8Peer_AddPlayerToGroup(peer, group, self, NULL, NULL, DPNADDPLAYERTOGROUP_SYNC), DPNERR_PLAYERALREADYINGROUP);
    EXPECT(IDirectPlay8Peer_AddPlayerToGroup(peer, 99, self, NULL, NULL, DPNADDPLAYERTOGROUP_SYNC), DPNERR_INVALIDGROUP);
    EXPECT(IDirectPlay8Peer_AddPlayerToGroup(peer, group, 99, NULL, NULL, DPNADDPLAYERTOGROUP_SYNC), DPNERR_INVALIDPLAYER);
    EXPECT(IDirectPlay8Peer_AddPlayerToGroup(peer, 0, self, NULL, NULL, DPNADDPLAYERTOGROUP_SYNC), DPNERR_INVALIDGROUP);
    EXPECT(IDirectPlay8Peer_RemovePlayerFromGroup(peer, group2, self, NULL, NULL, DPNREMOVEPLAYERFROMGROUP_SYNC), DPNERR_PLAYERNOTINGROUP);
    count = 0;
    EXPECT(IDirectPlay8Peer_EnumGroupMembers(peer, group, NULL, &count, 0), DPNERR_BUFFERTOOSMALL);
    CHECKF(count == 1, "one member (%lu)", count);
    EXPECT(IDirectPlay8Peer_EnumGroupMembers(peer, group, ids, &count, 0), S_OK);
    CHECKF(ids[0] == self, "member is the local player");
    count = 8;
    EXPECT(IDirectPlay8Peer_EnumGroupMembers(peer, group2, ids, &count, 0), S_OK);
    CHECKF(count == 0, "empty group lists no members (%lu)", count);
    count = 8;
    EXPECT(IDirectPlay8Peer_EnumGroupMembers(peer, DPNID_ALL_PLAYERS_GROUP, ids, &count, 0), S_OK);
    CHECKF(count == 1 && ids[0] == self, "all-players group lists the player (%lu)", count);
    EXPECT(IDirectPlay8Peer_EnumGroupMembers(peer, 99, ids, &count, 0), DPNERR_INVALIDGROUP);

    /* loopback send to self and via the group */
    clear_msgs();
    EXPECT(IDirectPlay8Peer_SendTo(peer, self, &bd, 1, 0, NULL, NULL, DPNSEND_SYNC), S_OK);
    m = find_msg(DPN_MSGID_RECEIVE, 0);
    CHECKF(m >= 0 && msgs[m].dpnid == self && msgs[m].size == 4 && !memcmp(msgs[m].data, "abcd", 4), "loopback receive");
    clear_msgs();
    EXPECT(IDirectPlay8Peer_SendTo(peer, self, &bd, 1, 0, NULL, NULL, DPNSEND_SYNC | DPNSEND_NOLOOPBACK), S_OK);
    CHECKF(count_msgs(DPN_MSGID_RECEIVE) == 0, "NOLOOPBACK suppresses the receive");
    clear_msgs();
    EXPECT(IDirectPlay8Peer_SendTo(peer, group, &bd, 1, 0, NULL, NULL, DPNSEND_SYNC), S_OK);
    CHECKF(count_msgs(DPN_MSGID_RECEIVE) == 1, "group containing the sender receives");
    clear_msgs();
    EXPECT(IDirectPlay8Peer_SendTo(peer, group2, &bd, 1, 0, NULL, NULL, DPNSEND_SYNC), S_OK);
    CHECKF(count_msgs(DPN_MSGID_RECEIVE) == 0, "empty group receives nothing");
    clear_msgs();
    h = 0;
    EXPECT(IDirectPlay8Peer_SendTo(peer, self, &bd, 1, 0, (void *)0x44, &h, 0), DPNSUCCESS_PENDING);
    check(wait_msg(DPN_MSGID_SEND_COMPLETE, 1), "async send completes");
    m = find_msg(DPN_MSGID_SEND_COMPLETE, 0);
    CHECKF(m >= 0 && msgs[m].handle == h && msgs[m].ctx == (void *)0x44 && msgs[m].hr == S_OK, "send completion");
    EXPECT(IDirectPlay8Peer_SendTo(peer, 99, &bd, 1, 0, NULL, NULL, DPNSEND_SYNC), DPNERR_INVALIDPLAYER);
    EXPECT(IDirectPlay8Peer_SendTo(peer, self, &bd, 1, 0, NULL, NULL, DPNSEND_PRIORITY_LOW | DPNSEND_PRIORITY_HIGH), DPNERR_INVALIDFLAGS);
    EXPECT(IDirectPlay8Peer_SendTo(peer, self, &bd, 1, 0, NULL, &h, DPNSEND_SYNC), DPNERR_INVALIDPARAM);
    (void)rbuf; (void)big; (void)i;

    clear_msgs();
    EXPECT(IDirectPlay8Peer_RemovePlayerFromGroup(peer, group, self, NULL, NULL, DPNREMOVEPLAYERFROMGROUP_SYNC), S_OK);
    m = find_msg(DPN_MSGID_REMOVE_PLAYER_FROM_GROUP, 0);
    CHECKF(m >= 0 && msgs[m].dpnid == group && msgs[m].dpnid2 == self, "REMOVE_PLAYER_FROM_GROUP message");
    clear_msgs();
    EXPECT(IDirectPlay8Peer_DestroyGroup(peer, group2, NULL, NULL, DPNDESTROYGROUP_SYNC), S_OK);
    m = find_msg(DPN_MSGID_DESTROY_GROUP, 0);
    CHECKF(m >= 0 && msgs[m].dpnid == group2 && msgs[m].ctx == (void *)0x502, "DESTROY_GROUP message");
    EXPECT(IDirectPlay8Peer_DestroyGroup(peer, group2, NULL, NULL, DPNDESTROYGROUP_SYNC), DPNERR_INVALIDGROUP);
    EXPECT(IDirectPlay8Peer_GetGroupContext(peer, group2, &ctx, 0), DPNERR_INVALIDGROUP);

    /* register lobby */
    EXPECT(IDirectPlay8Peer_RegisterLobby(peer, 1, NULL, 0), DPNERR_INVALIDFLAGS);
    EXPECT(IDirectPlay8Peer_RegisterLobby(peer, 1, NULL, DPNLOBBY_UNREGISTER), DPNERR_NOTREGISTERED);
    EXPECT(IDirectPlay8Peer_RegisterLobby(peer, 1, NULL, DPNLOBBY_REGISTER), DPNERR_INVALIDPARAM);

    /* close: everything goes away, messages for the player and the group */
    clear_msgs();
    EXPECT(IDirectPlay8Peer_Close(peer, 4), DPNERR_INVALIDFLAGS);
    EXPECT(IDirectPlay8Peer_Close(peer, 0), S_OK);
    m = find_msg(DPN_MSGID_DESTROY_GROUP, 0);
    CHECKF(m >= 0 && msgs[m].dpnid == group, "Close destroys the remaining group");
    m = find_msg(DPN_MSGID_DESTROY_PLAYER, 0);
    CHECKF(m >= 0 && msgs[m].dpnid == self && msgs[m].ctx == (void *)0xabc, "Close destroys the local player");
    EXPECT(IDirectPlay8Peer_Close(peer, 0), DPNERR_UNINITIALIZED);
    EXPECT(IDirectPlay8Peer_GetCaps(peer, &caps, 0), DPNERR_UNINITIALIZED);
    EXPECT(IDirectPlay8Peer_Initialize(peer, NULL, handler, 0), S_OK);
    size = 0;
    EXPECT(IDirectPlay8Peer_GetApplicationDesc(peer, NULL, &size, 0), DPNERR_NOCONNECTION);
    EXPECT(IDirectPlay8Peer_Close(peer, 0), S_OK);

    IDirectPlay8Peer_Release(peer);
    IDirectPlay8Address_Release(dev);
    IDirectPlay8Address_Release(host);
}

/* ---------------------------------------------------------------- server */
static void test_server(void)
{
    IDirectPlay8Server *server;
    IDirectPlay8Address *dev = make_addr(NULL), *addrs[1], *out;
    DPN_APPLICATION_DESC desc, buf;
    DPN_PLAYER_INFO info;
    DPN_GROUP_INFO ginfo;
    DPN_SP_CAPS spcaps;
    DPN_CAPS caps;
    DPN_CONNECTION_INFO cinfo;
    DPNID ids[4], group, serverid;
    DWORD size, count;
    DPNHANDLE h;
    void *ctx;
    HRESULT hr;
    int m;
    DPN_BUFFER_DESC sbd = {4, (BYTE *)"abcd"};

    hr = CoCreateInstance(&CLSID_DirectPlay8Server, NULL, CLSCTX_ALL, &IID_IDirectPlay8Server, (void **)&server);
    check(hr == S_OK, "create server");
    if (hr != S_OK) return;

    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.pwszSessionName = sessname;
    desc.dwMaxPlayers = 4;
    addrs[0] = dev;

    size = 0;
    EXPECT(IDirectPlay8Server_Close(server, 0), DPNERR_UNINITIALIZED);
    EXPECT(IDirectPlay8Server_Host(server, &desc, addrs, 1, NULL, NULL, NULL, 0), DPNERR_UNINITIALIZED);
    EXPECT(IDirectPlay8Server_GetCaps(server, &caps, 0), DPNERR_UNINITIALIZED);
    EXPECT(IDirectPlay8Server_SetServerInfo(server, NULL, NULL, NULL, DPNSETSERVERINFO_SYNC), E_POINTER);
    memset(&spcaps, 0, sizeof(spcaps));
    spcaps.dwSize = sizeof(spcaps);
    EXPECT(IDirectPlay8Server_GetSPCaps(server, &CLSID_DP8SP_TCPIP, &spcaps, 0), DPNERR_UNINITIALIZED);
    EXPECT(IDirectPlay8Server_Initialize(server, NULL, handler, 0), S_OK);
    EXPECT(IDirectPlay8Server_GetSPCaps(server, &CLSID_DP8SP_TCPIP, &spcaps, 0), S_OK);
    CHECKF(spcaps.dwMaxEnumPayloadSize == 983 && spcaps.dwSystemBufferSize == 0x10000 && spcaps.dwDefaultEnumCount == 5, "server SP caps defaults");
    spcaps.dwSystemBufferSize = 0x8000;
    EXPECT(IDirectPlay8Server_SetSPCaps(server, &CLSID_DP8SP_TCPIP, &spcaps, 0), S_OK);
    spcaps.dwSystemBufferSize = 0;
    EXPECT(IDirectPlay8Server_GetSPCaps(server, &CLSID_DP8SP_TCPIP, &spcaps, 0), S_OK);
    CHECKF(spcaps.dwSystemBufferSize == 0x8000, "server SP caps stored (%#lx)", spcaps.dwSystemBufferSize);
    spcaps.dwSize = 1;
    EXPECT(IDirectPlay8Server_SetSPCaps(server, &CLSID_DP8SP_TCPIP, &spcaps, 0), DPNERR_INVALIDPARAM);
    EXPECT(IDirectPlay8Server_GetSPCaps(server, &CLSID_DP8SP_TCPIP, &spcaps, 0), DPNERR_INVALIDPARAM);

    EXPECT(IDirectPlay8Server_GetApplicationDesc(server, NULL, &size, 0), DPNERR_NOCONNECTION);
    EXPECT(IDirectPlay8Server_SetApplicationDesc(server, &desc, 0), DPNERR_NOCONNECTION);
    EXPECT(IDirectPlay8Server_EnumPlayersAndGroups(server, ids, &count, DPNENUM_PLAYERS), DPNERR_NOCONNECTION);
    EXPECT(IDirectPlay8Server_GetClientAddress(server, 5, &out, 0), DPNERR_INVALIDPLAYER);
    EXPECT(IDirectPlay8Server_GetClientInfo(server, 5, &info, &size, 0), DPNERR_INVALIDPLAYER);
    EXPECT(IDirectPlay8Server_DestroyClient(server, 5, NULL, 0, 0), DPNERR_NOCONNECTION);

    clear_msgs();
    EXPECT(IDirectPlay8Server_Host(server, &desc, addrs, 1, NULL, NULL, (void *)0x42, 0), S_OK);
    m = find_msg(DPN_MSGID_CREATE_PLAYER, 0);
    serverid = m >= 0 ? msgs[m].dpnid : 0;
    CHECKF(m >= 0 && serverid && msgs[m].ctx == (void *)0x42, "server Host sends CREATE_PLAYER");
    EXPECT(IDirectPlay8Server_Host(server, &desc, addrs, 1, NULL, NULL, NULL, 0), DPNERR_HOSTING);

    size = 0;
    EXPECT(IDirectPlay8Server_GetApplicationDesc(server, NULL, &size, 0), DPNERR_BUFFERTOOSMALL);
    CHECKF(size == sizeof(buf) + sizeof(sessname), "server appdesc size %lu", size);
    {
        DPN_APPLICATION_DESC *b = calloc(1, size);
        b->dwSize = sizeof(*b);
        EXPECT(IDirectPlay8Server_GetApplicationDesc(server, b, &size, 0), S_OK);
        CHECKF(b->dwCurrentPlayers == 0 && (b->dwFlags & DPNSESSION_CLIENT_SERVER) && b->dwMaxPlayers == 4, "server session is client/server with no clients (%lu %#lx)", b->dwCurrentPlayers, b->dwFlags);
        free(b);
    }

    /* a server's own player is not a client */
    count = 8;
    EXPECT(IDirectPlay8Server_EnumPlayersAndGroups(server, ids, &count, DPNENUM_PLAYERS | DPNENUM_GROUPS), S_OK);
    CHECKF(count == 0, "empty session lists nothing (%lu)", count);
    EXPECT(IDirectPlay8Server_GetPlayerContext(server, serverid, &ctx, 0), DPNERR_INVALIDPLAYER);
    EXPECT(IDirectPlay8Server_GetClientInfo(server, serverid, &info, &size, 0), DPNERR_INVALIDPLAYER);
    EXPECT(IDirectPlay8Server_DestroyClient(server, 5, NULL, 0, 0), DPNERR_INVALIDPLAYER);
    memset(&cinfo, 0, sizeof(cinfo)); cinfo.dwSize = sizeof(cinfo);
    EXPECT(IDirectPlay8Server_GetConnectionInfo(server, 5, &cinfo, 0), DPNERR_INVALIDPLAYER);

    memset(&ginfo, 0, sizeof(ginfo));
    ginfo.dwSize = sizeof(ginfo);
    ginfo.dwInfoFlags = DPNINFO_NAME;
    ginfo.pwszName = L"lobby";
    clear_msgs();
    EXPECT(IDirectPlay8Server_CreateGroup(server, &ginfo, (void *)0x600, NULL, NULL, DPNCREATEGROUP_SYNC), S_OK);
    m = find_msg(DPN_MSGID_CREATE_GROUP, 0);
    group = m >= 0 ? msgs[m].dpnid : 0;
    CHECKF(group && msgs[m].dpnid2 == 0, "server group created, no owner player (%#lx)", m >= 0 ? msgs[m].dpnid2 : 1);
    count = 8;
    EXPECT(IDirectPlay8Server_EnumPlayersAndGroups(server, ids, &count, DPNENUM_GROUPS), S_OK);
    CHECKF(count == 1 && ids[0] == group, "the group is listed");
    EXPECT(IDirectPlay8Server_AddPlayerToGroup(server, group, serverid, NULL, NULL, DPNADDPLAYERTOGROUP_SYNC), DPNERR_INVALIDPLAYER);
    EXPECT(IDirectPlay8Server_SendTo(server, group, &sbd, 1, 0, NULL, NULL, DPNSEND_SYNC), S_OK);
    EXPECT(IDirectPlay8Server_SendTo(server, serverid, &sbd, 1, 0, NULL, NULL, DPNSEND_SYNC), DPNERR_INVALIDPLAYER);
    EXPECT(IDirectPlay8Server_DestroyGroup(server, group, NULL, NULL, DPNDESTROYGROUP_SYNC), S_OK);

    memset(&info, 0, sizeof(info));
    info.dwSize = sizeof(info);
    info.dwInfoFlags = DPNINFO_NAME;
    info.pwszName = L"srv";
    h = 0;
    clear_msgs();
    EXPECT(IDirectPlay8Server_SetServerInfo(server, &info, (void *)0x12, &h, 0), DPNSUCCESS_PENDING);
    check(wait_msg(DPN_MSGID_ASYNC_OP_COMPLETE, 1), "async SetServerInfo completes");

    caps.dwSize = sizeof(caps);
    EXPECT(IDirectPlay8Server_GetCaps(server, &caps, 0), S_OK);
    caps.dwConnectRetries = 9;
    EXPECT(IDirectPlay8Server_SetCaps(server, &caps, 0), S_OK);
    caps.dwConnectRetries = 0;
    EXPECT(IDirectPlay8Server_GetCaps(server, &caps, 0), S_OK);
    CHECKF(caps.dwConnectRetries == 9, "server caps stored (%lu)", caps.dwConnectRetries);

    count = 0;
    EXPECT(IDirectPlay8Server_GetLocalHostAddresses(server, &out, &count, 0), DPNERR_BUFFERTOOSMALL);
    CHECKF(count == 1, "server local addresses (%lu)", count);
    EXPECT(IDirectPlay8Server_SetApplicationDesc(server, &desc, 0), S_OK);

    clear_msgs();
    EXPECT(IDirectPlay8Server_Close(server, 0), S_OK);
    m = find_msg(DPN_MSGID_DESTROY_PLAYER, 0);
    CHECKF(m >= 0 && msgs[m].dpnid == serverid, "server Close destroys the server player");
    EXPECT(IDirectPlay8Server_Close(server, 0), DPNERR_UNINITIALIZED);
    IDirectPlay8Server_Release(server);
    IDirectPlay8Address_Release(dev);
}

/* ---------------------------------------------------------------- client */
static void test_client(void)
{
    IDirectPlay8Client *client;
    IDirectPlay8Address *dev = make_addr(NULL), *host = make_addr(L"127.0.0.1"), *out;
    DPN_APPLICATION_DESC desc;
    DPN_PLAYER_INFO info;
    DPN_CAPS caps;
    DPN_CONNECTION_INFO cinfo;
    DPN_BUFFER_DESC bd = {4, (BYTE *)"abcd"};
    DWORD size = 0, msgs_q = 5, bytes_q = 5;
    DPNHANDLE h = 0;
    HRESULT hr;
    int m;

    hr = CoCreateInstance(&CLSID_DirectPlay8Client, NULL, CLSCTX_ALL, &IID_IDirectPlay8Client, (void **)&client);
    check(hr == S_OK, "create client");
    if (hr != S_OK) return;

    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    memset(&cinfo, 0, sizeof(cinfo)); cinfo.dwSize = sizeof(cinfo);
    EXPECT(IDirectPlay8Client_Close(client, 0), DPNERR_UNINITIALIZED);
    EXPECT(IDirectPlay8Client_GetCaps(client, &caps, 0), DPNERR_UNINITIALIZED);
    EXPECT(IDirectPlay8Client_Connect(client, &desc, host, dev, NULL, NULL, NULL, 0, NULL, NULL, DPNCONNECT_SYNC), DPNERR_UNINITIALIZED);
    EXPECT(IDirectPlay8Client_Send(client, &bd, 1, 0, NULL, NULL, DPNSEND_SYNC), DPNERR_UNINITIALIZED);
    EXPECT(IDirectPlay8Client_Initialize(client, NULL, handler, 0), S_OK);

    EXPECT(IDirectPlay8Client_Send(client, &bd, 1, 0, NULL, NULL, DPNSEND_SYNC), DPNERR_NOCONNECTION);
    EXPECT(IDirectPlay8Client_Send(client, NULL, 0, 0, NULL, NULL, DPNSEND_SYNC), DPNERR_INVALIDPARAM);
    EXPECT(IDirectPlay8Client_GetSendQueueInfo(client, &msgs_q, &bytes_q, 0), DPNERR_NOCONNECTION);
    EXPECT(IDirectPlay8Client_GetApplicationDesc(client, NULL, &size, 0), DPNERR_NOCONNECTION);
    EXPECT(IDirectPlay8Client_GetServerInfo(client, &info, &size, 0), DPNERR_NOCONNECTION);
    EXPECT(IDirectPlay8Client_GetServerAddress(client, &out, 0), DPNERR_NOCONNECTION);
    EXPECT(IDirectPlay8Client_GetServerAddress(client, NULL, 0), DPNERR_INVALIDPOINTER);
    EXPECT(IDirectPlay8Client_GetConnectionInfo(client, &cinfo, 0), DPNERR_NOCONNECTION);
    EXPECT(IDirectPlay8Client_ReturnBuffer(client, 3, 0), DPNERR_INVALIDHANDLE);
    EXPECT(IDirectPlay8Client_RegisterLobby(client, 1, NULL, DPNLOBBY_UNREGISTER), DPNERR_NOTREGISTERED);

    EXPECT(IDirectPlay8Client_Connect(client, &desc, host, dev, NULL, NULL, NULL, 0, NULL, NULL, DPNCONNECT_SYNC), DPNERR_NORESPONSE);
    clear_msgs();
    EXPECT(IDirectPlay8Client_Connect(client, &desc, host, dev, NULL, NULL, NULL, 0, (void *)0x8, &h, 0), DPNSUCCESS_PENDING);
    check(wait_msg(DPN_MSGID_CONNECT_COMPLETE, 1), "client async connect completes");
    m = find_msg(DPN_MSGID_CONNECT_COMPLETE, 0);
    CHECKF(m >= 0 && msgs[m].hr == DPNERR_NORESPONSE && msgs[m].handle == h, "client connect completion");
    EXPECT(IDirectPlay8Client_Connect(client, &desc, host, dev, NULL, NULL, NULL, 0, NULL, NULL, DPNCONNECT_SYNC), DPNERR_NORESPONSE);

    memset(&info, 0, sizeof(info));
    info.dwSize = sizeof(info);
    info.dwInfoFlags = DPNINFO_NAME;
    info.pwszName = L"cli";
    h = 0;
    EXPECT(IDirectPlay8Client_SetClientInfo(client, &info, NULL, &h, 0), DPNSUCCESS_PENDING);
    EXPECT(IDirectPlay8Client_SetClientInfo(client, &info, NULL, NULL, DPNSETCLIENTINFO_SYNC), S_OK);
    caps.dwSize = sizeof(caps);
    EXPECT(IDirectPlay8Client_GetCaps(client, &caps, 0), S_OK);
    CHECKF(caps.dwConnectTimeout == 200, "client caps default");
    EXPECT(IDirectPlay8Client_Close(client, 0), S_OK);
    EXPECT(IDirectPlay8Client_Close(client, 0), DPNERR_UNINITIALIZED);
    IDirectPlay8Client_Release(client);
    IDirectPlay8Address_Release(dev);
    IDirectPlay8Address_Release(host);
}

/* -------------------------------------------------------- lobby classes */
static void test_lobby(void)
{
    IDirectPlay8LobbiedApplication *app;
    IDirectPlay8LobbyClient *lc;
    static const GUID guid = {0x6a8f3c10, 0x5b2d, 0x4c11, {0x9e, 0x01, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66}};
    DPL_PROGRAM_DESC prog;
    DPL_APPLICATION_INFO *infos;
    DPL_CONNECT_INFO ci;
    DPNHANDLE conn = 0, h;
    DWORD size, items;
    BYTE *buf;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_DirectPlay8LobbiedApplication, NULL, CLSCTX_ALL, &IID_IDirectPlay8LobbiedApplication, (void **)&app);
    check(hr == S_OK, "create lobbied application");
    if (hr != S_OK) return;

    memset(&prog, 0, sizeof(prog));
    prog.dwSize = sizeof(prog);
    prog.guidApplication = guid;
    EXPECT(IDirectPlay8LobbiedApplication_RegisterProgram(app, NULL, 0), DPNERR_INVALIDPOINTER);
    EXPECT(IDirectPlay8LobbiedApplication_RegisterProgram(app, &prog, 1), DPNERR_INVALIDFLAGS);
    EXPECT(IDirectPlay8LobbiedApplication_RegisterProgram(app, &prog, 0), DPNERR_INVALIDPARAM);
    prog.pwszApplicationName = L"SG Probe Game";
    prog.pwszExecutableFilename = L"probe.exe";
    prog.pwszExecutablePath = L"C:\\probe";
    prog.pwszCommandLine = L"-lobby";
    IDirectPlay8LobbiedApplication_UnRegisterProgram(app, (GUID *)&guid, 0);
    EXPECT(IDirectPlay8LobbiedApplication_RegisterProgram(app, &prog, 0), S_OK);
    EXPECT(IDirectPlay8LobbiedApplication_RegisterProgram(app, &prog, 0), DPNERR_ALREADYREGISTERED);

    hr = CoCreateInstance(&CLSID_DirectPlay8LobbyClient, NULL, CLSCTX_ALL, &IID_IDirectPlay8LobbyClient, (void **)&lc);
    check(hr == S_OK, "create lobby client");
    size = 0; items = 0;
    EXPECT(IDirectPlay8LobbyClient_EnumLocalPrograms(lc, NULL, NULL, &size, &items, 0), DPNERR_UNINITIALIZED);
    EXPECT(IDirectPlay8LobbyClient_Initialize(lc, NULL, handler, 0), S_OK);
    EXPECT(IDirectPlay8LobbyClient_EnumLocalPrograms(lc, (GUID *)&guid, NULL, NULL, &items, 0), DPNERR_INVALIDPOINTER);
    EXPECT(IDirectPlay8LobbyClient_EnumLocalPrograms(lc, (GUID *)&guid, NULL, &size, &items, 1), DPNERR_INVALIDFLAGS);
    EXPECT(IDirectPlay8LobbyClient_EnumLocalPrograms(lc, (GUID *)&guid, NULL, &size, &items, 0), DPNERR_BUFFERTOOSMALL);
    CHECKF(items == 1 && size == sizeof(DPL_APPLICATION_INFO) + sizeof(L"SG Probe Game"), "enum size %lu items %lu", size, items);
    buf = calloc(1, size);
    EXPECT(IDirectPlay8LobbyClient_EnumLocalPrograms(lc, (GUID *)&guid, buf, &size, &items, 0), S_OK);
    infos = (DPL_APPLICATION_INFO *)buf;
    CHECKF(items == 1 && IsEqualGUID(&infos[0].guidApplication, &guid) && !lstrcmpW(infos[0].pwszApplicationName, L"SG Probe Game") &&
           infos[0].dwNumRunning == 0 && infos[0].dwNumWaiting == 0, "registered program listed with its name");
    free(buf);
    {
        GUID other = guid;
        other.Data1++;
        size = 0; items = 9;
        EXPECT(IDirectPlay8LobbyClient_EnumLocalPrograms(lc, &other, NULL, &size, &items, 0), S_OK);
        CHECKF(items == 0 && size == 0, "other GUID: nothing (%lu/%lu)", items, size);
    }

    memset(&ci, 0, sizeof(ci));
    ci.dwSize = sizeof(ci);
    ci.guidApplication = guid;
    EXPECT(IDirectPlay8LobbyClient_ConnectApplication(lc, NULL, NULL, &conn, 0, 0), DPNERR_INVALIDPOINTER);
    ci.guidApplication.Data1++;
    EXPECT(IDirectPlay8LobbyClient_ConnectApplication(lc, &ci, NULL, &conn, 0, 0), DPNERR_INVALIDAPPLICATION);
    ci.guidApplication = guid;
    EXPECT(IDirectPlay8LobbyClient_ConnectApplication(lc, &ci, NULL, &conn, 0, 0), DPNERR_CANTLAUNCHAPPLICATION);
    h = 77;
    EXPECT(IDirectPlay8LobbyClient_Send(lc, h, (BYTE *)"x", 1, 0), DPNERR_INVALIDHANDLE);
    EXPECT(IDirectPlay8LobbyClient_ReleaseApplication(lc, h, 0), DPNERR_INVALIDHANDLE);
    size = 0;
    EXPECT(IDirectPlay8LobbyClient_GetConnectionSettings(lc, h, NULL, &size, 0), DPNERR_INVALIDHANDLE);
    EXPECT(IDirectPlay8LobbyClient_Close(lc, 0), S_OK);
    EXPECT(IDirectPlay8LobbyClient_Close(lc, 0), DPNERR_UNINITIALIZED);
    IDirectPlay8LobbyClient_Release(lc);

    /* lobbied application object */
    EXPECT(IDirectPlay8LobbiedApplication_SetAppAvailable(app, TRUE, 0), DPNERR_UNINITIALIZED);
    EXPECT(IDirectPlay8LobbiedApplication_Close(app, 0), DPNERR_UNINITIALIZED);
    EXPECT(IDirectPlay8LobbiedApplication_Initialize(app, NULL, handler, &conn, 0), S_OK);
    EXPECT(IDirectPlay8LobbiedApplication_SetAppAvailable(app, TRUE, 0), S_OK);
    EXPECT(IDirectPlay8LobbiedApplication_SetAppAvailable(app, TRUE, 0x8), DPNERR_INVALIDFLAGS);
    EXPECT(IDirectPlay8LobbiedApplication_UpdateStatus(app, 5, DPLSESSION_CONNECTED, 0), DPNERR_INVALIDHANDLE);
    EXPECT(IDirectPlay8LobbiedApplication_UpdateStatus(app, 5, 99, 0), DPNERR_INVALIDPARAM);
    EXPECT(IDirectPlay8LobbiedApplication_Send(app, 5, (BYTE *)"x", 1, 0), DPNERR_INVALIDHANDLE);
    size = 0;
    EXPECT(IDirectPlay8LobbiedApplication_GetConnectionSettings(app, 5, NULL, &size, 0), DPNERR_INVALIDHANDLE);
    EXPECT(IDirectPlay8LobbiedApplication_GetConnectionSettings(app, 5, NULL, NULL, 0), DPNERR_INVALIDPOINTER);
    EXPECT(IDirectPlay8LobbiedApplication_SetConnectionSettings(app, 5, NULL, 0), DPNERR_INVALIDPOINTER);

    EXPECT(IDirectPlay8LobbiedApplication_UnRegisterProgram(app, NULL, 0), DPNERR_INVALIDPOINTER);
    EXPECT(IDirectPlay8LobbiedApplication_UnRegisterProgram(app, (GUID *)&guid, 0), S_OK);
    EXPECT(IDirectPlay8LobbiedApplication_UnRegisterProgram(app, (GUID *)&guid, 0), DPNERR_DOESNOTEXIST);
    EXPECT(IDirectPlay8LobbiedApplication_Close(app, 0), S_OK);
    EXPECT(IDirectPlay8LobbiedApplication_Close(app, 0), DPNERR_UNINITIALIZED);
    IDirectPlay8LobbiedApplication_Release(app);
}

/* ------------------------------------------------------------ threadpool */
static void test_threadpool(void)
{
    IDirectPlay8ThreadPool *pool, *pool2;
    DWORD count = 99;
    HRESULT hr;

    hr = CoCreateInstance(&CLSID_DirectPlay8ThreadPool, NULL, CLSCTX_ALL, &IID_IDirectPlay8ThreadPool, (void **)&pool);
    check(hr == S_OK, "create thread pool");
    if (hr != S_OK) return;
    CoCreateInstance(&CLSID_DirectPlay8ThreadPool, NULL, CLSCTX_ALL, &IID_IDirectPlay8ThreadPool, (void **)&pool2);

    EXPECT(IDirectPlay8ThreadPool_GetThreadCount(pool, -1, &count, 0), DPNERR_UNINITIALIZED);
    EXPECT(IDirectPlay8ThreadPool_SetThreadCount(pool, -1, 2, 0), DPNERR_UNINITIALIZED);
    EXPECT(IDirectPlay8ThreadPool_DoWork(pool, 10, 0), DPNERR_UNINITIALIZED);
    EXPECT(IDirectPlay8ThreadPool_Initialize(pool, NULL, handler, 0), S_OK);
    EXPECT(IDirectPlay8ThreadPool_Initialize(pool2, NULL, handler, 0), DPNERR_ALREADYINITIALIZED);

    EXPECT(IDirectPlay8ThreadPool_GetThreadCount(pool, -1, NULL, 0), DPNERR_INVALIDPOINTER);
    EXPECT(IDirectPlay8ThreadPool_GetThreadCount(pool, -1, &count, 1), DPNERR_INVALIDFLAGS);
    EXPECT(IDirectPlay8ThreadPool_GetThreadCount(pool, 100000, &count, 0), DPNERR_INVALIDPARAM);
    EXPECT(IDirectPlay8ThreadPool_GetThreadCount(pool, -1, &count, 0), S_OK);
    CHECKF(count == 0, "no threads at first (%lu)", count);

    clear_msgs();
    EXPECT(IDirectPlay8ThreadPool_SetThreadCount(pool, -1, 4, 0), S_OK);
    CHECKF(count_msgs(DPN_MSGID_CREATE_THREAD) == 4, "4 CREATE_THREAD messages (%d)", count_msgs(DPN_MSGID_CREATE_THREAD));
    EXPECT(IDirectPlay8ThreadPool_SetThreadCount(pool2, -1, 6, 0), S_OK);
    CHECKF(count_msgs(DPN_MSGID_CREATE_THREAD) == 6, "shared pool: 6 CREATE_THREAD messages (%d)", count_msgs(DPN_MSGID_CREATE_THREAD));
    EXPECT(IDirectPlay8ThreadPool_GetThreadCount(pool, -1, &count, 0), S_OK);
    CHECKF(count == 6, "thread count is process wide (%lu)", count);
    EXPECT(IDirectPlay8ThreadPool_DoWork(pool, 10, 0), DPNERR_NOTREADY);
    EXPECT(IDirectPlay8ThreadPool_SetThreadCount(pool, -1, 1, 0), S_OK);
    CHECKF(count_msgs(DPN_MSGID_DESTROY_THREAD) == 5, "5 DESTROY_THREAD messages (%d)", count_msgs(DPN_MSGID_DESTROY_THREAD));
    EXPECT(IDirectPlay8ThreadPool_SetThreadCount(pool, 0, 2, 0), S_OK);
    EXPECT(IDirectPlay8ThreadPool_GetThreadCount(pool, 0, &count, 0), S_OK);
    CHECKF(count == 2, "per-processor count (%lu)", count);
    EXPECT(IDirectPlay8ThreadPool_GetThreadCount(pool, -1, &count, 0), S_OK);
    CHECKF(count == 3, "total includes processor 0 (%lu)", count);
    EXPECT(IDirectPlay8ThreadPool_SetThreadCount(pool, -1, 0, 0), S_OK);
    EXPECT(IDirectPlay8ThreadPool_SetThreadCount(pool, 0, 0, 0), S_OK);
    EXPECT(IDirectPlay8ThreadPool_DoWork(pool, 10, 1), DPNERR_INVALIDFLAGS);
    EXPECT(IDirectPlay8ThreadPool_DoWork(pool, 10, 0), S_OK);
    EXPECT(IDirectPlay8ThreadPool_SetThreadCount(pool, -1, 1, 3), DPNERR_INVALIDFLAGS);
    EXPECT(IDirectPlay8ThreadPool_Close(pool, 2), DPNERR_INVALIDFLAGS);
    EXPECT(IDirectPlay8ThreadPool_Close(pool, 0), S_OK);
    EXPECT(IDirectPlay8ThreadPool_Close(pool2, 0), DPNERR_UNINITIALIZED);
    IDirectPlay8ThreadPool_Release(pool);
    IDirectPlay8ThreadPool_Release(pool2);
}

int main(void)
{
    InitializeCriticalSection(&cs);
    CoInitialize(NULL);
    test_peer();
    test_server();
    test_client();
    test_lobby();
    test_threadpool();
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
