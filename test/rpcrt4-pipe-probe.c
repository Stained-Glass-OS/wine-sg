/* NDR [pipe] between two processes: a widl-generated client and server (ncalrpc and ncacn_ip_tcp on 127.0.0.1).
 *   probe server PROTSEQ ENDPOINT    serves until the client calls Stop
 *   probe client PROTSEQ ENDPOINT    runs the checks, prints RESULT: PASS or FAIL
 * Pipes of long, of a structure, of hyper and of char (the chunk header and the elements are aligned apart),
 * [in], [out] and [in, out], megabytes of data, short and long pull and push calls, an Alloc which gives less than
 * asked, a manager which leaves a pipe unread, and pipes between the other parameters and the return value. */
#include <windows.h>
#include <rpc.h>
#include <rpcndr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rpcrt4-pipe.h"

static int failures;
static void check(int ok, const char *what)
{
    static DWORD last;
    DWORD now = GetTickCount();
    printf("%s  %s (%lu ms)\n", ok ? "PASS" : "FAIL", what, last ? now - last : 0);
    last = now;
    fflush(stdout);
    if (!ok) failures++;
}

void *__RPC_USER MIDL_user_allocate(size_t size) { return malloc(size); }
void __RPC_USER MIDL_user_free(void *p) { free(p); }

#define N_BIG 1200000  /* 4.8 MB of long */
#define VAL(i) ((LONG)((ULONG)(i) * 3u + 1u))

/* ---- server: the manager routines ---- */

static LONG sum_pipe(LPIPE p)
{
    LONG buf[777];
    ULONG n, sum = 0, i;
    for (;;)
    {
        n = 0;
        p.Pull(p.state, buf, 777, &n);
        if (!n) break;
        for (i = 0; i < n; i++) sum += (ULONG)buf[i];
    }
    return (LONG)sum;
}

LONG s_SumIn(handle_t h, LPIPE p, LONG scale)
{
    return sum_pipe(p) * scale;
}

void s_Gen(handle_t h, LONG n, LONG chunk, LPIPE p)
{
    /* a negative chunk size: the manager forgets to end the pipe (the stub does it) */
    BOOL forget = chunk < 0;
    LONG *buf;
    LONG done = 0, i;
    if (forget) chunk = -chunk;
    buf = malloc(chunk * sizeof(LONG));
    while (done < n)
    {
        LONG count = min(chunk, n - done);
        for (i = 0; i < count; i++) buf[i] = VAL(done + i);
        p.Push(p.state, buf, count);
        done += count;
    }
    if (!forget) p.Push(p.state, buf, 0);
    free(buf);
}

void s_Echo(handle_t h, LPIPE p)
{
    LONG buf[500];
    ULONG n, i;
    for (;;)
    {
        n = 0;
        p.Pull(p.state, buf, 500, &n);
        if (!n) break;
        for (i = 0; i < n; i++) buf[i] += 1;
        p.Push(p.state, buf, n);
    }
    p.Push(p.state, buf, 0);
}

void s_Recs(handle_t h, RECPIPE r, LONG *total, CPIPE c)
{
    REC buf[100];
    char out[100];
    ULONG n, i, count = 0;
    LONG sum = 0;
    for (;;)
    {
        n = 0;
        r.Pull(r.state, buf, 100, &n);
        if (!n) break;
        for (i = 0; i < n; i++)
        {
            sum += buf[i].a + buf[i].b;
            out[i] = 'A' + (count + i) % 26;
        }
        count += n;
        c.Push(c.state, out, n);
    }
    c.Push(c.state, out, 0);
    *total = sum;
}

LONG s_Skip(handle_t h, LPIPE first, LPIPE second, LONG take_first)
{
    LONG sum = 0;
    if (take_first) sum += sum_pipe(first);
    sum += sum_pipe(second);
    return sum;
}

LONG s_Mixed(handle_t h, LONG a, HPIPE in_p, short b, HPIPE out_p, LONG *c)
{
    LONGLONG buf[64], sum = 0;
    ULONG n, i;
    for (;;)
    {
        n = 0;
        in_p.Pull(in_p.state, buf, 64, &n);
        if (!n) break;
        for (i = 0; i < n; i++) sum += buf[i];
    }
    buf[0] = sum;
    buf[1] = sum + 1;
    out_p.Push(out_p.state, buf, 2);
    buf[0] = a;
    out_p.Push(out_p.state, buf, 1);
    out_p.Push(out_p.state, buf, 0);
    *c = a + b;
    return a * b;
}

