/* Pointers in the NDR unmarshaller, called directly with hand-made format strings: [allocate(all_nodes)] gives everything
 * a pointer points to one block (one allocation, one free), and an object pointer which is unmarshalled over a pointer which
 * has data frees that data. */
#include <windows.h>
#include <rpc.h>
#include <rpcndr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

#define FC_END 0x5b
#define FC_PAD 0x5c
#define FC_LONG 8
#define FC_UP 0x12
#define FC_OP 0x13
#define FC_BOGUS_STRUCT 0x1a
#define FC_POINTER 0x36
#define FC_STRUCTPAD4 0x40
#define PTR_ALL_NODES 0x01
#define PTR_SIMPLE 0x08

static int allocs, frees;
static void *last_freed;

static void *__RPC_API my_alloc(size_t size) { allocs++; return calloc(1, size); }
static void __RPC_API my_free(void *p) { frees++; last_freed = p; free(p); }

struct node { LONG v; struct node *next; };

/* a list of nodes: a unique pointer to a complex structure with a long and a unique pointer to the next node */
static unsigned build_list_format(unsigned char *t, unsigned char attr)
{
    unsigned n = 0, members, table, size = sizeof(struct node);

    t[0] = FC_UP; t[1] = attr; t[2] = 2; t[3] = 0;                  /* the structure is at 4 */
    t[4] = FC_BOGUS_STRUCT; t[5] = 3; t[6] = size; t[7] = 0;
    t[8] = 0; t[9] = 0;                                              /* no conformant array */
    members = 12;
    t[members + n++] = FC_LONG;
    if (sizeof(void *) == 8) t[members + n++] = FC_STRUCTPAD4;
    t[members + n++] = FC_POINTER;
    t[members + n++] = FC_END;
    table = members + n;
    t[10] = (table - 10) & 0xff; t[11] = (table - 10) >> 8;         /* the pointer layout */
    t[table] = FC_UP; t[table + 1] = attr;
    t[table + 2] = (4 - (table + 2)) & 0xff; t[table + 3] = ((4 - (table + 2)) >> 8) & 0xff;
    return table + 4;
}

static unsigned build_list_wire(unsigned char *w, int nodes)
{
    unsigned n = 0;
    int i;
    ULONG v;

    v = 0x20000; memcpy(w + n, &v, 4); n += 4;                       /* the pointer to the first node */
    for (i = 0; i < nodes; i++)
    {
        v = 100 + i; memcpy(w + n, &v, 4); n += 4;                   /* the node: the value, the id of the next */
        v = i + 1 < nodes ? 0x20004 + 4 * i : 0; memcpy(w + n, &v, 4); n += 4;
    }
    return n;
}

static void init_server(MIDL_STUB_MESSAGE *msg, RPC_MESSAGE *rpc, MIDL_STUB_DESC *desc, unsigned char *types,
                        unsigned char *wire, unsigned len, BOOL client)
{
    memset(desc, 0, sizeof(*desc));
    memset(rpc, 0, sizeof(*rpc));
    desc->pfnAllocate = my_alloc;
    desc->pfnFree = my_free;
    desc->pFormatTypes = types;
    rpc->Buffer = wire; rpc->BufferLength = len;
    if (client) NdrClientInitializeNew(rpc, msg, desc, 0);
    else NdrServerInitializeNew(rpc, msg, desc);
    rpc->Buffer = wire; rpc->BufferLength = len;
    msg->Buffer = msg->BufferStart = wire;
    msg->BufferEnd = wire + len;
    msg->BufferLength = len;
}

static void test_list(BOOL all_nodes)
{
    MIDL_STUB_MESSAGE msg;
    RPC_MESSAGE rpc;
    MIDL_STUB_DESC desc;
    unsigned char types[64], wire[256];
    unsigned len;
    struct node *list = NULL, *n;
    char what[200];
    int count;

    build_list_format(types, all_nodes ? PTR_ALL_NODES : 0);
    len = build_list_wire(wire, 3);
    init_server(&msg, &rpc, &desc, types, wire, len, FALSE);
    allocs = frees = 0;
    NdrPointerUnmarshall(&msg, (unsigned char **)&list, types, 0);
    for (n = list, count = 0; n && count < 10; n = n->next) count++;
    sprintf(what, "%s: a list of three nodes is unmarshalled (%d nodes, values %ld %ld %ld)", all_nodes ? "all_nodes" : "plain pointers", count,
            list ? list->v : -1, list && list->next ? list->next->v : -1,
            list && list->next && list->next->next ? list->next->next->v : -1);
    check(count == 3 && list->v == 100 && list->next->v == 101 && list->next->next->v == 102 && !list->next->next->next, what);
    sprintf(what, "%s: %d allocations", all_nodes ? "all_nodes" : "plain pointers", allocs);
    check(all_nodes ? allocs == 1 : allocs == 3, what);
    frees = 0;
    NdrPointerFree(&msg, (unsigned char *)list, types);
    sprintf(what, "%s: freed with %d calls", all_nodes ? "all_nodes" : "plain pointers", frees);
    check(all_nodes ? frees == 1 && last_freed == list : frees == 3, what);
}

static void test_object_pointer(BOOL client)
{
    MIDL_STUB_MESSAGE msg;
    RPC_MESSAGE rpc;
    MIDL_STUB_DESC desc;
    unsigned char types[8], wire[16];
    ULONG v;
    LONG *old, *pointer;
    char what[200];

    /* an object pointer to a long, simple pointer */
    types[0] = FC_OP; types[1] = PTR_SIMPLE; types[2] = FC_LONG; types[3] = FC_PAD;
    v = 0x20000; memcpy(wire, &v, 4); v = 4242; memcpy(wire + 4, &v, 4);
    init_server(&msg, &rpc, &desc, types, wire, 8, client);
    old = my_alloc(sizeof(LONG));
    *old = 7;
    pointer = old;
    allocs = frees = 0; last_freed = NULL;
    NdrPointerUnmarshall(&msg, (unsigned char **)&pointer, types, 0);
    sprintf(what, "object pointer in a %s: new data %ld, freed %d (old data %s)", client ? "client" : "server",
            pointer ? *pointer : -1, frees, last_freed == old ? "freed" : "kept");
    if (client) check(pointer && *pointer == 4242 && frees == 1 && last_freed == old, what);
    else check(pointer && *pointer == 4242 && frees == 0, what);
    if (!client) free(old);

    /* nothing to free in a pointer with no data */
    init_server(&msg, &rpc, &desc, types, wire, 8, TRUE);
    pointer = NULL;
    frees = 0;
    NdrPointerUnmarshall(&msg, (unsigned char **)&pointer, types, 0);
    check(pointer && *pointer == 4242 && frees == 0, "object pointer with no data before: nothing is freed");
}

int main(int argc, char **argv)
{
    test_list(FALSE);
    test_list(TRUE);
    test_object_pointer(TRUE);
    test_object_pointer(FALSE);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
