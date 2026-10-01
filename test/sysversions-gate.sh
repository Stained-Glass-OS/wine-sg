#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The versions installers look for in the system's files (patches/sg/0636).
# TortoiseGit's MSI searches for shell32.dll >= 6.3.14392.0 ("Windows 10 1607
# or later") and vcruntime140.dll >= 14.42 ("the latest Visual C++ 2015-2022
# runtime"); Wine's shell32 said XP's 6.0.2900 and its vcruntime140 had no
# version at all, so the install stopped (msiexec 1603, SG Store "code 67").
# The probe makes a package with TortoiseGit's AppSearch rows and runs the
# action: both properties must be found, and the files must say 10.0.19041
# and 14.42.
#
#   WINE=/opt/wine-sg/bin/wine test/sysversions-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw-w64 not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-sysversions.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O2 -o "$T/sysver.exe" "$HERE/sysversions-probe.c" -lmsi -lversion ||
    { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/sysver.exe" "$WINEPREFIX/drive_c/"
out=$(timeout 120 "$WINE" 'C:\sysver.exe' 2>/dev/null | tr -d '\r')
printf '      %s\n' $out
v() { printf '%s\n' "$out" | sed -n "s/^$1=//p"; }
case "$(v win10)" in *[Ss]ys[Ww][Oo][Ww]64*) pass "an MSI finds \"Windows 10 1607 or later\" (shell32.dll >= 6.3.14392)" ;;
    *) fail "WIN10_1607_FOUND: '$(v win10)' (shell32 $(v shell32))" ;; esac
case "$(v vcredist)" in *vcruntime140.dll) pass "and the Visual C++ 2015-2022 runtime (vcruntime140.dll >= 14.42)" ;;
    *) fail "VC_REDIST_INSTALLED: '$(v vcredist)' (vcruntime140 $(v vcruntime140))" ;; esac
case "$(v shell32)" in 10.0.19041.*) pass "shell32.dll says 10.0.19041" ;; *) fail "shell32 $(v shell32)" ;; esac
[ "$(v vcruntime140)" = 14.42.34433.0 ] && [ "$(v vcruntime140_1)" = 14.42.34433.0 ] &&
    pass "vcruntime140 and vcruntime140_1 say 14.42.34433.0, as msvcp140" ||
    fail "vcruntime140 $(v vcruntime140), vcruntime140_1 $(v vcruntime140_1)"
exit $RC
