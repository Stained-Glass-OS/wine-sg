#!/bin/sh
# widl: [pipe] types (typedef pipe T P;) and [async] procedures of remote interfaces: the headers, the stubs and the
# format strings it writes, for 32 and 64 bit, and that the C it writes compiles.  (The behaviour is test/rpcrt4-pipe-gate.sh
# and test/rpcrt4-async-gate.sh.)
#
#   WIDL=/path/to/widl test/widl-pipe-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WIDL="${WIDL:-${WINE:+$(dirname "$WINE")/tools/widl/widl}}"
[ -x "${WIDL:-/nonexistent}" ] || { echo "SKIP: no widl"; exit 77; }
for cc in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do
    command -v $cc >/dev/null || { echo "SKIP: $cc not installed"; exit 77; }
done
RC=0
T=$(mktemp -d /var/tmp/sg-widl-pipe.XXXXXX)
trap 'rm -rf "$T"' EXIT INT TERM
check() { if [ "$1" = 0 ]; then echo "PASS  $2"; else echo "FAIL  $2"; RC=1; fi; }
has() { grep -q -- "$1" "$2"; }

for a in x86_64 i686; do
    case $a in x86_64) m=--win64; ptr=8 ;; *) m=--win32; ptr=4 ;; esac
    echo "== $a"
    mkdir -p "$T/$a"
    for idl in rpcrt4-pipe rpcrt4-async-rpc; do
        cp "$HERE/$idl.idl" "$T/$a/"
        (cd "$T/$a" && "$WIDL" $m -Oif --prefix-server=s_ -h -H $idl.h -o /dev/null $idl.idl \
            && "$WIDL" $m -Oif --prefix-server=s_ -c -o ${idl}_c.c $idl.idl \
            && "$WIDL" $m -Oif --prefix-server=s_ -s -o ${idl}_s.c $idl.idl)
        check $? "widl runs on $idl.idl"
        for f in c s; do
            TMPDIR=/var/tmp $a-w64-mingw32-gcc -c -w -I"$T/$a" -o "$T/$a/${idl}_$f.o" "$T/$a/${idl}_$f.c"
            check $? "the ${f} stub of $idl compiles"
        done
    done
    P=$T/$a/rpcrt4-pipe
    H=$P.h; C=${P}_c.c; S=${P}_s.c
    has 'void (__RPC_USER \*Pull)(char \*state,LONG \*buf,ULONG esize,ULONG \*ecount);' $H; check $? "a pipe of long is a struct with Pull(state, buf, esize, ecount)"
    has 'void (__RPC_USER \*Push)(char \*state,LONG \*buf,ULONG ecount);' $H; check $? "... Push(state, buf, ecount)"
    has 'void (__RPC_USER \*Alloc)(char \*state,ULONG bsize,LONG \*\*buf,ULONG \*bcount);' $H; check $? "... Alloc(state, bsize, buf, bcount)"
    has 'Pull)(char \*state,REC \*buf' $H; check $? "a pipe of structures has structures as its elements"
    has 'LPIPE p' $H; check $? "the pipe parameter is passed by value"
    has 'FC_PIPE' $C; check $? "the type format string has FC_PIPE"
    n=$(grep -c 'FC_PIPE' $C); [ "$n" -ge 3 ]; check $? "one FC_PIPE description for each pipe type used ($n)"
    has 'flags: must size, pipe, in' $C; check $? "the parameter flags say: pipe, in"
    has 'flags: must size, pipe, out' $C; check $? "... pipe, out"
    has 'flags: must size, pipe, in, out' $C; check $? "... pipe, in, out"
    # the Oi2 flags byte of SumIn (the line after "server buffer"): HasPipes is 0x08
    v=$(awk '/procedure PipeT::SumIn/ {f=1} f && /server buffer/ {getline; gsub(/[^0-9a-fx]/,""); print; exit}' $C)
    [ -n "$v" ] && [ $(( v & 8 )) -ne 0 ]; check $? "the Oi2 flags of a procedure with pipes say HasPipes ($v)"
    v=$(awk '/procedure PipeT::Stop/ {f=1} f && /server buffer/ {getline; gsub(/[^0-9a-fx]/,""); print; exit}' $C)
    [ -n "$v" ] && [ $(( v & 8 )) -eq 0 ]; check $? "... and a procedure without them does not ($v)"
    # the stack: the pipe struct is passed by reference where it is large (64 bit), inline otherwise
    v=$(awk '/procedure PipeT::Echo/ {f=1} f && /stack size/ {print; exit}' $C | sed 's/.*= \([0-9]*\).*/\1/')
    [ "$v" = "$((ptr == 8 ? 16 : 20))" ]; check $? "the stack size of Echo(handle, in out pipe) is $v"

    A=$T/$a/rpcrt4-async-rpc
    has 'PRPC_ASYNC_STATE Add_AsyncHandle,' $A.h; check $? "an [async] procedure takes the async state first (header)"
    has 'PRPC_ASYNC_STATE Add_AsyncHandle,' ${A}_c.c; check $? "... and its client stub"
    has 'NdrAsyncClientCall' ${A}_c.c; check $? "the client stub calls NdrAsyncClientCall"
    n=$(grep -c 'NdrAsyncClientCall' ${A}_c.c); [ "$n" = 6 ]; check $? "... for the six [async] procedures and not the other ($n)"
    n=$(grep -c 'NdrAsyncServerCall' ${A}_s.c); [ "$n" = 6 ]; check $? "the dispatch table has NdrAsyncServerCall for them ($n)"
    has 'NdrServerCall2' ${A}_s.c; check $? "... and NdrServerCall2 for the other"
    n=$(grep -c 'NdrFcLong(0x4000)' ${A}_c.c); [ "$n" = 6 ]; check $? "the procedures are marked RPCF_Asynchronous ($n)"
    v=$(awk '/procedure AsyT::Add/ {f=1} f && /server buffer/ {getline; gsub(/[^0-9a-fx]/,""); print; exit}' ${A}_c.c)
    [ -n "$v" ] && [ $(( v & 0x80 )) -ne 0 ]; check $? "their Oi2 flags say HasAsyncHandle ($v)"
    v=$(awk '/procedure AsyT::Add/ {f=1} f && /stack size/ {print; exit}' ${A}_c.c | sed 's/.*= \([0-9]*\).*/\1/')
    [ "$v" = "$((ptr * 5))" ]; check $? "the async state takes a slot of the stack: Add(handle, a, b, sum) has $v bytes"
    v=$(awk '/parameter a\)/ {f=1} f && /stack offset/ {print; exit}' ${A}_c.c | sed 's/.*= \([0-9]*\).*/\1/')
    [ "$v" = "$((ptr * 2))" ]; check $? "the first parameter after the async state and the handle is at offset $v"
done
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
