#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Gate: the icons we ship in Wine have every frame a display scale asks
# for -- 16, 20, 24, 32, 40, 48, 64, 96, 128 and 256 px -- each drawn for
# its size (wine-sg 1241; David 2026-10-06: get every icon rendered at a
# better resolution). Checked in the sources, always:
#   - theme/icons.py's drawings (files, folders, This PC, drives, Notepad,
#     IDI_WINLOGO and IDI_APPLICATION), rendered by the series' own
#     tools/buildimage as build.sh does: every frame
#   - the administrative tools' launchers (mmc, eventvwr, resmon, cleanmgr):
#     the .ico bytes in their .rc files, every frame
# and in an installed tree when WINE names one: user32's IDI_APPLICATION
# (32512, ours: a program window, not a wine glass) and IDI_WINLOGO (32517),
# Notepad's and the launchers' icons.
#
#   [WINE=/opt/wine-sg/bin/wine] test/iconframes-gate.sh
# Mutants: SG_MUTANT_ICON_FRAMES=1 (icons.py draws four sizes) fails the
# drawings; the series without 1241 fails the launchers (16-64 px).
set -u
export PYTHONDONTWRITEBYTECODE=1
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd)
WINE=${WINE:-}
fails=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; fails=$((fails + 1)); }
for t in rsvg-convert icotool convert python3; do command -v $t >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
TREE=${SG_SERIES_TREE:-$("$HERE/tools/lint-tree.sh")} || { echo "SKIP: no series tree"; exit 77; }
W=$(mktemp -d /var/tmp/iconframes-gate.XXXXXX); trap 'rm -rf "$W"' EXIT
WANT="16 20 24 32 40 48 64 96 128 256"
frames() { icotool -l "$1" 2>/dev/null | sed -n 's/.*--width=\([0-9]*\).*/\1/p' | sort -n | uniq | tr '\n' ' ' | sed 's/ $//'; }

# theme/icons.py's drawings, rendered as build.sh renders them
mkdir -p "$W/t"
for rel in $(python3 -c "import sys; sys.path.insert(0, '$HERE/theme'); import icons; print(' '.join(list(icons.ICONS) + list(icons.STRIPS)))"); do
    mkdir -p "$W/t/$(dirname "$rel")"
done
python3 "$HERE/theme/icons.py" "$W/t" >/dev/null || fail "theme/icons.py failed"
bad=""
for svg in $(cd "$W/t" && find . -name '*.svg' -newer "$W" | sed 's|^\./||' | grep -v '^dlls/comctl32/idb_view'); do
    CONVERT=convert ICOTOOL=icotool RSVG=rsvg-convert perl "$TREE/tools/buildimage" "$W/t/$svg" "$W/t/${svg%.svg}.ico" >/dev/null 2>&1
    got=$(frames "$W/t/${svg%.svg}.ico")
    [ "$got" = "$WANT" ] || bad="$bad ${svg##*/}:[$got]"
done
[ -z "$bad" ] && pass "theme/icons.py's icons render at $WANT px" || fail "icons without every frame:$bad"

# the launchers' icons, in their .rc files
for p in mmc eventvwr resmon cleanmgr; do
    rc="$TREE/programs/$p/$p.rc"
    python3 - "$rc" "$W/$p.ico" <<'PY' || { fail "$p.rc: no icon"; continue; }
import re, sys
rc = open(sys.argv[1]).read()
body = rc[rc.index("1 ICON"):]
body = body[body.index("BEGIN"):body.index("END")]
open(sys.argv[2], "wb").write(bytes.fromhex("".join(re.findall(r"'([0-9a-fA-F]*)'", body))))
PY
    got=$(frames "$W/$p.ico")
    [ "$got" = "$WANT" ] && pass "$p.exe's icon: $got px" || fail "$p.exe's icon has only [$got] px"
done

# an installed tree
if [ -n "$WINE" ] && command -v wrestool >/dev/null; then
    lib="$(dirname "$WINE")/../lib/wine/x86_64-windows"
    [ -d "$lib" ] || lib="$(dirname "$WINE")"
    one() {  # one FILE NAME WHAT
        wrestool -x --type=14 --name="$2" "$lib/$1" > "$W/i.ico" 2>/dev/null
        got=$(frames "$W/i.ico")
        [ "$got" = "$WANT" ] && pass "installed $3: $got px" || fail "installed $3 has [$got] px"
    }
    one user32.dll 32512 "IDI_APPLICATION"
    one user32.dll 32517 "IDI_WINLOGO"
    for p in mmc eventvwr resmon cleanmgr; do one $p.exe 1 "$p.exe's icon"; done
    # ours, not a wine glass: the window's title band is our purple
    wrestool -x --type=14 --name=32512 "$lib/user32.dll" > "$W/app.ico" 2>/dev/null
    idx=$(identify "$W/app.ico" 2>/dev/null | awk '/ 32x32 /{print NR-1; exit}')
    c=$(convert "$W/app.ico[${idx:-0}]" -format "%[fx:int(255*p{16,6}.r)],%[fx:int(255*p{16,6}.g)],%[fx:int(255*p{16,6}.b)]" info: 2>/dev/null)
    echo "$c" | awk -F, '{ exit !($3 > 150 && $1 < 160 && $2 < 110) }' && pass "IDI_APPLICATION is ours: a program window under our purple band ($c)" \
        || fail "IDI_APPLICATION is not ours (top band $c)"
fi
echo "iconframes-gate: $fails failure(s)"
[ "$fails" = 0 ]