void s_Stop(handle_t h)
{
    RpcMgmtStopServerListening(NULL);
}

/* ---- client: sources and sinks ---- */

struct src { LONG n, next; ULONG calls; };

static void WINAPI src_pull(char *state, LONG *buf, ULONG esize, ULONG *ecount)
{
    struct src *s = (struct src *)state;
    ULONG want = 1 + (s->calls++ * 37) % 5000, i;
    if (want > esize) want = esize;
    if (want > (ULONG)(s->n - s->next)) want = s->n - s->next;
    for (i = 0; i < want; i++) buf[i] = VAL(s->next + i);
    s->next += want;
    *ecount = want;
}

struct sink { LONG next, add, bad, ended, pushes; ULONG max_chunk; };

static void WINAPI sink_alloc(char *state, ULONG bsize, LONG **buf, ULONG *bcount)
{
    *buf = malloc(bsize);
    *bcount = min(bsize / sizeof(LONG), 1000);
}

static void WINAPI sink_push(char *state, LONG *buf, ULONG ecount)
{
    struct sink *s = (struct sink *)state;
    ULONG i;
    if (!ecount)
    {
        s->ended++;
        free(buf);
        return;
    }
    s->pushes++;
    if (ecount > s->max_chunk) s->max_chunk = ecount;
    for (i = 0; i < ecount; i++)
        if (buf[i] != VAL(s->next + i) + s->add) s->bad++;
    s->next += ecount;
    free(buf);
}

struct rsrc { LONG n, next; };

static void WINAPI rec_pull(char *state, REC *buf, ULONG esize, ULONG *ecount)
{
    struct rsrc *s = (struct rsrc *)state;
    ULONG want = min(esize, 41), i;
    if (want > (ULONG)(s->n - s->next)) want = s->n - s->next;
    for (i = 0; i < want; i++) { buf[i].a = (s->next + i) * 5; buf[i].b = (short)((s->next + i) % 100); }
    s->next += want;
    *ecount = want;
}

struct csink { LONG count, bad, ended; };

static void WINAPI char_alloc(char *state, ULONG bsize, char **buf, ULONG *bcount)
{
    *buf = malloc(bsize);
    *bcount = bsize;
}

static void WINAPI char_push(char *state, char *buf, ULONG ecount)
{
    struct csink *s = (struct csink *)state;
    ULONG i;
    if (!ecount) { s->ended++; free(buf); return; }
    for (i = 0; i < ecount; i++)
        if (buf[i] != 'A' + (s->count + i) % 26) s->bad++;
    s->count += ecount;
    free(buf);
}

struct hsrc { LONG n, next; };

static void WINAPI hyper_pull(char *state, LONGLONG *buf, ULONG esize, ULONG *ecount)
{
    struct hsrc *s = (struct hsrc *)state;
    ULONG want = min(esize, 9), i;
    if (want > (ULONG)(s->n - s->next)) want = s->n - s->next;
    for (i = 0; i < want; i++) buf[i] = ((LONGLONG)1 << 40) + s->next + i;
    s->next += want;
    *ecount = want;
}

struct hsink { LONGLONG v[8]; LONG count, ended; };

static void WINAPI hyper_alloc(char *state, ULONG bsize, LONGLONG **buf, ULONG *bcount)
{
    *buf = malloc(bsize);
    *bcount = bsize / sizeof(LONGLONG);
}

static void WINAPI hyper_push(char *state, LONGLONG *buf, ULONG ecount)
{
    struct hsink *s = (struct hsink *)state;
    ULONG i;
    if (!ecount) { s->ended++; free(buf); return; }
    for (i = 0; i < ecount && s->count < 8; i++) s->v[s->count++] = buf[i];
    free(buf);
}

struct echo { LONG n, next; ULONG calls; LONG rnext, bad, ended; };

static void WINAPI echo_pull(char *state, LONG *buf, ULONG esize, ULONG *ecount)
{
    struct echo *e = (struct echo *)state;
    ULONG want = min(esize, 1 + (e->calls++ * 13) % 900), i;
    if (want > (ULONG)(e->n - e->next)) want = e->n - e->next;
    for (i = 0; i < want; i++) buf[i] = VAL(e->next + i);
    e->next += want;
    *ecount = want;
}

