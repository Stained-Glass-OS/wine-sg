#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# MSI transforms made on this system (patches/sg/0402): an administrator's
# packaging tool customises a vendor package as a .mst -- Orca, InstEd and
# the like call MsiDatabaseGenerateTransform and
# MsiCreateTransformSummaryInfo, which Wine did not implement -- and deploys
# it with `msiexec /i package.msi TRANSFORMS=corp.mst /qn`. The probe builds
# a small registry-only package and a customised copy (a changed value, a new
# row, a removed row, a new table), makes the transform, and applies it to a
# third copy; then msiexec installs the package with it.
#
#   WINE=/opt/wine-sg/bin/wine test/msitransform-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-msitransform.XXXXXX)
mkdir -p "$T/home"
export HOME="$T/home" XDG_CONFIG_HOME="$T/home/.config" XDG_DATA_HOME="$T/home/.local/share" XDG_DESKTOP_DIR="$T/home/Desktop"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -municode -O2 -o "$T/msitransform-probe.exe" "$HERE/msitransform-probe.c" -lmsi || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
mkdir -p "$WINEPREFIX/drive_c/t"
cp "$T/msitransform-probe.exe" "$WINEPREFIX/drive_c/"
out=$(timeout -s KILL 120 "$WINE" 'C:\msitransform-probe.exe' 'C:\t' 2>/dev/null | tr -d '\r')
echo "$out"
has() { printf '%s\n' "$out" | grep -qx "$1"; }
has base=0 || fail "the test package was not built"
has same=232 && has differ=0 && pass "MsiDatabaseGenerateTransform with no file says whether two databases differ" || fail "compare-only"
has generate=0 && [ -s "$WINEPREFIX/drive_c/t/corp.mst" ] && pass "it writes the transform" || fail "generate"
has summary=0 && pass "MsiCreateTransformSummaryInfo gives it its summary information" || fail "summary"
has apply=0 && has site=corp.sgtest.lan && has 'mode=#42' && has 'gone=(no row)' && has 'newtable=from the transform' \
    && pass "applied to another copy, it changes, adds and removes rows and adds a table" || fail "apply"

timeout -s KILL 300 "$WINE" msiexec /i 'C:\t\base.msi' 'TRANSFORMS=C:\t\corp.mst' /qn >/dev/null 2>&1; rc=$?
q=$(timeout -s KILL 60 "$WINE" reg query 'HKLM\Software\SGTEST\TransformTest' /reg:64 2>/dev/null | tr -d '\r')
echo "$q"
[ $rc = 0 ] && printf '%s' "$q" | grep -q 'Site.*REG_SZ.*corp.sgtest.lan' && printf '%s' "$q" | grep -q 'Mode.*REG_DWORD.*0x2a' \
    && ! printf '%s' "$q" | grep -q 'Removed' \
    && pass "msiexec /i package.msi TRANSFORMS=corp.mst /qn installs the customised package" || fail "msiexec with the transform (rc $rc)"
exit $RC
