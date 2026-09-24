#!/bin/sh
# Does Windows.ApplicationModel.Resources.ResourceLoader read an application's
# resources.pri? (patches/sg/0032.) Without it every MRT-localised program --
# winget among them -- prints resource keys instead of its text.
#
# The fixture is written by test/mkpri.py, so no Microsoft tool or file is
# needed. If WINGET_DIR names an unpacked winget whose resources.pri is there,
# its strings are checked too: the format written by Microsoft's makepri, not
# only by our own writer.
#
#   WINE=/opt/wine-sg/bin/wine test/resources-gate.sh
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

T=$(mktemp -d /var/tmp/sg-resources.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml="
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$(dirname "$WINE")/wineserver" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

"$MINGW" -O2 -o "$T/resloader-probe.exe" "$HERE/resloader-probe.c" -lruntimeobject || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
APP="$WINEPREFIX/drive_c/app"
mkdir -p "$APP"
cp "$T/resloader-probe.exe" "$APP/"
python3 "$HERE/mkpri.py" "$APP/resources.pri" || { fail "fixture not written"; exit 1; }

probe() { (cd "$APP" && LC_ALL="${LOCALE:-en_US.UTF-8}" timeout -s KILL 120 "$WINE" resloader-probe.exe "$@" 2>/dev/null | tr -d '\r'); }
expect() {   # expect OUTPUT KEY VALUE WHAT
    if printf '%s\n' "$1" | grep -qxF "$2=$3"; then pass "$4"
    else fail "$4: wanted '$2=$3', got: $(printf '%s' "$1" | tr '\n' ' ')"; fi
}

out=$(probe default Greeting Farewell Nested/Deep Missing)
expect "$out" Greeting Hello "default loader: an inline string, English"
expect "$out" Farewell Goodbye "a value stored as a data item"
expect "$out" Nested/Deep "Deep value" "a nested resource"
expect "$out" Missing "" "a missing resource is an empty string, not an error"

out=$(probe byname custom Title Utf8)
expect "$out" Title "Custom map" "a named map (IResourceLoaderFactory)"
expect "$out" Utf8 "Grüße" "a UTF-8 value"

out=$(probe indep custom Title /Resources/Greeting)
expect "$out" Title "Custom map" "GetForViewIndependentUse(name)"
expect "$out" /Resources/Greeting Hello "a path from the root"

out=$(probe uri ms-resource:///Resources/Farewell ms-resource:///custom/Title)
expect "$out" ms-resource:///Resources/Farewell Goodbye "GetStringForReference: ms-resource URI"
expect "$out" ms-resource:///custom/Title "Custom map" "GetStringForReference: another map"

# Windows.Foundation.Uri itself (patches/sg/0033): parsed, not echoed.
out=$(probe uriparts 'https://user:pw@www.example.com:8080/dir/file.txt?q=1#frag' ../other/x.html)
expect "$out" SchemeName https "Uri: scheme"
expect "$out" Host www.example.com "Uri: host"
expect "$out" Domain example.com "Uri: domain"
expect "$out" Port 8080 "Uri: port"
expect "$out" Path /dir/file.txt "Uri: path"
expect "$out" Extension .txt "Uri: extension"
expect "$out" Query '?q=1' "Uri: query"
expect "$out" Fragment '#frag' "Uri: fragment"
expect "$out" UserName user "Uri: user name"
expect "$out" Password pw "Uri: password"
expect "$out" EqualsSelf 1 "Uri: Equals"
expect "$out" Combined 'https://user:pw@www.example.com:8080/other/x.html' "Uri: CombineUri"
out=$(probe uriparts 'https://www.example.com')
expect "$out" AbsoluteUri 'https://www.example.com/' "Uri: AbsoluteUri is canonical"
expect "$out" Port 443 "Uri: the scheme's default port"
out=$(probe uriparts 'not a uri')
expect "$out" create 'ERROR 0x80070057' "Uri: an invalid URI is E_INVALIDARG"

out=$(LOCALE=de_DE.UTF-8 probe default Greeting)
expect "$out" Greeting Hallo "a German user gets the German candidate"

if [ -n "${WINGET_DIR:-}" ] && [ -f "$WINGET_DIR/resources.pri" ]; then
    cp "$WINGET_DIR/resources.pri" "$APP/resources.pri"
    out=$(probe indep winget ToolDescription AvailableCommands)
    expect "$out" ToolDescription "The winget command line utility enables installing applications and other packages from the command line." \
        "winget's own resources.pri (written by makepri)"
    expect "$out" AvailableCommands "The following commands are available:" "winget: another string"
else
    echo "info  WINGET_DIR not set: skipping the check against a makepri-written index"
fi

echo
if [ "$RC" -eq 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL"; fi
exit "$RC"
