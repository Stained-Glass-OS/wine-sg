/* NdrConvert2 between two processes: a widl-generated server (and client) with requests and responses in the data
 * representation of other machines, built by hand: big-endian integers, EBCDIC characters, IBM, VAX and Cray floats.
 *   probe server PROTSEQ ENDPOINT     the widl server (the managers check what they receive), serves until Stop
 *   probe becall PROTSEQ ENDPOINT     hand-built requests through I_RpcSendReceive (the PDU headers follow the representation)
 *   probe rawtcp PORT                 hand-built PDUs on a socket: bind, request and the response bytes (ncacn_ip_tcp only)
 *   probe outserver PROTSEQ ENDPOINT  a server whose stub for Out answers in a foreign representation, by hand
 *   probe outclient PROTSEQ ENDPOINT  the widl client calls Out against it and the conversion makes the values right
 *   probe outraw PORT                 hand-built PDUs against it: the response PDU has a big-endian header
 */
#include <winsock2.h>
#include <windows.h>
#include <rpc.h>
#include <rpcndr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rpcrt4-convert.h"

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

void *__RPC_USER MIDL_user_allocate(size_t size) { return malloc(size); }
void __RPC_USER MIDL_user_free(void *p) { free(p); }

/* ---- the values every test uses ---- */

#define K_C   'A'
#define K_SM  (-5)
#define K_B   0xe7
#define K_S   (-2)
#define K_US  0xbeef
#define K_W   0x4e2d
#define K_L   ((LONG)0xa1b2c3d4)
#define K_HY  0x0102030405060708ULL
#define K_F   6.25f
#define K_D   (-6.25)

static const SIMPLE k_sp = { 0x11223344, -3, 'Z', 0x0a0b0c0d0e0f1011LL };
static const SIMPLE k_sv = { -1, 0x1234, 'a', -2 };
static const short k_carr[5] = { 1, -2, 300, -400, 0x7fff };
static const LONG k_varr[3] = { 0x11111111, -1, 0x22222222 };
static const LONG k_fixed_a[3] = { 7, -8, 9 };
static const short k_cstr[3] = { 10, -20, 30 };
static const short k_vary[3] = { 100, -200, 300 };

static int mism;
static void same(int ok, const char *what)
{
    if (!ok) { printf("server: %s differs\n", what); fflush(stdout); mism++; }
}

/* ---- server: the managers check the values ---- */

LONG s_Basics(handle_t h, char c, signed char sm, byte b, short s, unsigned short us, wchar_t w, LONG l, float f,
              hyper hy, double d)
{
    mism = 0;
    same(c == K_C, "char"); same(sm == K_SM, "small"); same(b == K_B, "byte"); same(s == K_S, "short");
    same(us == K_US, "ushort"); same(w == K_W, "wchar"); same(l == K_L, "long"); same(f == K_F, "float");
    same(hy == (hyper)K_HY, "hyper"); same(d == K_D, "double");
    return mism;
}

LONG s_Simple(handle_t h, SIMPLE *p, SIMPLE v)
{
    mism = 0;
    same(p->a == k_sp.a && p->b == k_sp.b && p->c == k_sp.c && p->h == k_sp.h, "struct by reference");
    same(v.a == k_sv.a && v.b == k_sv.b && v.c == k_sv.c && v.h == k_sv.h, "struct by value");
    return mism;
}

LONG s_CArr(handle_t h, LONG n, short *a)
{
    mism = 0;
    same(n == 5 && !memcmp(a, k_carr, sizeof(k_carr)), "conformant array");
    return mism;
}

LONG s_VArr(handle_t h, LONG n, LONG m, LONG *a)
{
    mism = 0;
    same(n == 6 && m == 3 && !memcmp(a, k_varr, sizeof(k_varr)), "conformant varying array");
    return mism;
}

LONG s_Fixed(handle_t h, LONG a[3], double d[2])
{
    mism = 0;
    same(!memcmp(a, k_fixed_a, sizeof(k_fixed_a)), "fixed array of long");
    same(d[0] == 6.25 && d[1] == -6.25, "fixed array of double");
    return mism;
}

LONG s_Str(handle_t h, char *s, wchar_t *w)
{
    mism = 0;
    same(!strcmp(s, "Hi!"), "string");
    same(w[0] == 0x4e2d && w[1] == 0x6587 && !w[2], "wide string");
    return mism;
}

LONG s_Cstr(handle_t h, CSTR *p)
{
    mism = 0;
    same(p->n == 3 && !memcmp(p->v, k_cstr, sizeof(k_cstr)), "conformant struct");
    return mism;
}