static void WINAPI echo_alloc(char *state, ULONG bsize, LONG **buf, ULONG *bcount)
{
    *buf = malloc(bsize);
    *bcount = bsize / sizeof(LONG);
}

static void WINAPI echo_push(char *state, LONG *buf, ULONG ecount)
{
    struct echo *e = (struct echo *)state;
    ULONG i;
    if (!ecount) { e->ended++; free(buf); return; }
    for (i = 0; i < ecount; i++)
        if (buf[i] != VAL(e->rnext + i) + 1) e->bad++;
    e->rnext += ecount;
    free(buf);
}

static ULONG expect_sum(LONG n)
{
    ULONG sum = 0;
    LONG i;
    for (i = 0; i < n; i++) sum += (ULONG)VAL(i);
    return sum;
}

static void run_checks(handle_t h)
{
    struct src s;
    struct sink k;
    struct echo e;
    LPIPE in, out, io;
    LONG ret;

    /* [in] pipe, megabytes, with the other parameters around it */
    memset(&s, 0, sizeof(s)); s.n = N_BIG;
    in.Pull = src_pull; in.Push = NULL; in.Alloc = NULL; in.state = (char *)&s;
    ret = SumIn(h, in, 3);
    check(ret == (LONG)(expect_sum(N_BIG) * 3u), "[in] pipe of 1.2 million longs: the sum comes back scaled by the other parameter");
    check(s.next == N_BIG && s.calls > 200, "the stub pulled the source to its end with many pull calls");

    memset(&s, 0, sizeof(s)); s.n = 0;
    ret = SumIn(h, in, 7);
    check(ret == 0, "an empty [in] pipe");

    memset(&s, 0, sizeof(s)); s.n = 1;
    ret = SumIn(h, in, 7);
    check(ret == 7, "an [in] pipe of one element");

    /* [out] pipe */
    memset(&k, 0, sizeof(k));
    out.Pull = NULL; out.Push = sink_push; out.Alloc = sink_alloc; out.state = (char *)&k;
    Gen(h, N_BIG, 4096, out);
    check(k.next == N_BIG && k.bad == 0 && k.ended == 1, "[out] pipe of 1.2 million longs in chunks of 4096: every element, one end");
    check(k.max_chunk <= 1000 && k.pushes >= 1200, "an Alloc which gives less than asked: the stub pushes in pieces of the size it got");

    memset(&k, 0, sizeof(k));
    Gen(h, 0, 10, out);
    check(k.next == 0 && k.ended == 1 && k.pushes == 0, "an empty [out] pipe");

    memset(&k, 0, sizeof(k));
    Gen(h, 1, 10, out);
    check(k.next == 1 && k.bad == 0 && k.ended == 1, "an [out] pipe of one element");

    /* a manager which forgets to end its [out] pipe: the stub ends it */
    memset(&k, 0, sizeof(k));
    Gen(h, 1000, -300, out);
    check(k.next == 1000 && k.bad == 0 && k.ended == 1, "an [out] pipe the manager did not end is ended when the manager returns");

    /* [in, out] pipe: pulled and pushed back, interleaved on the server */
    memset(&e, 0, sizeof(e)); e.n = 100000;
    io.Pull = echo_pull; io.Push = echo_push; io.Alloc = echo_alloc; io.state = (char *)&e;
    Echo(h, io);
    check(e.rnext == 100000 && e.bad == 0 && e.ended == 1, "[in, out] pipe of 100000 longs: every element comes back incremented");

    /* a pipe of structures, an [out] pipe of char and an [out] long after them */
    {
        struct rsrc rs = { 5000, 0 };
        struct csink cs = { 0, 0, 0 };
        RECPIPE rp;
        CPIPE cp;
        LONG total = 0, want = 0, i;

        rp.Pull = rec_pull; rp.Push = NULL; rp.Alloc = NULL; rp.state = (char *)&rs;
        cp.Pull = NULL; cp.Push = char_push; cp.Alloc = char_alloc; cp.state = (char *)&cs;
        for (i = 0; i < 5000; i++) want += i * 5 + i % 100;
        Recs(h, rp, &total, cp);
        check(total == want, "[in] pipe of structures (long, short): the [out] long after the pipes is right");
        check(cs.count == 5000 && cs.bad == 0 && cs.ended == 1, "[out] pipe of char: 5000 bytes in order, one end");
    }

    /* a manager which never reads the first pipe, then one which reads both */
    {
        struct src a, b;
        LPIPE pa, pb;

        memset(&a, 0, sizeof(a)); a.n = 5000;
        memset(&b, 0, sizeof(b)); b.n = 7000;
        pa.Pull = pb.Pull = src_pull; pa.Push = pb.Push = NULL; pa.Alloc = pb.Alloc = NULL;
        pa.state = (char *)&a; pb.state = (char *)&b;
        ret = Skip(h, pa, pb, 0);
        check(ret == (LONG)expect_sum(7000), "a manager which leaves the first [in] pipe unread still reads the second");
        check(a.next == 5000 && b.next == 7000, "the client's pull routines ran to the end all the same");

        memset(&a, 0, sizeof(a)); a.n = 5000;
        memset(&b, 0, sizeof(b)); b.n = 7000;
        ret = Skip(h, pa, pb, 1);
        check(ret == (LONG)(expect_sum(5000) + expect_sum(7000)), "a manager which reads both pipes");
    }

    /* pipes between the other parameters: hyper elements (8 byte aligned), [out] pipe, [out] long, return value */
    {
        struct hsrc hs = { 100, 0 };
        struct hsink hk;
        HPIPE hi, ho;
        LONG c = 0;
        LONGLONG sum = 0;
        int i;

        memset(&hk, 0, sizeof(hk));
        hi.Pull = hyper_pull; hi.Push = NULL; hi.Alloc = NULL; hi.state = (char *)&hs;
        ho.Pull = NULL; ho.Push = hyper_push; ho.Alloc = hyper_alloc; ho.state = (char *)&hk;
        for (i = 0; i < 100; i++) sum += ((LONGLONG)1 << 40) + i;
        ret = Mixed(h, 1000, hi, -7, ho, &c);
        check(ret == -7000 && c == 993, "the return value and the [out] long after the pipes");
        check(hk.count == 3 && hk.v[0] == sum && hk.v[1] == sum + 1 && hk.v[2] == 1000 && hk.ended == 1,
              "hyper pipes: the sum of 100 values (8 byte elements) comes back in an [out] pipe");
    }
}


