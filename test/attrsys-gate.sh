#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# SYSTEM (or an elevated administrator) sets and clears the read-only flag of a file another account owns,
# as on Windows (patches/sg/7795): only a file's owner may chmod it, so ntdll asks the system wineserver,
# which makes its CAP_FOWNER (permitted only) effective for that one fchmod -- for SYSTEM and elevated
# administrators' processes only: a standard user is refused (access denied), the file unchanged. Found
# with GOG GALAXY's SYSTEM service and the files its client, run by the user, had made (7794 covered only
# a flag that does not change). The shared-prefix harness of sysfowner-gate.sh: a copy of the server given
# cap_fowner+p, the prefix's owner is SYSTEM, sgconf a standard user.
#
#   WINE=/opt/wine-sg/bin/wine test/attrsys-gate.sh
#   mutants: SG_MUTANT_CHMOD_NO_SERVER (ntdll unix/file.c), SG_MUTANT_CHMOD_ANYONE (server/fd.c)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER=$(sg_wineserver)
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
id -nG | tr ' ' '\n' | grep -qx "$SG_GROUP" || { echo "SKIP: not in $SG_GROUP"; exit 77; }
id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null || { echo "SKIP: no $SG_OTHER/sudo"; exit 77; }
command -v setcap >/dev/null || [ -x /usr/sbin/setcap ] || [ -x /sbin/setcap ] || { echo "SKIP: no setcap (libcap2-bin)"; exit 77; }

T=$(mktemp -d /var/tmp/sg-attrsys.XXXXXX); chmod 755 "$T"
cp "$WINESERVER" "$T/wineserver"
sudo -n /usr/sbin/setcap cap_fowner+p "$T/wineserver" 2>/dev/null || sudo -n setcap cap_fowner+p "$T/wineserver" 2>/dev/null \
    || { rm -rf "$T"; echo "SKIP: cannot give the server CAP_FOWNER (sudo setcap)"; exit 77; }
WINESERVER="$T/wineserver"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER DISPLAY=
cleanup() { "$WINESERVER" -k 2>/dev/null; sudo -n -u "$SG_OTHER" rm -rf "$WINEPREFIX/drive_c/shared" 2>/dev/null; rm -rf "$T" "$SG_GATE_HOME"; }
trap cleanup EXIT INT TERM
"$MINGW" -O2 -municode -o "$T/attrsys-probe.exe" "$HERE/attrsys-probe.c" || { fail "probe did not build"; exit 1; }
chmod 755 "$T/attrsys-probe.exe"
mkdir "$WINEPREFIX"; chgrp "$SG_GROUP" "$WINEPREFIX"; chmod 2770 "$WINEPREFIX"; touch "$WINEPREFIX/.sg-system-prefix"
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"
sg "$SG_GROUP" -c "umask 002; timeout -s KILL 300 '$WINE' wineboot -i" >/dev/null 2>&1
chmod -R g+rwX "$WINEPREFIX" 2>/dev/null
S="$WINEPREFIX/drive_c/shared"; mkdir -p "$S"; chgrp "$SG_GROUP" "$S"; chmod 2775 "$S"
PROBE="Z:$(printf '%s' "$T/attrsys-probe.exe" | tr / '\\')"
as_user() { sudo -n -u "$SG_OTHER" env WINEPREFIX="$WINEPREFIX" WINEDEBUG=-all HOME="$HOME" DISPLAY= "$WINE" "$PROBE" "$@" 2>/dev/null | tr -d '\r'; }
as_system() { "$WINE" "$PROBE" "$@" 2>/dev/null | tr -d '\r'; }
made=$(as_user make 'C:\shared\users-file.exe')
[ "$made" = "made 1" ] || fail "the user could not make a file: $made"
F="$S/users-file.exe"
echo "      $(stat -c '%A %U' "$F")"

out=$(as_system ro 'C:\shared\users-file.exe'); m1=$(stat -c '%A' "$F")
case "$out" in "set=OK attr=0x"*[13579bdf]) ok=1 ;; *) ok=0 ;; esac   # FILE_ATTRIBUTE_READONLY
case "$m1" in *w*) ok=0 ;; esac
[ $ok = 1 ] && pass "SYSTEM makes the user's file read-only ($out, $m1)" \
    || fail "SYSTEM read-only on the user's file: $out, mode $m1"
out=$(as_system normal 'C:\shared\users-file.exe'); m2=$(stat -c '%A' "$F")
case "$out" in "set=OK attr=0x"*[02468ace]) ok=1 ;; *) ok=0 ;; esac
[ $ok = 1 ] && [ "$(printf '%s' "$m2" | cut -c3)" = w ] && pass "and writable again ($out, $m2)" \
    || fail "SYSTEM clearing read-only: $out, mode $m2"

# a standard user may not do it to SYSTEM's file
as_system make 'C:\shared\system-file.exe' >/dev/null
chmod 664 "$S/system-file.exe"
out=$(as_user ro 'C:\shared\system-file.exe'); m3=$(stat -c '%A' "$S/system-file.exe")
case "$out" in "set=ERR 5 "*) ok=1 ;; *) ok=0 ;; esac
[ $ok = 1 ] && [ "$m3" = "-rw-rw-r--" ] && pass "a standard user is refused on SYSTEM's file, which stays as it was ($out)" \
    || fail "standard user on SYSTEM's file: $out, mode $m3"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
