/* A network provider that is only a test double: it knows one share, \\sgfake\ok, and records the
 * drive letters it was asked to connect.  No network is involved. */
#define UNICODE
#include <windows.h>
#include <npapi.h>

static WCHAR connected[8][4];
static int nconnected;

DWORD WINAPI NPGetCaps(DWORD index)
{
    switch (index)
    {
    case WNNC_SPEC_VERSION: return 0x00050001;
    case WNNC_NET_TYPE: return 0x00ff0000;
    case WNNC_DRIVER_VERSION: return 1;
    case WNNC_CONNECTION: return WNNC_CON_ADDCONNECTION3 | WNNC_CON_CANCELCONNECTION | WNNC_CON_GETCONNECTIONS;
    default: return 0;
    }
}

DWORD WINAPI NPAddConnection3(HWND hwnd, LPNETRESOURCEW res, LPTSTR password, LPTSTR user, DWORD flags)
{
    int i;

    if (!res || !res->lpRemoteName || lstrcmpiW(res->lpRemoteName, L"\\\\sgfake\\ok")) return WN_BAD_NETNAME;
    if (flags & CONNECT_INTERACTIVE) return WN_BAD_NETNAME;
    if (res->lpLocalName && res->lpLocalName[0])
    {
        for (i = 0; i < nconnected; i++)
            if (connected[i][0] == res->lpLocalName[0]) return WN_ALREADY_CONNECTED;
        if (nconnected < 8) { connected[nconnected][0] = res->lpLocalName[0]; connected[nconnected][1] = ':'; connected[nconnected++][2] = 0; }
    }
    return WN_SUCCESS;
}

DWORD WINAPI NPCancelConnection(LPTSTR name, BOOL force)
{
    return WN_SUCCESS;
}
