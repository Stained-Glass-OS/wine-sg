#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.Data.Json (patches/sg/7721): IJsonObjectWithDefaultValues (GetNamed*OrDefault give the named
# value or the default), and JsonObject's and JsonArray's statics (Parse, TryParse). PowerToys' runner (C++/WinRT, GetNamedBoolean(name, default)) stopped
# at its start: E_NOINTERFACE -> std::terminate (Store, PowerToys 0.102 machine setup, 2026-11-09).
#   WINE=/opt/wine-sg/bin/wine test/jsondefault-gate.sh
# Mutants: SG_MUTANT_JSON_NO_DEFAULTS (json_value.c), SG_MUTANT_JSON_NO_PARSE_STATICS (json_object.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-jsondefault.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
trap '"$(dirname "$WINE")/wineserver" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/probe.exe" "$HERE/jsondefault-probe.c" -lruntimeobject || { echo "FAIL  the probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
cp "$T/probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" 'C:\probe.exe' 2>/dev/null | tr -d '\r')
expect() {
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then echo "PASS  $3"; else echo "FAIL  $3: $(printf '%s' "$out" | tr '\n' ' ')"; RC=1; fi
}
expect Statics 0x00000000 "JsonObject's statics (IJsonObjectStatics)"
expect Parse 0x00000000 "JsonObject.Parse"
expect TryParseArrayAsObject "0x00000000 0 1" "JsonObject.TryParse of an array: not an object, no error"
expect ArrayParse 0x00000000 "JsonArray.Parse (IJsonArrayStatics)"
expect Defaults 0x00000000 "the object answers IJsonObjectWithDefaultValues"
expect StringPresent x "GetNamedStringOrDefault: the value there"
expect StringMissing def "the default for a name it does not have"
expect NumberPresent 3 "GetNamedNumberOrDefault: the value"
expect NumberMissing 7 "the default"
expect BoolPresent 1 "GetNamedBooleanOrDefault: the value"
expect BoolMissing 1 "the default"
expect ObjectPresent 1 "GetNamedObjectOrDefault: the object"
expect ObjectMissing "0x00000000 1" "a null default for a missing name"
expect ValueMissing "0x00000000 1" "GetNamedValueOrDefault: likewise"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