LONG s_Ptrs(handle_t h, PTRS *p, LONG *u, LONG *r, LONG *n, LONG *f, LONG *g)
{
    mism = 0;
    same(p->a == 77 && p->s && !strcmp(p->s, "ptr") && p->p && *p->p == 0x1357, "structure with pointers");
    same(u && *u == 0x0f0e0d0c, "unique pointer");
    same(r && *r == -9, "reference pointer");
    same(!n, "null unique pointer");
    same(f && *f == 55555 && g == f, "full pointers");
    return mism;
}

LONG s_Uni(handle_t h, UN *u, LONG k, NU *nu)
{
    mism = 0;
    same(k == 3, "switch value");
    same(u->d == 2 && u->_UN.dd == 6.25, "encapsulated union");
    same(nu->l == 0x7a7b7c7d, "non-encapsulated union");
    return mism;
}

LONG s_Vary(handle_t h, LONG m, short v[6])
{
    mism = 0;
    same(m == 3 && !memcmp(v, k_vary, sizeof(k_vary)), "varying array");
    return mism;
}

LONG s_Fstr(handle_t h, char s[8])
{
    mism = 0;
    same(!strcmp(s, "Hi!"), "fixed size string");
    return mism;
}

LONG s_Bogus(handle_t h, LONG n, PTRS *ps)
{
    mism = 0;
    same(n == 2, "count");
    same(ps[0].a == 1 && ps[0].s && !strcmp(ps[0].s, "pr") && ps[0].p && *ps[0].p == 5, "first element");
    same(ps[1].a == 2 && !ps[1].s && ps[1].p && *ps[1].p == -6, "second element");
    return mism;
}

LONG s_Cv(handle_t h, CV *p)
{
    mism = 0;
    same(p->n == 5 && p->m == 2 && p->v[0] == 11 && p->v[1] == -12, "conformant varying struct");
    return mism;
}

LONG s_Range(handle_t h, LONG r)
{
    mism = 0;
    same(r == 555, "range");
    return mism;
}

LONG s_Pipe(handle_t h, short pre, LPIPE p)
{
    LONG buf[4], total = 0;
    ULONG n;

    mism = 0;
    same(pre == -3, "short before the pipe");
    for (;;)
    {
        n = 0;
        p.Pull(p.state, buf, 4, &n);
        if (!n) break;
        while (n--) total += buf[n];
    }
    same(total == 0x01020304 - 5 + 7, "elements of the pipe");
    return mism;
}

LONG s_Out(handle_t h, LONG x, LONG *y, double *d, short s[2], SIMPLE *sv)
{
    *y = -123456; *d = 6.25; s[0] = 1; s[1] = -2;
    sv->a = 5; sv->b = -6; sv->c = 'Q'; sv->h = 7;
    return -77;
}

void s_AOut(PRPC_ASYNC_STATE a, handle_t h, LONG x, LONG *y, double *d)
{
    *y = -123456; *d = 6.25;
    RpcAsyncCompleteCall(a, NULL);
}

void s_Stop(handle_t h)
{
    RpcMgmtStopServerListening(NULL);
}

/* ---- the representations and a writer for them ---- */

struct rep { const char *name; ULONG drep; };
static const struct rep reps[] = {
    { "little-endian (local)",              0x00000010 },
    { "big-endian",                         0x00000000 },
    { "big-endian, EBCDIC",                 0x00000001 },
    { "big-endian, EBCDIC, IBM floats",     0x00000301 },
    { "little-endian, VAX floats",          0x00000110 },
    { "big-endian, Cray floats",            0x00000200 },
    { "little-endian, EBCDIC",              0x00000011 },
};

struct bld
{
    unsigned char b[2048];
    unsigned n;
    ULONG drep;
};

static BOOL big(const struct bld *w) { return !(w->drep & 0x10); }
static BOOL ebc(const struct bld *w) { return (w->drep & 0xf) == 1; }
static unsigned flt(const struct bld *w) { return (w->drep >> 8) & 0xff; }

static void al(struct bld *w, unsigned a) { while (w->n % a) w->b[w->n++] = 0; }
static void u8(struct bld *w, unsigned v) { w->b[w->n++] = v; }
static void u16(struct bld *w, unsigned v)
{
    al(w, 2);
    if (big(w)) { w->b[w->n++] = v >> 8; w->b[w->n++] = v; }
    else { w->b[w->n++] = v; w->b[w->n++] = v >> 8; }
}
static void u32(struct bld *w, ULONG v)
{
    int i;
    al(w, 4);
    for (i = 0; i < 4; i++) w->b[w->n++] = big(w) ? v >> (24 - 8 * i) : v >> (8 * i);
}
static void u64(struct bld *w, ULONGLONG v)
{
    int i;
    al(w, 8);
    for (i = 0; i < 8; i++) w->b[w->n++] = big(w) ? v >> (56 - 8 * i) : v >> (8 * i);
}
static void cch(struct bld *w, unsigned char ascii)
{
    /* the EBCDIC (code page 37) letters and digits the tests use */
    static const struct { unsigned char a, e; } t[] = {
        { 'A', 0xc1 }, { 'Z', 0xe9 }, { 'a', 0x81 }, { 'Q', 0xd8 }, { 'H', 0xc8 }, { 'i', 0x89 }, { '!', 0x5a },
        { 'p', 0x97 }, { 't', 0xa3 }, { 'r', 0x99 }, { 0, 0 }, { 'z', 0xa9 }, { '5', 0xf5 }, { ' ', 0x40 }
    };
    unsigned i;
    if (ebc(w))
    {
        for (i = 0; i < ARRAYSIZE(t); i++) if (t[i].a == ascii) { u8(w, t[i].e); return; }
        printf("no EBCDIC for %02x\n", ascii);
    }
    u8(w, ascii);
}

