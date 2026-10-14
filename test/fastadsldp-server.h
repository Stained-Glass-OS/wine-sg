/* A small in-process LDAPv3 server for the adsldp gate: simple bind, search (base, one level, subtree with
 * equality, presence, and, or, not filters), modify, add, delete over a directory held in memory. */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAXV 16
#define MAXA 24
#define MAXE 32
typedef struct { unsigned char *d; int len; } LVal;
typedef struct { char name[48]; int n; LVal v[MAXV]; } LAttr;
typedef struct { char dn[200]; int n; LAttr a[MAXA]; int live; } LEntry;

static LEntry g_dit[MAXE];
static CRITICAL_SECTION g_cs;
static SOCKET g_listen;
static int g_port;
static int g_binds, g_searches, g_modifies, g_adds, g_deletes;
static char g_last_filter[200];

static void sv_add_attr(LEntry *e, const char *name, const void *data, int len)
{
    LAttr *a = NULL; int i;
    for (i = 0; i < e->n; i++) if (!_stricmp(e->a[i].name, name)) { a = &e->a[i]; break; }
    if (!a) { a = &e->a[e->n++]; strcpy(a->name, name); a->n = 0; }
    a->v[a->n].d = malloc(len + 1); memcpy(a->v[a->n].d, data, len); a->v[a->n].d[len] = 0; a->v[a->n].len = len; a->n++;
}
static void sv_add_str(LEntry *e, const char *name, const char *val) { sv_add_attr(e, name, val, (int)strlen(val)); }
static LEntry *sv_new(const char *dn) { LEntry *e = &g_dit[0]; int i; for (i = 0; i < MAXE; i++) if (!g_dit[i].live) { e = &g_dit[i]; break; } memset(e, 0, sizeof(*e)); strcpy(e->dn, dn); e->live = 1; return e; }
static LEntry *sv_find(const char *dn) { int i; for (i = 0; i < MAXE; i++) if (g_dit[i].live && !_stricmp(g_dit[i].dn, dn)) return &g_dit[i]; return NULL; }
static LAttr *sv_attr(LEntry *e, const char *name) { int i; for (i = 0; i < e->n; i++) if (!_stricmp(e->a[i].name, name)) return &e->a[i]; return NULL; }
static void sv_free_attr(LAttr *a) { int i; for (i = 0; i < a->n; i++) free(a->v[i].d); a->n = 0; }
static void sv_del_attr(LEntry *e, LAttr *a) { int idx = (int)(a - e->a); sv_free_attr(a); memmove(a, a + 1, (e->n - idx - 1) * sizeof(LAttr)); e->n--; }

