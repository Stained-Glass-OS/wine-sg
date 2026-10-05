#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# File Explorer on a network share (patches/sg/0824, 0825), on a stand-in SMB
# share: a tmpfs where sg-netmountd mounts shares (with sudo, removed after)
# and test/smbshim.c answering as the kernel's SMB client does -- opening a
# folder below the share costs a network round trip, opening a program two.
# David: a share's folder "draws the icons somewhat slowly at first and then
# it draws them quickly after that, always doing a redraw of the icons".
#
#   - a folder kept in Large icons is drawn once, in Large icons: the view
#     was drawn in Details first and again (mutant SG_MUTANT_VIEW_PAINTS_EARLY,
#     dlls/shell32/shlview.c)
#   - its folders' icons need no listing of each folder (a desktop.ini
#     looked for in each, a round trip apiece, row by row as the list drew;
#     mutant SG_MUTANT_REMOTE_DESKTOP_INI, dlls/shell32/shlfolder.c)
#   - programs' own icons are read beside the view on a slow share: the
#     window answers at once while they come (mutants
#     SG_MUTANT_SYNC_REMOTE_ICONS, dlls/shell32/shlview.c, and
#     SG_MUTANT_ICON_LOAD_LOCKED, dlls/shell32/iconcache.c: the icon cache's
#     lock was held while a file was read)
#   - a File Explorer that ended abruptly (a crash) leaves no dead window in
#     the shell's list of File Explorer windows -- opening its folder again
#     brought the dead window "forward" and quit (mutant
#     SG_MUTANT_STALE_SHELLWINDOW, programs/explorer/desktop.c)
#   - an open File Explorer does not ask a network drive's server what kind
#     of drive it is every two seconds (mutant SG_MUTANT_DRIVES_POLL_ALL,
#     programs/explorer/fileexplorer.c)
#
#   WINE=... test/smbview-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
NET=/run/stained-glass-net
SHARE=$NET/unc/sgsmbsrv/big
DRIVES=$NET/drives/$(id -u)
RC=0; XP=; DBG=-all
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in Xvfb "$MINGW" gcc; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
sudo -n true 2>/dev/null || { echo "SKIP: no sudo"; exit 77; }
mountpoint -q "$SHARE" 2>/dev/null && { echo "SKIP: $SHARE is in use"; exit 77; }
[ -e "$DRIVES" ] && { echo "SKIP: $DRIVES exists (a session's drives)"; exit 77; }
made_net=; [ -d "$NET" ] || made_net=1

