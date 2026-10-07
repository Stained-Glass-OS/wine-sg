#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# File Explorer's Network folder (patches/sg/0469): it lists the computers on
# the network, a computer lists its shares, and a share is a folder at
# \\computer\share. It was empty, and \\server paths could not be parsed --
# Network and \\name in File Explorer did nothing. The lists come from
# sg-session's sg-netbrowse; here a stand-in (SG_NETBROWSE) names two
# computers and two shares.
#
# A share's comment is in the Comments column (1471): sg-netbrowse's shares2
# gives "name<tab>comment"; an older helper (no shares2: exit 2) still lists
# the names. Mutant: SG_MUTANT_NO_SHARE_COMMENT (shell32
# shfldr_netplaces.c) must fail it.
#
#   WINE=/opt/wine-sg/bin/wine test/netplaces-gate.sh
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

T=$(mktemp -d /var/tmp/sg-netplaces.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
"$MINGW" -O2 -o "$T/netplaces-probe.exe" "$HERE/netplaces-probe.c" -lshell32 -lole32 -lshlwapi -luuid || { fail "probe did not build"; exit 1; }
cat > "$T/netbrowse" <<'EOS'
#!/bin/sh
case "$1" in
computers) printf 'SERVER1\nNAS2\n' > "$2" ;;
shares) [ "$3" = SERVER1 ] && printf 'Public\nMusic\n' > "$2" || : > "$2" ;;
shares2) [ -n "${NETPLACES_OLD:-}" ] && exit 2
    [ "$3" = SERVER1 ] && printf 'Public\tShared files for everyone\nMusic\t\n' > "$2" || : > "$2" ;;
esac
EOS
chmod 755 "$T/netbrowse"
export SG_NETBROWSE="$T/netbrowse"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
out=$("$WINE" "$T/netplaces-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
printf '%s\n' "$out" | grep -qF '    SERVER1  [\\SERVER1]' && printf '%s\n' "$out" | grep -qF '    NAS2  [\\NAS2]' \
    && pass "Network lists the computers, by name (parsing name \\\\name)" || fail "no computers"
printf '%s\n' "$out" | grep -qF 'parse \\SERVER1: 00000000' && pass "\\\\computer parses" || fail "\\\\computer does not parse"
printf '%s\n' "$out" | grep -qF '    Public  [\\SERVER1\Public]' && printf '%s\n' "$out" | grep -qF '    Music  [\\SERVER1\Music]' \
    && pass "a computer lists its shares, each at \\\\computer\\share" || fail "no shares"
printf '%s\n' "$out" | grep -qF '    Public  [\\SERVER1\Public]  {Shared files for everyone}' \
    && printf '%s\n' "$out" | grep -qF '    Music  [\\SERVER1\Music]  {}' \
    && pass "a share's comment is in the Comments column (none when it has none)" || fail "the shares' comments"
old=$(NETPLACES_OLD=1 "$WINE" "$T/netplaces-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$old" | grep -qF '    Public  [\\SERVER1\Public]' && printf '%s\n' "$old" | grep -qF '    Music  [\\SERVER1\Music]' \
    && pass "an older sg-netbrowse (no shares2) still lists the shares" \
    || fail "with an older sg-netbrowse: $(printf '%s\n' "$old" | grep '^    ' | tr '\n' ' ')"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