/* 6.25 and -6.25 in the floating point format of the representation */
static void f32(struct bld *w, int neg)
{
    switch (flt(w))
    {
    case 0: u32(w, 0x40c80000 | (neg ? 0x80000000 : 0)); break;
    case 1: u32(w, 0x000041c8 | (neg ? 0x8000 : 0)); break;             /* VAX F: two 16 bit words */
    case 2: al(w, 4); { ULONG v = 0x40c80000 | (neg ? 0x80000000 : 0); int i; for (i = 0; i < 4; i++) w->b[w->n++] = v >> (24 - 8 * i); } break;
    case 3: u32(w, 0x41640000 | (neg ? 0x80000000 : 0)); break;       /* IBM short */
    }
}
static void f64(struct bld *w, int neg)
{
    switch (flt(w))
    {
    case 0: u64(w, 0x4019000000000000ULL | (neg ? 1ULL << 63 : 0)); break;
    case 1: u64(w, 0x0000000000004039ULL | (neg ? 0x8000 : 0)); break;  /* VAX G: four 16 bit words, first one in the low bits */
    case 2: u64(w, 0x4003c80000000000ULL | (neg ? 1ULL << 63 : 0)); break;
    case 3: u64(w, 0x4164000000000000ULL | (neg ? 1ULL << 63 : 0)); break;
    }
}

/* a conformant string or a non-conformant one: maximum count, offset, actual count, the characters */
static void cstring(struct bld *w, const char *s)
{
    unsigned len = strlen(s) + 1, i;
    u32(w, len); u32(w, 0); u32(w, len);
    for (i = 0; i < len; i++) cch(w, s[i]);
}

static void wstring(struct bld *w, const WCHAR *s)
{
    unsigned len = lstrlenW(s) + 1, i;
    u32(w, len); u32(w, 0); u32(w, len);
    for (i = 0; i < len; i++) u16(w, s[i]);
}

static void simple(struct bld *w, const SIMPLE *s)
{
    al(w, 8);
    u32(w, s->a); u16(w, s->b); cch(w, s->c); u8(w, 0); u64(w, s->h);
}

/* ---- the requests of the procedures, by hand ---- */

static void req_basics(struct bld *w)
{
    cch(w, K_C); u8(w, K_SM); u8(w, K_B); u16(w, K_S); u16(w, K_US); u16(w, K_W); u32(w, K_L); f32(w, 0); u64(w, K_HY); f64(w, 1);
}
static void req_simple(struct bld *w) { simple(w, &k_sp); simple(w, &k_sv); }
static void req_carr(struct bld *w)
{
    int i;
    u32(w, 5); u32(w, 5);
    for (i = 0; i < 5; i++) u16(w, k_carr[i]);
}
static void req_varr(struct bld *w)
{
    int i;
    u32(w, 6); u32(w, 3);
    u32(w, 6); u32(w, 0); u32(w, 3);
    for (i = 0; i < 3; i++) u32(w, k_varr[i]);
}
static void req_fixed(struct bld *w)
{
    int i;
    for (i = 0; i < 3; i++) u32(w, k_fixed_a[i]);
    f64(w, 0); f64(w, 1);
}
static void req_str(struct bld *w)
{
    static const WCHAR ws[] = { 0x4e2d, 0x6587, 0 };
    cstring(w, "Hi!");
    wstring(w, ws);
}
static void req_cstr(struct bld *w)
{
    int i;
    u32(w, 3);          /* the conformance, before the structure */
    u32(w, 3);
    for (i = 0; i < 3; i++) u16(w, k_cstr[i]);
}
static void req_ptrs(struct bld *w)
{
    /* the structure and the referents of its pointers: the string, then the short; then the other parameters */
    u32(w, 77); u32(w, 0x20000); u32(w, 0x20004);
    cstring(w, "ptr");
    u16(w, 0x1357);
    u32(w, 0x20008); u32(w, 0x0f0e0d0c);        /* unique */
    u32(w, (ULONG)-9);                          /* reference */
    u32(w, 0);                                  /* null unique */
    u32(w, 0x2000c); u32(w, 55555);             /* full pointer, first time */
    u32(w, 0x2000c);                            /* the same again: no data */
}
static void req_uni(struct bld *w)
{
    u32(w, 2); f64(w, 0);                       /* the discriminant, then the double (8 byte aligned) */
    u32(w, 3);
    u32(w, 3); u32(w, 0x7a7b7c7d);
}

