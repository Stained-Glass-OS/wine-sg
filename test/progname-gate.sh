#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Programs' own names (patches/sg/5967). Open with, Task Manager and a program's Properties
# show a program's FileDescription; Wine's said "Wine Registry Editor", "Wine Task Manager",
# "Wine Internet Explorer" (Open with's More apps listed that one). Windows' own programs give
# their plain names. Each program here now gives its own name, and iexplore -- the old
# browser kept for programs that start it -- is kept out of Open with (NoOpenWith), as the
# Applications Windows hides are.
#   WINE=/opt/wine-sg/bin/wine test/progname-gate.sh
# What people meet says no "Wine" either (patches/sg/5970): the old browser's frame ("Web
# Browser"), Help, the C runtime's error box, the sound mapper. (The desktop window keeps
# "<name> - Wine Desktop": nobody sees it, and the compositor knows it by it.)
# Mutation: an rc's description put back to "Wine ..." / the NoOpenWith line taken out.
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0; XP=
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v Xvfb >/dev/null || { echo "SKIP: Xvfb missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
W=$(dirname "$WINE")
# the programs: an install's lib/wine, or a build tree's programs/*/x86_64-windows
if [ -d "$W/../lib/wine/x86_64-windows" ]; then set -- "$W"/../lib/wine/x86_64-windows/*.exe
else set -- "$W"/programs/*/x86_64-windows/*.exe "$W"/dlls/*.exe/x86_64-windows/*.exe; fi
desc() {
    python3 -I -c '
import sys
d = open(sys.argv[1], "rb").read()
k = "FileDescription".encode("utf-16-le")
i = d.rfind(k)
if i < 0: sys.exit()
j = i + len(k)
while d[j:j + 2] == b"\0\0": j += 2
e = j
while d[e:e + 2] != b"\0\0": e += 2
print(d[j:e].decode("utf-16-le", "replace"))' "$1"
}
wine_named=; n=0
for f in "$@"; do
    d=$(desc "$f"); [ -n "$d" ] || continue
    n=$((n + 1))
    case $d in "Wine "*) wine_named="$wine_named $(basename "$f")=\"$d\"" ;; esac
done
[ $n -gt 5 ] || fail "found the programs' names ($n)"
[ -z "$wine_named" ] && pass "no program calls itself \"Wine ...\" ($n named)" || fail "programs named Wine:$wine_named"
for p in regedit.exe:"Registry Editor" taskmgr.exe:"Task Manager" explorer.exe:"File Explorer"; do
    e=${p%%:*}; want=${p#*:}
    for f in "$@"; do [ "$(basename "$f")" = "$e" ] && got=$(desc "$f") && break; done
    [ "$got" = "$want" ] && pass "$e is \"$want\"" || fail "$e is \"$got\", not \"$want\""
done
# what people meet: window titles, error boxes, device names (patches/sg/5970) -- the old
# browser's frame, Help, the C runtime's error box, the sound mapper
if [ -d "$W/../lib/wine/x86_64-windows" ]; then L="$W/../lib/wine/x86_64-windows"/*
else L="$W/dlls/*/x86_64-windows/*.dll $W/dlls/*/x86_64-windows/*.cpl $W/programs/*/x86_64-windows/*.exe"; fi
seen=$(python3 -I -c '
import sys
bad = ["Wine Internet Explorer", "Wine C++ Runtime Library", "Wine Help", "Wine Sound Mapper",
       "Wine File Manager", "Wine Wordpad", "Wine Application Uninstaller", "start the Wine debugger"]
hits = set()
for f in sys.argv[1:]:
    try: d = open(f, "rb").read()
    except OSError: continue
    for b in bad:
        if b.encode("utf-16-le") in d: hits.add(b + " (" + f.rsplit("/", 1)[-1] + ")")
print("; ".join(sorted(hits)))' $L)
[ -z "$seen" ] && pass "no window title, error box or device says \"Wine ...\"" || fail "still says Wine: $seen"
T=$(mktemp -d /var/tmp/sg-progname.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; [ -n "$XP" ] && kill "$XP" 2>/dev/null; rm -rf "$T"' EXIT INT TERM
Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
export DISPLAY=":$(cat "$T/display")" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER
export WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d;winedbg.exe=d"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
out=$(timeout -s KILL 60 "$WINE" reg query 'HKCR\Applications\iexplore.exe' /v NoOpenWith 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | grep -q NoOpenWith && pass "iexplore is kept out of Open with (NoOpenWith)" \
    || fail "Applications\\iexplore.exe has no NoOpenWith"
out=$(timeout -s KILL 60 "$WINE" reg query 'HKCR\Applications\iexplore.exe\shell\open\command' 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | grep -qi iexplore.exe && pass "and still opens what is given to it by name" \
    || fail "Applications\\iexplore.exe lost its open command"
exit $RC
