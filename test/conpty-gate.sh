#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A terminal's own conpty.dll runs over Wine's pseudo consoles (patches/sg/7003). VS Code (node-pty)
# and Windows Terminal ship conpty.dll, which starts its own OpenConsole.exe on the kernel's console
# driver (\Device\ConDrv\Server): VS Code's terminal failed with "Cannot launch conpty". wine.inf
# sets HKLM\Software\Wine\DllOverrides conpty=builtin and Wine's conpty.dll keeps its calls on
# kernelbase's pseudo console and conhost: create, resize, clear (conhost drops the cleared lines),
# show/hide and reparent (read whole, so the next signal is found), release (conhost then exits with
# its last client), close. The probe (test/conpty-probe.c) loads a stand-in conpty.dll from its own
# folder (test/conpty-fake.c answers a marker HRESULT), 64- and 32-bit.
#   WINE=/opt/wine-sg/bin/wine test/conpty-gate.sh
# Mutants: SG_MUTANT_CONPTY_RELEASE_KEEPS (dlls/conpty/conpty.c),
#          SG_MUTANT_CONPTY_CLEAR_IGNORED (programs/conhost/conhost.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in x86_64-w64-mingw32-gcc i686-w64-mingw32-gcc; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-conpty.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
mkdir -p "$T/t64" "$T/t32"
for b in 64 32; do
    cc=x86_64-w64-mingw32-gcc; [ $b = 32 ] && cc=i686-w64-mingw32-gcc
    TMPDIR=/var/tmp $cc -O1 -o "$T/t$b/probe.exe" "$HERE/conpty-probe.c" || { fail "probe ($b-bit) did not build"; exit 1; }
    TMPDIR=/var/tmp $cc -O1 -shared -Wl,--kill-at -o "$T/t$b/conpty.dll" "$HERE/conpty-fake.c" || { fail "stand-in ($b-bit) did not build"; exit 1; }
done
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp -r "$T/t64" "$T/t32" "$WINEPREFIX/drive_c/"
for b in 64 32; do
    out=$(timeout -s KILL 180 "$WINE" "C:\\t$b\\probe.exe" "C:\\t$b" 2>/dev/null | tr -d '\r')
    printf '%s\n' "$out" | sed "s/^/      $b-bit: /"
    check() {
        if printf '%s\n' "$out" | grep -Eq "^(.* )?$1=1( |$)"; then pass "$b-bit: $2"
        else fail "$b-bit: $2 ($(printf '%s\n' "$out" | grep -o "$1=[^ ]*" | head -1 || echo none))"; fi
    }
    check builtin_used        "the program's own conpty.dll is replaced by Wine's"
    check created             "ConptyCreatePseudoConsole makes a pseudo console"
    check spawned             "cmd starts on it"
    check output              "what cmd writes comes out of the pseudo console"
    check clear_ok            "ConptyClearPseudoConsole succeeds"
    check resize_ok           "ConptyResizePseudoConsole succeeds"
    check cleared_not_redrawn "after a clear, the repaint does not draw the cleared lines again"
    check signals_in_sync     "after show/hide, reparent, clear and resize the host still reads its input"
    check release_ok          "ConptyReleasePseudoConsole succeeds"
    check eof_after_release   "released, the host goes with its last client: the output pipe ends"
    check done                "the probe ran to the end"
done
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
