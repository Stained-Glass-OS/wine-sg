#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Disk and partition answers from the host (patches/sg/1629):
# test/diskinfo-probe.c asks \\.\C: (the disk under the prefix) and the
# gate compares the answers with sysfs. mountmgr made these up: a disk of
# 10000 cylinders of 512-byte sectors, no seek penalty (every disk an SSD),
# no maker or model, zeroed volume extents; partition information and
# IOCTL_DISK_GET_LENGTH_INFO were not answered (backup tools, installers,
# defragmenters and game launchers ask).
#
#   WINE=/opt/wine-sg/bin/wine test/diskinfo-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
# Skips when the prefix is on a filesystem with no block device under it
# (tmpfs, overlay, btrfs subvolumes).
# Mutant: SG_MUTANT_NO_DISK_INFO (mountmgr.sys/device.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v x86_64-w64-mingw32-gcc >/dev/null || { echo "SKIP: mingw not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-diskinfo.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
cleanup() { "$WINESERVER" -k 2>/dev/null; rm -rf "$T"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp x86_64-w64-mingw32-gcc -O1 -o "$T/probe.exe" "$HERE/diskinfo-probe.c" || { echo "FAIL  probe did not build"; exit 1; }
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

# what sysfs says of the device under drive_c
hex=$(stat -c %D "$WINEPREFIX/drive_c")
dev=$(printf '%d' "0x$hex")
maj=$(( (dev >> 8) & 0xfff )); min=$(( (dev & 0xff) | ((dev >> 12) & 0xfff00) ))
[ "$maj" = 0 ] && { echo "SKIP: no block device under the prefix"; exit 77; }
sys=$(readlink -f "/sys/dev/block/$maj:$min" 2>/dev/null)
[ -d "$sys" ] || { echo "SKIP: no sysfs entry for $maj:$min"; exit 77; }
if [ -e "$sys/partition" ]; then
    disk=$(dirname "$sys"); start=$(( $(cat "$sys/start") * 512 )); size=$(( $(cat "$sys/size") * 512 ))
else
    [ -n "$(ls "$sys/slaves" 2>/dev/null)" ] && { echo "SKIP: a mapped device ($sys)"; exit 77; }
    disk=$sys; start=0; size=$(( $(cat "$sys/size") * 512 ))
fi
rot=$(cat "$disk/queue/rotational"); lsec=$(cat "$disk/queue/logical_block_size")
dsize=$(( $(cat "$disk/size") * 512 ))
model=$(cat "$disk/device/model" 2>/dev/null | sed 's/ *$//')
echo "sysfs: $sys disk $disk start $start size $size rotational $rot sector $lsec disk size $dsize model '$model'"

out=$(cd "$T" && timeout -s KILL 120 env DISPLAY= "$WINE" "$T/probe.exe" 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
v() { printf '%s\n' "$out" | sed -n "s/^$1 //p"; }
[ "$(v seekpenalty)" = "$rot" ] && pass "the seek penalty is the disk's (rotational $rot)" || fail "seek penalty: $(v seekpenalty), sysfs $rot"
[ "$(v logicalsector)" = "$lsec" ] && pass "the logical sector size is the disk's" || fail "logical sector: $(v logicalsector), sysfs $lsec"
[ "$(v bytespersector)" = "$lsec" ] && pass "... and the geometry's" || fail "geometry sector: $(v bytespersector)"
[ "$(v length)" = "$size" ] && pass "IOCTL_DISK_GET_LENGTH_INFO: the partition's size" || fail "length: $(v length), sysfs $size"
[ "$(v partlength)" = "$size" ] && [ "$(v partstart)" = "$start" ] && pass "IOCTL_DISK_GET_PARTITION_INFO_EX: its start and length" ||
    fail "partition: $(v partstart) $(v partlength), sysfs $start $size"
[ "$(v extents)" = 1 ] && [ "$(v extentstart)" = "$start" ] && [ "$(v extentlength)" = "$size" ] &&
    pass "IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS: one extent, the partition" || fail "extents: $(v extents) $(v extentstart) $(v extentlength)"
[ "$(v disksize)" = "$dsize" ] && pass "the disk's size" || fail "disk size: $(v disksize), sysfs $dsize"
if [ -n "$model" ]; then
    [ "$(v product)" = "$model" ] && pass "the disk's model ($model)" || fail "product: '$(v product)', sysfs '$model'"
fi
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