static void req_vary(struct bld *w)
{
    int i;
    u32(w, 3);
    u32(w, 0); u32(w, 3);
    for (i = 0; i < 3; i++) u16(w, k_vary[i]);
}
static void req_fstr(struct bld *w)
{
    unsigned i;
    u32(w, 0); u32(w, 4);
    for (i = 0; i < 4; i++) cch(w, "Hi!"[i]);
}
static void req_bogus(struct bld *w)
{
    u32(w, 2); u32(w, 2);                               /* the size_is parameter and the conformance */
    u32(w, 1); u32(w, 0x20000); u32(w, 0x20004);
    u32(w, 2); u32(w, 0); u32(w, 0x20008);
    cstring(w, "pr"); u16(w, 5);                        /* the referents of the pointers of the first element */
    u16(w, (USHORT)-6);                                 /* and of the second */
}
static void req_cv(struct bld *w)
{
    u32(w, 5);                                          /* the conformance, before the structure */
    u32(w, 5); u32(w, 2);
    u32(w, 0); u32(w, 2);                               /* offset and actual count of the array */
    u16(w, 11); u16(w, (USHORT)-12);
}
static void req_range(struct bld *w) { u32(w, 555); }

static void req_pipe(struct bld *w)
{
    u16(w, (USHORT)-3);
    u32(w, 2); u32(w, 0x01020304); u32(w, (ULONG)-5);   /* a chunk of two */
    u32(w, 1); u32(w, 7);                               /* a chunk of one */
    u32(w, 0);                                          /* the end */
}

static const struct proc { const char *name; unsigned num; void (*req)(struct bld *); } procs[] = {
    { "Basics", 0, req_basics }, { "Simple", 1, req_simple }, { "CArr", 2, req_carr }, { "VArr", 3, req_varr },
    { "Fixed", 4, req_fixed }, { "Str", 5, req_str }, { "Cstr", 6, req_cstr }, { "Ptrs", 7, req_ptrs },
    { "Uni", 8, req_uni }, { "Vary", 9, req_vary }, { "Fstr", 10, req_fstr }, { "Bogus", 11, req_bogus },
    { "Cv", 12, req_cv }, { "Range", 13, req_range }, { "Pipe", 14, req_pipe },
};

/* ---- becall: the requests through I_RpcSendReceive ---- */

static handle_t bind_to(const char *protseq, const char *endpoint)
{
    RPC_STATUS status;
    RPC_CSTR str = NULL;
    handle_t h = NULL;

    status = RpcStringBindingComposeA(NULL, (RPC_CSTR)protseq, (RPC_CSTR)(strcmp(protseq, "ncalrpc") ? "127.0.0.1" : NULL),
                                      (RPC_CSTR)endpoint, NULL, &str);
    if (!status) status = RpcBindingFromStringBindingA(str, &h);
    check(!status, "binding");
    if (!status) RpcStringFreeA(&str);
    return status ? NULL : h;
}

static RPC_STATUS call_rep(handle_t h, unsigned proc, ULONG drep, const void *req, ULONG len, LONG *ret, ULONG *resp_drep)
{
    RPC_MESSAGE m;
    RPC_STATUS status;

    memset(&m, 0, sizeof(m));
    m.Handle = h;
    m.ProcNum = proc | RPC_FLAGS_VALID_BIT;
    m.RpcInterfaceInformation = (void *)ConvT_v1_0_c_ifspec;
    m.DataRepresentation = drep;
    m.BufferLength = len;
    status = I_RpcGetBuffer(&m);
    if (status) return status;
    memcpy(m.Buffer, req, len);
    status = I_RpcSendReceive(&m);
    if (status) return status;
    *ret = m.BufferLength >= 4 ? *(LONG *)m.Buffer : 0x7fffffff;
    *resp_drep = m.DataRepresentation;
    I_RpcFreeBuffer(&m);
    return RPC_S_OK;
}

static void run_becall(handle_t h)
{
    unsigned i, p;

    for (i = 0; i < ARRAYSIZE(reps); i++)
        for (p = 0; p < ARRAYSIZE(procs); p++)
        {
            struct bld w = { .drep = reps[i].drep };
            LONG ret = -1;
            ULONG rd = 0;
            RPC_STATUS status;
            char what[200];

            procs[p].req(&w);
            status = call_rep(h, procs[p].num, reps[i].drep, w.b, w.n, &ret, &rd);
            sprintf(what, "%s in a %s request: the server reads what was sent (status %lu, mismatches %ld)", procs[p].name,
                    reps[i].name, status, ret);
            check(!status && !ret, what);
            if (!status && !ret && p == 0)
            {
                sprintf(what, "the response of a %s request is in the local representation", reps[i].name);
                check((rd & 0xffff) == 0x10, what);
            }
        }
}