static void sv_init(void)
{
    static const char *types[] = {
        "( 2.5.4.0 NAME 'objectClass' SYNTAX 1.3.6.1.4.1.1466.115.121.1.38 )",
        "( 2.5.4.3 NAME ( 'cn' 'commonName' ) SYNTAX 1.3.6.1.4.1.1466.115.121.1.15 SINGLE-VALUE )",
        "( 2.5.4.4 NAME 'sn' SYNTAX 1.3.6.1.4.1.1466.115.121.1.15 )",
        "( 2.5.4.20 NAME 'telephoneNumber' SYNTAX 1.3.6.1.4.1.1466.115.121.1.15 )",
        "( 0.9.2342.19200300.100.1.3 NAME 'mail' SYNTAX 1.3.6.1.4.1.1466.115.121.1.15 )",
        "( 2.5.4.13 NAME 'description' SYNTAX 1.3.6.1.4.1.1466.115.121.1.15 )",
        "( 1.3.6.1.1.1.1.0 NAME 'uidNumber' SYNTAX 1.3.6.1.4.1.1466.115.121.1.27 SINGLE-VALUE )",
        "( 1.2.3.4.1 NAME 'enabled' SYNTAX 1.3.6.1.4.1.1466.115.121.1.7 SINGLE-VALUE )",
        "( 1.2.3.4.2 NAME 'objectGUID' SYNTAX 1.3.6.1.4.1.1466.115.121.1.40 SINGLE-VALUE )",
        "( 2.5.4.31 NAME 'member' SYNTAX 1.3.6.1.4.1.1466.115.121.1.12 )",
        "( 1.2.3.4.3 NAME 'ntMixedDomain' SYNTAX 1.3.6.1.4.1.1466.115.121.1.27 SINGLE-VALUE )",
        "( 1.2.3.4.4 NAME 'fSMORoleOwner' SYNTAX 1.3.6.1.4.1.1466.115.121.1.12 SINGLE-VALUE )",
        NULL };
    unsigned char guid[16]; LEntry *e; int i;
    InitializeCriticalSection(&g_cs);
    e = sv_new("");
    sv_add_str(e, "defaultNamingContext", "DC=wine,DC=test");
    sv_add_str(e, "subschemaSubentry", "CN=Aggregate,CN=Schema,CN=Configuration,DC=wine,DC=test");
    sv_add_str(e, "supportedLDAPVersion", "3");
    sv_add_str(e, "schemaNamingContext", "CN=Schema,CN=Configuration,DC=wine,DC=test");
    sv_add_str(e, "configurationNamingContext", "CN=Configuration,DC=wine,DC=test");
    e = sv_new("CN=Aggregate,CN=Schema,CN=Configuration,DC=wine,DC=test");
    for (i = 0; types[i]; i++) sv_add_str(e, "attributeTypes", types[i]);
    e = sv_new("DC=wine,DC=test");
    sv_add_str(e, "objectClass", "top"); sv_add_str(e, "objectClass", "domainDNS"); sv_add_str(e, "ntMixedDomain", "0");
    sv_add_str(e, "fSMORoleOwner", "CN=NTDS Settings,CN=DC1,CN=Servers,DC=wine,DC=test");
    e = sv_new("CN=Users,DC=wine,DC=test");
    sv_add_str(e, "objectClass", "top"); sv_add_str(e, "objectClass", "container"); sv_add_str(e, "cn", "Users");
    for (i = 0; i < 16; i++) guid[i] = (unsigned char)(0x10 + i);
    e = sv_new("CN=Jeff Smith,CN=Users,DC=wine,DC=test");
    sv_add_str(e, "objectClass", "top"); sv_add_str(e, "objectClass", "person"); sv_add_str(e, "objectClass", "user");
    sv_add_str(e, "cn", "Jeff Smith"); sv_add_str(e, "sn", "Smith"); sv_add_str(e, "telephoneNumber", "425-707-9790");
    sv_add_str(e, "telephoneNumber", "425-707-9791"); sv_add_str(e, "uidNumber", "42"); sv_add_str(e, "enabled", "TRUE");
    sv_add_str(e, "mail", "jeff@wine.test"); sv_add_attr(e, "objectGUID", guid, 16);
    e = sv_new("CN=Group One,CN=Users,DC=wine,DC=test");
    sv_add_str(e, "objectClass", "top"); sv_add_str(e, "objectClass", "group"); sv_add_str(e, "cn", "Group One");
    sv_add_str(e, "member", "CN=Jeff Smith,CN=Users,DC=wine,DC=test");
}

/* ---- BER ---- */
typedef struct { unsigned char b[65536]; int n; } Buf;
static void b_len(Buf *o, int len)
{
    if (len < 128) o->b[o->n++] = (unsigned char)len;
    else if (len < 256) { o->b[o->n++] = 0x81; o->b[o->n++] = (unsigned char)len; }
    else { o->b[o->n++] = 0x82; o->b[o->n++] = (unsigned char)(len >> 8); o->b[o->n++] = (unsigned char)len; }
}
static void b_tlv(Buf *o, int tag, const void *d, int len) { o->b[o->n++] = (unsigned char)tag; b_len(o, len); memcpy(o->b + o->n, d, len); o->n += len; }
static void b_int(Buf *o, int tag, int v) { unsigned char t = (unsigned char)v; b_tlv(o, tag, &t, 1); }
static void b_str(Buf *o, const char *s) { b_tlv(o, 0x04, s, (int)strlen(s)); }
/* wraps what was written since start into a constructed element */
static void b_wrap(Buf *o, int start, int tag)
{
    int len = o->n - start; Buf h; h.n = 0; h.b[h.n++] = (unsigned char)tag; b_len(&h, len);
    memmove(o->b + start + h.n, o->b + start, len); memcpy(o->b + start, h.b, h.n); o->n += h.n;
}

