#!/bin/sh
# WebView2 apps on wine-sg -- patches/sg/0190-0191.
#
# The Microsoft Edge WebView2 Runtime is the user's (downloaded from
# Microsoft at their request, never shipped -- licensing rule); apps bring
# WebView2Loader.dll themselves. This gate installs the Evergreen standalone
# runtime silently into a fresh prefix, builds test/wv2test.c (our own app)
# against the public WebView2 SDK header (the Microsoft.Web.WebView2 NuGet
# package, BSD-3 -- fetched at test time, never committed), and checks:
#
#   - the installer registers the runtime where the loader looks
#     (EdgeUpdate\Clients\{F3017226-...} pv) and puts msedgewebview2.exe there
#   - the app finds it, makes an environment and a controller, navigates to
#     a local page whose script sets the title, and ExecuteScript answers
#   - the page is DRAWN in the window (its green, #129A3C, on the screen):
#     without 0190-0191 the GPU process fails DCompositionCreateDevice /
#     CreateSwapChainForComposition, restarts, and nothing is ever shown
#
#   WV2_INSTALLER=.../MicrosoftEdgeWebView2RuntimeInstallerX64.exe
#   WV2_SDK=<unpacked Microsoft.Web.WebView2 package>   (build/native/include, x64)
#   NETWORK=1  fetches whichever of the two is not given into
#              ~/.cache/stained-glass/webview2/ (Microsoft's and nuget.org's
#              own links)
#
# Skips (77) without them, mingw, Xvfb or ImageMagick. Screenshots in
# $OUT (default: build/webview2-*.png).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
OUT="${OUT:-$HERE/../build}"
CACHE="${XDG_CACHE_HOME:-$HOME/.cache}/stained-glass/webview2"
SDK_VERSION=1.0.4191.47
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

for need in Xvfb import convert unzip "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: $WINE missing"; exit 77; }
mkdir -p "$OUT"

INSTALLER="${WV2_INSTALLER:-}"
SDK="${WV2_SDK:-}"
if [ -z "$INSTALLER" ] && [ "${NETWORK:-0}" = 1 ]; then
    mkdir -p "$CACHE"
    INSTALLER="$CACHE/MicrosoftEdgeWebView2RuntimeInstallerX64.exe"
    [ -s "$INSTALLER" ] || curl -fsSL --retry 3 -o "$INSTALLER" 'https://go.microsoft.com/fwlink/?linkid=2124701' \
        || { echo "SKIP: could not download the WebView2 runtime"; rm -f "$INSTALLER"; exit 77; }
fi
if [ -z "$SDK" ] && [ "${NETWORK:-0}" = 1 ]; then
    mkdir -p "$CACHE"
    SDK="$CACHE/sdk-$SDK_VERSION"
    if [ ! -f "$SDK/build/native/include/WebView2.h" ]; then
        curl -fsSL --retry 3 -o "$CACHE/webview2.nupkg" "https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/$SDK_VERSION" \
            && mkdir -p "$SDK" && unzip -q -o "$CACHE/webview2.nupkg" -d "$SDK" || { echo "SKIP: could not fetch the WebView2 SDK"; exit 77; }
    fi
fi
[ -n "$INSTALLER" ] && [ -s "$INSTALLER" ] || { echo "SKIP: WV2_INSTALLER not given (or NETWORK=1)"; exit 77; }
[ -n "$SDK" ] && [ -f "$SDK/build/native/include/WebView2.h" ] && [ -f "$SDK/build/native/x64/WebView2Loader.dll" ] \
    || { echo "SKIP: WV2_SDK not given (or NETWORK=1)"; exit 77; }

T=$(mktemp -d /var/tmp/sg-webview2.XXXXXX)
# shellcheck disable=SC2317
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM

# --- the app -----------------------------------------------------------------------------------
mkdir -p "$T/shim" "$T/app"
printf '#include <eventtoken.h>\n' > "$T/shim/EventToken.h"   # mingw's name is lower-case
"$MINGW" -O2 -w -I"$T/shim" -I"$SDK/build/native/include" -o "$T/app/wv2test.exe" "$HERE/wv2test.c" \
    -lole32 -luuid -luser32 -lgdi32 || { fail "wv2test.c does not build"; exit 1; }
