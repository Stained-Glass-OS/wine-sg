#!/bin/bash
# Gate for wine-sg 0259: a program whose manifest says requireAdministrator
# asks for consent instead of failing.
#
# Installers (7-Zip, VLC, ...) declare requireAdministrator. Wine's loader
# elevated inside the new process, which 0019 forbids in a shared prefix, so
# they ran as the user and failed on Program Files ("Access denied") with no
# prompt. Now, as on Windows, CreateProcess refuses them with
# ERROR_ELEVATION_REQUIRED (740) for a caller that is not elevated, and
# ShellExecuteEx -- Explorer, Start-Process -- hands them to the elevation
# broker (0022's path; SG_ELEVATE names a stand-in here that records the
# command). asInvoker and highestAvailable programs run as before, and so does
# everything inside a program the broker started (SG_IN_BROKER).
#
# This user owns the prefix; SG_OTHER (default sgconf, in SG_GROUP) is the
# standard user. Needs passwordless `sudo -u $SG_OTHER`.
#   WINE=... test/elevreq-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}
W=$(mktemp -d /var/tmp/elevreq.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }

id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null || { echo "SKIP: no $SG_OTHER/sudo"; exit 77; }
command -v "$MINGW" >/dev/null && command -v "$WINDRES" >/dev/null || { echo "SKIP: no mingw"; exit 77; }
cleanup() {
    set +e
    WINEPREFIX=$PFX "$WINESERVER" -k 2>/dev/null
    sleep 1
    chmod -R u+w "$W" 2>/dev/null
    sudo -n rm -rf "$W"
}
trap cleanup EXIT

build() { # name level
    cat > "$W/$1.manifest" <<EOM
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
<trustInfo xmlns="urn:schemas-microsoft-com:asm.v2"><security><requestedPrivileges>
<requestedExecutionLevel level="$2" uiAccess="false"/>
</requestedPrivileges></security></trustInfo></assembly>
EOM
    printf '1 24 "%s"\n' "$W/$1.manifest" > "$W/$1.rc"
    "$WINDRES" "$W/$1.rc" -O coff -o "$W/$1.res" && \
        "$MINGW" -O2 -o "$W/$1.exe" "$HERE/elevreq-probe.c" "$W/$1.res" -lshell32 || { echo "FAIL build $1"; exit 1; }
}
build admin requireAdministrator
build highest highestAvailable
build invoker asInvoker
chmod 755 "$W"/*.exe
mkdir -m 777 "$W/out"
# the stand-in broker client: records its arguments
cat > "$W/elevate" <<EOS
#!/bin/sh
printf '%s\n' "\$*" >> "$W/out/elevate.log"   # not echo: dash's eats Windows paths' backslashes
EOS
chmod 755 "$W/elevate"

mkdir "$PFX"
chgrp "$SG_GROUP" "$PFX"
chmod 2770 "$PFX"
touch "$PFX/.sg-system-prefix"
export WINEPREFIX=$PFX WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml="
unset DISPLAY
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"
sg "$SG_GROUP" -c "umask 002; '$WINE' wineboot -i" >/dev/null 2>&1
chmod -R g+rwX "$PFX" 2>/dev/null

other() {
    sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" \
        HOME=/var/tmp SG_ELEVATE="$W/elevate" "$@" 2>/dev/null | tr -d '\r' | grep -E '^(CREATE|SHELL) '
}
zp() { printf 'Z:%s' "${1//\//\\}"; }

out=$(other "$WINE" "$W/invoker.exe" create "$(zp "$W/admin.exe")")
[ "$out" = "CREATE err 740" ] && pass "CreateProcess of a requireAdministrator program: ERROR_ELEVATION_REQUIRED, as on Windows" \
    || fail "requireAdministrator CreateProcess: '$out'"
out=$(other "$WINE" "$W/invoker.exe" create "$(zp "$W/invoker.exe")")
[ "$out" = "CREATE ok 7" ] && pass "an asInvoker program runs" || fail "asInvoker: '$out'"
out=$(other "$WINE" "$W/invoker.exe" create "$(zp "$W/highest.exe")")
[ "$out" = "CREATE ok 7" ] && pass "a highestAvailable program runs as the invoker" || fail "highestAvailable: '$out'"
rm -f "$W/out/elevate.log"
out=$(other "$WINE" "$W/invoker.exe" shell "$(zp "$W/admin.exe")")
sleep 1
log=$(cat "$W/out/elevate.log" 2>/dev/null)
[ "$out" = "SHELL ok" ] && [ "${log#--wine }" != "$log" ] && grep -q 'admin.exe' <<<"$log" \
    && pass "ShellExecuteEx hands it to the elevation broker ($log)" || fail "ShellExecuteEx: '$out', broker saw '$log'"
rm -f "$W/out/elevate.log"
out=$(other "$WINE" "$W/invoker.exe" shell "$(zp "$W/invoker.exe")")
[ "$out" = "SHELL ok" ] && [ ! -s "$W/out/elevate.log" ] && pass "an asInvoker program is not sent to the broker" \
    || fail "asInvoker via ShellExecuteEx: '$out', broker '$(cat "$W/out/elevate.log" 2>/dev/null)'"
out=$(other env SG_IN_BROKER=1 "$WINE" "$W/invoker.exe" create "$(zp "$W/admin.exe")")
[ "$out" = "CREATE ok 7" ] && pass "inside a program the broker started, it runs (no loop)" || fail "SG_IN_BROKER: '$out'"
# no broker installed (a relative SG_ELEVATE is not a stand-in, and this
# machine has no /usr/libexec/stained-glass/sg-elevate): Wine as before
if [ ! -e /usr/libexec/stained-glass/sg-elevate ]; then
    out=$(sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all HOME=/var/tmp SG_ELEVATE=relative/elevate \
          "$WINE" "$W/invoker.exe" create "$(zp "$W/admin.exe")" 2>/dev/null | tr -d '\r' | grep '^CREATE')
    [ "$out" = "CREATE ok 7" ] && pass "with no elevation broker the program runs as before" || fail "no broker: '$out'"
fi
echo "elevreq-gate: $fails failure(s)"
exit $((fails > 0))