typedef struct { const unsigned char *p; int len; } Cur;
static int c_tlv(Cur *c, int *tag, Cur *val)
{
    int l, i;
    if (c->len < 2) return 0;
    *tag = c->p[0]; l = c->p[1]; i = 2;
    if (l & 0x80) { int nb = l & 0x7f; l = 0; while (nb-- > 0) l = (l << 8) | c->p[i++]; }
    if (i + l > c->len) return 0;
    val->p = c->p + i; val->len = l; c->p += i + l; c->len -= i + l;
    return 1;
}
static int c_intval(Cur *v) { int r = 0, i; for (i = 0; i < v->len; i++) r = (r << 8) | v->p[i]; return r; }
static void c_cstr(Cur *v, char *out, int max) { int n = v->len < max - 1 ? v->len : max - 1; memcpy(out, v->p, n); out[n] = 0; }

static int elem_hdr(const unsigned char *p, int *hdr, int *vlen)
{
    int l = p[1];
    if (l & 0x80) { int nb = l & 0x7f, i; l = 0; for (i = 0; i < nb; i++) l = (l << 8) | p[2 + i]; *hdr = 2 + nb; }
    else *hdr = 2;
    *vlen = l;
    return p[0];
}
static int filter_eval(const unsigned char *p, LEntry *e)
{
    int hdr, vlen, tag = elem_hdr(p, &hdr, &vlen); const unsigned char *v = p + hdr;
    if (tag == 0xA0 || tag == 0xA1) {
        int ok = tag == 0xA0, off = 0;
        while (off < vlen) {
            int h2, l2, r;
            elem_hdr(v + off, &h2, &l2);
            r = filter_eval(v + off, e);
            if (tag == 0xA0) ok = ok && r; else ok = ok || r;
            off += h2 + l2;
        }
        return ok;
    }
    if (tag == 0xA2) return !filter_eval(v, e);
    if (tag == 0xA3) {
        Cur c = { v, vlen }, a, val; int t2, i; char name[48], want[200]; LAttr *at;
        c_tlv(&c, &t2, &a); c_tlv(&c, &t2, &val);
        c_cstr(&a, name, sizeof(name)); c_cstr(&val, want, sizeof(want));
        sprintf(g_last_filter, "(%s=%s)", name, want);
        at = sv_attr(e, name);
        if (!at) return 0;
        for (i = 0; i < at->n; i++) if (at->v[i].len == val.len && !_strnicmp((char*)at->v[i].d, want, val.len)) return 1;
        return 0;
    }
    if (tag == 0x87) {
        char name[48]; int n = vlen < 47 ? vlen : 47;
        memcpy(name, v, n); name[n] = 0;
        sprintf(g_last_filter, "(%s=*)", name);
        return sv_attr(e, name) != NULL || !_stricmp(name, "objectClass");
    }
    return 1;
}

static int is_under(const char *dn, const char *base, int onelevel)
{
    size_t ld = strlen(dn), lb = strlen(base); const char *p;
    if (!lb) { if (!onelevel) return 1; return strchr(dn, ',') == NULL; }
    if (ld <= lb + 0 || _stricmp(dn + ld - lb, base) || dn[ld - lb - 1] != ',') return 0;
    if (!onelevel) return 1;
    for (p = dn; p < dn + ld - lb - 1; p++) if (*p == ',') return 0;
    return 1;
}

static void send_all(SOCKET s, Buf *o) { int off = 0; while (off < o->n) { int r = send(s, (char*)o->b + off, o->n - off, 0); if (r <= 0) break; off += r; } }
static void send_result(SOCKET s, int msgid, int optag, int code, const char *diag)
{
    static Buf o; int s0, s1; o.n = 0; s0 = o.n; b_int(&o, 0x02, msgid); s1 = o.n; b_int(&o, 0x0A, code); b_str(&o, ""); b_str(&o, diag ? diag : ""); b_wrap(&o, s1, optag); b_wrap(&o, s0, 0x30); send_all(s, &o);
}

