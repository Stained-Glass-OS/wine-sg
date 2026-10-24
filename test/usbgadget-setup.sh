#!/bin/sh
# usbgadget-setup.sh start [USER] | stop: the WinUSB test gadget, for test-winusb (patches/sg/
# 6519-6520). RUN AS ROOT ON A TEST VM ONLY (it loads dummy_hcd and libcomposite): a virtual USB
# device 1209:0001 (pid.codes' test ID) on the dummy host controller, declaring WinUSB in its
# Microsoft OS descriptors, served by test/usbgadget-ffs.py (FunctionFS). With USER it makes the
# gadget's own device node usable by that user if it is not already (that node only; nothing
# else's permissions change, and nothing persists past "stop" or a reboot).
#   sudo test/usbgadget-setup.sh start qa      # then, as qa: make test-winusb
#   sudo test/usbgadget-setup.sh stop
set -eu
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
ACTION=${1:-start}
TUSER=${2:-${SUDO_USER:-}}
G=/sys/kernel/config/usb_gadget/sgwinusb
FFS=/run/sgwinusb-ffs
PIDFILE=/run/sgwinusb.pid
READY=/run/sgwinusb.ready

[ "$(id -u)" = 0 ] || { echo "run as root (on a test VM)"; exit 1; }

device_node() {
    for d in /sys/bus/usb/devices/*; do
        [ -f "$d/idVendor" ] || continue
        if [ "$(cat "$d/idVendor")" = 1209 ] && [ "$(cat "$d/idProduct")" = 0001 ]; then
            printf '/dev/bus/usb/%03d/%03d\n' "$(cat "$d/busnum")" "$(cat "$d/devnum")"
            return 0
        fi
    done
    return 1
}

stop() {
    [ -f "$G/UDC" ] && echo "" > "$G/UDC" 2>/dev/null || true
    [ -f "$PIDFILE" ] && kill "$(cat "$PIDFILE")" 2>/dev/null || true
    rm -f "$PIDFILE" "$READY"
    mountpoint -q "$FFS" && umount "$FFS" || true
    if [ -d "$G" ]; then
        rm -f "$G/os_desc/c.1" "$G/configs/c.1/ffs.sgwinusb"
        rmdir "$G/configs/c.1/strings/0x409" "$G/configs/c.1" "$G/functions/ffs.sgwinusb" "$G/strings/0x409" "$G" 2>/dev/null || true
    fi
    rmdir "$FFS" 2>/dev/null || true
    echo "stopped"
}

start() {
    command -v python3 >/dev/null || { echo "python3 is needed"; exit 1; }
    modprobe libcomposite
    modprobe dummy_hcd || { echo "this kernel has no dummy_hcd module"; exit 1; }
    mountpoint -q /sys/kernel/config || mount -t configfs none /sys/kernel/config
    [ -d "$G" ] && stop >/dev/null
    mkdir -p "$G"
    echo 0x1209 > "$G/idVendor"
    echo 0x0001 > "$G/idProduct"
    echo 0x0200 > "$G/bcdUSB"
    echo 0x0100 > "$G/bcdDevice"
    mkdir -p "$G/strings/0x409"
    echo "Stained Glass OS" > "$G/strings/0x409/manufacturer"
    echo "WinUSB test gadget" > "$G/strings/0x409/product"
    echo "SG0001" > "$G/strings/0x409/serialnumber"
    mkdir -p "$G/configs/c.1/strings/0x409"
    echo "WinUSB test" > "$G/configs/c.1/strings/0x409/configuration"
    echo 100 > "$G/configs/c.1/MaxPower"
    echo 1 > "$G/os_desc/use"
    echo 0xcd > "$G/os_desc/b_vendor_code"
    echo MSFT100 > "$G/os_desc/qw_sign"
    ln -s "$G/configs/c.1" "$G/os_desc/c.1"
    mkdir -p "$G/functions/ffs.sgwinusb"
    ln -s "$G/functions/ffs.sgwinusb" "$G/configs/c.1/ffs.sgwinusb"
    mkdir -p "$FFS"
    mount -t functionfs sgwinusb "$FFS"
    rm -f "$READY"
    python3 "$HERE/usbgadget-ffs.py" "$FFS" "$READY" >/run/sgwinusb.log 2>&1 &
    echo $! > "$PIDFILE"
    i=0; while [ ! -f "$READY" ] && [ $i -lt 50 ]; do sleep 0.1; i=$((i + 1)); done
    [ -f "$READY" ] || { echo "the gadget's function did not start (/run/sgwinusb.log)"; stop; exit 1; }
    udc=$(ls /sys/class/udc | grep -m1 dummy_udc || true)
    [ -n "$udc" ] || { echo "no dummy_udc"; stop; exit 1; }
    echo "$udc" > "$G/UDC"
    i=0; node=
    while [ $i -lt 50 ]; do node=$(device_node || true); [ -n "$node" ] && [ -e "$node" ] && break; sleep 0.1; i=$((i + 1)); done
    [ -n "$node" ] || { echo "the gadget did not appear"; stop; exit 1; }
    echo "gadget: $node"
    if [ -n "$TUSER" ] && ! su -s /bin/sh -c "test -r '$node' && test -w '$node'" "$TUSER"; then
        # the gadget's own node only, for the test user, until the gadget goes
        if command -v setfacl >/dev/null; then setfacl -m "u:$TUSER:rw" "$node"; else chmod 0666 "$node"; fi
        echo "made $node usable by $TUSER"
    fi
}

case "$ACTION" in
start) start ;;
stop) stop ;;
*) echo "usage: $0 start [USER] | stop"; exit 2 ;;
esac
