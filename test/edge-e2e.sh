#!/bin/sh
# Microsoft Edge (A4) on wine-sg -- patches/sg/0049-0054.
#
# Always: test/edgeapi-probe.c asks each function Edge needed and Wine lacked
# (WofSetFileDataLocation, IsWindowArranged, GetPointerPenInfo, the effective
# power mode, InternetGetCookieEx2, DeriveAppContainerSidFromAppContainerName,
# AddConditionalAce) for the answer it must give.
#
# With EDGE_MSI set to Microsoft's enterprise installer -- supplied by the
# user, downloaded by them, never shipped (licensing rule) -- also: install it
# silently into a fresh prefix, start it on a private X server with a local
# page, and check the page loaded and its script ran (the window title is set
# by the page's JavaScript) and it painted (a pixel of its colour).
#
# Edge runs here with --no-sandbox: its sandbox does not work on Wine yet
# (restricted tokens, integrity levels, job limits, and Wine's display state
# in the registry, which a lockdown token cannot read). The gate reports the
# sandboxed run as info so the day it starts working is visible.
#
#   EDGE_MSI=/path/to/MicrosoftEdgeEnterpriseX64.msi test/edge-e2e.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

for t in "$MINGW" xvfb-run; do command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-edge.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all
# shellcheck disable=SC2317  # invoked via trap
cleanup() { "$(dirname "$WINE")/wineserver" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM

"$MINGW" -O2 -o "$T/edgeapi-probe.exe" "$HERE/edgeapi-probe.c" -lwininet -ladvapi32 || { fail "the probe did not build"; exit 1; }

out=$(xvfb-run -a sh -c "timeout 300 '$WINE' '$T/edgeapi-probe.exe' 2>/dev/null" | tr -d '\r')
expect() { if printf '%s\n' "$out" | grep -qxF "$1"; then pass "$2"; else fail "$2: wanted '$1'"; fi; }
expect "Exports=1" "every function Edge delay-loads is exported"
expect "IsWindowArranged=0" "IsWindowArranged: nothing is snapped"
expect "GetPointerPenInfo=0,87" "GetPointerPenInfo: no pen pointer (ERROR_INVALID_PARAMETER)"
expect "PowerRegister=0x00000000" "effective power mode: registration succeeds"
expect "PowerMode=2" "and the callback is told Balanced"
expect "AppContainerSid=S-1-15-2-3624051433-2125758914-1423191267-1740899205-1073925389-3782572162-737981194" \
    "AppContainer SID derivation reproduces the published SID of the legacy Edge package"
expect "GetCookieEx2=0,1" "InternetGetCookieEx2 returns the cookie set for the URL"
expect "Cookie=sgcookie=stained domain=sgtest.example path=/ expires=1" "with its domain, path and expiry"
expect "AddConditionalAce=0,50" "AddConditionalAce refuses (ERROR_NOT_SUPPORTED) rather than drop the condition"
expect "WofSetFileDataLocation=0x80070032" "WofSetFileDataLocation: the file system declines, nothing crashes"

if [ -z "${EDGE_MSI:-}" ]; then
    echo "info  EDGE_MSI not set: Edge itself not installed or run"
else
    C="$WINEPREFIX/drive_c"
    cp "$EDGE_MSI" "$T/edge.msi"
    printf '<html><body style="background:#129A3C"><h1 style="color:white">Stained Glass</h1><script>document.title="SGTEST-"+(40+2)</script></body></html>' > "$T/page.html"
    cat > "$T/run.sh" <<EOF
#!/bin/sh
"$WINE" wineboot -i >/dev/null 2>&1; "$(dirname "$WINE")/wineserver" -w
cp "$T/page.html" "$C/sgtest.html"
timeout 1200 "$WINE" msiexec /i "$T/edge.msi" /qn >/dev/null 2>&1
echo "msiexec=\$?" > "$T/msiexec.rc"
EDGE='C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe'
edge_title() { # edge_title FLAGS...: start Edge, wait for the page's title, screenshot
    timeout 150 "$WINE" "\$EDGE" --no-first-run --do-not-de-elevate --user-data-dir='C:\\edgeprofile' "\$@" 'file:///C:/sgtest.html' >/dev/null 2>&1 &
    i=0; while [ \$i -lt 60 ]; do
        xdotool search --name 'SGTEST-42' >/dev/null 2>&1 && break
        sleep 2; i=\$((i + 1))
    done
    sleep 4
    xdotool search --name 'SGTEST-42' >/dev/null 2>&1 && echo yes || echo no
}
edge_title --no-sandbox --disable-gpu > "$T/unsandboxed"
import -window root "$T/unsandboxed.png" 2>/dev/null
"$(dirname "$WINE")/wineserver" -k; sleep 2
edge_title --disable-gpu > "$T/sandboxed"
"$(dirname "$WINE")/wineserver" -k
EOF
    chmod +x "$T/run.sh"
    timeout 2400 xvfb-run -a -s "-screen 0 1024x768x24" "$T/run.sh"
    if grep -qx 'msiexec=0' "$T/msiexec.rc" 2>/dev/null && [ -f "$C/Program Files (x86)/Microsoft/Edge/Application/msedge.exe" ]; then
        pass "Edge installs silently from its MSI"
    else fail "Edge did not install: $(cat "$T/msiexec.rc" 2>/dev/null)"; fi
    if [ "$(cat "$T/unsandboxed" 2>/dev/null)" = yes ]; then pass "Edge opens a page and runs its script (window titled by the page)"
    else fail "Edge never showed the page"; fi
    px=$(convert "$T/unsandboxed.png" -crop 1x1+500+500 -depth 8 txt:- 2>/dev/null | sed -n 's/.*#\([0-9A-F]\{6\}\).*/\1/p')
    if [ "$px" = 129A3C ]; then pass "and paints it (#129A3C)"; else fail "the page's colour is not on screen (#$px)"; fi
    echo "info  with its sandbox on, Edge shows the page: $(cat "$T/sandboxed" 2>/dev/null) (not working on Wine yet)"
fi

echo
if [ "$RC" -eq 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL"; fi
exit "$RC"
