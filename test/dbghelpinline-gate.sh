#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# dbghelp skips inline sites it cannot resolve without losing its place
# (patches/sg/0634). An S_INLINESITE whose inlinee is not in the PDB's IPI
# stream (LTCG builds reference other modules' ids: OBS Studio) is skipped to
# its S_INLINESITE_END -- and the jump was computed from the END record's
# bytes and without its length field, so the walk went on in the middle of
# records: obs.dll's PDB gave 10.7 million bogus symbols, gigabytes and 2
# minutes, then an assertion that aborted the program loading them (OBS's
# crash handler, winedbg). The gate builds a DLL and a PDB for it (with
# llvm-pdbutil yaml2pdb) whose first function holds two such nested sites;
# the function after them must be read, and nothing else invented.
#
#   WINE=/opt/wine-sg/bin/wine test/dbghelpinline-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw-w64 not installed"; exit 77; }
command -v llvm-pdbutil >/dev/null || { echo "SKIP: llvm-pdbutil (llvm) not installed"; exit 77; }
command -v python3 >/dev/null || { echo "SKIP: python3 not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-dbghelpinline.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
printf 'int __declspec(dllexport) before_inline(int x) { return x + 1; }\nint __declspec(dllexport) after_inline(int x) { return x * 2; }\n' > "$T/t.c"
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -shared -o "$T/t.dll" "$T/t.c" -Wl,--pdb="$T/t.pdb" &&
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O2 -o "$T/inl.exe" "$HERE/dbghelpinline-probe.c" -ldbghelp ||
    { fail "probe did not build"; exit 1; }
# the PDB the DLL names: its GUID and age, the functions' offsets in .text
guid=$(python3 - "$T/t.dll" <<'EOF'
import sys, uuid
d = open(sys.argv[1], 'rb').read()
i = d.find(b'RSDS')
print('{%s}' % str(uuid.UUID(bytes_le=d[i + 4:i + 20])).upper())
EOF
)
offs=$(x86_64-w64-mingw32-nm "$T/t.dll" | python3 -c '
import sys
syms = {l.split()[2]: int(l.split()[0], 16) for l in sys.stdin if len(l.split()) == 3}
text = min(v for k, v in syms.items() if k in ("before_inline", "after_inline")) & ~0xfff
print(syms["before_inline"] - text, syms["after_inline"] - text)')
set -- $offs
proc() { # proc NAME END OFFSET
    printf '          - Kind: S_GPROC32\n            ProcSym:\n              PtrParent: 0\n              PtrEnd: %s\n              PtrNext: 0\n              CodeSize: 4\n              DbgStart: 0\n              DbgEnd: 3\n              FunctionType: 116\n              Segment: 1\n              Offset: %s\n              Flags: [ ]\n              DisplayName: %s\n' "$2" "$3" "$1"
}
site() { # site PARENT END INLINEE
    printf '          - Kind: S_INLINESITE\n            InlineSiteSym:\n              PtrParent: %s\n              PtrEnd: %s\n              Inlinee: %s\n' "$1" "$2" "$3"
}
ends() { printf '          - Kind: %s\n            ScopeEndSym: {}\n' "$1"; }
{
    printf -- '---\nPdbStream:\n  Age: 1\n  Guid: '"'"'%s'"'"'\n  Signature: 1\n  Features: [ VC140 ]\n  Version: VC70\n' "$guid"
    printf 'DbiStream:\n  VerHeader: V70\n  Age: 1\n  BuildNumber: 36363\n  PdbDllVersion: 0\n  PdbDllRbld: 0\n  Flags: 0\n  MachineType: Amd64\n'
    printf '  Modules:\n    - Module: t.obj\n      ObjFile: t.obj\n      Modi:\n        Signature: 4\n        Records:\n'
    # offsets: proc 4 (56), sites 60 and 76 (16 each), their ends 92 and 96,
    # S_END 100, after_inline 104 (52), its S_END 156
    proc before_inline 100 "$1"
    site 4 96 2147483649
    site 60 92 2147483650
    ends S_INLINESITE_END; ends S_INLINESITE_END; ends S_END
    proc after_inline 156 "$2"
    ends S_END
    printf '...\n'
} > "$T/t.yaml"
rm -f "$T/t.pdb"
llvm-pdbutil yaml2pdb --pdb="$T/t.pdb" "$T/t.yaml" >/dev/null 2>&1 &&
    llvm-pdbutil dump --symbols "$T/t.pdb" | grep -q "156 | S_END" || { fail "the test PDB was not made as planned"; exit 1; }

timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/t.dll" "$T/t.pdb" "$T/inl.exe" "$WINEPREFIX/drive_c/"
out=$(timeout 120 "$WINE" 'C:\inl.exe' 'C:\t.dll' 2>/dev/null | tr -d '\r')
printf '      %s\n' $out
v() { printf '%s\n' "$out" | sed -n "s/^$1=//p"; }
[ "$(v before)" = 1 ] && pass "the function holding the inline sites is read" || fail "before_inline: $(v before)"
[ "$(v after)" = 1 ] && pass "the function after the sites they cannot resolve is read" || fail "after_inline: $(v after)"
[ "$(v symbols)" -le 6 ] 2>/dev/null && pass "and nothing is invented ($(v symbols) symbols)" || fail "symbols: $(v symbols)"
exit $RC
