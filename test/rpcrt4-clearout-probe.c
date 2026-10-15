/* Probe for patches/sg/5018: NdrClearOutParameters "frees resources of the
 * out parameter and clears its memory if the RPC call to the server fails":
 * an interface pointer is released, a string freed with the stub message's
 * free routine, a structure zeroed to its size and no further; the caller's
 * own variable is never freed. Before a proxy unmarshals anything (another
 * phase, or nothing read from the buffer yet) the parameter is left alone.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#define COBJMACROS
#include <windows.h>
#include <rpcproxy.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static void *allocated[8];
static int n_alloc, n_free, n_bad_free;
static void * __RPC_USER my_alloc(size_t size) { return allocated[n_alloc++] = HeapAlloc(GetProcessHeap(), 0, size); }
static void __RPC_USER my_free(void *p)
{
    int i;
    for (i = 0; i < n_alloc; i++) if (allocated[i] == p) { allocated[i] = NULL; n_free++; HeapFree(GetProcessHeap(), 0, p); return; }
    n_bad_free++;
}

static LONG refs;
static HRESULT WINAPI qi(IUnknown *iface, REFIID riid, void **out) { *out = iface; return S_OK; }
static ULONG WINAPI addref(IUnknown *iface) { return InterlockedIncrement(&refs); }
static ULONG WINAPI release(IUnknown *iface) { return InterlockedDecrement(&refs); }
static IUnknownVtbl vtbl = {qi, addref, release};
static IUnknown unk = {&vtbl};

static const unsigned char fmt_iface[] = {0x11, 0x14, 0x02, 0x00, 0x2f, 0x5a,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46};
static const unsigned char fmt_string[] = {0x11, 0x14, 0x02, 0x00, 0x12, 0x08, 0x22, 0x5c};
static const unsigned char fmt_string_nostack[] = {0x11, 0x10, 0x02, 0x00, 0x12, 0x08, 0x22, 0x5c};
static const unsigned char fmt_struct[] = {0x11, 0x04, 0x02, 0x00, 0x15, 0x03, 0x0c, 0x00, 0x08, 0x08, 0x08, 0x5b};

enum kind { IFACE, STRING, STRUCT, NULLARG };

int main(void)
{
    static const struct { const char *what; enum kind kind; const unsigned char *fmt; DWORD phase; BOOL read; BOOL cleared; } tests[] =
    {
        {"interface, unmarshalling", IFACE, fmt_iface, PROXY_UNMARSHAL, TRUE, TRUE},
        {"interface, nothing read yet", IFACE, fmt_iface, PROXY_UNMARSHAL, FALSE, FALSE},
        {"interface, marshalling", IFACE, fmt_iface, PROXY_MARSHAL, TRUE, FALSE},
        {"interface, sizing", IFACE, fmt_iface, PROXY_CALCSIZE, TRUE, FALSE},
        {"string, unmarshalling", STRING, fmt_string, PROXY_UNMARSHAL, TRUE, TRUE},
        {"string, top pointer not on stack", STRING, fmt_string_nostack, PROXY_UNMARSHAL, TRUE, TRUE},
        {"string, sending", STRING, fmt_string, PROXY_SENDRECEIVE, TRUE, FALSE},
        {"structure, unmarshalling", STRUCT, fmt_struct, PROXY_UNMARSHAL, TRUE, TRUE},
        {"structure, getting a buffer", STRUCT, fmt_struct, PROXY_GETBUFFER, TRUE, FALSE},
        {"NULL parameter", NULLARG, fmt_iface, PROXY_UNMARSHAL, TRUE, FALSE},
    };
    unsigned int i, j;

    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        MIDL_STUB_MESSAGE msg;
        unsigned char buffer[16];
        IUnknown *punk = &unk;
        char *str = NULL;
        unsigned char st[16];
        BOOL zero = TRUE, tail = TRUE;

        memset(&msg, 0, sizeof(msg));
        msg.pfnAllocate = my_alloc;
        msg.pfnFree = my_free;
        msg.dwStubPhase = tests[i].phase;
        msg.BufferStart = buffer;
        msg.BufferEnd = buffer + sizeof(buffer);
        msg.Buffer = tests[i].read ? buffer + 8 : buffer;
        n_alloc = n_free = n_bad_free = 0;
        refs = 1;
        memset(st, 0xcc, sizeof(st));

        switch (tests[i].kind)
        {
        case IFACE:
            NdrClearOutParameters(&msg, (PFORMAT_STRING)tests[i].fmt, &punk);
            if (tests[i].cleared) check(!punk && refs == 0, "%s: released (%ld refs) and cleared (%p)", tests[i].what, refs, punk);
            else check(punk == &unk && refs == 1, "%s: left alone (%ld refs, %p)", tests[i].what, refs, punk);
            break;
        case STRING:
            str = my_alloc(8);
            strcpy(str, "abc");
            NdrClearOutParameters(&msg, (PFORMAT_STRING)tests[i].fmt, &str);
            if (tests[i].cleared)
                check(!str && n_free == 1 && !n_bad_free, "%s: freed (%d, %d bad) and cleared (%p)", tests[i].what, n_free,
                      n_bad_free, str);
            else check(str && !n_free && !n_bad_free, "%s: left alone (%d frees)", tests[i].what, n_free);
            if (str) my_free(str);
            break;
        case STRUCT:
            NdrClearOutParameters(&msg, (PFORMAT_STRING)tests[i].fmt, st);
            for (j = 0; j < 12; j++) if (st[j]) zero = FALSE;
            for (j = 12; j < 16; j++) if (st[j] != 0xcc) tail = FALSE;
            if (tests[i].cleared) check(zero && tail && !n_bad_free, "%s: its 12 bytes cleared, no more (%d %d)", tests[i].what, zero, tail);
            else check(st[0] == 0xcc && tail, "%s: left alone", tests[i].what);
            break;
        case NULLARG:
            NdrClearOutParameters(&msg, (PFORMAT_STRING)tests[i].fmt, NULL);
            check(!n_free && !n_bad_free, "%s: nothing done", tests[i].what);
            break;
        }
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