/* ---- the wire: requests and responses written out by hand ---- */

#define U32(v) (v) & 0xff, ((v) >> 8) & 0xff, ((v) >> 16) & 0xff, ((v) >> 24) & 0xff
#define U64(v) U32((unsigned)(v)), U32((unsigned)((unsigned long long)(v) >> 32))

/* SumIn(h, pipe of VAL(0), VAL(1), VAL(2) pulled 1 then 2 at a time, 3): the non-pipe parameter first,
 * then the chunks: a count and the elements, the end a chunk of none */
static const unsigned char sumin_req[] = {
    U32(3),
    U32(1), U32(1),
    U32(2), U32(4), U32(7),
    U32(0)
};
static const unsigned char sumin_resp[] = { U32(36) };

/* Mixed(h, 1000, pipe of 1<<40, (1<<40)+1, -7, out pipe, &c): long, short (2 bytes of padding),
 * the chunk (its count, 4 bytes of padding so that the hyper elements are 8 byte aligned), the end */
#define H0 (1ULL << 40)
#define HS (2ULL * H0 + 1)
static const unsigned char mixed_req[] = {
    U32(1000), 0xf9, 0xff, 0, 0,
    U32(2), U32(0), U64(H0), U64(H0 + 1),
    U32(0)
};
/* the [out] pipe first: chunk of 2 (sum, sum+1), chunk of 1 (1000), the end; then the [out] long and the return value */
static const unsigned char mixed_resp[] = {
    U32(2), U32(0), U64(HS), U64(HS + 1),
    U32(1), U32(0), U64(1000),
    U32(0),
    U32(993),
    U32((unsigned)-7000)
};

static void raw_reply(PRPC_MESSAGE msg, const void *data, ULONG len)
{
    msg->BufferLength = len;
    if (I_RpcGetBuffer(msg)) RpcRaiseException(RPC_S_OUT_OF_MEMORY);
    memcpy(msg->Buffer, data, len);
}