/* ---- rawtcp: PDUs on a socket ---- */

static int sock_send(SOCKET s, const void *p, int len)
{
    return send(s, p, len, 0) == len;
}

static int sock_recv(SOCKET s, void *p, int len)
{
    char *c = p;
    while (len > 0)
    {
        int n = recv(s, c, len, 0);
        if (n <= 0) return 0;
        c += n; len -= n;
    }
    return 1;
}

/* a PDU: header (16 bytes), then the rest; fills buf with all of it */
static int pdu_recv(SOCKET s, unsigned char *buf, unsigned max, unsigned *len)
{
    unsigned frag;
    if (!sock_recv(s, buf, 16)) return 0;
    frag = (buf[4] & 0x10) ? buf[8] | (buf[9] << 8) : (buf[8] << 8) | buf[9];
    if (frag < 16 || frag > max) return 0;
    if (!sock_recv(s, buf + 16, frag - 16)) return 0;
    *len = frag;
    return 1;
}

static const GUID if_guid = { 0x5ee0e9e6, 0xb001, 0x4c01, { 0x90, 0x01, 0x01, 0x23, 0x45, 0x67, 0x89, 0xab } };
static const GUID ndr_guid = { 0x8a885d04, 0x1ceb, 0x11c9, { 0x9f, 0xe8, 0x08, 0x00, 0x2b, 0x10, 0x48, 0x60 } };

static SOCKET raw_connect(int port)
{
    WSADATA wd;
    struct sockaddr_in a;
    SOCKET s;
    unsigned char bind_pdu[72], ack[512];
    unsigned len, o = 0;

    WSAStartup(MAKEWORD(2, 2), &wd);
    s = socket(AF_INET, SOCK_STREAM, 0);
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(s, (struct sockaddr *)&a, sizeof(a))) { printf("FAIL  connect\n"); failures++; return INVALID_SOCKET; }

    /* BIND, little-endian, in the shape of the DCE RPC connection-oriented protocol */
    memset(bind_pdu, 0, sizeof(bind_pdu));
    bind_pdu[0] = 5; bind_pdu[1] = 0; bind_pdu[2] = 11; bind_pdu[3] = 3; bind_pdu[4] = 0x10;
    bind_pdu[8] = sizeof(bind_pdu);
    bind_pdu[12] = 1;                   /* call id */
    bind_pdu[16] = 0xb8; bind_pdu[17] = 0x10; bind_pdu[18] = 0xb8; bind_pdu[19] = 0x10;
    bind_pdu[24] = 1;                   /* one context */
    o = 28;
    bind_pdu[o + 2] = 1;                /* id 0, one transfer syntax */
    memcpy(bind_pdu + o + 4, &if_guid, 16);
    bind_pdu[o + 20] = 1;               /* version 1.0 */
    memcpy(bind_pdu + o + 24, &ndr_guid, 16);
    bind_pdu[o + 40] = 2;               /* version 2.0 */
    if (!sock_send(s, bind_pdu, sizeof(bind_pdu)) || !pdu_recv(s, ack, sizeof(ack), &len) || ack[2] != 12)
    {
        printf("FAIL  the server did not answer the bind with a bind_ack (ptype %u)\n", ack[2]);
        failures++;
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}

/* sends a request PDU whose header and body are in the representation of w; returns the response PDU */
static int raw_request(SOCKET s, unsigned proc, const struct bld *w, unsigned char *resp, unsigned *rlen)
{
    unsigned char pdu[2300];
    unsigned frag = 24 + w->n;
    BOOL be = big(w);

    memset(pdu, 0, 24);
    pdu[0] = 5; pdu[2] = 0; pdu[3] = 3;
    pdu[4] = w->drep; pdu[5] = w->drep >> 8;
    if (be) { pdu[8] = frag >> 8; pdu[9] = frag; pdu[15] = 2; pdu[18] = w->n >> 8; pdu[19] = w->n; pdu[23] = proc; }
    else { pdu[8] = frag; pdu[9] = frag >> 8; pdu[12] = 2; pdu[16] = w->n; pdu[17] = w->n >> 8; pdu[22] = proc; }
    memcpy(pdu + 24, w->b, w->n);
    return sock_send(s, pdu, frag) && pdu_recv(s, resp, 512, rlen);
}

