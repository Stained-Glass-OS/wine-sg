#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A new user's DWM colour settings (patches/sg/9756). Windows writes them for every profile
# (HKCU\Software\Microsoft\Windows\DWM: AccentColor, ColorizationColor, ColorizationAfterglow and the
# balances, ColorPrevalence, EnableAeroPeek, Composition); Wine wrote none until Settings saved an
# accent, so programs that read them found nothing (MusicBrainz Picard: "Failed reading
# ColorizationColor from registry", it fell back to its own colour). They are the defaults Stained
# Glass uses everywhere else (Settings, dwmapi, uxtheme: purple 7B2FBE), written only where unset.
#   WINE=/opt/wine-sg/bin/wine test/dwmdefaults-gate.sh
# No mutant: the defaults are inf data (wine.inf.in is not preprocessed); the gate fails on 10.0-1151.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: needs mingw"; exit 77; }
T=$(mktemp -d /var/tmp/sg-dwmdefaults.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/dwmdefaults-probe.c" -ladvapi32 || { echo "FAIL  the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
printf '      %s\n' "$out"
rc=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; rc=1; }
r() { printf '%s\n' "$out" | awk -v k="$1" '$1 == "REG" && $2 == k { print $3; exit }'; }
[ "$(r AccentColor)" = ffbe2f7b ] && pass "AccentColor is Stained Glass purple (ABGR ffbe2f7b)" || fail "AccentColor '$(r AccentColor)'"
[ "$(r ColorizationColor)" = c47b2fbe ] && [ "$(r ColorizationAfterglow)" = c47b2fbe ] && pass "ColorizationColor and ColorizationAfterglow: the same colour, ARGB c47b2fbe" || fail "colorization '$(r ColorizationColor)' '$(r ColorizationAfterglow)'"
[ "$(r ColorizationColorBalance)" = 00000059 ] && [ "$(r ColorPrevalence)" = 00000000 ] && [ "$(r EnableAeroPeek)" = 00000001 ] && pass "the balance, colour prevalence and Peek as Windows writes them" || fail "balance '$(r ColorizationColorBalance)' prevalence '$(r ColorPrevalence)' peek '$(r EnableAeroPeek)'"
[ "$(printf '%s\n' "$out" | sed -n 's/^DWMAPI //p')" = "00000000 c47b2fbe" ] && pass "DwmGetColorizationColor gives the same colour" || fail "dwmapi '$(printf '%s\n' "$out" | sed -n 's/^DWMAPI //p')'"
# Saved accents stay: wineboot -u over a chosen accent keeps it
"$WINE" reg add 'HKCU\Software\Microsoft\Windows\DWM' /v ColorizationColor /t REG_DWORD /d 0xc40078d7 /f >/dev/null 2>&1
timeout -s KILL 300 "$WINE" wineboot -u >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout 120 "$WINE" "$T/probe.exe" 2>/dev/null | tr -d '\r')
[ "$(r ColorizationColor)" = c40078d7 ] && pass "an update keeps a colour the user chose" || fail "after wineboot -u: '$(r ColorizationColor)'"
[ $rc = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