static void do_search(SOCKET s, int msgid, Cur req)
{
    static Buf o; char base[200]; int tag, scope, i, j, k, s0, s1, s2, s3, s4; Cur v, filter, attrs; int nreq = 0; char reqn[16][48]; LEntry *be;
    c_tlv(&req, &tag, &v); c_cstr(&v, base, sizeof(base));
    c_tlv(&req, &tag, &v); scope = c_intval(&v);
    c_tlv(&req, &tag, &v); c_tlv(&req, &tag, &v); c_tlv(&req, &tag, &v); c_tlv(&req, &tag, &v);
    { Cur save = req; int ft; Cur fv; c_tlv(&save, &ft, &fv); filter.p = req.p; filter.len = (int)(save.p - req.p); req = save; }
    c_tlv(&req, &tag, &attrs);
    while (attrs.len && nreq < 16) { Cur a; c_tlv(&attrs, &tag, &a); c_cstr(&a, reqn[nreq++], 48); }
    EnterCriticalSection(&g_cs); g_searches++;
    be = sv_find(base);
    if (!be) { LeaveCriticalSection(&g_cs); send_result(s, msgid, 0x65, 32, "no such object"); return; }
    for (i = 0; i < MAXE; i++) {
        LEntry *e = &g_dit[i];
        if (!e->live) continue;
        if (scope == 0 && _stricmp(e->dn, base)) continue;
        if (scope == 1 && !is_under(e->dn, base, 1)) continue;
        if (scope == 2 && _stricmp(e->dn, base) && !is_under(e->dn, base, 0)) continue;
        if (!filter_eval(filter.p, e)) continue;
        o.n = 0; s0 = o.n; b_int(&o, 0x02, msgid); s1 = o.n; b_str(&o, e->dn); s2 = o.n;
        for (j = 0; j < e->n; j++) {
            int want = !nreq;
            for (k = 0; k < nreq; k++) if (!_stricmp(reqn[k], e->a[j].name) || !strcmp(reqn[k], "*")) want = 1;
            if (!want) continue;
            s3 = o.n; b_str(&o, e->a[j].name); s4 = o.n;
            for (k = 0; k < e->a[j].n; k++) b_tlv(&o, 0x04, e->a[j].v[k].d, e->a[j].v[k].len);
            b_wrap(&o, s4, 0x31); b_wrap(&o, s3, 0x30);
        }
        b_wrap(&o, s2, 0x30); b_wrap(&o, s1, 0x64); b_wrap(&o, s0, 0x30);
        send_all(s, &o);
    }
    LeaveCriticalSection(&g_cs);
    send_result(s, msgid, 0x65, 0, "");
}

static void do_modify(SOCKET s, int msgid, Cur req)
{
    char dn[200]; int tag, rc = 0; Cur v, changes; LEntry *e;
    c_tlv(&req, &tag, &v); c_cstr(&v, dn, sizeof(dn));
    c_tlv(&req, &tag, &changes);
    EnterCriticalSection(&g_cs); g_modifies++;
    e = sv_find(dn);
    if (!e) rc = 32;
    while (!rc && changes.len) {
        Cur ch, op, mod, name, vals; int op_v; char an[48]; LAttr *a;
        c_tlv(&changes, &tag, &ch); c_tlv(&ch, &tag, &op); op_v = c_intval(&op);
        c_tlv(&ch, &tag, &mod); c_tlv(&mod, &tag, &name); c_tlv(&mod, &tag, &vals); c_cstr(&name, an, sizeof(an));
        a = sv_attr(e, an);
        if (!_stricmp(an, "cn") && op_v != 0) { rc = 67; break; }   /* the naming attribute cannot be changed */
        if (op_v == 0) { /* add */
            while (vals.len) { Cur x; int dup = 0, i; c_tlv(&vals, &tag, &x);
                if (a) for (i = 0; i < a->n; i++) if (a->v[i].len == x.len && !memcmp(a->v[i].d, x.p, x.len)) dup = 1;
                if (dup) continue;
                sv_add_attr(e, an, x.p, x.len); a = sv_attr(e, an); }
        } else if (op_v == 1) { /* delete */
            if (!a) continue;
            if (!vals.len) sv_del_attr(e, a);
            else while (vals.len) { Cur x; int i, found = 0; c_tlv(&vals, &tag, &x);
                for (i = 0; i < a->n; i++) if (a->v[i].len == x.len && !memcmp(a->v[i].d, x.p, x.len)) { free(a->v[i].d); memmove(&a->v[i], &a->v[i + 1], (a->n - i - 1) * sizeof(LVal)); a->n--; found = 1; break; }
                (void)found; }
            a = sv_attr(e, an); if (a && !a->n) sv_del_attr(e, a);
        } else { /* replace */
            if (a) sv_free_attr(a);
            if (vals.len) { while (vals.len) { Cur x; c_tlv(&vals, &tag, &x); sv_add_attr(e, an, x.p, x.len); } }
            else if (a) sv_del_attr(e, a);
        }
    }
    LeaveCriticalSection(&g_cs);
    send_result(s, msgid, 0x67, rc, "");
}