cp "$SDK/build/native/x64/WebView2Loader.dll" "$T/app/"

Xvfb -displayfd 3 -screen 0 1024x768x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 40 ]; do sleep 0.25; i=$((i + 1)); done
DPY=$(cat "$T/display" 2>/dev/null); [ -n "$DPY" ] || { fail "Xvfb did not start"; exit 1; }
export DISPLAY=":$DPY" WINEPREFIX="$T/prefix" WINEARCH=win64 WINEDEBUG=-all WINEDLLOVERRIDES='winemenubuilder.exe=d'
"$WINE" wineboot --init >/dev/null 2>&1
"$WINESERVER" -w

# --- the runtime ------------------------------------------------------------------------------------
cp "$INSTALLER" "$T/wv2setup.exe"
timeout 600 "$WINE" "$T/wv2setup.exe" /silent /install >/dev/null 2>&1
rc=$?
[ $rc = 0 ] && pass "the WebView2 runtime installer succeeds (/silent /install)" || fail "the installer returned $rc"
pv=$("$WINE" reg query 'HKLM\SOFTWARE\WOW6432Node\Microsoft\EdgeUpdate\Clients\{F3017226-FE2A-4295-8BDF-00C3A9A7E4C5}' /v pv 2>/dev/null \
     | tr -d '\r' | sed -n 's/.*REG_SZ *//p')
[ -n "$pv" ] && pass "the runtime is registered where the loader looks (pv $pv)" || fail "no EdgeUpdate pv for the runtime"
ls "$WINEPREFIX/drive_c/Program Files (x86)/Microsoft/EdgeWebView/Application/$pv/msedgewebview2.exe" >/dev/null 2>&1 \
    && pass "msedgewebview2.exe $pv is installed" || fail "msedgewebview2.exe $pv missing"

# --- the app, and what is on the screen ----------------------------------------------------------------
mkdir -p "$WINEPREFIX/drive_c/wv2app"; cp "$T/app/"* "$WINEPREFIX/drive_c/wv2app/"
cd "$WINEPREFIX/drive_c/wv2app" || exit 1
WV2TEST_HOLD=40 timeout 180 "$WINE" wv2test.exe > "$T/out" 2>"$T/err" &
AP=$!
i=0; while ! grep -q RESULT "$T/out" 2>/dev/null && [ $i -lt 120 ]; do sleep 1; i=$((i + 1)); done
tr -d '\r' < "$T/out" | sed 's/^/    /'
grep -q 'RESULT=PASS' "$T/out" && pass "the app's WebView2 loads its page and runs its script" || fail "the app: $(grep -a -o 'RESULT=[A-Z]*' "$T/out")"
grep -q '^Title=SGWV2-42' "$T/out" && pass "the page's script set the title" || fail "title: $(grep -a '^Title' "$T/out")"
grep -q '^Script=.*SGWV2-42|42' "$T/out" && pass "ExecuteScript answers" || fail "script: $(grep -a '^Script' "$T/out")"
# the page's colour where the WebView is, within 40 s of the result
got=""
i=0; while [ $i -lt 40 ]; do
    import -window root "$OUT/webview2-page.png" 2>/dev/null
    px=$(convert "$OUT/webview2-page.png" -crop 1x1+400+300 -depth 8 txt:- 2>/dev/null | tail -1 | awk '{print $3}')
    [ "$px" = "#129A3C" ] && { got=$i; break; }
    sleep 1; i=$((i + 1))
done
[ -n "$got" ] && pass "the page is drawn in the window (its green at 400,300 after ${got}s)" \
    || fail "the page is not drawn (400,300 is $px) -- DirectComposition / CreateSwapChainForComposition"
grep -q 'ProcessFailed' "$T/out" && echo "NOTE  $(grep -a ProcessFailed "$T/out" | head -1)"
wait $AP 2>/dev/null
[ $RC = 0 ] && echo "webview2-gate: all passed" || echo "webview2-gate: FAILED"
exit $RC
