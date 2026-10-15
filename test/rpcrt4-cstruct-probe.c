/* NdrConformantStructMemorySize (a stub that returned 0 and moved nothing) and NdrContextHandleSize (did
 * nothing).  A conformant struct is marshalled with the Ndr functions and the format strings widl generated
 * (this file includes the generated client stub for its type format string); then the memory size of the
 * data in the buffer must be the in-memory size of the struct with its array and pointees, the buffer
 * position must end where NdrConformantStructUnmarshall ends, and MemorySize must not be touched by it. */
#include <windows.h>
#include <rpc.h>
#include <rpcndr.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rpcrt4-cstruct_c.c"

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

void *__RPC_USER MIDL_user_allocate(size_t size) { return malloc(size); }
void __RPC_USER MIDL_user_free(void *p) { free(p); }

#define FC_CSTRUCT_BYTE 0x17
#define FC_CPSTRUCT_BYTE 0x18

/* the struct format: type, alignment - 1, memory size (a WORD) */
static const unsigned char *find_type(unsigned char type, unsigned short memory_size)
{
    const unsigned char *f = __MIDL_TypeFormatString.Format;
    size_t i;
    for (i = 0; i + 8 < sizeof(__MIDL_TypeFormatString.Format); i++)
        if (f[i] == type && (f[i + 1] == 3 || f[i + 1] == 7) && *(const unsigned short *)(f + i + 2) == memory_size)
            return f + i;
    return NULL;
}

static void run_case(const unsigned char *fmt, void *mem, const char *name, ULONG expect_mem)
{
    MIDL_STUB_MESSAGE sm;
    RPC_MESSAGE rpc;
    unsigned char *buf, *start;
    ULONG size, mem_size;
    unsigned char *copy = NULL;
    char text[160];

    memset(&sm, 0, sizeof(sm));
    memset(&rpc, 0, sizeof(rpc));
    NdrClientInitializeNew(&rpc, &sm, &CStructTest_StubDesc, 0);
    sm.BufferLength = 0;
    NdrConformantStructBufferSize(&sm, mem, fmt);
    size = sm.BufferLength;
    buf = start = calloc(1, size + 64);
    rpc.Buffer = buf;
    sm.Buffer = sm.BufferStart = buf;
    sm.BufferEnd = buf + size;
    NdrConformantStructMarshall(&sm, mem, fmt);
    check(sm.Buffer - start <= size, "marshalling fits the buffer size");
    size = sm.Buffer - start;

    /* memory size of what is in the buffer */
    sm.Buffer = start;
    sm.BufferEnd = start + size;
    sm.MemorySize = 0;
    mem_size = NdrConformantStructMemorySize(&sm, fmt);
    sprintf(text, "%s: the memory size is %lu (expected %lu), MemorySize %lu", name, mem_size, expect_mem, sm.MemorySize);
    check(mem_size == expect_mem && sm.MemorySize == expect_mem, text);
    sprintf(text, "%s: the buffer ends where the data does (%ld of %lu bytes)", name, (long)(sm.Buffer - start), size);
    check(sm.Buffer == start + size, text);

    /* the same position as unmarshalling gives */
    sm.Buffer = start;
    sm.BufferEnd = start + size;
    sm.IsClient = 0;
    sm.StubDesc = &CStructTest_StubDesc;
    NdrConformantStructUnmarshall(&sm, &copy, fmt, 1);
    sprintf(text, "%s: unmarshalling ends at the same place (%ld)", name, (long)(sm.Buffer - start));
    check(sm.Buffer == start + size && copy, text);
    free(buf);
}

int main(void)
{
    const unsigned char *cs_fmt = find_type(FC_CSTRUCT_BYTE, offsetof(cs_t, a)),
                        *cps_fmt = find_type(FC_CPSTRUCT_BYTE, offsetof(cps_t, a));
    cs_t *cs;
    cps_t *cps;
    ULONG n, i;
    LONG pointee = 77;
    MIDL_STUB_MESSAGE sm;
    char text[120];
    static const unsigned char ctx_fmt[] = { 0x30 /* FC_BIND_CONTEXT */, 0x30, 0, 0 };

    check(cs_fmt != NULL, "the format string of the conformant struct is found");
    /* with 64-bit pointers widl describes the struct with a pointer as a complex struct: only 32-bit has a CPSTRUCT */
    check(cps_fmt != NULL || sizeof(void *) == 8, "the format string of the struct with a pointer is found (32-bit)");
    if (!cs_fmt) { printf("RESULT: FAIL\n"); return 1; }

    for (n = 0; n <= 6; n += 3)
    {
        cs = calloc(1, offsetof(cs_t, a) + n * sizeof(LONG) + 8);
        cs->n = n;
        for (i = 0; i < n; i++) cs->a[i] = i + 1;
        sprintf(text, "conformant struct with %lu elements", n);
        run_case(cs_fmt, cs, text, offsetof(cs_t, a) + n * sizeof(LONG));
        free(cs);
    }

    for (n = 0; cps_fmt && n <= 4; n += 4)
    {
        cps = calloc(1, offsetof(cps_t, a) + n * sizeof(LONG) + 8);
        cps->n = n;
        cps->q = &pointee;
        for (i = 0; i < n; i++) cps->a[i] = i + 1;
        sprintf(text, "conformant struct with a pointer and %lu elements", n);
        run_case(cps_fmt, cps, text, offsetof(cps_t, a) + n * sizeof(LONG) + sizeof(LONG));
        cps->q = NULL;
        sprintf(text, "the same with a NULL pointer, %lu elements", n);
        run_case(cps_fmt, cps, text, offsetof(cps_t, a) + n * sizeof(LONG));
        free(cps);
    }

    /* NdrContextHandleSize: aligned to 4, then the 20 bytes of a wire context handle */
    memset(&sm, 0, sizeof(sm));
    sm.BufferLength = 5;
    NdrContextHandleSize(&sm, NULL, ctx_fmt);
    sprintf(text, "NdrContextHandleSize from 5: %lu", sm.BufferLength);
    check(sm.BufferLength == 28, text);
    sm.BufferLength = 0;
    NdrContextHandleSize(&sm, NULL, ctx_fmt);
    check(sm.BufferLength == 20, "NdrContextHandleSize from 0: 20");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
