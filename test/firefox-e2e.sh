#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Firefox starts, shows a local page and shuts down (patches/sg/0170).
#
# Installs the compatibility suite's pinned Firefox (test/compat/apps.list;
# cached in ~/.cache/sg-compat, hash-checked, never committed or shipped)
# into a fresh prefix joined to a shell desktop under xvfb, starts it with its
# GPU process (the default), waits for the page's title, closes the window as
# a person would and requires every Firefox process to be gone within 30 s.
# Before 0170 its vsync flood starved the GPU process (IPC reply timeouts),
# the browser grew to tens of GB and never exited -- so the whole run is in a
# memory-capped scope when systemd-run is available.
#
#   WINE=/opt/wine-sg/bin/wine test/firefox-e2e.sh      (needs the network once)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
CACHE="${CACHE:-${SG_REAL_HOME:-$HOME}/.cache/sg-compat}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in "$MINGW" xvfb-run import curl sha256sum; do command -v "$t" >/dev/null || { echo "SKIP: $t not installed"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

line=$(grep '^Firefox' "$HERE/compat/apps.list" | head -1)
file=$(printf '%s' "$line" | cut -d'|' -f2); sha=$(printf '%s' "$line" | cut -d'|' -f3); url=$(printf '%s' "$line" | cut -d'|' -f4)
mkdir -p "$CACHE"
[ -s "$CACHE/$file" ] || curl -sfL --max-time 1800 -o "$CACHE/$file.part" "$url" && [ -s "$CACHE/$file.part" ] && mv "$CACHE/$file.part" "$CACHE/$file"
[ "$(sha256sum "$CACHE/$file" 2>/dev/null | cut -d' ' -f1)" = "$sha" ] || { echo "SKIP: no Firefox installer ($file)"; exit 77; }

T=$(mktemp -d /var/tmp/sg-firefox.XXXXXX)
P="$T/prefix"
export WINEPREFIX="$P" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
"$MINGW" -municode -O2 -o "$T/compat-probe.exe" "$HERE/compat/compat-probe.c" || { fail "probe did not build"; exit 1; }
mkdir -p "$P"
timeout -s KILL 600 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Explorer' /v Desktop /d shell /f >/dev/null 2>&1
"$WINE" reg add 'HKCU\Software\Wine\Explorer\Desktops' /v shell /d 1280x800 /f >/dev/null 2>&1
"$WINE" reg add 'HKLM\Software\Policies\Mozilla\Firefox' /v DisableAppUpdate /t REG_DWORD /d 1 /f >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/compat-probe.exe" "$P/drive_c/"
mkdir -p "$P/drive_c/web" "$P/drive_c/prof"
printf '<html><head><title>SG local page</title></head><body><h1>Stained Glass</h1><p>A local page.</p></body></html>\n' > "$P/drive_c/web/index.html"
inst=$("$WINE" winepath -w "$CACHE/$file" 2>/dev/null | tr -d '\r')

# the Firefox processes of this prefix (other Firefoxes may be running)
cat > "$T/procs.sh" <<'PEOF'
#!/bin/sh
# prints the kind of each Firefox process (gpu, tab, ...; the browser: "browser")
for d in /proc/[0-9]*; do
    { tr '\0' '\n' < "$d/environ" | grep -qxF "WINEPREFIX=$1"; } 2>/dev/null || continue
    c=$(tr '\0' ' ' < "$d/cmdline" 2>/dev/null)
    case "$c" in
        *'Mozilla Firefox'*-contentproc*) printf '%s\n' "$c" | awk '{ print $NF }' ;;
        *'Mozilla Firefox'*) echo browser ;;
    esac
done
PEOF
chmod +x "$T/procs.sh"

cat > "$T/session.sh" <<SEOF
#!/bin/sh
R="$T/r"
cd "$P/drive_c"
WINEDEBUG=trace+explorer "$WINE" explorer /desktop=shell,1280x800 > "$T/explorer.out" 2>&1 &
i=0; while ! grep -q 'desktop message loop starting' "$T/explorer.out" 2>/dev/null && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
timeout -s KILL 600 "$WINE" "$inst" /S > "$T/install.log" 2>&1; echo "install_rc=\$?" >> "\$R"
"$WINE" 'C:\\Program Files\\Mozilla Firefox\\firefox.exe' -no-remote -profile 'C:\\prof' 'file:///C:/web/index.html' > "$T/firefox.log" 2>&1 &
hw=""; i=0
while [ \$i -lt 120 ]; do
    hw=\$("$WINE" 'C:\\compat-probe.exe' list 2>/dev/null | tr -d '\r' | grep 'vis=1' | grep 'title=SG local page' | head -1 | cut -d' ' -f1)
    [ -n "\$hw" ] && break; sleep 1; i=\$((i + 1))
done
echo "window=\$hw after=\$i" >> "\$R"
sleep 8
import -window root "$T/firefox.png"
"$T/procs.sh" "$P" | sort | uniq -c | tr '\n' ' ' | sed 's/^/procs=/' >> "\$R"; echo >> "\$R"
if [ -n "\$hw" ]; then
    "$WINE" 'C:\\compat-probe.exe' close \$hw > "$T/close.out" 2>/dev/null
    tr -d '\r' < "$T/close.out" >> "\$R"
    i=0; while [ -n "\$("$T/procs.sh" "$P")" ] && [ \$i -lt 60 ]; do sleep 0.5; i=\$((i + 1)); done
    echo "gone_after_halfsec=\$i left=\$("$T/procs.sh" "$P" | tr '\n' ' ')" >> "\$R"
fi
SEOF
chmod +x "$T/session.sh"; : > "$T/r"
run() { if command -v systemd-run >/dev/null && systemd-run --user --scope -q true 2>/dev/null; then
            systemd-run --user --scope -q -p MemoryMax=${FIREFOX_MEMMAX:-6G} -p MemorySwapMax=0 "$@"
        else "$@"; fi; }
run timeout -s KILL 1200 xvfb-run -a -s '-screen 0 1280x800x24' "$T/session.sh" > "$T/session.log" 2>&1
sed 's/^/      /' "$T/r"
[ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T/firefox.png" "$ARTIFACTS/firefox-e2e.png" 2>/dev/null

grep -q '^install_rc=0' "$T/r" && pass "Firefox installs silently" || fail "install: $(grep install_rc "$T/r")"
grep -q '^window=0x' "$T/r" && pass "the local page's title is on its window" || fail "no window with the page's title"
grep -q '^procs=.* gpu' "$T/r" && pass "with its GPU process" || fail "no GPU process ($(grep '^procs=' "$T/r"))"
grep -q '^closed=1' "$T/r" && pass "the window closes" || fail "the window did not close"
if grep -q '^gone_after_halfsec=[0-9]* left= *$' "$T/r"; then
    pass "every Firefox process exits ($(sed -n 's/^gone_after_halfsec=\([0-9]*\).*/\1/p' "$T/r") half-seconds)"
else fail "Firefox processes left after 30 s: $(sed -n 's/^gone_after_halfsec=.*left=//p' "$T/r")"; fi
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
