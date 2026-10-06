#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# A standard user prints through a printer's Windows driver installed by the
# Windows system, and never installs a driver itself (patches/sg/0922, 0924).
#
# On a Stained Glass machine every user shares the machine's prefix and
# wineserver; only the Windows system (SYSTEM) and administrators may write
# its printer drivers. A standard user's program that tried (an update of a
# driver, a maker's package to install) was refused half-way, and a driver
# registered without its files left a printer nobody could print to. The
# gate shares a prefix and server between us (the "administrator": the
# prefix's owner, standing in for SYSTEM) and a second Unix user in the
# prefix's group (SG_SECOND_USER, default sgconf, reached with sudo -n; the
# gate is skipped without one). With a CUPS queue whose model has a maker's
# package staged ("SG Test UMPD", printdrv-umpd.c):
#   - the standard user's InstallPrinterDriverFromPackage is refused at once
#     (E_ACCESSDENIED), and its programs install nothing;
#   - the Windows system's side installs the package (splwow64 drivers);
#   - the standard user then prints through the driver: a raw CUPS job of
#     the driver's output.
#
#   WINE=/opt/wine-sg/bin/wine test/stduserprint-gate.sh
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINE="$(cd "$(dirname "$WINE")" && pwd)/$(basename "$WINE")"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
CUPSD="${CUPSD:-/usr/sbin/cupsd}"
U="${SG_SECOND_USER:-sgconf}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$CUPSD" ] || { echo "SKIP: no cupsd"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
sudo -n -u "$U" true 2>/dev/null || { echo "SKIP: no second user $U to sudo to"; exit 77; }
G=$(id -gn "$U" 2>/dev/null); for g in $(id -Gn "$U"); do id -Gn | tr ' ' '\n' | grep -qx "$g" && [ "$g" != "$G" ] && G=$g; done
id -Gn | tr ' ' '\n' | grep -qx "$G" || { echo "SKIP: no group shared with $U"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-stduserprint.XXXXXX); CP=
P="$T/prefix"
chgrp "$G" "$T"; chmod 2775 "$T"
mkdir -p "$P"; chgrp "$G" "$P"; chmod 2770 "$P"; touch "$P/.sg-system-prefix"; chmod 664 "$P/.sg-system-prefix"
export WINEPREFIX="$P" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER CUPS_SERVER="$T/cups.sock" CUPS_SERVERROOT="$T/root"
trap '"$WINESERVER" -k 2>/dev/null; [ -n "$CP" ] && { kill "$CP" 2>/dev/null; wait "$CP" 2>/dev/null; }; rm -rf "$T" 2>/dev/null; sudo -n -u "$U" rm -rf "$T" 2>/dev/null' EXIT INT TERM
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

mkdir -p "$T/root/ppd" "$T/cache" "$T/state" "$T/spool" "$T/log"
printf 'Listen %s/cups.sock\nLogLevel info\nIdleExitTimeout 0\nPreserveJobFiles Yes\n<Location />\nOrder allow,deny\nAllow all\n</Location>\n' "$T" > "$T/root/cupsd.conf"
printf 'ServerRoot %s/root\nCacheDir %s/cache\nStateDir %s/state\nRequestRoot %s/spool\nTempDir %s/spool\nErrorLog %s/log/error_log\nServerBin /usr/lib/cups\nDataDir /usr/share/cups\nUser %s\nGroup %s\n' \
    "$T" "$T" "$T" "$T" "$T" "$T" "$(id -un)" "$(id -gn)" > "$T/root/cups-files.conf"
printf '<Printer SGLabel>\nInfo Label\nMakeModel SG Test UMPD\nDeviceURI file:///dev/null\nState Idle\nAccepting Yes\n</Printer>\n' > "$T/root/printers.conf"
sed -n '/^\*PPD-Adobe/,/^\*Font Courier/p' "$HERE/cupsdrv-gate.sh" | sed 's/\$1/SG Test UMPD/g' > "$T/root/ppd/SGLabel.ppd"
"$CUPSD" -f -c "$T/root/cupsd.conf" -s "$T/root/cups-files.conf" > "$T/cupsd.out" 2>&1 & CP=$!
i=0; while [ ! -S "$T/cups.sock" ] && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
chmod 777 "$T/cups.sock"
lpstat -v 2>/dev/null | grep -q SGLabel || { echo "SKIP: the gate's CUPS server did not start"; exit 77; }

printf 'LIBRARY gdi32.dll\nEXPORTS\nEngCreateBitmap\nEngAssociateSurface\nEngDeleteSurface\nEngWritePrinter\n' > "$T/eng.def"
"${MINGW%-gcc}-dlltool" -d "$T/eng.def" -l "$T/libeng.a" || { echo "SKIP: no dlltool"; exit 77; }
"$MINGW" -shared -O2 -o "$T/sgtestumpd.dll" "$HERE/printdrv-umpd.c" "$T/libeng.a" -lgdi32 -lwinspool || { echo "FAIL  driver did not build"; exit 1; }
"$MINGW" -municode -O2 -o "$T/probe64.exe" "$HERE/printdrv-probe.c" -lwinspool -lgdi32 || { echo "FAIL  probe did not build"; exit 1; }

# the shared server, its group's
sg "$G" -c "\"$WINESERVER\" -p" &
sleep 1
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
cp "$T/probe64.exe" "$P/drive_c/"
S="$P/drive_c/windows/system32/DriverStore/FileRepository/sgtestumpd.inf_1"
mkdir -p "$S/amd64"; cp "$T/sgtestumpd.dll" "$S/amd64/"; printf 'SG test data\r\n' > "$S/sgtest.dat"
sed -n '/^\[Version\]/,/^sgtest.dat = 2/p' "$HERE/cupsdrv-gate.sh" > "$S/sgtestumpd.inf"
# the machine prefix is its group's (D13), and its drivers' directory the
# Windows system's: a standard user writes neither the drivers nor their keys
chgrp -R "$G" "$P" 2>/dev/null; chmod -R g+rwX "$P" 2>/dev/null
D="$P/drive_c/windows/system32/spool/drivers"
mkdir -p "$D/x64/3"; chmod -R g-w "$D"
as_user() { sudo -n -u "$U" env WINEPREFIX="$P" WINEDEBUG=-all WINESERVER="$WINESERVER" HOME="$T" CUPS_SERVER="$T/cups.sock" CUPS_SERVERROOT="$T/root" \
    WINEDLLOVERRIDES="winemenubuilder.exe=d" sh -c 'cd "$1/drive_c" && shift && exec timeout 120 "$@"' _ "$P" "$WINE" "$@" 2>/dev/null | tr -d '\r'; }
as_admin() { (cd "$P/drive_c" && timeout 120 "$WINE" "$@" 2>/dev/null | tr -d '\r'); }

# the standard user's attempts, with winspool's trace: refused before
# anything is written (no AddPrinterDriverEx), and a CUPS printer the Windows
# system has not made yet is left to it
as_user_trace() { sudo -n -u "$U" env WINEPREFIX="$P" WINEDEBUG=+winspool WINESERVER="$WINESERVER" HOME="$T" \
    CUPS_SERVER="$T/cups.sock" CUPS_SERVERROOT="$T/root" WINEDLLOVERRIDES="winemenubuilder.exe=d" \
    sh -c 'cd "$1/drive_c" && shift && exec timeout 120 "$@"' _ "$P" "$WINE" "$@" > "$T/user.trace" 2>&1; }
as_user_trace 'C:\probe64.exe' install "SG Test UMPD"
if grep -q '^install 0x80070005' "$T/user.trace" && ! grep -q 'AddPrinterDriverExW' "$T/user.trace"; then
    pass "a standard user's driver install is refused before anything is written"
else
    fail "the standard user's install: $(grep '^install' "$T/user.trace"), $(grep -c AddPrinterDriverExW "$T/user.trace") driver writes"
fi
# a printer added to CUPS meanwhile, met first by the standard user
kill "$CP"; wait "$CP" 2>/dev/null
printf '<Printer SGNew>\nInfo New\nMakeModel SG Test New\nDeviceURI file:///dev/null\nState Idle\nAccepting Yes\n</Printer>\n' >> "$T/root/printers.conf"
cp "$T/root/ppd/SGLabel.ppd" "$T/root/ppd/SGNew.ppd"
"$CUPSD" -f -c "$T/root/cupsd.conf" -s "$T/root/cups-files.conf" > "$T/cupsd.out" 2>&1 & CP=$!
i=0; while ! lpstat -v 2>/dev/null | grep -q SGNew && [ $i -lt 50 ]; do sleep 0.2; i=$((i + 1)); done
chmod 777 "$T/cups.sock"
as_user_trace 'C:\probe64.exe' enum
out=$(grep '^enum SGLabel' "$T/user.trace" | tr -d '\r')
if grep -q 'SGNew.*left to SYSTEM' "$T/user.trace" && ! grep -q 'AddPrinterDriverExW\|AddPrinterW' "$T/user.trace" &&
   [ "$out" != "${out%|wineps}" ]; then
    pass "a standard user's programs make no printer and install no driver ($out)"
else
    fail "a standard user's program wrote printers or drivers: $out, $(grep -c 'AddPrinter' "$T/user.trace") writes"
fi
as_admin splwow64.exe drivers >/dev/null
out=$(as_user 'C:\probe64.exe' enum | grep '^enum SGLabel')
[ "$out" = "enum SGLabel|SG Test UMPD|CUPS:SGLabel|winprint" ] &&
    pass "after the Windows system's side the standard user's printer has the Windows driver" ||
    fail "the standard user's printer: '$out'"
out=$(as_user 'C:\probe64.exe' print SGLabel)
sleep 2
job=""
for f in "$T/spool"/d*-001; do head -1 "$f" 2>/dev/null | grep -q '^SGTD START' && job=$f; done
page=$(grep -a '^PAGE' "$job" 2>/dev/null | tr -d '\r')
[ "$out" = "print ok" ] && [ "$page" = "PAGE 1 200x100 dark=5000 box=20,10-119,59" ] &&
    pass "the standard user printed through the Windows driver" ||
    fail "the standard user's print: '$out', '$page'"

[ $RC = 0 ] && echo "stduserprint gate: PASS" || echo "stduserprint gate: FAIL"
exit $RC
