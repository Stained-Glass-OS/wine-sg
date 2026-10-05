#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A network share's folder as Windows programs read it (patches/sg/0822,
# 0823), on a stand-in SMB share: a tmpfs where sg-netmountd mounts shares
# (/run/stained-glass-net/unc/<server>/<share>, with sudo, removed after)
# and test/smbshim.c, LD_PRELOADed, answering as the kernel's SMB client
# does -- statfs says SMB, "user.cifs.dosattrib" gives the server's
# attributes at once, and "user.DOSATTRIB" costs a 3 ms network round trip.
#
#   - reading a folder of 2000 files asks the server nothing per file (it
#     asked for user.DOSATTRIB 2000 times: 8 s on Wi-Fi), and the server's
#     hidden files are hidden (0822; mutant SG_MUTANT_SMB_DOSATTRIB_EA in
#     dlls/ntdll/unix/file.c)
#   - \\server\share\ has volume information (mutants SG_MUTANT_UNC_NOT_ROOT in
#     dlls/kernelbase/volume.c, SG_MUTANT_SMB_NO_VOLUME in
#     dlls/ntdll/unix/file.c), so cmd's DIR shows the share's volume, and a
#     folder below the share is not a root
#
#   WINE=... test/smbshare-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
NET=/run/stained-glass-net
SHARE=$NET/unc/sgsmbsrv/big
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
for t in "$MINGW" gcc; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
sudo -n true 2>/dev/null || { echo "SKIP: no sudo"; exit 77; }
mountpoint -q "$SHARE" 2>/dev/null && { echo "SKIP: $SHARE is in use"; exit 77; }
made_net=; [ -d "$NET" ] || made_net=1

T=$(mktemp -d /var/tmp/sg-smbshare.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    sudo -n umount "$SHARE" 2>/dev/null
    sudo -n rmdir "$SHARE" "$NET/unc/sgsmbsrv" 2>/dev/null
    [ -n "$made_net" ] && sudo -n rm -rf "$NET" 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
gcc -shared -fPIC -O2 -o "$T/smbshim.so" "$HERE/smbshim.c" -ldl || { echo "FAIL  the shim did not build"; exit 1; }
"$MINGW" -O2 -municode -o "$T/smbshare-probe.exe" "$HERE/smbshare-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
sudo -n mkdir -p "$SHARE" && sudo -n mount -t tmpfs -o size=64m,mode=0777 tmpfs "$SHARE" || { echo "SKIP: cannot mount"; exit 77; }
i=0; while [ $i -lt 1960 ]; do : > "$SHARE/doc$i.txt"; i=$((i + 1)); done
i=0; while [ $i -lt 20 ]; do : > "$SHARE/hidden$i.txt"; mkdir "$SHARE/Folder $i"; i=$((i + 1)); done
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
cp "$T/smbshare-probe.exe" "$WINEPREFIX/drive_c/"

run() {   # run CMD...: a Wine program on the stand-in share
    (cd "$WINEPREFIX/drive_c" && LD_PRELOAD="$T/smbshim.so" SG_SMBSHIM_ROOT="$SHARE" SG_SMBSHIM_LOG="$T/log" \
        timeout 120 "$WINE" "$@" 2>/dev/null </dev/null | tr -d '\r')
}
: > "$T/log"
out=$(run 'C:\smbshare-probe.exe' '\\sgsmbsrv\big' 'Folder 1')
printf '%s\n' "$out" | sed 's/^/      /'
asked=$(tr -cd D < "$T/log" | wc -c)
smb=$(tr -cd S < "$T/log" | wc -c)
[ "$smb" -gt 0 ] || { echo "FAIL  the shim was not used (no statfs of the share)"; echo "RESULT: FAIL"; exit 1; }
ms=$(printf '%s\n' "$out" | sed -n 's/^list=\([0-9]*\) .*/\1/p')
entries=$(printf '%s\n' "$out" | sed -n 's/.*entries=\([0-9]*\).*/\1/p')
hidden=$(printf '%s\n' "$out" | sed -n 's/.*hidden=\([0-9]*\).*/\1/p')
[ "${entries:-0}" -ge 2000 ] && pass "the share's folder lists its $entries entries" || fail "entries: ${entries:-none}"
[ "$asked" -lt 20 ] && pass "without asking the server for each file's DOS attributes ($asked asked)" ||
    fail "the server was asked for user.DOSATTRIB $asked times"
[ -n "$ms" ] && [ "$ms" -lt 1500 ] && pass "read in $ms ms (< 1500; 3 ms a file before)" || fail "reading took ${ms:-?} ms"
[ "${hidden:-0}" = 20 ] && pass "the server's hidden files are hidden ($hidden)" || fail "hidden: ${hidden:-none} of 20"
case "$out" in *'vol=OK'*) pass "\\\\server\\share\\ has volume information" ;; *) fail "volume of the share: $(printf '%s\n' "$out" | grep '^vol')" ;; esac
case "$out" in *'sub=ERR144'*) pass "a folder below the share is not a root (ERROR_DIR_NOT_ROOT)" ;; *) fail "folder below: $(printf '%s\n' "$out" | grep '^sub')" ;; esac
case "$out" in *'drive=OK'*) pass "a handle in the share has volume information" ;; *) fail "handle: $(printf '%s\n' "$out" | grep '^drive')" ;; esac
out=$(run cmd /c 'dir \\sgsmbsrv\big\Folder 1')
case "$out" in *'Volume in drive \\sgsmbsrv\big has no label'*) pass "cmd's DIR names the share's volume" ;;
    *) fail "dir: $(printf '%s\n' "$out" | head -3 | tr '\n' ' ')" ;; esac

echo
if [ "$RC" -eq 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL"; fi
exit "$RC"
