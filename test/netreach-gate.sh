#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Network places that cannot be reached, in Windows' words, and File Explorer's logon to a
# share with saved and remembered credentials (patches/sg/9658), and
# SHHelpShortcuts_RunDLL Connect (patches/sg/9659). test/netreach-probe.c against
# test/netreach-netmountd.py, a stand-in for sg-netmountd on /run/stained-glass-net/netmount.sock
# -- in a private network and mount namespace of the gate's own, with a tmpfs on /run (the
# machine's /run/stained-glass-net is never touched), as root for the stand-in, the probe as you:
#   - a computer that is not there is ERROR_BAD_NETPATH (53), a share it does not have
#     ERROR_BAD_NET_NAME (67), a share that refuses a guest ERROR_ACCESS_DENIED (5): every one
#     was ERROR_BAD_DEV_TYPE (66). Mutant SG_MUTANT_NET_ONE_STATUS (ntdll/unix/file.c, ntlanman).
#   - File Explorer's parse: \\nosuchpc fails 0x80070035 (it opened an empty folder; mutant
#     SG_MUTANT_NET_ANY_COMPUTER), \\srv\nosuch 0x80070043 with no password prompt (it asked;
#     mutant SG_MUTANT_NET_PROMPT_ALWAYS) (shell32/shfldr_netplaces.c);
#   - a refusing share opens with the name and password Credential Manager keeps for its
#     computer, no prompt (mpr; mutant SG_MUTANT_NO_SAVED_CRED_USE in dlls/mpr/wnet.c);
#   - else the prompt offers "Remember my credentials" and saves them for the computer
#     (mutant SG_MUTANT_NET_NO_REMEMBER);
#   - rundll32 shell32.dll,SHHelpShortcuts_RunDLL Connect opens Map Network Drive (a stub;
#     mutant SG_MUTANT_HELPSHORTCUTS_STUB in shell32_main.c).
#   WINE=... test/netreach-gate.sh       (needs sudo -n, unshare, Xvfb, python3)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER=$(sg_wineserver)
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in Xvfb "$MINGW" python3 unshare setpriv; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
sudo -n true 2>/dev/null || { echo "SKIP: no sudo"; exit 77; }
W=$(mktemp -d /var/tmp/sg-netreach.XXXXXX) || exit 1
chmod 755 "$W"
cleanup() { sudo -n rm -rf "$W"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -municode -o "$W/probe.exe" "$HERE/netreach-probe.c" -lole32 -lshell32 -ladvapi32 -lmpr -luuid \
    || { echo "FAIL  probe did not build"; exit 1; }
cp "$HERE/netreach-netmountd.py" "$W/standin.py"
mkdir -p "$W/home"
cat > "$W/inner.sh" <<'INNER'
# runs as root in the private network and mount namespace
set -u
W=$1; WINE=$2; WINESERVER=$3; U=$4; G=$5; N=$6; H=$7
# a /tmp of its own: the display's lock and socket, and the prefix's wineserver socket
mount -t tmpfs -o mode=1777,size=64m tmpfs /tmp || { echo "FAIL  /tmp tmpfs"; exit 1; }
mkdir -m 1777 /tmp/.X11-unix
mount -t tmpfs -o mode=755,size=8m tmpfs /run || { echo "FAIL  /run tmpfs"; exit 1; }
ip link set lo up
mkdir -p /run/stained-glass-net/unc /run/stained-glass-net/users
python3 "$W/standin.py" /run/stained-glass-net/netmount.sock /run/stained-glass-net Right1pass "$W/requests" "$U" & SP=$!
i=0; while [ ! -S /run/stained-glass-net/netmount.sock ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
me() { setpriv --reuid="$U" --regid="$G" --init-groups -- env USER="$N" LOGNAME="$N" HOME="$H" "$@"; }
me Xvfb :479 -screen 0 1024x700x24 -nolisten tcp >"$W/xvfb.log" 2>&1 & XP=$!
sleep 1
export DISPLAY=:479 WINEPREFIX="$W/prefix" WINEDEBUG=${GATE_WINEDEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
me timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; me "$WINESERVER" -w
cp "$W/probe.exe" "$WINEPREFIX/drive_c/"; chown "$U:$G" "$WINEPREFIX/drive_c/probe.exe"
(cd "$WINEPREFIX/drive_c" && me timeout -s KILL 240 "$WINE" 'C:\probe.exe' Right1pass 2>"$W/trace" </dev/null) | tr -d '\r' > "$W/out"
me "$WINESERVER" -k 2>/dev/null
kill $XP $SP 2>/dev/null; wait $XP 2>/dev/null
for m in $(awk '$2 ~ "^/run/stained-glass-net/users/" { print $2 }' /proc/mounts); do umount -l "$m"; done
cat "$W/out"; [ -n "${GATE_TRACE:-}" ] && cat "$W/trace" "$W/xvfb.log" > "$GATE_TRACE"
sed 's/^/      server: /' "$W/requests" 2>/dev/null
grep -q 'Right1pass' "$W/requests" && echo "FAIL  a password reached the log"
grep -q '^LOGON srv locked srv.saveduser -> OK' "$W/requests" && echo "PASS  the saved name logged on to the share" ||
    echo "FAIL  no LOGON with the saved name"
INNER
out=$(sudo -n -E unshare -n -m --propagation private -- sh "$W/inner.sh" "$W" "$WINE" "$WINESERVER" "$(id -u)" "$(id -g)" "$(id -un)" "$HOME")
printf '%s\n' "$out"
if printf '%s\n' "$out" | grep -q '^FAIL' || ! printf '%s\n' "$out" | grep -qx 'RESULT: PASS'; then echo "GATE: FAIL"; exit 1; fi
echo "GATE: PASS"