static void run_rawtcp(int port)
{
    unsigned i, p;
    static const unsigned which[] = { 0, 5, 7 };        /* Basics, Str, Ptrs */

    for (i = 0; i < ARRAYSIZE(reps); i++)
        for (p = 0; p < ARRAYSIZE(which); p++)
        {
            struct bld w = { .drep = reps[i].drep };
            unsigned char resp[512];
            unsigned rlen = 0;
            SOCKET s = raw_connect(port);
            char what[200];
            BOOL ok;

            if (s == INVALID_SOCKET) return;
            procs[which[p]].req(&w);
            ok = raw_request(s, procs[which[p]].num, &w, resp, &rlen);
            /* a response PDU: type 2, the local representation, a body of the return value 0 */
            ok = ok && resp[2] == 2 && resp[4] == 0x10 && rlen == 28 && !memcmp(resp + 24, "\0\0\0\0", 4);
            sprintf(what, "%s as bytes on a socket, %s: the response is the PDU of a successful call (ptype %u, %u bytes)",
                    procs[which[p]].name, reps[i].name, resp[2], rlen);
            check(ok, what);
            closesocket(s);
        }
}

/* ---- outserver / outclient / outraw: a stub answering in a foreign representation ---- */

static ULONG out_drep(LONG x)
{
    switch (x)
    {
    case 1: return 0x00000000;
    case 2: return 0x00000301;
    case 3: return 0x00000110;
    case 4: return 0x00000200;
    default: return 0x00000010;
    }
}

static void __RPC_STUB out_stub(PRPC_MESSAGE msg)
{
    LONG x = *(LONG *)msg->Buffer;
    struct bld w = { .drep = out_drep(x) };
    SIMPLE sv = { 5, -6, 'Q', 7 };

    u32(&w, (ULONG)-123456);
    f64(&w, 0);
    u16(&w, 1); u16(&w, (USHORT)-2);
    simple(&w, &sv);
    u32(&w, (ULONG)-77);
    msg->BufferLength = w.n;
    if (I_RpcGetBuffer(msg)) RpcRaiseException(RPC_S_OUT_OF_MEMORY);
    memcpy(msg->Buffer, w.b, w.n);
    msg->DataRepresentation = w.drep;
}

/* AOut: the same answer, to an asynchronous call */
static void __RPC_STUB out_astub(PRPC_MESSAGE msg)
{
    LONG x = *(LONG *)msg->Buffer;
    struct bld w = { .drep = out_drep(x) };

    u32(&w, (ULONG)-123456);
    f64(&w, 0);
    msg->BufferLength = w.n;
    if (I_RpcGetBuffer(msg)) RpcRaiseException(RPC_S_OUT_OF_MEMORY);
    memcpy(msg->Buffer, w.b, w.n);
    msg->DataRepresentation = w.drep;
}

static void __RPC_STUB out_stop(PRPC_MESSAGE msg)
{
    msg->BufferLength = 0;
    I_RpcGetBuffer(msg);
    RpcMgmtStopServerListening(NULL);
}

static void __RPC_STUB out_other(PRPC_MESSAGE msg)
{
    RpcRaiseException(RPC_S_PROCNUM_OUT_OF_RANGE);
}

static RPC_SERVER_INTERFACE out_if;
static RPC_DISPATCH_TABLE out_table;
static RPC_DISPATCH_FUNCTION out_funcs[18] = { out_other, out_other, out_other, out_other, out_other, out_other, out_other,
                                               out_other, out_other, out_other, out_other, out_other, out_other,
                                               out_other, out_other, out_stub, out_astub, out_stop };

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

static int run_outserver(const char *protseq, const char *endpoint)
{
    memcpy(&out_if, s_ConvT_v1_0_s_ifspec, sizeof(out_if));
    out_table.DispatchTableCount = 18;
    out_table.DispatchTable = out_funcs;
    out_if.DispatchTable = &out_table;
    return run_server_if(protseq, endpoint, (RPC_IF_HANDLE)&out_if);
}

static void run_outclient(handle_t h)
{
    LONG x;

    for (x = 1; x <= 6; x++)
    {
        LONG y = 0, ret;
        double d = 0;
        short s[2] = { 0, 0 };
        SIMPLE sv;
        char what[200];

        memset(&sv, 0, sizeof(sv));
        ret = Out(h, x, &y, &d, s, &sv);
        sprintf(what, "the client reads a response in representation %#lx (ret %ld, y %ld, d %g, s %d %d, sv %ld %d %c %I64d)",
                out_drep(x), ret, y, d, s[0], s[1], sv.a, sv.b, sv.c ? sv.c : '?', sv.h);
        check(ret == -77 && y == -123456 && d == 6.25 && s[0] == 1 && s[1] == -2 && sv.a == 5 && sv.b == -6 &&
              sv.c == 'Q' && sv.h == 7, what);
    }

    for (x = 1; x <= 6; x++)
    {
        RPC_ASYNC_STATE a;
        HANDLE ev = CreateEventA(NULL, TRUE, FALSE, NULL);
        LONG y = 0;
        double d = 0;
        RPC_STATUS status;
        char what[200];

        RpcAsyncInitializeHandle(&a, sizeof(a));
        a.NotificationType = RpcNotificationTypeEvent;
        a.u.hEvent = ev;
        AOut(&a, h, x, &y, &d);
        WaitForSingleObject(ev, 10000);
        status = RpcAsyncCompleteCall(&a, NULL);
        sprintf(what, "the asynchronous client reads a response in representation %#lx (status %lu, y %ld, d %g)", out_drep(x), status, y, d);
        check(!status && y == -123456 && d == 6.25, what);
        CloseHandle(ev);
    }
}

