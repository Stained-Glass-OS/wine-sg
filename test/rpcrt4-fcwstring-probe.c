/* Probe for patches/sg/5040: an FC_WSTRING (non-conformant wide string, a
 * fixed number of characters) is sized, marshalled, unmarshalled and memory-
 * sized through the NDR dispatch tables, as FC_CSTRING already was; its slot
 * was empty, so a pointer to one failed ("no marshaller"). Uses a unique
 * pointer to the string, through NdrPointer*.
 * Table driven; lines "ok"/"FAIL", last "RESULT: PASS|FAIL". */
#include <windows.h>
#include <rpcndr.h>
#include <stdio.h>

static int failures;
#define check(c, ...) do { if (c) printf("ok   "); else { printf("FAIL "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static void * __RPC_USER my_alloc(size_t size) { return HeapAlloc(GetProcessHeap(), 0, size); }
static void __RPC_USER my_free(void *p) { HeapFree(GetProcessHeap(), 0, p); }

int main(void)
{
    /* FC_UP [simple_pointer] -> FC_WSTRING, FC_PAD, 12 characters; FC_CSTRING alike */
    static const unsigned char fmt_w[] = {0x12, 0x08, 0x29, 0x5c, 12, 0};
    static const unsigned char fmt_c[] = {0x12, 0x08, 0x26, 0x5c, 12, 0};
    static const struct { const char *what; const unsigned char *fmt; BOOL wide; const char *text; } tests[] =
    {
        {"FC_WSTRING", fmt_w, TRUE, "hello"}, {"FC_WSTRING, 11 characters", fmt_w, TRUE, "hello world"},
        {"FC_CSTRING", fmt_c, FALSE, "hello"},
    };
    MIDL_STUB_DESC desc;
    unsigned int i;

    memset(&desc, 0, sizeof(desc));
    desc.pfnAllocate = my_alloc;
    desc.pfnFree = my_free;
    desc.Version = 0x50002;
    desc.MIDLVersion = 0x50100a4;
    for (i = 0; i < ARRAYSIZE(tests); i++)
    {
        RPC_MESSAGE rpc;
        MIDL_STUB_MESSAGE msg;
        WCHAR wide[12];
        char narrow[12];
        unsigned char *mem = tests[i].wide ? (unsigned char *)wide : (unsigned char *)narrow, *out = NULL;
        ULONG size, len = strlen(tests[i].text) + 1, j;
        BOOL same;

        memset(wide, 0, sizeof(wide)); memset(narrow, 0, sizeof(narrow));
        for (j = 0; j < len; j++) { wide[j] = tests[i].text[j]; narrow[j] = tests[i].text[j]; }
        memset(&rpc, 0, sizeof(rpc));
        NdrClientInitializeNew(&rpc, &msg, &desc, 0);
        msg.BufferLength = 0;
        NdrPointerBufferSize(&msg, mem, (PFORMAT_STRING)tests[i].fmt);
        size = msg.BufferLength;
        check(size >= 4 + 8 + len * (tests[i].wide ? 2 : 1), "%s: buffer size %lu", tests[i].what, size);
        msg.RpcMsg->Buffer = msg.BufferStart = msg.Buffer = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, size + 16);
        msg.BufferEnd = msg.BufferStart + size;
        NdrPointerMarshall(&msg, mem, (PFORMAT_STRING)tests[i].fmt);
        check(msg.Buffer - msg.BufferStart == size, "%s: marshalled %lu bytes", tests[i].what, (ULONG)(msg.Buffer - msg.BufferStart));
        msg.Buffer = msg.BufferStart;
        size = NdrPointerMemorySize(&msg, (PFORMAT_STRING)tests[i].fmt);
        check(msg.Buffer - msg.BufferStart >= 12, "%s: memory-sized (%lu, read %lu)", tests[i].what, size,
              (ULONG)(msg.Buffer - msg.BufferStart));
        msg.Buffer = msg.BufferStart;
        msg.MemorySize = 0;
        NdrPointerUnmarshall(&msg, &out, (PFORMAT_STRING)tests[i].fmt, 1);
        same = out && (tests[i].wide ? !lstrcmpW((WCHAR *)out, wide) : !strcmp((char *)out, narrow));
        check(same, "%s: unmarshalled \"%s\"", tests[i].what, same ? tests[i].text : "?");
        if (out) my_free(out);
        HeapFree(GetProcessHeap(), 0, msg.BufferStart);
    }
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return !!failures;
}