T=$(mktemp -d /var/tmp/sg-smbview.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    sudo -n umount "$SHARE" 2>/dev/null
    sudo -n rmdir "$SHARE" "$NET/unc/sgsmbsrv" 2>/dev/null
    sudo -n rm -rf "$DRIVES"
    [ -n "$made_net" ] && sudo -n rm -rf "$NET" 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
gcc -shared -fPIC -O2 -o "$T/smbshim.so" "$HERE/smbshim.c" -ldl || { echo "FAIL  the shim did not build"; exit 1; }
"$MINGW" -O2 -o "$T/smbview-probe.exe" "$HERE/smbview-probe.c" -lgdi32 -lole32 -loleaut32 -lshell32 -luuid || { echo "FAIL  probe did not build"; exit 1; }
sudo -n mkdir -p "$SHARE" && sudo -n mount -t tmpfs -o size=128m,mode=0777 tmpfs "$SHARE" || { echo "SKIP: cannot mount"; exit 77; }
mkdir "$SHARE/docs" "$SHARE/apps"
i=0; while [ $i -lt 24 ]; do mkdir "$SHARE/docs/Folder $i"; : > "$SHARE/docs/Folder $i/inside.txt"; i=$((i + 1)); done
i=0; while [ $i -lt 400 ]; do : > "$SHARE/docs/note$i.txt"; i=$((i + 1)); done

unset DISPLAY XAUTHORITY
Xvfb -displayfd 3 -screen 0 1280x800x24 -nolisten tcp 3>"$T/display" >/dev/null 2>&1 & XP=$!
i=0; while [ ! -s "$T/display" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
DPY=":$(cat "$T/display")"
case "$DPY" in :|:0) echo "FAIL  no display of our own"; exit 1 ;; esac
export DISPLAY="$DPY"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
exe=$WINEPREFIX/drive_c/windows/system32/winemine.exe; [ -f "$exe" ] || exe=$WINEPREFIX/drive_c/windows/regedit.exe
i=0; while [ $i -lt 60 ]; do cp "$exe" "$SHARE/apps/program$i.exe"; i=$((i + 1)); done
cp "$T/smbview-probe.exe" "$WINEPREFIX/drive_c/"
for f in docs apps; do
    "$WINE" reg add 'HKCU\Software\Stained Glass\Explorer\FolderViews' /v "\\\\sgsmbsrv\\big\\$f" /t REG_DWORD /d 0x6001 /f >/dev/null 2>&1
done

run() {   # run RTT_US CMD...: a Wine program on the stand-in share
    # (to a file: the File Explorer the probe starts keeps a pipe open)
    rtt=$1; shift
    (cd "$WINEPREFIX/drive_c" && LD_PRELOAD="$T/smbshim.so" SG_SMBSHIM_ROOT="$SHARE" SG_SMBSHIM_LOG="$T/log" \
        SG_SMBSHIM_RTT_US=$rtt WINEDEBUG="$DBG" timeout 120 "$WINE" "$@" > "$T/out" 2>"$T/trace" </dev/null)
    tr -d '\r' < "$T/out"
}
count() { tr -cd "$1" < "$T/log" | wc -c; }
field() { printf '%s\n' "$2" | sed -n "s/.*$1=\([0-9]*\).*/\1/p"; }

# 1. a folder of folders and documents, kept in Large icons; the list view's
# drawings are counted by kind (Details is drawn by LISTVIEW_RefreshReport)
: > "$T/log"
DBG=trace+listview
out=$(run 3000 'C:\smbview-probe.exe' paint 424 1500 'explorer.exe \\sgsmbsrv\big\docs')
DBG=-all
echo "      docs: $out"
folders=$(count F)
details=$(grep -c 'LISTVIEW_RefreshReport' "$T/trace")
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w
[ -n "$(field first "$out")" ] && [ "$(field first "$out")" -gt 0 ] || fail "the view was never drawn"
[ "$(field changes "$out")" = 0 ] && pass "a share's folder in Large icons is drawn once (no second drawing)" ||
    fail "the view was drawn $(( $(field changes "$out") + 1 )) times"
[ "$details" = 0 ] && pass "and never in Details first" || fail "it was drawn in Details first ($details drawings)"
[ "$folders" -lt 4 ] && pass "its folders' icons are drawn without listing each folder ($folders listed)" ||
    fail "$folders folders were listed over the network to draw their icons"

# 2. programs, on a slow share: 20 ms a round trip
: > "$T/log"
out=$(run 20000 'C:\smbview-probe.exe' paint 60 2500 'explorer.exe \\sgsmbsrv\big\apps')
echo "      apps: $out"
read_exes=$(count E)
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w
slowest=$(field slowest "$out")
[ -n "$slowest" ] && [ "$slowest" -lt 400 ] && pass "File Explorer answers while the programs' icons are read ($slowest ms at most)" ||
    fail "File Explorer stood still ${slowest:-?} ms while it read the programs' icons"
[ "$read_exes" -ge 10 ] && pass "and their icons are read ($read_exes programs opened)" || fail "only $read_exes programs were read"

# 3. a File Explorer window whose program ended abruptly
(cd "$WINEPREFIX/drive_c" && timeout 60 "$WINE" 'C:\smbview-probe.exe' stale > "$T/out" 2>/dev/null </dev/null)
out=$(tr -d '\r' < "$T/out")
echo "      stale: $out"
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w
case "$out" in *'alive=00000000 dead=00000001'*) pass "a dead File Explorer window is not offered for its folder (which then opens again)" ;;
    *) fail "the shell's window list: $out (a dead window is found: File Explorer brings it 'forward' and quits)" ;; esac

# 4. a network drive, while File Explorer stands open on the share
sudo -n mkdir -p "$DRIVES" && sudo -n chown "$(id -u)" "$DRIVES" && ln -s "$SHARE" "$DRIVES/s:"
out=$(run 3000 'C:\smbview-probe.exe' paint 424 500 'explorer.exe \\sgsmbsrv\big\docs')
sleep 1; before=$(count S); sleep 8; after=$(count S)
"$WINESERVER" -k 2>/dev/null; "$WINESERVER" -w
[ $((after - before)) -le 1 ] && pass "an open File Explorer leaves the network drive's server alone ($((after - before)) asked in 8 s)" ||
    fail "File Explorer asked the network drive's server $((after - before)) times in 8 s"

echo
if [ "$RC" -eq 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL"; fi
exit "$RC"
