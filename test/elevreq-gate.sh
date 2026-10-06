#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
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
# command). asInvoker programs run as before, highestAvailable ones too for a
# standard user (an administrator's session, SG_USER_ADMIN, asks: 0623), and
# so does everything inside a program the broker started (SG_IN_BROKER).
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

build() { # name level [comment]
    cat > "$W/$1.manifest" <<EOM
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
<trustInfo xmlns="urn:schemas-microsoft-com:asm.v2"><security><requestedPrivileges>
${3:-}
<requestedExecutionLevel level="$2" uiAccess="false"/>
</requestedPrivileges></security></trustInfo></assembly>
EOM
    printf '1 24 "%s"\n' "$W/$1.manifest" > "$W/$1.rc"
    "$WINDRES" "$W/$1.rc" -O coff -o "$W/$1.res" && \
        "$MINGW" -O2 -o "$W/$1.exe" "$HERE/elevreq-probe.c" "$W/$1.res" -lshell32 -lole32 -luuid || { echo "FAIL build $1"; exit 1; }
}
build admin requireAdministrator
build highest highestAvailable
build invoker asInvoker
# HandBrake's: the level it does not use, commented out, before the one it does
build commented asInvoker '<!-- <requestedExecutionLevel level="requireAdministrator" uiAccess="false"/> -->'
chmod 755 "$W"/*.exe
mkdir -m 777 "$W/out"
# the stand-in broker client: records its arguments
cat > "$W/elevate" <<EOS
#!/bin/sh
# --ready FILE (0625): the broker's answer, "0" launched; "slow-ready" -- as
# a consent prompt and an elevated program's start take -- four seconds late
ready=""
if [ "\$1" = --ready ]; then ready=\$2; shift 2; printf 'ready\n' >> "$W/out/ready.log"; fi
printf '%s\n' "\$*" >> "$W/out/elevate.log"   # not echo: dash's eats Windows paths' backslashes
case "\$*" in *slow-ready*) sleep 4 ;; esac
# "decline-me": the person said no (or let the prompt time out) -- the
# broker's answer "1", and sg-elevate's own exit 1
case "\$*" in *decline-me*) sleep 1; [ -n "\$ready" ] && printf 1 > "\$ready"; exit 1 ;; esac
[ -n "\$ready" ] && printf 0 > "\$ready"
case "\$*" in *wait-me*) sleep 2; exit 7 ;; *slow-ready*) sleep 6 ;; *exit-one*) sleep 1; exit 1 ;; esac
EOS
printf 'ready\n' > "$W/elevate.features"
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
        HOME=/var/tmp SG_ELEVATE="$W/elevate" SG_ELEVATE_FEATURES="$W/elevate.features" \
        ${SG_USER_ADMIN:+SG_USER_ADMIN=$SG_USER_ADMIN} "$@" 2>/dev/null |
        tr -d '\r' | grep -E '^(CREATE|SHELL|RUNASTIME) '
}
zp() { printf 'Z:%s' "${1//\//\\}"; }
other_wait() {   # MODE: the runas probe with the --ready stand-in
    sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" HOME=/var/tmp \
        SG_ELEVATE="$W/elevate" SG_ELEVATE_FEATURES="$W/elevate.features" \
        "$WINE" "$W/invoker.exe" "$1" "$(zp "$W/admin.exe")" 2>/dev/null | tr -d '\r' | grep '^WAIT '
}

out=$(other "$WINE" "$W/invoker.exe" create "$(zp "$W/admin.exe")")
[ "$out" = "CREATE err 740" ] && pass "CreateProcess of a requireAdministrator program: ERROR_ELEVATION_REQUIRED, as on Windows" \
    || fail "requireAdministrator CreateProcess: '$out'"
out=$(other "$WINE" "$W/invoker.exe" create "$(zp "$W/invoker.exe")")
[ "$out" = "CREATE ok 7" ] && pass "an asInvoker program runs" || fail "asInvoker: '$out'"
out=$(other "$WINE" "$W/invoker.exe" create "$(zp "$W/commented.exe")")
[ "$out" = "CREATE ok 7" ] && pass "requireAdministrator inside a manifest's comment does not count (HandBrake): it runs" \
    || fail "commented-out requireAdministrator: '$out'"
# Its compatibility settings (the Compatibility tab: AppCompatFlags\Layers,
# wine-sg 0420) say "run as an administrator": the same as requireAdministrator.
cp "$W/invoker.exe" "$W/layered.exe"; chmod 755 "$W/layered.exe"
other "$WINE" reg add 'HKCU\Software\Microsoft\Windows NT\CurrentVersion\AppCompatFlags\Layers' \
    /v "$(zp "$W/layered.exe")" /d '~ RUNASADMIN' /f >/dev/null
out=$(other "$WINE" "$W/invoker.exe" create "$(zp "$W/layered.exe")")
[ "$out" = "CREATE err 740" ] && pass "a program whose compatibility settings say run as an administrator: ERROR_ELEVATION_REQUIRED" \
    || fail "Layers RUNASADMIN: '$out'"
out=$(other "$WINE" "$W/invoker.exe" create "$(zp "$W/highest.exe")")
[ "$out" = "CREATE ok 7" ] && pass "a highestAvailable program runs as the invoker" || fail "highestAvailable: '$out'"
# ... for a standard user. An administrator's session (SG_USER_ADMIN, from
# sg-session-start) asks, as Windows does: Geany's installer (NSIS
# "highest") otherwise installed into the profile, unregistered (0623).
out=$(SG_USER_ADMIN=1 other "$WINE" "$W/invoker.exe" create "$(zp "$W/highest.exe")")
[ "$out" = "CREATE err 740" ] && pass "for an administrator a highestAvailable program needs elevation: ERROR_ELEVATION_REQUIRED" \
    || fail "highestAvailable, administrator: '$out'"
out=$(SG_USER_ADMIN=1 other "$WINE" "$W/invoker.exe" create "$(zp "$W/invoker.exe")")
[ "$out" = "CREATE ok 7" ] && pass "and an asInvoker program still runs for an administrator" || fail "asInvoker, administrator: '$out'"
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
# Run as administrator on a shortcut (the Start menu's PowerShell 7): the
# broker is given the shortcut's target, quoted -- it was given the shortcut's
# own path, split at its first space ("C:\ProgramData\Microsoft\Windows\Start"),
# and nothing ran (0444).
rm -f "$W/out/elevate.log"
# the account's profile, as its session would have made it (sg-profile-create):
# without a Desktop folder the shell namespace does not open at all
sudo -n -u "$SG_OTHER" mkdir -p "$PFX/drive_c/users/$SG_OTHER/Desktop"
out=$(other "$WINE" "$W/invoker.exe" runaslnk "$(zp "$W/highest.exe")")
sleep 1
log=$(cat "$W/out/elevate.log" 2>/dev/null)
if [ "$out" = "SHELL ok" ] && grep -q 'highest\.exe' <<<"$log" && ! grep -qi '\.lnk' <<<"$log" && grep -q -- '-NoLogo' <<<"$log"; then
    pass "Run as administrator on a shortcut hands its target and arguments to the broker ($log)"
else fail "runas on a shortcut: '$out', broker saw '$log'"; fi
# the caller of an elevated program can wait for it and read its exit code
# (Get a web browser waits for Edge's installer): Wine gives no handle for a
# Unix program, so the broker's client runs inside rundll32 (0448)
out=$(sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" HOME=/var/tmp \
      SG_ELEVATE="$W/elevate" "$WINE" "$W/invoker.exe" runaswait "$(zp "$W/admin.exe")" 2>/dev/null | tr -d '\r' | grep '^WAIT ')
case "$out" in "WAIT code 7 secs "[2-9]*) pass "Run as administrator: the caller waits for the elevated program and gets its code ($out)" ;;
    *) fail "waiting on an elevated program: '$out'" ;; esac
# Consent not given (No, or no answer before the prompt gave up): ShellExecuteEx
# fails with ERROR_CANCELLED (1223), as Windows' does -- it "succeeded" and the
# handle ended with 1, so SG Store said "msvbvm60.dll could not be put in place
# (code 1)". A program that was launched and itself ended with 1 still reads
# as code 1. Mutant SG_MUTANT_CONSENT_CODE1 (dlls/shell32/shlexec.c).
out=$(other_wait runasdecline)
[ "$out" = "WAIT err 1223" ] && pass "consent not given: ShellExecuteEx runas fails with ERROR_CANCELLED ($out)" \
    || fail "consent not given: '$out' (want WAIT err 1223)"
out=$(other_wait runasone)
case "$out" in "WAIT code 1 "*) pass "a launched program that ends with 1 still gives code 1 ($out)" ;;
    *) fail "launched, exit 1: '$out'" ;; esac
# NSIS's UAC plugin: its /UAC:<window> switch is not passed on (0457) -- the
# elevated copy could not reach the window and installed nothing
rm -f "$W/out/elevate.log"
sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=" HOME=/var/tmp \
    SG_ELEVATE="$W/elevate" "$WINE" "$W/invoker.exe" runasuac "$(zp "$W/admin.exe")" >/dev/null 2>&1
log=$(cat "$W/out/elevate.log" 2>/dev/null)
if grep -q 'wait-me' <<<"$log" && ! grep -qi '/UAC:' <<<"$log"; then pass "an NSIS installer's /UAC: switch is not passed to its elevated copy ($log)"
else fail "/UAC: $log"; fi
# start (cmd's, Wine's start.exe) runs a program through CreateProcess; one
# that needs an administrator goes on to ShellExecuteEx, which asks (0623):
# it reported "could not be started" instead.
rm -f "$W/out/elevate.log"
other "$WINE" start /wait "$(zp "$W/admin.exe")" >/dev/null
log=$(cat "$W/out/elevate.log" 2>/dev/null)
case "$log" in *admin.exe*) pass "start of a requireAdministrator program goes to the elevation broker ($log)" ;;
    *) fail "start /wait admin.exe: broker saw '$log'" ;; esac
rm -f "$W/out/elevate.log"
SG_USER_ADMIN=1 other "$WINE" start /wait "$(zp "$W/highest.exe")" >/dev/null
log=$(cat "$W/out/elevate.log" 2>/dev/null)
case "$log" in *highest.exe*) pass "an administrator's start of a highestAvailable program too ($log)" ;;
    *) fail "start /wait highest.exe, administrator: broker saw '$log'" ;; esac
# ShellExecuteEx "runas" returns once the elevated program has started, as
# Windows' does (0625): Total Commander's installer connects to its elevated
# copy straight after, and gave up while the consent prompt was still up
rm -f "$W/out/ready.log"
out=$(other "$WINE" "$W/invoker.exe" runastime "$(zp "$W/invoker.exe")")
ms=${out#RUNASTIME ms }
case "$out" in "RUNASTIME ms "*) [ "$ms" -ge 3500 ] && [ "$ms" -lt 9000 ] && [ -s "$W/out/ready.log" ] \
        && pass "ShellExecuteEx runas returns once the broker says launched (${ms} ms, not before)" \
        || fail "runas returned after ${ms} ms (ready asked: $(cat "$W/out/ready.log" 2>/dev/null))" ;;
    *) fail "runastime: '$out'" ;; esac
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
