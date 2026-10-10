/* A DirectPlay service provider and a lobby service provider that do nothing
 * but answer success and remember what they are asked, for
 * test/dplayx-*-probe.c: no network is needed to run a DirectPlay session on
 * them. Built as sgfakesp.dll, registered by the gates under
 * HKLM\Software\Microsoft\DirectPlay\{Service,Lobby} Providers. */
#include <windows.h>
#include "dplayx-fakesp.h"

/* The layouts below are those of dplaysp.h of the SDK (SPINITDATA and the
 * callback table) as used by DirectPlay 6. */
typedef struct
{
    void *lpCB;
    SGSPUnknown *lpISP;
    LPWSTR lpszName;
    LPGUID lpGuid;
    DWORD dwReserved1, dwReserved2;
    DWORD dwSPHeaderSize;
    void *lpAddress;
    DWORD dwAddressSize;
    DWORD dwSPVersion;
} SPINITDATA;

typedef struct { DPID idPlayer; DWORD dwFlags; void *lpSPMessageHeader; SGSPUnknown *lpISP; } CREATEPLAYERDATA;
typedef struct { DPID idPlayer; DWORD dwFlags; SGSPUnknown *lpISP; } DELETEPLAYERDATA;
typedef struct { DPID idGroup; DWORD dwFlags; SGSPUnknown *lpISP; } DELETEGROUPDATA;
typedef struct { DPID idPlayer; DWORD dwFlags; void *lpAddress; DWORD *lpdwAddressSize; SGSPUnknown *lpISP; } GETADDRESSDATA;
typedef struct { SGSPUnknown *lpISP; DWORD dwFlags; void *(*lprglpvSPMsgID)[]; DWORD cSPMsgID; DWORD min, max; } CANCELDATA;
typedef struct { UINT len; BYTE *pData; } SGBUF;
typedef struct { SGSPUnknown *lpISP; DWORD dwFlags; DPID idPlayerTo, idPlayerFrom; SGBUF *buffers; DWORD cBuffers, dwMessageSize, dwPriority, dwTimeout; void *ctx; DWORD *lpdwSPMsgID; BOOL bSystemMessage; } SENDEXDATA;
typedef struct { SGSPUnknown *lpISP; DWORD dwFlags; DPID idGroupTo, idPlayerFrom; SGBUF *buffers; DWORD cBuffers, dwMessageSize, dwPriority, dwTimeout; void *ctx; DWORD *lpdwSPMsgID; } SENDTOGROUPEXDATA;
typedef struct { DWORD dwFlags; DPID to, from; void *msg; DWORD size; BOOL sys; SGSPUnknown *lpISP; } SENDDATA;

static SGSPLog sp_log;
static SGSPUnknown *sp_isp;
static SGLSPLog lsp_log;
static SGSPUnknown *lsp_isp;

static HRESULT WINAPI cb_ok( void *data ) { return DP_OK; }

/* The session this provider "finds" on the network: answers an enumeration
 * the way a provider does, by handing the reply to DirectPlay */
typedef struct { void *lpMessage; DWORD dwMessageSize; SGSPUnknown *lpISP; BOOL bReturnStatus; } ENUMSESSIONSDATA;

/* DirectPlay's own layout: the envelope, the session description and one
 * more DWORD, without padding, then the name of the session */
#pragma pack(push, 1)
struct enum_reply
{
    DWORD magic;
    WORD  command;
    WORD  version;
    DPSESSIONDESC2 sd;
    DWORD unknown;
};
#pragma pack(pop)

static HRESULT WINAPI cb_enumsessions( ENUMSESSIONSDATA *d )
{
    BYTE buffer[ sizeof(struct enum_reply) + 64 ];
    struct enum_reply reply;
    static const WCHAR name[] = { 's', 'g', '-', 's', 'e', 's', 's', 'i', 'o', 'n', 0 };

    ZeroMemory( &reply, sizeof(reply) );
    reply.magic = 0x79616c70; /* "play" */
    reply.command = 1;        /* the reply to a session enumeration */
    reply.version = 11;
    reply.sd.dwSize = sizeof(DPSESSIONDESC2);
    reply.sd.dwFlags = 0;
    reply.sd.guidInstance = SGSESSION_INSTANCE;
    reply.sd.guidApplication = SGSESSION_APPLICATION;
    reply.sd.dwMaxPlayers = 5;
    reply.sd.dwCurrentPlayers = 2;
    reply.sd.dwUser1 = 11; reply.sd.dwUser2 = 22; reply.sd.dwUser3 = 33; reply.sd.dwUser4 = 44;
    reply.unknown = 0x5c;
    ZeroMemory( buffer, sizeof(buffer) );
    CopyMemory( buffer, &reply, sizeof(reply) );
    lstrcpyW( (WCHAR *)( buffer + sizeof(reply) ), name );

    sp_log.enumsessions_calls++;
    d->lpISP->lpVtbl->HandleMessage( d->lpISP, buffer, sizeof(buffer), NULL );
    return DP_OK;
}

