#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.ApplicationModel.Resources.Core.ResourceManager (patches/sg/6128, 6130): Current, the main
# resource map, values and subtrees, a candidate's string, a missing resource, resource references,
# a map as a map view (HasKey, Lookup, a named resource's candidates, 6130), over the
# application's resources.pri (test/mkpri.py). NanaZip's file manager gets its strings this way;
# the class did not exist. test/resman-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/resman-gate.sh
# Mutant (dlls/windows.applicationmodel/resource_manager.c): SG_MUTANT_RESMAN_NOT_FOUND,
# SG_MUTANT_RESMAP_NO_KEYS (6130).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v python3 >/dev/null || { echo "SKIP: python3 not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-resman.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml="
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$(dirname "$WINE")/wineserver" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

"$MINGW" -O2 -o "$T/resman-probe.exe" "$HERE/resman-probe.c" -lruntimeobject || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
APP="$WINEPREFIX/drive_c/app"
mkdir -p "$APP"
cp "$T/resman-probe.exe" "$APP/"
python3 "$HERE/mkpri.py" "$APP/resources.pri" || { fail "fixture not written"; exit 1; }

probe() { (cd "$APP" && LC_ALL="${LOCALE:-en_US.UTF-8}" timeout -s KILL 120 "$WINE" resman-probe.exe "$@" 2>/dev/null | tr -d '\r'); }
expect() {   # expect OUTPUT KEY VALUE WHAT
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}

out=$(probe)
printf '%s\n' "$out" | sed 's/^/      /'
expect "$out" factory 00000000 "the ResourceManager class's statics"
expect "$out" current 00000000 "ResourceManager.Current"
expect "$out" manager_qi 00000000 "is an IResourceManager"
expect "$out" mainmap 00000000 "its MainResourceMap"
expect "$out" map_qi 00000000 "is an IResourceMap"
expect "$out" greeting Hello "a value by its path in the main map (English)"
expect "$out" missing "hr 80073aa7" "a resource that does not exist: ERROR_MRM_NAMED_RESOURCE_NOT_FOUND"
expect "$out" farewell Goodbye "a value in a subtree"
expect "$out" deep "Deep value" "a nested value in a subtree"
expect "$out" fromroot "Custom map" "a path from the root, in a subtree"
expect "$out" utf8 "Grüße" "another map's value, stored as UTF-8"
expect "$out" mapview_qi 00000000 "a map is a map view (IMapView<String, NamedResource>)"
expect "$out" lookup "has 1 n 1 Goodbye" "HasKey, then Lookup(key).Candidates().GetAt(0).ValueAsString(), as NanaZip reads its strings"
expect "$out" lookup_missing "has 0 hr 8000000b" "a key that is not there: HasKey false, Lookup E_BOUNDS"
expect "$out" isref 1 "an ms-resource: URI is a resource reference"
expect "$out" notref 0 "plain text is not"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