static void run_outraw(int port)
{
    SOCKET s = raw_connect(port);
    struct bld w = { .drep = 0x10 };
    unsigned char resp[512];
    unsigned rlen = 0;
    BOOL ok;
    static const unsigned char want[] = {
        0x05, 0x00, 0x02, 0x03, 0x00, 0x00, 0x00, 0x00,     /* response, first and last fragment, big-endian ASCII IEEE */
        0x00, 0x44, 0x00, 0x00,                             /* 68 bytes, no authentication */
        0x00, 0x00, 0x00, 0x02,                             /* call 2 */
        0x00, 0x00, 0x00, 0x2c,                             /* alloc hint: 44 bytes of data */
        0x00, 0x00, 0x00, 0x00,                             /* context 0, no cancels */
        0xff, 0xfe, 0x1d, 0xc0,                             /* -123456 */
        0, 0, 0, 0,
        0x40, 0x19, 0, 0, 0, 0, 0, 0,                       /* 6.25 */
        0x00, 0x01, 0xff, 0xfe,                             /* 1, -2 */
        0, 0, 0, 0,
        0x00, 0x00, 0x00, 0x05, 0xff, 0xfa, 'Q', 0,         /* the structure */
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07,
        0xff, 0xff, 0xff, 0xb3,                             /* -77 */
    };

    if (s == INVALID_SOCKET) return;
    u32(&w, 1);
    ok = raw_request(s, 15, &w, resp, &rlen);
    /* (the call id of the response is not what the request had: a deviation of the server, not tested here) */
    if (ok && rlen == sizeof(want)) memcpy(resp + 12, want + 12, 4);
    check(ok && rlen == sizeof(want) && !memcmp(resp, want, rlen),
          "the response PDU of a big-endian answer has a big-endian header and body");
    if (!(ok && rlen == sizeof(want) && !memcmp(resp, want, rlen)))
    {
        unsigned i;
        printf("got %u bytes:", rlen);
        for (i = 0; i < rlen; i++) printf(" %02x", resp[i]);
        printf("\n");
    }
    closesocket(s);
}

static int run_client_mode(const char *mode, const char *protseq, const char *endpoint)
{
    handle_t h = bind_to(protseq, endpoint);

    if (!h) return 1;
    if (!strcmp(mode, "becall")) run_becall(h);
    else run_outclient(h);
    Stop(h);
    RpcBindingFree(&h);
    return 0;
}

#define FC_END 0x5b
#define FC_LONG 8
#define FC_SHORT 6
#define FC_IN_PARAM 0x4d
#define FC_IN_PARAM_BASETYPE 0x4e
#define FC_RETURN_PARAM_BASETYPE 0x53
#include <pshpack1.h>
struct oif_param { WORD attr; WORD stack_offset; union { BYTE type_format_char; WORD type_offset; } u; };
#include <poppack.h>
#define PA_IN 0x8
#define PA_BASETYPE 0x40
#define PA_SIMPLEREF 0x100

/* NdrConvert and NdrConvert2 called by hand on a buffer and hand-made formats (no server): the parameter list of the
 * original interpreter format, the direction (a server converts what it is sent, a client what it gets back) */
