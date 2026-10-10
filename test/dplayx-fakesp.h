/* Shared by test/dplayx-fakesp.c (a DirectPlay service provider and lobby
 * provider that do nothing but remember what they are asked) and the
 * dplayx-*-probe.c programs. The layouts are those of the service provider
 * interface of DirectPlay 6: callback tables handed out by SPInit and
 * DPLSPInit, and the interfaces DirectPlay hands to the provider. */
#ifndef SG_DPLAYX_FAKESP_H
#define SG_DPLAYX_FAKESP_H

#include <windows.h>
#include <dplay.h>
#include <dplobby.h>

#define SGSP_NAME   "SG Fake Service Provider"
#define SGLSP_NAME  "SG Fake Lobby Provider"

/* {7d2ab7e1-93a3-4a0a-8a07-5b2c43b24e01} and {7d2ab7e2-93a3-4a0a-8a07-5b2c43b24e01} */
static const GUID SGSP_GUID  = { 0x7d2ab7e1, 0x93a3, 0x4a0a, { 0x8a, 0x07, 0x5b, 0x2c, 0x43, 0xb2, 0x4e, 0x01 } };
static const GUID SGLSP_GUID = { 0x7d2ab7e2, 0x93a3, 0x4a0a, { 0x8a, 0x07, 0x5b, 0x2c, 0x43, 0xb2, 0x4e, 0x01 } };

/* the session the provider reports to every enumeration */
static const GUID SGSESSION_INSTANCE    = { 0x5e5510a1, 0x1111, 0x4222, { 0x93, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66 } };
static const GUID SGSESSION_APPLICATION = { 0x5e5510a2, 0x1111, 0x4222, { 0x93, 0x33, 0x44, 0x44, 0x55, 0x55, 0x66, 0x66 } };

/* --- what DirectPlay gives a service provider --- */
typedef struct SGSPUnknown SGSPUnknown;
typedef struct SGSPVtbl
{
    HRESULT (WINAPI *QueryInterface)(SGSPUnknown *, REFIID, void **);
    ULONG   (WINAPI *AddRef)(SGSPUnknown *);
    ULONG   (WINAPI *Release)(SGSPUnknown *);
    HRESULT (WINAPI *AddMRUEntry)(SGSPUnknown *, LPCWSTR section, LPCWSTR key, const void *data, DWORD size, DWORD max);
    HRESULT (WINAPI *CreateAddress)(SGSPUnknown *, REFGUID sp, REFGUID type, const void *data, DWORD size, void *address, DWORD *address_size);
    HRESULT (WINAPI *EnumAddress)(SGSPUnknown *, LPDPENUMADDRESSCALLBACK, const void *address, DWORD size, void *context);
    HRESULT (WINAPI *EnumMRUEntries)(SGSPUnknown *, LPCWSTR section, LPCWSTR key, BOOL (CALLBACK *cb)(const void *, DWORD, void *), void *context);
    HRESULT (WINAPI *GetPlayerFlags)(SGSPUnknown *, DPID, DWORD *);
    HRESULT (WINAPI *GetSPPlayerData)(SGSPUnknown *, DPID, void **, DWORD *, DWORD);
    HRESULT (WINAPI *HandleMessage)(SGSPUnknown *, void *body, DWORD size, void *header);
    HRESULT (WINAPI *SetSPPlayerData)(SGSPUnknown *, DPID, void *, DWORD, DWORD);
    HRESULT (WINAPI *CreateCompoundAddress)(SGSPUnknown *, const DPCOMPOUNDADDRESSELEMENT *, DWORD, void *address, DWORD *size);
    HRESULT (WINAPI *GetSPData)(SGSPUnknown *, void **, DWORD *, DWORD);
    HRESULT (WINAPI *SetSPData)(SGSPUnknown *, void *, DWORD, DWORD);
    void    (WINAPI *SendComplete)(SGSPUnknown *, void *, DWORD);
} SGSPVtbl;
struct SGSPUnknown { const SGSPVtbl *lpVtbl; };

/* what the provider reports about its use */
typedef struct SGSPLog
{
    DWORD getaddress_calls;
    DPID  getaddress_player;
    DWORD getaddress_flags;
    DWORD cancel_calls;
    DWORD cancel_flags;
    DWORD cancel_count;
    ULONG_PTR cancel_first;
    DWORD cancel_minprio, cancel_maxprio;
    DWORD createplayer_calls, deleteplayer_calls, deletegroup_calls;
    DPID  deleteplayer_id; DWORD deleteplayer_flags;
    DPID  deletegroup_id;  DWORD deletegroup_flags;
    DWORD sendex_calls;
    DWORD enumsessions_calls;
    DWORD sendtogroup_calls;
    DWORD newid_requests;
    DWORD newid_flags;
    DWORD join_requests;
    DWORD sendex_msgid;
} SGSPLog;

typedef struct SGLSPLog
{
    DWORD getgroupconn_calls; DWORD getgroupconn_flags; DWORD getgroupconn_group;
    DWORD setgroupconn_calls; DWORD setgroupconn_flags; DWORD setgroupconn_group; void *setgroupconn_conn;
    DWORD startsession_calls; DWORD startsession_flags; DWORD startsession_group;
    DWORD chat_calls; DWORD chat_from, chat_to, chat_flags;
} SGLSPLog;

/* Exports of the provider (found with GetProcAddress) */
typedef SGSPLog *(WINAPI *SGSP_GETLOG)(void);
typedef SGSPUnknown *(WINAPI *SGSP_GETISP)(void);
typedef void (WINAPI *SGSP_COMPLETE)(void *msgid, DWORD result);
typedef SGLSPLog *(WINAPI *SGLSP_GETLOG)(void);
typedef SGSPUnknown *(WINAPI *SGLSP_GETISP)(void);

#endif
