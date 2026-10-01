#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A new user's first taskbar pins (patches/sg/0741). David wanted SG Store
# pinned to the taskbar out of the box. The system's layout is
# HKLM\Software\Stained Glass\Taskbar DefaultPins ("Name|program" each,
# sg-shell ships File Explorer and SG Store); the taskbar pins them when it
# starts for a user who has no PinOrder yet, in their order, leaving out a
# program that is not installed -- and only then: a pin the user removed
# stays removed at the next start.
#
#   WINE=/opt/wine-sg/bin/wine test/defaultpins-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
unset DISPLAY WAYLAND_DISPLAY
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-defaultpins.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/taskbar-probe.exe" "$HERE/taskbar-probe.c" -lshell32 -lgdi32 -luser32 || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/taskbar-probe.exe" "$WINEPREFIX/drive_c/"
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1024x700 /f >/dev/null 2>&1
# the layout: File Explorer, a program that is not installed, notepad (in SG Store's place)
"$WINE" reg add 'HKLM\Software\Stained Glass\Taskbar' /v DefaultPins /t REG_MULTI_SZ \
    /d 'File Explorer|%SystemRoot%\explorer.exe\0Not There|C:\nowhere\none.exe\0Notepad|%SystemRoot%\notepad.exe' /f >/dev/null 2>&1
"$WINESERVER" -w
PINS="$WINEPREFIX/drive_c/users/$(id -un)/AppData/Roaming/Microsoft/Internet Explorer/Quick Launch/User Pinned/TaskBar"

cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
"$WINE" explorer /desktop=shell,1024x700 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
sleep 3
"$WINE" taskbar-probe.exe state 2>/dev/null | tr -d '\r' > "$T/\$1.state"
"$WINE" reg query 'HKCU\Software\Stained Glass\Taskbar' /v PinOrder 2>/dev/null | tr -d '\r' > "$T/\$1.order"
ls "$PINS" > "$T/\$1.pins" 2>&1
EOF
chmod +x "$T/session.sh"
timeout -s KILL 200 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh" first
"$WINESERVER" -k; "$WINESERVER" -w
# the user unpins notepad (as the taskbar's Unpin does: the shortcut and its order)
rm -f "$PINS/Notepad.lnk"
"$WINE" reg add 'HKCU\Software\Stained Glass\Taskbar' /v PinOrder /t REG_MULTI_SZ /d 'File Explorer.lnk' /f >/dev/null 2>&1
"$WINESERVER" -w
timeout -s KILL 200 xvfb-run -a -s '-screen 0 1024x700x24' "$T/session.sh" again

pins() { grep -c '^pin=' "$T/$1.state"; }
grep -qx 'File Explorer.lnk' "$T/first.pins" && grep -qx 'Notepad.lnk' "$T/first.pins" \
    && pass "a new user gets the layout's pins ($(tr '\n' ' ' < "$T/first.pins"))" || fail "first pins: $(tr '\n' ' ' < "$T/first.pins")"
grep -qi 'not there' "$T/first.pins" && fail "a program that is not installed was pinned" || pass "a program that is not installed is left out"
tr '\0' '\n' < "$T/first.order" | grep -q 'File Explorer.lnk.*Notepad.lnk' \
    && pass "in the layout's order (PinOrder: $(sed -n 's/.*REG_MULTI_SZ *//p' "$T/first.order"))" || fail "order: $(cat "$T/first.order")"
[ "$(pins first)" = 2 ] && pass "the taskbar shows them: 2 pin buttons" || fail "pin buttons: $(pins first) ($(tr '\n' ' ' < "$T/first.state"))"
grep -qx 'Notepad.lnk' "$T/again.pins" && fail "the pin the user removed came back" || pass "a removed pin stays removed at the next start"
[ "$(pins again)" = 1 ] && pass "next start: 1 pin button" || fail "again: $(pins again) pin buttons"

echo
[ "$RC" -eq 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