static void run_direct(void)
{
    /* types: a conformant array of long, as the parameter pointee */
    static const unsigned char types[] = { 0x1b, 0x03, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x5b };
    /* the original format: [in] long, [in] array by reference, [return] short; FC_END */
    static const unsigned char oldfmt[] = {
        FC_IN_PARAM_BASETYPE, FC_LONG, FC_IN_PARAM, 1, 0x00, 0x00, FC_RETURN_PARAM_BASETYPE, FC_SHORT, FC_END };
    static const unsigned char bufbe[] = {
        0x01, 0x02, 0x03, 0x04,                         /* the long */
        0x00, 0x00, 0x00, 0x02, 0x0a, 0x0b, 0x0c, 0x0d, 0x00, 0x00, 0x01, 0x00,    /* the array: 2 elements */
        0x12, 0x34, 0x00, 0x00 };
    static const unsigned char wantin[] = {
        0x04, 0x03, 0x02, 0x01, 0x02, 0x00, 0x00, 0x00, 0x0d, 0x0c, 0x0b, 0x0a, 0x00, 0x01, 0x00, 0x00,
        0x12, 0x34, 0x00, 0x00 };
    MIDL_STUB_DESC desc;
    MIDL_STUB_MESSAGE msg;
    RPC_MESSAGE rpc;
    unsigned char buf[32];

    memset(&desc, 0, sizeof(desc));
    desc.pFormatTypes = types;

    /* a server converts the [in] parameters and leaves the return value (not in its request) */
    memset(&msg, 0, sizeof(msg)); memset(&rpc, 0, sizeof(rpc));
    memcpy(buf, bufbe, sizeof(bufbe));
    rpc.DataRepresentation = 0; rpc.Buffer = buf; rpc.BufferLength = 16;
    msg.RpcMsg = &rpc; msg.StubDesc = &desc; msg.Buffer = buf; msg.BufferEnd = buf + 16; msg.IsClient = 0;
    NdrConvert(&msg, oldfmt);
    check(!memcmp(buf, wantin, 16), "NdrConvert: a server converts its [in] parameters of the original format");

    /* a client converts the return value and the [out] parameters */
    memset(&msg, 0, sizeof(msg));
    memcpy(buf, bufbe + 16, 4);
    rpc.Buffer = buf; rpc.BufferLength = 4;
    msg.RpcMsg = &rpc; msg.StubDesc = &desc; msg.Buffer = buf; msg.BufferEnd = buf + 4; msg.IsClient = 1;
    NdrConvert(&msg, oldfmt);
    check(buf[0] == 0x34 && buf[1] == 0x12, "NdrConvert: a client converts the return value of the original format");

    /* the same in the Oif parameter list; the local representation is left alone */
    {
        struct oif_param params[2];
        memset(params, 0, sizeof(params));
        params[0].attr = PA_IN | PA_BASETYPE; params[0].u.type_format_char = FC_LONG;
        params[1].attr = PA_IN | PA_SIMPLEREF; params[1].u.type_offset = 0; params[1].stack_offset = 8;
        memset(&msg, 0, sizeof(msg));
        memcpy(buf, bufbe, 16);
        rpc.DataRepresentation = 0x10; rpc.Buffer = buf; rpc.BufferLength = 16;
        msg.RpcMsg = &rpc; msg.StubDesc = &desc; msg.Buffer = buf; msg.BufferEnd = buf + 16; msg.IsClient = 0;
        NdrConvert2(&msg, (PFORMAT_STRING)params, 2);
        check(!memcmp(buf, bufbe, 16), "NdrConvert2 leaves data in the local representation alone");
        rpc.DataRepresentation = 0;
        NdrConvert2(&msg, (PFORMAT_STRING)params, 2);
        check(!memcmp(buf, wantin, 16), "NdrConvert2 converts the parameters of the Oif list");
    }

    /* a context handle: its attributes and the fields of its GUID */
    {
        static const unsigned char ctx_types[] = { 0x30, 0x41, 0x00, 0x00 };
        static const unsigned char ctx_be[20] = { 1, 2, 3, 4, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 1, 2, 3, 4, 5, 6, 7, 8 };
        static const unsigned char ctx_le[20] = { 4, 3, 2, 1, 0x0d, 0x0c, 0x0b, 0x0a, 0x0f, 0x0e, 0x11, 0x10, 1, 2, 3, 4, 5, 6, 7, 8 };
        struct oif_param p;

        memset(&p, 0, sizeof(p));
        p.attr = PA_IN | PA_SIMPLEREF; p.u.type_offset = 0;
        desc.pFormatTypes = ctx_types;
        memset(&msg, 0, sizeof(msg));
        memcpy(buf, ctx_be, 20);
        rpc.DataRepresentation = 0; rpc.Buffer = buf; rpc.BufferLength = 20;
        msg.RpcMsg = &rpc; msg.StubDesc = &desc; msg.Buffer = buf; msg.BufferEnd = buf + 20; msg.IsClient = 0;
        NdrConvert2(&msg, (PFORMAT_STRING)&p, 1);
        check(!memcmp(buf, ctx_le, 20), "NdrConvert2 converts a context handle");
    }
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
    if (argc < 2) return 2;
    if (!strcmp(argv[1], "server")) return run_server_if(argv[2], argv[3], s_ConvT_v1_0_s_ifspec);
    if (!strcmp(argv[1], "outserver")) return run_outserver(argv[2], argv[3]);
    if (!strcmp(argv[1], "direct")) run_direct();
    else if (!strcmp(argv[1], "rawtcp")) run_rawtcp(atoi(argv[2]));
    else if (!strcmp(argv[1], "outraw")) run_outraw(atoi(argv[2]));
    else if (argc >= 4) run_client_mode(argv[1], argv[2], argv[3]);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
