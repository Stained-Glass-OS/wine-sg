#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# DirectWrite finds the font GDI uses for a substituted name (patches/sg/0177).
#   WINE=/opt/wine-sg/bin/wine test/dwlogfont-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
fc-list 2>/dev/null | grep -q 'Liberation Sans' || { echo "SKIP: Liberation Sans is not installed"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dwlogfont.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/dwlogfont-probe.exe" "$HERE/dwlogfont-probe.c" -ldwrite -lgdi32 -luuid || { fail "probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
# a substitute as a Stained Glass machine has them (sg-shell theme/52-sg-fonts.reg)
"$WINE" reg add 'HKLM\Software\Microsoft\Windows NT\CurrentVersion\FontSubstitutes' /v 'Segoe UI' /d 'Liberation Sans' /f >/dev/null 2>&1
"$WINESERVER" -w
out=$(timeout -s KILL 60 "$WINE" "$T/dwlogfont-probe.exe" "Segoe UI" "MS Shell Dlg" Tahoma "No Such Font Xyz" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
v() { printf '%s\n' "$out" | sed -n "s/^$1=//p"; }
seg=$(v 'Segoe UI'); msd=$(v 'MS Shell Dlg'); tah=$(v Tahoma); none=$(v 'No Such Font Xyz')
case "$seg" in *",Liberation Sans,0") pass "Segoe UI (substituted): the font GDI uses, Liberation Sans" ;; *) fail "Segoe UI: $seg" ;; esac
g=${msd%%,*}; case "$msd" in *",0") pass "MS Shell Dlg: found (GDI: $g)" ;; *) fail "MS Shell Dlg: $msd" ;; esac
case "$tah" in *",Tahoma,0") pass "a real family is still itself" ;; *) fail "Tahoma: $tah" ;; esac
case "$none" in *",0x88985002") pass "an unknown name is still DWRITE_E_NOFONT (not GDI's default font)" ;; *) fail "unknown: $none" ;; esac
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