static void do_add(SOCKET s, int msgid, Cur req)
{
    char dn[200]; int tag, rc = 0; Cur v, attrs; LEntry *e;
    c_tlv(&req, &tag, &v); c_cstr(&v, dn, sizeof(dn));
    c_tlv(&req, &tag, &attrs);
    EnterCriticalSection(&g_cs); g_adds++;
    if (sv_find(dn)) rc = 68;
    else {
        e = sv_new(dn);
        while (attrs.len) { Cur at, name, vals; char an[48]; c_tlv(&attrs, &tag, &at); c_tlv(&at, &tag, &name); c_tlv(&at, &tag, &vals); c_cstr(&name, an, sizeof(an));
            while (vals.len) { Cur x; c_tlv(&vals, &tag, &x); sv_add_attr(e, an, x.p, x.len); } }
    }
    LeaveCriticalSection(&g_cs);
    send_result(s, msgid, 0x69, rc, "");
}

static DWORD WINAPI conn_thread(void *arg)
{
    SOCKET s = (SOCKET)(ULONG_PTR)arg; static unsigned char buf[65536]; int have = 0;
    for (;;) {
        Cur c, msg, id, op; int tag, t2, t3, msgid, r;
        r = recv(s, (char*)buf + have, sizeof(buf) - have, 0);
        if (r <= 0) break;
        have += r;
        for (;;) {
            c.p = buf; c.len = have;
            if (have < 2) break;
            if (!c_tlv(&c, &tag, &msg)) break;   /* incomplete */
            { int used = have - c.len; Cur m = msg; Cur opc;
              c_tlv(&m, &t2, &id); msgid = c_intval(&id); c_tlv(&m, &t3, &op); opc = op;
              switch (t3) {
              case 0x60: g_binds++; send_result(s, msgid, 0x61, 0, ""); break;
              case 0x63: do_search(s, msgid, opc); break;
              case 0x66: do_modify(s, msgid, opc); break;
              case 0x68: do_add(s, msgid, opc); break;
              case 0x4A: { char dn[200]; LEntry *e; c_cstr(&opc, dn, sizeof(dn)); EnterCriticalSection(&g_cs); g_deletes++; e = sv_find(dn);
                           if (e) e->live = 0; LeaveCriticalSection(&g_cs); send_result(s, msgid, 0x6B, e ? 0 : 32, ""); break; }
              case 0x42: closesocket(s); return 0;
              default: break;
              }
              memmove(buf, buf + used, have - used); have -= used; }
        }
    }
    closesocket(s);
    return 0;
}
static DWORD WINAPI accept_thread(void *arg)
{
    for (;;) { SOCKET c = accept(g_listen, NULL, NULL); if (c == INVALID_SOCKET) break; CloseHandle(CreateThread(NULL, 0, conn_thread, (void*)(ULONG_PTR)c, 0, NULL)); }
    return 0;
}
static int sv_start(void)
{
    WSADATA wd; struct sockaddr_in sa; int sl = sizeof(sa);
    WSAStartup(MAKEWORD(2, 2), &wd);
    sv_init();
    g_listen = socket(AF_INET, SOCK_STREAM, 0);
    memset(&sa, 0, sizeof(sa)); sa.sin_family = AF_INET; sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK); sa.sin_port = 0;
    if (bind(g_listen, (struct sockaddr*)&sa, sizeof(sa)) || listen(g_listen, 4)) return 0;
    getsockname(g_listen, (struct sockaddr*)&sa, &sl); g_port = ntohs(sa.sin_port);
    CloseHandle(CreateThread(NULL, 0, accept_thread, NULL, 0, NULL));
    return 1;
}
