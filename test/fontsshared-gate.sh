#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# The fonts folder in a shared prefix (patches/sg/6533): the owner's wineboot makes the links in
# C:\windows\Fonts, a second user reads them (a fontconfig-style scan finds Wine's and a host font)
# and counts the same font families, and an update run by the owner (wineboot -u) leaves the other
# user's view as it was (test/fontsfolder-probe.c).
# This user owns the prefix; SG_OTHER (default sgconf, in SG_GROUP) is the standard user. Needs
# passwordless `sudo -u $SG_OTHER`.
#   WINE=... test/fontsshared-gate.sh
# Mutant: SG_MUTANT_NO_FONT_LINKS (programs/wineboot).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }
id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null || { echo "SKIP: no $SG_OTHER/sudo"; exit 77; }
command -v "$MINGW" >/dev/null && command -v fc-list >/dev/null || { echo "SKIP: no mingw or fc-list"; exit 77; }
HOSTPATH=$(fc-list --format '%{file}\n' | grep -E '^/usr/share/fonts/.*\.(ttf|otf)$' | sort | head -n 1)
[ -n "$HOSTPATH" ] || { echo "SKIP: no host TrueType/OpenType font"; exit 77; }
HOSTFONT=$(basename "$HOSTPATH")
W=$(mktemp -d /var/tmp/fontsshared.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
cleanup() {
    set +e
    WINEPREFIX=$PFX "$WINESERVER" -k 2>/dev/null
    sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" "$WINESERVER" -k 2>/dev/null
    sleep 1
    chmod -R u+w "$W" 2>/dev/null
    sudo -n rm -rf "$W"
}
trap cleanup EXIT
"$MINGW" -O1 -o "$W/probe.exe" "$HERE/fontsfolder-probe.c" -lgdi32 -luser32 -ldwrite || { fail "probe did not build"; exit 1; }
chmod 755 "$W/probe.exe"

mkdir "$PFX"
chgrp "$SG_GROUP" "$PFX"
chmod 2770 "$PFX"
touch "$PFX/.sg-system-prefix"
export WINEPREFIX=$PFX WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"
sg "$SG_GROUP" -c "umask 002; '$WINE' wineboot -i" >/dev/null 2>&1
chmod -R g+rwX "$PFX" 2>/dev/null

other() {
    sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" \
        HOME=/var/tmp "$WINE" "$W/probe.exe" "$@" 2>/dev/null | tr -d '\r'
}
mine() { "$WINE" "$W/probe.exe" "$@" 2>/dev/null | tr -d '\r'; }

LINKS=$(find "$PFX/drive_c/windows/Fonts" -maxdepth 1 -type l | wc -l)
printf '      %s links\n' "$LINKS"
scan=$(other scan "$HOSTFONT"); printf '%s\n' "$scan" | sed 's/^/      other: /'
grep -q '^wine_font=1' <<<"$scan" && pass "the other user's fontconfig-style scan finds Wine's tahoma.ttf" || fail "other user: no tahoma.ttf"
grep -q '^host_font=1' <<<"$scan" && pass "and the host's $HOSTFONT (the links are readable)" || fail "other user: no $HOSTFONT"
owner_count=$(mine count); other_before=$(other count)
printf '      owner: %s\n      other: %s\n' "$owner_count" "$other_before"
[ -n "$other_before" ] && [ "$owner_count" = "$other_before" ] && pass "both users count the same font families" || fail "family counts differ"

# the owner updates the prefix while the other user's server runs
sg "$SG_GROUP" -c "umask 002; '$WINE' wineboot -u" >/dev/null 2>&1
other_after=$(other count); scan2=$(other scan "$HOSTFONT")
[ "$other_after" = "$other_before" ] && pass "after the owner's wineboot -u the other user counts the same families" \
    || fail "other user after update: '$other_after' vs '$other_before'"
grep -q '^host_font=1' <<<"$scan2" && [ "$(find "$PFX/drive_c/windows/Fonts" -maxdepth 1 -type l | wc -l)" = "$LINKS" ] \
    && pass "and still finds the fonts; no link doubled or lost" || fail "after update: $(grep '^host_font' <<<"$scan2"), $(find "$PFX/drive_c/windows/Fonts" -maxdepth 1 -type l | wc -l) links"
echo "fontsshared-gate: $fails failure(s)"
[ "$fails" = 0 ]