static HRESULT WINAPI cb_createplayer( CREATEPLAYERDATA *d )
{
    DWORD local = 0x4c4c4c4c, remote = 0x52525252;

    sp_log.createplayer_calls++;
    /* A provider keeps what it needs for a player with the player */
    d->lpISP->lpVtbl->SetSPPlayerData( d->lpISP, d->idPlayer, &local, sizeof(local), DPSET_LOCAL );
    d->lpISP->lpVtbl->SetSPPlayerData( d->lpISP, d->idPlayer, &remote, sizeof(remote), DPSET_REMOTE );
    return DP_OK;
}

static HRESULT WINAPI cb_deleteplayer( DELETEPLAYERDATA *d )
{
    sp_log.deleteplayer_calls++;
    sp_log.deleteplayer_id = d->idPlayer;
    sp_log.deleteplayer_flags = d->dwFlags;
    return DP_OK;
}

static HRESULT WINAPI cb_deletegroup( DELETEGROUPDATA *d )
{
    sp_log.deletegroup_calls++;
    sp_log.deletegroup_id = d->idGroup;
    sp_log.deletegroup_flags = d->dwFlags;
    return DP_OK;
}

/* The address of a player is a compound address of 24 bytes: the total size
 * element only */
static HRESULT WINAPI cb_getaddress( GETADDRESSDATA *d )
{
    sp_log.getaddress_calls++;
    sp_log.getaddress_player = d->idPlayer;
    sp_log.getaddress_flags = d->dwFlags;

    if ( !d->lpAddress || *d->lpdwAddressSize < 24 )
    {
        *d->lpdwAddressSize = 24;
        return DPERR_BUFFERTOOSMALL;
    }

    ZeroMemory( d->lpAddress, 24 );
    ( (DPADDRESS *) d->lpAddress )->guidDataType = DPAID_TotalSize;
    ( (DPADDRESS *) d->lpAddress )->dwDataSize = sizeof(DWORD);
    *(DWORD *)( (BYTE *) d->lpAddress + sizeof(DPADDRESS) ) = 24;
    *d->lpdwAddressSize = 24;
    return DP_OK;
}

typedef struct { DPID idPlayer; DPCAPS *lpCaps; DWORD dwFlags; SGSPUnknown *lpISP; } GETCAPSDATA;
static HRESULT WINAPI cb_getcaps2( GETCAPSDATA *d )
{
    d->lpCaps->dwFlags = DPCAPS_ASYNCSUPPORTED | DPCAPS_GUARANTEEDSUPPORTED;
    d->lpCaps->dwMaxBufferSize = 1024;
    d->lpCaps->dwMaxQueueSize = 0;
    d->lpCaps->dwMaxPlayers = 64;
    d->lpCaps->dwHundredBaud = 10000;
    d->lpCaps->dwLatency = 1;
    d->lpCaps->dwMaxLocalPlayers = 64;
    d->lpCaps->dwHeaderLength = 0;
    d->lpCaps->dwTimeout = 500;
    return DP_OK;
}

static HRESULT WINAPI cb_cancel( CANCELDATA *d )
{
    sp_log.cancel_calls++;
    sp_log.cancel_flags = d->dwFlags;
    sp_log.cancel_count = d->cSPMsgID;
    sp_log.cancel_first = d->lprglpvSPMsgID && d->cSPMsgID ? (ULONG_PTR)(*d->lprglpvSPMsgID)[0] : 0;
    sp_log.cancel_minprio = d->min;
    sp_log.cancel_maxprio = d->max;
    return DP_OK;
}

/* The host of the session this provider "finds" answers what a peer asks it:
 * a request for the id of a new player or group (command 5) gets the next id of
 * a counter, a request to join (command 19) gets an empty list of the players
 * and groups of the session. The replies are handed to DirectPlay the way a
 * provider hands it what arrives from the network. */
#pragma pack(push, 1)
struct newid_reply
{
    DWORD magic; WORD command, version;
    DWORD id;
    DPSECURITYDESC security;
    DWORD sspi_offset, capi_offset;
    HRESULT result;
};
struct superenum_reply
{
    DWORD magic; WORD command, version;
    DWORD player_count, group_count, packed_offset, shortcut_count, description_offset, name_offset, password_offset;
};
#pragma pack(pop)

