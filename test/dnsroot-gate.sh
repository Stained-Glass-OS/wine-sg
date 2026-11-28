#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# systemd-resolved's stub resolv.conf says "search ." when there is no search domain; "." is the
# DNS root, never a suffix Windows lists (patches/sg/9660). Every adapter's "Connection-specific
# DNS suffix" was "." (ipconfig /all, GetAdaptersAddresses' DnsSuffix, .NET), and the Network
# List Manager named every network "." after it. In a private network and mount namespace with a
# resolv.conf of the gate's own: "search ." gives an empty search list and empty suffixes; a real
# "search corp.example lan.example" still gives both domains, the first as the suffix.
#   WINE=... test/dnsroot-gate.sh        (needs sudo -n, unshare, setpriv)
#   mutant: SG_MUTANT_DNS_ROOT_SUFFIX (dlls/dnsapi/libresolv.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER=$(sg_wineserver)
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
for t in "$MINGW" unshare setpriv ip; do command -v "$t" >/dev/null || { echo "SKIP: $t missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
sudo -n true 2>/dev/null || { echo "SKIP: no sudo"; exit 77; }
W=$(mktemp -d /var/tmp/sg-dnsroot.XXXXXX) || exit 1
chmod 755 "$W"
cleanup() { sudo -n rm -rf "$W"; }
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$W/probe.exe" "$HERE/dnsroot-probe.c" -ldnsapi -liphlpapi -lws2_32 || { echo "FAIL  probe did not build"; exit 1; }
cat > "$W/inner.sh" <<'INNER'
set -u
W=$1; WINE=$2; WINESERVER=$3; U=$4; G=$5; N=$6; H=$7
ip link set lo up
ip link add sgeth0 type dummy && ip addr add 10.66.0.5/24 dev sgeth0 && ip link set sgeth0 up
R=$(readlink -f /etc/resolv.conf); mkdir -p "$(dirname "$R")"; [ -e "$R" ] || : > "$R"
printf 'nameserver 127.0.0.53\noptions edns0 trust-ad\nsearch .\n' > "$W/root.conf"
printf 'nameserver 127.0.0.53\nsearch corp.example lan.example\n' > "$W/real.conf"
me() { setpriv --reuid="$U" --regid="$G" --init-groups -- env USER="$N" LOGNAME="$N" HOME="$H" DISPLAY= "$@"; }
export WINEPREFIX="$W/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
me timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; me "$WINESERVER" -w
cp "$W/probe.exe" "$WINEPREFIX/drive_c/"
for c in root real; do
    mount --bind "$W/$c.conf" "$R" || { echo "FAIL  resolv.conf bind"; exit 1; }
    (cd "$WINEPREFIX/drive_c" && me timeout -s KILL 60 "$WINE" 'C:\probe.exe' 2>/dev/null </dev/null) | tr -d '\r' > "$W/$c.out"
    me "$WINESERVER" -k 2>/dev/null; me "$WINESERVER" -w
    umount "$R"
done
sed 's/^/      search .: /' "$W/root.out"; sed 's/^/      real:     /' "$W/real.out"
grep -qx 'SEARCH' "$W/root.out" && echo "PASS  \"search .\" is an empty search list" || echo "FAIL  \"search .\": $(grep SEARCH "$W/root.out")"
grep -q '^SUFFIX sgeth0 \[\]$' "$W/root.out" && ! grep -q '\[\.\]' "$W/root.out" && echo "PASS  and no adapter's DNS suffix is \".\"" ||
    echo "FAIL  suffixes: $(grep SUFFIX "$W/root.out" | tr '\n' ' ')"
grep -qx 'SEARCH \[corp.example\] \[lan.example\]' "$W/real.out" && grep -q '^SUFFIX sgeth0 \[corp.example\]$' "$W/real.out" &&
    echo "PASS  a real search list is kept, its first domain the suffix" || echo "FAIL  real: $(tr '\n' ' ' < "$W/real.out")"
INNER
out=$(sudo -n -E unshare -n -m --propagation private -- sh "$W/inner.sh" "$W" "$WINE" "$WINESERVER" "$(id -u)" "$(id -g)" "$(id -un)" "$HOME")
printf '%s\n' "$out"
if printf '%s\n' "$out" | grep -q '^FAIL' || ! printf '%s\n' "$out" | grep -q '^PASS'; then echo "RESULT: FAIL"; exit 1; fi
echo "RESULT: PASS"
