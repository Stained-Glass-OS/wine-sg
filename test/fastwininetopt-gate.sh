#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# wininet: INTERNET_OPTION_PROXY on a session handle, FindFirst/NextUrlCacheContainer.
#
#   WINE=/opt/wine-sg/bin/wine test/fastwininetopt-gate.sh   (mutants SG_MUTANT_WININET_NOPROXYSET, SG_MUTANT_WININET_NOCONT)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
T=$(mktemp -d /var/tmp/sg-fastwininetopt.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastwininetopt-probe.c" -lole32 -loleaut32 -luuid -lwininet || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/run.sh" <<EOS
#!/bin/sh
timeout -s KILL 120 "$WINE" "$T/probe.exe" >"$T/o" 2>/dev/null </dev/null
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
tr -d '\r' < "$T/o" > "$T/o2"; mv "$T/o2" "$T/o"
v() { sed -n "s/^$1=//p" "$T/o" | head -1; }
w() { sed -n "s/^$1 //p" "$T/o" | head -1; }
grep -q "^DONE" "$T/o" || { fail "the probe did not finish"; head -30 "$T/o"; echo "RESULT: FAIL"; exit 1; }
chk() { [ "$(v "$1")" = "$2" ] && pass "$3" || fail "$3: got '$(v "$1")'"; }
chk P0 "1 1 - -" "a direct session has no proxy"
chk P1set "1 0" "INTERNET_OPTION_PROXY can be set on a session handle"
chk P1 "1 3 proxy.example:8080 *.local" "the proxy and the bypass list are returned"
chk P2 "1 3 other.example:3128 <local>" "the ANSI form sets it too"
chk P3 "1 1 - -" "direct access removes the proxy"
chk P4 "0 87" "a buffer shorter than INTERNET_PROXY_INFO is refused"
chk P5 "0 87" "a proxy access type without a proxy name is refused"
chk P6 "0 87" "an unknown access type is refused"
chk C0 "0 122 1" "container enumeration with a small buffer reports the size"
chk C1 "3 259 Cookies[Cookie:]1,History[Visited:]1,Content[]1," "wide: three containers, then no more items"
chk C1b "1" "FindCloseUrlCache closes a container enumeration"
chk C2 "3 259 Cookies[Cookie:],History[Visited:],Content[]," "ANSI container enumeration"
chk C3 "0 6" "an entry enumeration call refuses a container handle"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