static DWORD next_id = 0x7000;

static void answer_system_message( SGSPUnknown *isp, SGBUF *buffers, DWORD count )
{
    DWORD i;

    for ( i = 0; i < count; i++ )
    {
        BYTE *msg = buffers[ i ].pData;

        if ( !msg || buffers[ i ].len < 8 || *(DWORD *) msg != 0x79616c70 )
            continue;

        switch ( *(WORD *)( msg + 4 ) )
        {
        case 5: /* request for a new id */
        {
            struct newid_reply reply;
            ZeroMemory( &reply, sizeof(reply) );
            reply.magic = 0x79616c70; reply.command = 7; reply.version = 11;
            reply.id = ++next_id;
            sp_log.newid_requests++;
            sp_log.newid_flags = buffers[ i ].len >= 12 ? *(DWORD *)( msg + 8 ) : 0;
            isp->lpVtbl->HandleMessage( isp, &reply, sizeof(reply), NULL );
            break;
        }
        case 19: /* forwarded request to add a player: the session has no players yet */
        {
            struct superenum_reply reply;
            ZeroMemory( &reply, sizeof(reply) );
            reply.magic = 0x79616c70; reply.command = 41; reply.version = 11;
            reply.packed_offset = sizeof(reply);
            sp_log.join_requests++;
            isp->lpVtbl->HandleMessage( isp, &reply, sizeof(reply), NULL );
            break;
        }
        }
        return;
    }
}

/* A send with DPSEND_ASYNC stays queued: the provider says so */
static HRESULT WINAPI cb_sendex( SENDEXDATA *d )
{
    sp_log.sendex_calls++;
    if ( d->bSystemMessage )
        answer_system_message( d->lpISP, d->buffers, d->cBuffers );
    if ( ( d->dwFlags & DPSEND_ASYNC ) && d->lpdwSPMsgID && !d->bSystemMessage )
    {
        *d->lpdwSPMsgID = 0x5000 + sp_log.sendex_calls;
        sp_log.sendex_msgid = *d->lpdwSPMsgID;
        return DPERR_PENDING;
    }
    return DP_OK;
}

static HRESULT WINAPI cb_sendtogroupex( SENDTOGROUPEXDATA *d ) { sp_log.sendtogroup_calls++; return DP_OK; }
static HRESULT WINAPI cb_send( SENDDATA *d ) { return DP_OK; }

static BYTE spcb_storage[ 8 + 22 * sizeof(void *) ];

/* callback table of DirectPlay's own layout: size, version and then in this
 * order EnumSessions, Reply, Send, AddPlayerToGroup, Close, CreateGroup,
 * CreatePlayer, DeleteGroup, DeletePlayer, GetAddress, GetCaps, Open,
 * RemovePlayerFromGroup, SendToGroup, Shutdown, CloseEx, ShutdownEx,
 * GetAddressChoices, SendEx, SendToGroupEx, Cancel, GetMessageQueue */
__declspec(dllexport) HRESULT WINAPI SPInit( SPINITDATA *d )
{
    DWORD *table = (DWORD *) spcb_storage;
    void **fn = (void **)( spcb_storage + 8 );

    ZeroMemory( spcb_storage, sizeof(spcb_storage) );
    ( (DWORD *) spcb_storage )[ 0 ] = sizeof(spcb_storage);
    table[ 1 ] = 0;

    fn[ 0 ] = cb_enumsessions;   /* EnumSessions */
    fn[ 1 ] = cb_ok;             /* Reply */
    fn[ 2 ] = cb_send;           /* Send */
    fn[ 3 ] = cb_ok;             /* AddPlayerToGroup */
    fn[ 4 ] = NULL;              /* Close */
    fn[ 5 ] = cb_ok;             /* CreateGroup */
    fn[ 6 ] = cb_createplayer;   /* CreatePlayer */
    fn[ 7 ] = cb_deletegroup;    /* DeleteGroup */
    fn[ 8 ] = cb_deleteplayer;   /* DeletePlayer */
    fn[ 9 ] = cb_getaddress;     /* GetAddress */
    fn[ 10 ] = cb_getcaps2;      /* GetCaps */
    fn[ 11 ] = cb_ok;            /* Open */
    fn[ 12 ] = cb_ok;            /* RemovePlayerFromGroup */
    fn[ 13 ] = NULL;             /* SendToGroup */
    fn[ 14 ] = NULL;             /* Shutdown */
    fn[ 15 ] = cb_ok;            /* CloseEx */
    fn[ 16 ] = cb_ok;            /* ShutdownEx */
    fn[ 17 ] = NULL;             /* GetAddressChoices */
    fn[ 18 ] = cb_sendex;        /* SendEx */
    fn[ 19 ] = cb_sendtogroupex; /* SendToGroupEx */
    fn[ 20 ] = cb_cancel;        /* Cancel */
    fn[ 21 ] = NULL;             /* GetMessageQueue */

    d->lpCB = spcb_storage;
    d->dwSPHeaderSize = 0;
    sp_isp = d->lpISP;
    return DP_OK;
}

