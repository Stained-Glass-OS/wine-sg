#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# icuuc.dll, the system ICU's converters (6084). Windows 10 and later carry ICU in system32 and programs built
# against it import icuuc.dll by its unversioned names; Wine had none, so KeePassXC 2.8 (its Qt6Core imports
# icuuc's ucnv_* for Qt's text codecs) did not start at all ("icuuc.dll ... not found", c0000135). Our own
# icuuc.dll gives the documented converter API over the system's code pages. The expected values are what the
# real ICU (libicu 76) answers for the same calls; programs that look for the whole ICU (.NET looks for icu.dll,
# then icuuc.dll with icuin.dll) still find it incomplete and keep using the system's NLS, as before.
# The probe runs 64-bit and 32-bit (wow64).
#   WINE=/opt/wine-sg/bin/wine test/icuconv-gate.sh   (mutants SG_MUTANT_ICU_NO_PENDING, SG_MUTANT_ICU_NO_CALLBACK)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-icuconv.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
for arch in x86_64 i686; do
    if ! command -v $arch-w64-mingw32-gcc >/dev/null; then echo "SKIP  $arch: no compiler"; continue; fi
    TMPDIR=/var/tmp $arch-w64-mingw32-gcc -O1 -o "$T/probe-$arch.exe" "$HERE/icuconv-probe.c" 2>"$T/cc.log" ||
        { cat "$T/cc.log"; fail "$arch probe did not build"; continue; }
    out=$(timeout -s KILL 120 "$WINE" "$T/probe-$arch.exe" 2>/dev/null </dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed 's/^/      /'
    expect() {   # expect KEY VALUE WHAT
        if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$arch: $3"
        else fail "$arch: $3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
    }
    expect loaded 1 "icuuc.dll loads by name"
    expect system32 1 "from system32"
    expect exports 1 "with the converter functions Qt 6 imports"
    expect name_cp1252 windows-1252 "cp1252 is an alias of windows-1252"
    expect max_cp1252 1 "a single-byte converter: 1 byte a character at most"
    expect max_utf8 3 "UTF-8: 3 bytes a UTF-16 unit at most (ICU's figure)"
    expect missing "1 err 4" "an unknown charset: no converter, U_FILE_ACCESS_ERROR"
    expect available 1 "the list of converters starts with UTF-8"
    expect cp1252_to "0063 0061 0066 00e9 0020 20ac err 0" "windows-1252 to Unicode (0x80 is the euro sign)"
    expect latin1_from "6e 61 ef 76 65 20 1a err 0" "ISO-8859-1 has no euro sign: SUB (0x1a), as ICU"
    expect cp1252_from "6e 61 ef 76 65 20 80 err 0" "windows-1252 has it"
    expect sjis_to "0068 0069 3042 err 0" "Shift_JIS double-byte to Unicode"
    expect sjis_from "68 69 82 a0 err 0" "and back"
    expect badutf8_to "fffd 0028 err 0" "bad UTF-8: U+FFFD for the bad sequence only"
    expect utf16_bom "0041 0042 err 0" "UTF-16 reads its byte order mark"
    expect utf16_from "ff fe 68 00 69 00 err 0" "and writes one, then little-endian, as ICU does on this platform"
    expect koi8_to "043f 0440 0438 0432 0435 0442 err 0" "KOI8-R"
    expect split_first "0078 pending 2 err 0" "a character split across calls is held back (2 bytes pending)"
    expect split_whole "0078 20ac 0079 err 0" "and finished by the next call"
    expect truncated "fffd err 0" "input ending inside a character is substituted at flush"
    expect overflow "0061 0062 err 15" "a full target: U_BUFFER_OVERFLOW_ERROR"
    expect overflow_rest "0061 0062 0063 0064 0065 err 0" "the rest comes with the next call"
    expect callback "0061 0000 0062 invalid 1 err 0" "a program's own callback runs, with its context, and writes through ucnv_cbToUWriteUChars"
    expect done 1 "the probe ran to the end"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
