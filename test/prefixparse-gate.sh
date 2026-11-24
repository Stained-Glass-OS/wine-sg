#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The shell's parsing of Win32 names with the \\?\ and \\.\ prefixes (patches/sg/9653).
# Transmission 4.1 (Qt) parses its settings folder in its long-path form, "\\?\C:\users\...",
# at start; it went to the Network as a computer called "?" and an "Enter network credentials"
# dialog stood in front of the program. Now:
#   \\?\C:\windows\system32 is the item C:\windows\system32 (as PathGetDriveNumber reads it)
#   \\.\pipe\... and \\?\GLOBALROOT\... fail at once (no computer "." or "?" is looked for,
#   nothing is asked)
#   WINE=/opt/wine-sg/bin/wine test/prefixparse-gate.sh
# Mutants (shell32): SG_MUTANT_PREFIXED_PATH_IS_NETWORK (shfldr_desktop.c),
# SG_MUTANT_DOT_IS_COMPUTER (shfldr_netplaces.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: needs mingw"; exit 77; }
command -v Xvfb >/dev/null || { echo "SKIP: needs Xvfb"; exit 77; }
T=$(mktemp -d /var/tmp/sg-prefixparse.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
DPY="${PREFIXPARSE_DPY:-334}"
while [ -e "/tmp/.X${DPY}-lock" ] || [ -e "/tmp/.X11-unix/X${DPY}" ]; do DPY=$((DPY + 1)); done
[ "$DPY" = 0 ] && { echo "refusing display :0"; exit 2; }
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XPID=$!
trap '"$WINESERVER" -k 2>/dev/null; kill $XPID 2>/dev/null; rm -rf "$T"' EXIT INT TERM
python3 -c "import time; time.sleep(2)"
export DISPLAY=":$DPY"
x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/prefixparse-probe.c" -lshell32 -lole32 \
    || { echo "FAIL  the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout 150 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '      %s\n' "$out"
rc=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; rc=1; }
l() { printf '%s\n' "$out" | awk -v t="$1" '$1 == "PARSE" && $2 == t { $1 = ""; $2 = ""; sub(/^  /, ""); print; exit }'; }
case "$(l drive)" in "00000000 C:\\windows\\system32") pass "\\\\?\\C:\\windows\\system32 is C:\\windows\\system32" ;;
    *) fail "\\\\?\\C:\\windows\\system32: '$(l drive)' (want 00000000 C:\\windows\\system32)" ;; esac
case "$(l plain)" in "00000000 C:\\windows\\system32") pass "C:\\windows\\system32 as before" ;; *) fail "plain: '$(l plain)'" ;; esac
case "$(l device)" in hung|"") fail "\\\\.\\pipe\\...: '$(l device)' (a dialog, or the probe stopped)" ;;
    00000000*) fail "\\\\.\\pipe\\... parsed: '$(l device)'" ;; *) pass "\\\\.\\pipe\\... fails at once, nothing asked" ;; esac
case "$(l qmark)" in hung|"") fail "\\\\?\\GLOBALROOT\\...: '$(l qmark)' (a dialog, or the probe stopped)" ;;
    00000000*) fail "\\\\?\\GLOBALROOT\\... parsed: '$(l qmark)'" ;; *) pass "\\\\?\\GLOBALROOT\\... fails at once, nothing asked" ;; esac
[ $rc = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
