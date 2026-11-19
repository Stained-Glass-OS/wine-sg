/* kmtstats-probe (wine-sg 8004): D3DKMTQueryStatistics, which GPU monitors (System Informer, Process
 * Explorer) read for an adapter's memory segments and engines, was a stub that left the result
 * untouched and returned success. The adapter is opened by its display name for its LUID. The structure
 * layout is Windows' (ddk/d3dkmthk.h: the result union is 776 bytes; hProcess makes the query fields 8 bytes
 * later in 64-bit code), so the 32-bit build checks the WoW64 thunk too. */
#include <windows.h>
#include <winternl.h>
#include <stdio.h>
#include <string.h>

typedef UINT D3DKMT_HANDLE;
typedef struct { WCHAR DeviceName[32]; D3DKMT_HANDLE hAdapter; LUID AdapterLuid; UINT VidPnSourceId; } OPENBYNAME;
typedef struct
{
    UINT Type;
    LUID AdapterLuid;
    HANDLE hProcess;
    union { ULONG64 align; BYTE bytes[776]; } QueryResult;
    ULONG QueryId;   /* QuerySegment.SegmentId, QueryNode.NodeId, QueryVidPnSource.VidPnSourceId */
} QUERYSTATISTICS;
enum { ADAPTER, PROCESS, PROCESS_ADAPTER, SEGMENT, PROCESS_SEGMENT, NODE, PROCESS_NODE };

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)
#define STATUS_INVALID_PARAMETER_ ((NTSTATUS)0xc000000d)

static NTSTATUS (WINAPI *pQuery)(QUERYSTATISTICS *);
static LUID luid;

static NTSTATUS query(UINT type, HANDLE process, ULONG id, QUERYSTATISTICS *q)
{
    memset(q, 0xcc, sizeof(*q));
    q->Type = type;
    q->AdapterLuid = luid;
    q->hProcess = process;
    q->QueryId = id;
    return pQuery(q);
}

#define U32(q, off) (*(ULONG *)((q).QueryResult.bytes + (off)))
#define U64(q, off) (*(ULONG64 *)((q).QueryResult.bytes + (off)))

int main(void)
{
    HMODULE gdi32 = GetModuleHandleA("gdi32.dll");
    NTSTATUS (WINAPI *pOpen)(OPENBYNAME *) = (void *)GetProcAddress(gdi32, "D3DKMTOpenAdapterFromGdiDisplayName");
    DISPLAY_DEVICEW dd = {sizeof(dd)};
    OPENBYNAME byname;
    QUERYSTATISTICS q;
    NTSTATUS st;
    ULONG segments, i, local = 0, aperture = 0;
    ULONG64 commit0 = 0;

    pQuery = (void *)GetProcAddress(gdi32, "D3DKMTQueryStatistics");
    check(pQuery && pOpen, "gdi32 exports D3DKMTQueryStatistics");
    if (!pQuery || !pOpen) { printf("RESULT: FAIL\n"); return 1; }
    for (i = 0; EnumDisplayDevicesW(NULL, i, &dd, 0); i++) if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) break;
    memset(&byname, 0, sizeof(byname));
    lstrcpyW(byname.DeviceName, dd.DeviceName);
    st = pOpen(&byname);
    check(!st, "the primary display's adapter: %#lx", st);
    luid = byname.AdapterLuid;

    st = query(ADAPTER, NULL, 0, &q);
    segments = U32(q, 0);
    check(!st && segments >= 1 && segments <= 16 && U32(q, 4) == 1 && U32(q, 8) == 1,
          "ADAPTER: %lu segments, %lu node, %lu VidPN source (%#lx)", segments, U32(q, 4), U32(q, 8), st);
    for (i = 0; i < segments && i < 16; i++)
    {
        st = query(SEGMENT, NULL, i, &q);
        check(!st && U64(q, 0) > 0 && U64(q, 0) != 0xcccccccccccccccc && U64(q, 8) <= U64(q, 0) && U32(q, 40) <= 1,
              "SEGMENT %lu: commit limit %lu MB, committed %lu MB, aperture %lu (%#lx)", i, (ULONG)(U64(q, 0) >> 20),
              (ULONG)(U64(q, 8) >> 20), U32(q, 40), st);
        if (U32(q, 40)) aperture++; else local++;
        if (!i) commit0 = U64(q, 0);
    }
    check(local + aperture == segments, "%lu local and %lu aperture segments", local, aperture);
    st = query(SEGMENT, NULL, segments, &q);
    check(st == STATUS_INVALID_PARAMETER_, "a segment past the last: STATUS_INVALID_PARAMETER (%#lx)", st);

    st = query(PROCESS_SEGMENT, GetCurrentProcess(), 0, &q);
    check(!st && U64(q, 8) == commit0, "PROCESS_SEGMENT 0 of this process: its working set limit is the segment (%#lx)", st);
    st = query(PROCESS_SEGMENT, NULL, 0, &q);
    check(st == STATUS_INVALID_PARAMETER_, "PROCESS_SEGMENT without a process: STATUS_INVALID_PARAMETER (%#lx)", st);
    st = query(PROCESS_ADAPTER, GetCurrentProcess(), 0, &q);
    check(!st && U32(q, 0) == segments && U32(q, 4) == 1, "PROCESS_ADAPTER: the adapter's segments and node (%#lx)", st);
    st = query(NODE, NULL, 0, &q);
    check(!st, "NODE 0 (%#lx)", st);
    st = query(NODE, NULL, 1, &q);
    check(st == STATUS_INVALID_PARAMETER_, "NODE 1: STATUS_INVALID_PARAMETER (%#lx)", st);

    luid.LowPart ^= 0x5a5a5a5a;
    st = query(ADAPTER, NULL, 0, &q);
    check(st == STATUS_INVALID_PARAMETER_, "an unknown adapter LUID: STATUS_INVALID_PARAMETER (%#lx)", st);

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
