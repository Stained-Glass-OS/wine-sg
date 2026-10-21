#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The session's X11 hints for Linux programs (sg-common.sh SG_LINUX_APP_ENV: GDK_BACKEND=x11,
# SDL_VIDEODRIVER=x11, QT_QPA_PLATFORM=xcb) are Unix-side only (patches/sg/5946). A Windows build of
# GTK or SDL read them and found no X11: Geany said "cannot open display" and ended when started from
# Start or the desktop. With them set:
#   a Windows program does not see GDK_, GTK_, GSK_, SDL_ (nor QT_, upstream); MOZ_ENABLE_WAYLAND,
#   not a toolkit's, stays
#   a Unix program a Windows child process starts (as Start's gio launch) still gets them, and one a
#   Windows program starts with CreateProcess
#   a value the Windows side sets itself reaches its Windows children
#   WINE=/opt/wine-sg/bin/wine test/toolkitenv-gate.sh
# Mutants: ntdll SG_MUTANT_TOOLKIT_ENV_VISIBLE, SG_MUTANT_UNIX_ENV_DROPPED.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: needs mingw"; exit 77; }
T=$(mktemp -d /var/tmp/sg-toolkitenv.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/toolkitenv-probe.c" || { echo "FAIL  the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
rc=0
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; rc=1; }
v() { printf '%s\n' "$1" | tr -d '\r' | awk -v k="$2" '$1 == "WINENV" && $2 == k { print $3; exit }'; }
export GDK_BACKEND=x11 SDL_VIDEODRIVER=x11 GTK_THEME=Adwaita:dark GSK_RENDERER=gl QT_QPA_PLATFORM=xcb MOZ_ENABLE_WAYLAND=0
out=$(timeout 120 "$WINE" "$T/probe.exe" check 2>/dev/null)
{ [ "$(v "$out" GDK_BACKEND)" = - ] && [ "$(v "$out" SDL_VIDEODRIVER)" = - ] && [ "$(v "$out" GTK_THEME)" = - ] \
  && [ "$(v "$out" GSK_RENDERER)" = - ] && [ "$(v "$out" QT_QPA_PLATFORM)" = - ]; } \
    && pass "a Windows program does not see GDK_BACKEND, SDL_VIDEODRIVER, GTK_THEME, GSK_RENDERER, QT_QPA_PLATFORM" \
    || fail "seen by a Windows program: $(printf '%s' "$out" | tr -d '\r' | tr '\n' ' ')"
[ "$(v "$out" MOZ_ENABLE_WAYLAND)" = 0 ] && pass "...MOZ_ENABLE_WAYLAND, not a toolkit's, it does" || fail "MOZ_ENABLE_WAYLAND: $(v "$out" MOZ_ENABLE_WAYLAND)"
timeout 120 "$WINE" "$T/probe.exe" child "$T/unix.txt" >/dev/null 2>&1
[ "$(cat "$T/unix.txt" 2>/dev/null)" = "x11 x11 xcb" ] && pass "a Unix program a Windows child starts gets them (x11 x11 xcb)" \
    || fail "the Unix grandchild got '$(cat "$T/unix.txt" 2>/dev/null)' (want 'x11 x11 xcb')"
timeout 120 "$WINE" "$T/probe.exe" env 'C:\env.txt' >/dev/null 2>&1
{ grep -qx GDK_BACKEND=x11 "$WINEPREFIX/drive_c/env.txt" && grep -qx SDL_VIDEODRIVER=x11 "$WINEPREFIX/drive_c/env.txt" \
  && grep -qx QT_QPA_PLATFORM=xcb "$WINEPREFIX/drive_c/env.txt"; } 2>/dev/null \
    && pass "a Unix program a Windows one starts with CreateProcess gets them" \
    || fail "the Unix program's environment: $(grep -E '^(GDK|SDL|QT)_' "$WINEPREFIX/drive_c/env.txt" 2>/dev/null | tr '\n' ' ')"
out=$(timeout 120 "$WINE" cmd /c "set SDL_VIDEODRIVER=windows&& $(printf '%s' 'Z:')$(printf '%s' "$T/probe.exe" | tr / '\\') check" 2>/dev/null)
[ "$(v "$out" SDL_VIDEODRIVER)" = windows ] && pass "a value the Windows side sets reaches its Windows children" \
    || fail "Windows-set SDL_VIDEODRIVER: $(printf '%s' "$out" | tr -d '\r' | tr '\n' ' ')"
[ $rc = 0 ] && { echo "RESULT: PASS"; exit 0; }
echo "RESULT: FAIL"; exit 1