static void dump(const char *what, const unsigned char *p, ULONG len)
{
    ULONG i;
    printf("raw: %s (%lu bytes):", what, len);
    for (i = 0; i < len; i++) printf(" %02x", p[i]);
    printf("\n");
    fflush(stdout);
}

static BOOL raw_expect(PRPC_MESSAGE msg, const char *what, const void *req, ULONG len)
{
    BOOL ok = msg->BufferLength == len && !memcmp(msg->Buffer, req, len);
    if (!ok)
    {
        printf("raw: the %s request is not what the wire format asks for\n", what);
        dump("expected", req, len);
        dump("got", msg->Buffer, msg->BufferLength);
    }
    return ok;
}

static void __RPC_STUB raw_sumin(PRPC_MESSAGE msg)
{
    static const unsigned char bad[] = { U32(0xdeadbeef) };
    if (raw_expect(msg, "SumIn", sumin_req, sizeof(sumin_req))) raw_reply(msg, sumin_resp, sizeof(sumin_resp));
    else raw_reply(msg, bad, sizeof(bad));
}

static void __RPC_STUB raw_mixed(PRPC_MESSAGE msg)
{
    unsigned char bad[sizeof(mixed_resp)];
    if (raw_expect(msg, "Mixed", mixed_req, sizeof(mixed_req))) raw_reply(msg, mixed_resp, sizeof(mixed_resp));
    else { memset(bad, 0xee, sizeof(bad)); raw_reply(msg, bad, sizeof(bad)); }
}

static void __RPC_STUB raw_stop(PRPC_MESSAGE msg)
{
    raw_reply(msg, "", 0);
    RpcMgmtStopServerListening(NULL);
}

static void __RPC_STUB raw_other(PRPC_MESSAGE msg)
{
    printf("raw: unexpected call of procedure %u\n", msg->ProcNum);
    RpcRaiseException(RPC_S_PROCNUM_OUT_OF_RANGE);
}

static RPC_SERVER_INTERFACE raw_if;
static RPC_DISPATCH_TABLE raw_table;
static RPC_DISPATCH_FUNCTION raw_funcs[7] = { raw_sumin, raw_other, raw_other, raw_other, raw_other, raw_mixed, raw_stop };

static int run_server_if(const char *protseq, const char *endpoint, RPC_IF_HANDLE ifh)
{
    RPC_STATUS status;

    status = RpcServerUseProtseqEpA((RPC_CSTR)protseq, RPC_C_PROTSEQ_MAX_REQS_DEFAULT, (RPC_CSTR)endpoint, NULL);
    if (status) { printf("server: RpcServerUseProtseqEp failed %lu\n", status); return 1; }
    status = RpcServerRegisterIf(ifh, NULL, NULL);
    if (status) { printf("server: RpcServerRegisterIf failed %lu\n", status); return 1; }
    printf("READY\n");
    fflush(stdout);
    status = RpcServerListen(1, RPC_C_LISTEN_MAX_CALLS_DEFAULT, FALSE);
    printf("server: done (%lu)\n", status);
    return 0;
}

static int run_server(const char *protseq, const char *endpoint)
{
    return run_server_if(protseq, endpoint, s_PipeT_v1_0_s_ifspec);
}

static int run_rawserver(const char *protseq, const char *endpoint)
{
    memcpy(&raw_if, s_PipeT_v1_0_s_ifspec, sizeof(raw_if));
    raw_table.DispatchTableCount = 7;
    raw_table.DispatchTable = raw_funcs;
    raw_if.DispatchTable = &raw_table;
    return run_server_if(protseq, endpoint, (RPC_IF_HANDLE)&raw_if);
}

/* a request sent as bytes to the real server; the response must be the bytes of the wire format */
static RPC_STATUS raw_call(handle_t h, unsigned proc, const void *req, ULONG len, unsigned char **resp, ULONG *resp_len)
{
    RPC_MESSAGE m;
    RPC_STATUS status;

    memset(&m, 0, sizeof(m));
    m.Handle = h;
    m.ProcNum = proc | RPC_FLAGS_VALID_BIT;
    m.RpcInterfaceInformation = (void *)PipeT_v1_0_c_ifspec;
    m.DataRepresentation = NDR_LOCAL_DATA_REPRESENTATION;
    m.BufferLength = len;
    status = I_RpcGetBuffer(&m);
    if (status) return status;
    memcpy(m.Buffer, req, len);
    status = I_RpcSendReceive(&m);
    if (status) return status;
    *resp = malloc(m.BufferLength);
    memcpy(*resp, m.Buffer, m.BufferLength);
    *resp_len = m.BufferLength;
    I_RpcFreeBuffer(&m);
    return RPC_S_OK;
}

