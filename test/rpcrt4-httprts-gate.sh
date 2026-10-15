#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# CORE rpcrt4: RTS command parsing (MS-RPCH 2.2.3.5) in RPCRT4_IsValidHttpPacket / RPCRT4_NextHttpHeaderField.
# Host-compiled harness: the two functions are cut out of the patched dlls/rpcrt4/rpc_message.c ($SRC).
#   SRC=/path/to/wine/dlls/rpcrt4/rpc_message.c test/rpcrt4-httprts-gate.sh
# Mutants (compile with CFLAGS=-DMUT_x): see patches/notes/5119.txt.
set -u
SRC="${SRC:-/var/tmp/fast-a/t6/wine-10.0/dlls/rpcrt4/rpc_message.c}"
[ -f "$SRC" ] || { echo "SKIP: no $SRC"; exit 77; }
D=$(mktemp -d "${TMPDIR:-/var/tmp}/httprts.XXXXXX") || exit 1
trap 'rm -rf "$D/h.c" "$D/h" "$D/body.c"; rmdir "$D"' EXIT
a=$(grep -n '^/\* Size in bytes of the data following the command type' "$SRC" | cut -d: -f1)
b=$(grep -n '^#define READ_HTTP_PAYLOAD_FIELD_TYPE' "$SRC" | cut -d: -f1)
[ -n "$a" ] && [ -n "$b" ] || { echo "FAIL  functions not found in $SRC"; exit 1; }
sed -n "${a},$((b-1))p" "$SRC" > "$D/body.c"
cat > "$D/h.c" <<'EOC'
#include <stdio.h>
#include <string.h>
typedef unsigned char BYTE; typedef unsigned long long ULONGLONG;
typedef unsigned int ULONG; typedef int BOOL; typedef unsigned short USHORT;
typedef struct { ULONG a; unsigned short b, c; unsigned char d[8]; } GUID;
#define TRUE 1
#define FALSE 0
#define FIXME(...) ((void)0)
typedef struct { struct { unsigned short num_data_items; } http; } RpcPktHdr;
#include "body.c"
static int fails;
static void chk(const char *name, int ok) { printf("%s  %s\n", ok ? "ok  " : "FAIL", name); if (!ok) fails++; }
static unsigned char buf[256];
/* one command: type, then payload of 'extra' bytes (first dword = payload[0]) */
static int valid(unsigned type, unsigned first, unsigned total_after_type, int avail_after_type)
{
    RpcPktHdr h; h.http.num_data_items = 1;
    memset(buf, 0, sizeof(buf));
    *(ULONG *)buf = type; *(ULONG *)(buf + 4) = first;
    return RPCRT4_IsValidHttpPacket(&h, buf, 4 + avail_after_type);
    (void)total_after_type;
}
static int next(unsigned type, unsigned first)
{
    memset(buf, 0, sizeof(buf));
    *(ULONG *)buf = type; *(ULONG *)(buf + 4) = first;
    return (int)(RPCRT4_NextHttpHeaderField(buf) - buf);
}
struct t { const char *n; unsigned type, first; int size; } tab[] = {
    {"ReceiveWindowSize", 0, 0, 4}, {"FlowControlAck", 1, 0, 24}, {"ConnectionTimeout", 2, 0, 4},
    {"Cookie", 3, 0, 16}, {"ChannelLifetime", 4, 0, 4}, {"ClientKeepalive", 5, 0, 4},
    {"Version", 6, 0, 4}, {"Empty", 7, 0, 0}, {"Padding 0", 8, 0, 4}, {"Padding 10", 8, 10, 14},
    {"Padding 0xffff", 8, 0xffff, 4 + 0xffff},
    {"NegativeANCE", 9, 0, 0}, {"ANCE", 0xa, 0, 0},
    {"ClientAddress IPv4", 0xb, 0, 20}, {"ClientAddress IPv6", 0xb, 1, 32},
    {"AssociationGroupId", 0xc, 0, 16}, {"Destination", 0xd, 0, 4}, {"PingTrafficSentNotify", 0xe, 0, 4},
};
int main(void)
{
    unsigned i; char name[96];
    for (i = 0; i < sizeof(tab)/sizeof(tab[0]); i++)
    {
        struct t *e = &tab[i];
        if (e->size > 200) { /* too big for buf: only the validator-limit check below */ continue; }
        snprintf(name, sizeof(name), "%s: next field after %d bytes", e->n, 4 + e->size);
        chk(name, next(e->type, e->first) == 4 + e->size);
        snprintf(name, sizeof(name), "%s: valid with exactly %d bytes", e->n, e->size);
        chk(name, valid(e->type, e->first, e->size, e->size));
        if (e->size > 4 || (e->size == 4 && e->type != 8 && e->type != 0xb))
        {
            snprintf(name, sizeof(name), "%s: rejected when one byte short", e->n);
            chk(name, !valid(e->type, e->first, e->size, e->size - 1));
        }
    }
    chk("Padding with ConformanceCount 0x10000 is rejected", !valid(8, 0x10000, 0, 100));
    chk("ClientAddress with address type 2 is rejected", !valid(0xb, 2, 0, 100));
    chk("unknown command 0xf is rejected", !valid(0xf, 0, 0, 100));
    chk("two commands are walked (Empty, Version)", ({ RpcPktHdr h; h.http.num_data_items = 2;
        memset(buf, 0, sizeof(buf)); *(ULONG *)buf = 7; *(ULONG *)(buf + 4) = 6;
        RPCRT4_IsValidHttpPacket(&h, buf, 12) && !RPCRT4_IsValidHttpPacket(&h, buf, 11); }));
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
EOC
${CC:-gcc} -w -I"$D" ${CFLAGS:-} -o "$D/h" "$D/h.c" || { echo "FAIL  compile"; exit 1; }
"$D/h"
