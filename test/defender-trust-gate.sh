#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# SG Defender on execute (9456, 9461): Wine's own installed files are trusted
# system files, as Windows trusts its catalog-signed ones -- a program's
# start never waits for a scan of them. Trusted: under the running Wine's
# lib/wine, root's, writable by root alone, one link, in folders that are
# root's and writable by root alone all the way up. The prefix's copies of
# Wine's builtins are not asked about either (Wine maps its own instead).
#   1. the install in root's folders (here: bound on a root-owned tmpfs):
#      starting notepad asks about none of Wine's files, nor the prefix's
#      copies; a program of the person's own is still asked about
#   2. the very same files reached through a folder others can write
#      (/var/tmp): asked about -- a copy anyone could have swapped is not
#      trusted
# A stand-in service (defender-exec-service.py) logs each request.
# Needs root (sudo -n, or root: CI) for the mount namespace, and WINE in a
# root-owned install (an installed wine-sg, /opt/wine-sg); skips (77) else.
#   WINE=/opt/wine-sg/bin/wine test/defender-trust-gate.sh
#   SG_TRUST_SERVICE=.../sg-defender SG_TRUST_CLAMD=SOCKET: the real service and clamd
#   instead of the stand-in (timings only)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
case "${1:-}" in
--run) ;;
--inner)
    # root, in its own mount and network namespace
    SRC=$2 T=$3 U=$4 G=$5
    mount -t tmpfs -o mode=1777,size=1m tmpfs /tmp/.X11-unix || exit 97
    ip link set lo up 2>/dev/null
    mount -t tmpfs -o mode=755 tmpfs /mnt || exit 97
    mkdir -m 755 /mnt/sgtrust && mount --bind "$SRC" /mnt/sgtrust || exit 97
    mkdir -p "$T/untrusted" && mount --bind "$SRC" "$T/untrusted" || exit 97
    exec setpriv --reuid="$U" --regid="$G" --init-groups -- sh "$0" --run "$T"
    ;;
*)
    SRC=$(CDPATH='' cd -- "$(dirname -- "$WINE")/.." 2>/dev/null && pwd)
    for need in Xvfb xdpyinfo xdotool python3 x86_64-w64-mingw32-gcc; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
    [ -n "$SRC" ] && [ "$(stat -c %u "$SRC/lib/wine/x86_64-windows/ntdll.dll" 2>/dev/null)" = 0 ] \
        || { echo "SKIP: $WINE is not a root-owned install"; exit 77; }
    if [ "$(id -u)" = 0 ]; then SUDO=""; else sudo -n true 2>/dev/null || { echo "SKIP: needs root (sudo -n)"; exit 77; }; SUDO="sudo -n -E"; fi
    T=$(mktemp -d /var/tmp/sg-defender-trust.XXXXXX)
    U=$(id -u); G=$(id -g)
    [ "$U" = 0 ] && { U=$(id -u nobody); G=$(id -g nobody); chown "$U:$G" "$T"; }
    $SUDO unshare -m -n --propagation private -- sh "$0" --inner "$SRC" "$T" "$U" "$G"; rc=$?
    rm -rf "$T" 2>/dev/null
    [ $rc = 97 ] && { echo "SKIP: no mount namespace here"; exit 77; }
    exit $rc
    ;;
esac

# --run: the person, inside the namespace
T=$2
RC=0; XP=""; SP=""
pass() { echo "PASS  $*"; }
fail() { echo "FAIL  $*"; RC=1; }
DPY=${DEFENDER_TRUST_DPY:-535}
cleanup() {
    [ -n "$SP" ] && kill "$SP" 2>/dev/null
    for w in /mnt/sgtrust "$T/untrusted"; do WINEPREFIX="$T/prefix" "$w/bin/wineserver" -k 2>/dev/null; done
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
}
trap cleanup EXIT INT TERM
export HOME="$T/home" XDG_RUNTIME_DIR="$T/run"; mkdir -p "$HOME" "$XDG_RUNTIME_DIR"; chmod 700 "$XDG_RUNTIME_DIR"
Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
export DISPLAY=":$DPY" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset WAYLAND_DISPLAY
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
printf 'int main(void) { return 42; }\n' > "$T/c.c"
TMPDIR="$T" x86_64-w64-mingw32-gcc -O2 -o "$T/mine.exe" "$T/c.c" || { echo "FAIL  no test program"; exit 1; }