static void run_rawclient(handle_t h)
{
    unsigned char *resp;
    ULONG len;
    RPC_STATUS status;

    status = raw_call(h, 0, sumin_req, sizeof(sumin_req), &resp, &len);
    check(!status && len == sizeof(sumin_resp) && !memcmp(resp, sumin_resp, len),
          "the server stub reads a request of the wire format by hand and answers with the bytes of the format (SumIn)");
    if (!status && (len != sizeof(sumin_resp) || memcmp(resp, sumin_resp, len))) dump("SumIn response", resp, len);

    status = raw_call(h, 5, mixed_req, sizeof(mixed_req), &resp, &len);
    check(!status && len == sizeof(mixed_resp) && !memcmp(resp, mixed_resp, len),
          "the same with 8 byte elements and the [out] pipe before the other [out] parameters (Mixed)");
    if (!status && (len != sizeof(mixed_resp) || memcmp(resp, mixed_resp, len))) dump("Mixed response", resp, len);
}

/* the widl client against a server which checks the request bytes */
static void run_rawcheck(handle_t h)
{
    struct src s;
    struct hsrc hs = { 2, 0 };
    struct hsink hk;
    LPIPE in;
    HPIPE hi, ho;
    LONG ret, c = 0;

    memset(&s, 0, sizeof(s)); s.n = 3;
    in.Pull = src_pull; in.Push = NULL; in.Alloc = NULL; in.state = (char *)&s;
    ret = SumIn(h, in, 3);
    check(ret == 36, "the client stub sends the request of the wire format (SumIn)");

    memset(&hk, 0, sizeof(hk));
    hi.Pull = hyper_pull; hi.Push = NULL; hi.Alloc = NULL; hi.state = (char *)&hs;
    ho.Pull = NULL; ho.Push = hyper_push; ho.Alloc = hyper_alloc; ho.state = (char *)&hk;
    ret = Mixed(h, 1000, hi, -7, ho, &c);
    check(ret == -7000 && c == 993 && hk.count == 3 && hk.v[0] == (LONGLONG)HS && hk.v[1] == (LONGLONG)HS + 1 && hk.v[2] == 1000 && hk.ended == 1,
          "and reads the response of the wire format (Mixed)");
}

static int run_client(const char *protseq, const char *endpoint, int mode)
{
    RPC_STATUS status;
    RPC_CSTR str = NULL;
    handle_t h = NULL;

    status = RpcStringBindingComposeA(NULL, (RPC_CSTR)protseq, (RPC_CSTR)(strcmp(protseq, "ncalrpc") ? "127.0.0.1" : NULL),
                                      (RPC_CSTR)endpoint, NULL, &str);
    if (!status) status = RpcBindingFromStringBindingA(str, &h);
    check(!status, "binding");
    if (status) return 1;
    RpcStringFreeA(&str);

    if (mode == 1) run_rawclient(h);
    else if (mode == 2) run_rawcheck(h);
    else run_checks(h);
    Stop(h);
    RpcBindingFree(&h);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}

static LONG WINAPI filter(EXCEPTION_POINTERS *ep)
{
    printf("FAIL  exception %08lx\n", ep->ExceptionRecord->ExceptionCode);
    printf("RESULT: FAIL\n");
    fflush(stdout);
    ExitProcess(3);
    return EXCEPTION_EXECUTE_HANDLER;
}

int main(int argc, char **argv)
{
    SetUnhandledExceptionFilter(filter);
    if (argc < 4) return 2;
    if (!strcmp(argv[1], "server")) return run_server(argv[2], argv[3]);
    if (!strcmp(argv[1], "rawserver")) return run_rawserver(argv[2], argv[3]);
    if (!strcmp(argv[1], "rawclient")) return run_client(argv[2], argv[3], 1);
    if (!strcmp(argv[1], "rawcheck")) return run_client(argv[2], argv[3], 2);
    return run_client(argv[2], argv[3], 0);
}