/* --- the lobby provider --- */
typedef struct { DWORD dwSize; DWORD dwFlags; SGSPUnknown *lpISP; DWORD dwGroupID; DWORD *lpdwBufferSize; void *lpBuffer; } GETGROUPCONN;
typedef struct { DWORD dwSize; DWORD dwFlags; SGSPUnknown *lpISP; DWORD dwGroupID; void *lpConn; } SETGROUPCONN;
typedef struct { DWORD dwSize; SGSPUnknown *lpISP; DWORD dwFlags; DWORD dwGroupID; } STARTSESSION;
typedef struct { DWORD dwSize; SGSPUnknown *lpISP; DWORD from, to, flags; void *chat; } CHATMSG;

static HRESULT WINAPI lcb_getgroupconn( GETGROUPCONN *d )
{
    lsp_log.getgroupconn_calls++; lsp_log.getgroupconn_flags = d->dwFlags; lsp_log.getgroupconn_group = d->dwGroupID;
    return DPERR_NOTLOBBIED;
}
static HRESULT WINAPI lcb_setgroupconn( SETGROUPCONN *d )
{
    lsp_log.setgroupconn_calls++; lsp_log.setgroupconn_flags = d->dwFlags; lsp_log.setgroupconn_group = d->dwGroupID;
    lsp_log.setgroupconn_conn = d->lpConn;
    return DP_OK;
}
static HRESULT WINAPI lcb_startsession( STARTSESSION *d )
{
    lsp_log.startsession_calls++; lsp_log.startsession_flags = d->dwFlags; lsp_log.startsession_group = d->dwGroupID;
    return DP_OK;
}
static HRESULT WINAPI lcb_chat( CHATMSG *d )
{
    lsp_log.chat_calls++; lsp_log.chat_from = d->from; lsp_log.chat_to = d->to; lsp_log.chat_flags = d->flags;
    return DP_OK;
}

/* lobby callback table: size, flags, then AddGroupToGroup, AddPlayerToGroup,
 * BuildParentalHierarchy, Close, CreateGroup, CreateGroupInGroup,
 * CreatePlayer, DeleteGroupFromGroup, DeletePlayerFromGroup, DestroyGroup,
 * DestroyPlayer, EnumSessions, GetCaps, GetGroupConnectionSettings,
 * GetGroupData, GetPlayerCaps, GetPlayerData, Open, Send, SendChatMessage,
 * SetGroupConnectionSettings, SetGroupData, SetGroupName, SetPlayerData,
 * SetPlayerName, Shutdown, StartSession */
static BYTE lspcb_storage[ 8 + 27 * sizeof(void *) ];
typedef struct { void *lpCB; DWORD dwSPVersion; SGSPUnknown *lpISP; void *lpAddress; } LSPINIT;

__declspec(dllexport) HRESULT WINAPI DPLSPInit( LSPINIT *d )
{
    void **fn = (void **)( lspcb_storage + 8 );

    ZeroMemory( lspcb_storage, sizeof(lspcb_storage) );
    ( (DWORD *) lspcb_storage )[ 0 ] = sizeof(lspcb_storage);
    fn[ 13 ] = lcb_getgroupconn;
    fn[ 19 ] = lcb_chat;
    fn[ 20 ] = lcb_setgroupconn;
    fn[ 26 ] = lcb_startsession;

    d->lpCB = lspcb_storage;
    lsp_isp = d->lpISP;
    return DP_OK;
}

__declspec(dllexport) SGSPLog * WINAPI SGSP_GetLog( void ) { return &sp_log; }
__declspec(dllexport) SGSPUnknown * WINAPI SGSP_GetISP( void ) { return sp_isp; }
__declspec(dllexport) void WINAPI SGSP_Complete( void *msgid, DWORD result ) { sp_isp->lpVtbl->SendComplete( sp_isp, msgid, result ); }
__declspec(dllexport) SGLSPLog * WINAPI SGLSP_GetLog( void ) { return &lsp_log; }
__declspec(dllexport) SGSPUnknown * WINAPI SGLSP_GetISP( void ) { return lsp_isp; }

BOOL WINAPI DllMain( HINSTANCE inst, DWORD reason, void *reserved )
{
    return TRUE;
}