service() {
    [ -n "$SP" ] && kill "$SP" 2>/dev/null; rm -f "$T/sock"
    if [ -n "${SG_TRUST_SERVICE:-}" ]; then
        mkdir -p "$T/svc"
        SG_DEFENDER_CLAMD="$SG_TRUST_CLAMD" SG_DEFENDER_RUNTIME="$T/svc" SG_DEFENDER_STATE="$T/svc/state" \
        SG_DEFENDER_NOTICES="$T/svc/notices" SG_DEFENDER_HOMES="$HOME" SG_DEFENDER_CONF="$T/svc/conf" \
        SG_DEFENDER_EXCLUSIONS="$T/svc/excl" SG_DEFENDER_PREFIX="$T/svc/prefix" SG_DEFENDER_SCHEDULE_HOURS=1000 \
            python3 "$SG_TRUST_SERVICE" 2>"$T/svc.log" & SP=$!
        i=0; while [ ! -S "$T/svc/socket" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
        ln -sf "$T/svc/socket" "$T/sock"
    else
        python3 "$HERE/defender-exec-service.py" "$T/sock" "$T/log" & SP=$!
        i=0; while [ ! -S "$T/sock" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
    fi
}
notepad() {   # WINEDIR: start notepad on a cold server, the seconds until its window
    "$1/bin/wineserver" -k 2>/dev/null; "$1/bin/wineserver" -w 2>/dev/null
    t0=$(date +%s.%N)
    "$1/bin/wine" notepad >/dev/null 2>&1 &
    i=0; while [ $i -lt 600 ] && ! xdotool search --name 'Notepad' >/dev/null 2>&1; do sleep 0.1; i=$((i + 1)); done
    t1=$(date +%s.%N)
    echo "$t0 $t1" | awk '{ printf "%.1f", $2 - $1 }'
    "$1/bin/wineserver" -k 2>/dev/null; "$1/bin/wineserver" -w 2>/dev/null
}

W=/mnt/sgtrust
SG_DEFENDER_SOCKET=/nonexistent "$W/bin/wine" wineboot -i >/dev/null 2>&1; "$W/bin/wineserver" -w
service
export SG_DEFENDER_SOCKET="$T/sock"
: > "$T/log"
took=$(notepad "$W")
echo "      cold notepad start, Wine in root's folders: $took s"
if [ -z "${SG_TRUST_SERVICE:-}" ]; then
    grep -q "fd=$W/" "$T/log" && fail "Wine's own installed files were asked about: $(grep "fd=$W/" "$T/log" | head -3)" \
        || pass "Wine's own installed files (root's, in root's folders) are not asked about"
    grep -qi "fd=$WINEPREFIX/drive_c/windows/sys" "$T/log" && fail "the prefix's copies were asked about: $(grep -i "drive_c/windows/sys" "$T/log" | head -2)" \
        || pass "...nor the prefix's copies of Wine's builtins"
    n=$(grep -c . "$T/log"); [ "$n" = 0 ] && pass "a cold notepad start asks SG Defender nothing (no scan to wait for)" || fail "notepad start asked $n times: $(head -3 "$T/log")"
    : > "$T/log"
    "$W/bin/wine" "$T/mine.exe" >/dev/null 2>&1
    grep -q "fd=$T/mine.exe" "$T/log" && pass "a program of the person's own is still asked about" || fail "mine.exe not asked about"
fi
"$W/bin/wineserver" -k 2>/dev/null

# 2. the same files through a folder others can write
W="$T/untrusted"
: > "$T/log"
took=$(notepad "$W")
echo "      cold notepad start, the same Wine through /var/tmp: $took s"
if [ -z "${SG_TRUST_SERVICE:-}" ]; then
    grep -q "fd=$W/lib/wine/" "$T/log" && pass "the same files reached through a folder others can write are asked about ($(grep -c "fd=$W/" "$T/log") requests)" \
        || fail "files under a world-writable folder were trusted: $(head -3 "$T/log")"
fi
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
