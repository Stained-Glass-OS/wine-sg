#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Direct2D drawn on a GDI DC, on screen and on paper (patches/sg/0300, 0301).
#
# LibreOffice prints its text through a premultiplied ID2D1DCRenderTarget
# bound to the printer DC; printed pages came out with a black box where
# every line of text should be. Two causes, one gate:
#
#  - 0300 (d2d1): the DC render target BitBlt'ed its whole bitmap, transparent
#    parts included, over the DC -- a premultiplied target now starts
#    transparent and is composited with AlphaBlend (screen: the rest of a
#    white DIB stays white);
#  - 0301 (wineps): the PostScript driver dropped EMR_ALPHABLEND records --
#    an AlphaBlend on a printer DC now prints its opaque pixels and leaves
#    the transparent part of the bitmap showing what is under it.
#
# Printing goes to a private, unprivileged cupsd (Wine lists its printers
# from CUPS); the job is captured on the Wine side through
# HKCU\Software\Wine\Printing\Spooler (port CUPS:<queue> -> a Unix file) and
# rendered with Ghostscript at the printer's resolution.
#
#   WINE=/opt/wine-sg/bin/wine test/dcrt-print-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
export WINESERVER
RC=0; CP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

for t in xvfb-run gs python3 "$MINGW"; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x /usr/sbin/cupsd ] && [ -x /usr/sbin/lpadmin ] || { echo "SKIP: needs cups-daemon and cups-client"; exit 77; }
python3 -c 'import PIL' 2>/dev/null || { echo "SKIP: python3-pil missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-dcrt-print.XXXXXX); chmod 755 "$T"
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$CP" ] && kill "$CP" 2>/dev/null
    [ -n "${ARTIFACTS:-}" ] && mkdir -p "$ARTIFACTS" && cp "$T"/*.png "$T"/*.ps "$ARTIFACTS"/ 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
# A scratch HOME and no menu/desktop integration: a prefix links its Desktop,
# Documents, Downloads... to $HOME's, and installers write shortcuts there.
export HOME="$T/home" XDG_CONFIG_HOME="$T/home/.config" XDG_DATA_HOME="$T/home/.local/share" \
    XDG_DESKTOP_DIR="$T/home/Desktop"
mkdir -p "$HOME/Desktop"
export WINEDLLOVERRIDES="winemenubuilder.exe=d${WINEDLLOVERRIDES:+;$WINEDLLOVERRIDES}"

"$MINGW" -municode -O2 -o "$T/dcrt-print-probe.exe" "$HERE/dcrt-print-probe.c" -ld2d1 -lgdi32 -luser32 -luuid ||
    { echo "FAIL  probe did not build"; exit 1; }

mkdir -p "$T/cups/spool" "$T/cups/cache" "$T/cups/state" "$T/cups/logs" "$T/cups/tmp"
cat > "$T/cups/cupsd.conf" <<EOF
Listen $T/cups/cups.sock
LogLevel warn
DefaultAuthType None
WebInterface No
<Location />
  Order allow,deny
  Allow all
</Location>
<Location /admin>
  Order allow,deny
  Allow all
</Location>
EOF
cat > "$T/cups/cups-files.conf" <<EOF
ServerRoot $T/cups
CacheDir $T/cups/cache
StateDir $T/cups/state
RequestRoot $T/cups/spool
TempDir $T/cups/tmp
ServerBin /usr/lib/cups
DataDir /usr/share/cups
AccessLog $T/cups/logs/access_log
ErrorLog $T/cups/logs/error_log
PageLog $T/cups/logs/page_log
FileDevice Yes
Sandboxing relaxed
EOF
/usr/sbin/cupsd -f -c "$T/cups/cupsd.conf" -s "$T/cups/cups-files.conf" >/dev/null 2>&1 & CP=$!
export CUPS_SERVER="$T/cups/cups.sock"
i=0; while [ ! -S "$CUPS_SERVER" ] && [ $i -lt 20 ]; do sleep 0.3; i=$((i + 1)); done
/usr/sbin/lpadmin -p sgdcrt -E -v file:///dev/null && /usr/sbin/lpadmin -d sgdcrt || { echo "FAIL  no test printer"; exit 1; }

export WINEPREFIX="$T/prefix" WINEDEBUG=-all
"$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
"$WINE" reg add 'HKCU\Software\Wine\Printing\Spooler' /v CUPS:sgdcrt /d "$T/out.ps" /f >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/dcrt-print-probe.exe" "$WINEPREFIX/drive_c/"

cat > "$T/session.sh" <<EOF
#!/bin/sh
case "\$DISPLAY" in :0|:0.*) echo "refusing DISPLAY :0"; exit 1 ;; esac
cd "$WINEPREFIX/drive_c"
timeout -s KILL 120 "$WINE" dcrt-print-probe.exe screen > "$T/screen.out" 2>/dev/null
timeout -s KILL 180 "$WINE" dcrt-print-probe.exe print sgdcrt > "$T/print.out" 2>/dev/null
EOF
chmod +x "$T/session.sh"
timeout -s KILL 600 xvfb-run -a -s '-screen 0 1024x768x24' "$T/session.sh" >/dev/null 2>&1

s=$(tr -d '\r' < "$T/screen.out" 2>/dev/null)
case "$s" in *"inside=ff0000"*) pass "a DC render target draws what it is asked ($s)" ;; *) fail "screen: '$s'" ;; esac
case "$s" in *"outside=ffffff"*) pass "what it does not draw leaves the DC's white showing" ;; *) fail "outside the fill the DC was overwritten: '$s'" ;; esac

p=$(tr -d '\r' < "$T/print.out" 2>/dev/null | tr '\n' ' ')
case "$p" in *printed=1*) ;; *) fail "the probe did not print: '$p'" ;; esac
dpi=$(echo "$p" | sed -n 's/.*dpi=\([0-9]*\).*/\1/p')
if [ -s "$T/out.ps" ] && [ -n "$dpi" ]; then
    gs -q -dSAFER -dNOPAUSE -dBATCH -sDEVICE=png16m -r"$dpi" -sOutputFile="$T/page.png" "$T/out.ps" >/dev/null 2>&1
    # the grey band's top-left corner is device (100,200): everything else is
    # measured from it, whatever margin the driver translated the page by
    python3 - "$T/page.png" > "$T/samples" <<'EOF'
import sys
from PIL import Image
im = Image.open(sys.argv[1]).convert("RGB")
w, h = im.size
px = im.load()
grey = lambda c: all(100 <= v <= 160 for v in c) and max(c) - min(c) < 12
band = None
for y in range(h):
    for x in range(w):
        if grey(px[x, y]):
            band = (x, y); break
    if band: break
if not band:
    print("band=none"); sys.exit()
bx, by = band
def at(dx, dy):
    c = px[bx + dx - 100, by + dy - 200]
    return "black" if max(c) < 60 else "grey" if grey(c) else "white" if min(c) > 200 else "%02x%02x%02x" % c
print("band=%d,%d" % band)
print("ab_left=%s" % at(350, 350))
print("ab_right=%s" % at(450, 350))
print("rt_left=%s" % at(400, 750))
print("rt_right=%s" % at(600, 750))
EOF
    r=$(tr '\n' ' ' < "$T/samples")
    case "$r" in *"ab_left=black"*) pass "AlphaBlend on a printer prints its opaque half ($r)" ;; *) fail "AlphaBlend's opaque half is missing on paper: $r" ;; esac
    case "$r" in *"ab_right=grey"*) pass "and leaves what is under its transparent half" ;; *) fail "AlphaBlend's transparent half covered the page: $r" ;; esac
    case "$r" in *"rt_left=black"*) pass "a DC render target on a printer prints what it drew" ;; *) fail "the DC render target's drawing is missing on paper: $r" ;; esac
    case "$r" in *"rt_right=grey"*) pass "and not a box over the rest of its rectangle" ;; *) fail "the DC render target printed a box over its whole rectangle: $r" ;; esac
    cp "$T/page.png" "$T/dcrt-page.png" 2>/dev/null
else
    fail "no PostScript was captured ($p)"
fi

[ $RC = 0 ] && echo "dcrt-print-gate: all passed" || echo "dcrt-print-gate: FAILURES"
exit $RC
