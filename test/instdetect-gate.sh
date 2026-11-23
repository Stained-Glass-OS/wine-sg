#!/bin/bash
. "$(dirname "$0")/scratch-home.sh"
# Gate for wine-sg 9000: UAC's Installer Detection, as Windows does it.
#
# OpenSCAD 2021's installer (NSIS 2: 32-bit, no requestedExecutionLevel in its
# manifest, "Installer" in its name) ran as the person and failed on Program
# Files with no prompt; Windows takes such a program for an installer and asks.
# A 32-bit program with no level stated, started interactively, whose file
# name or version resource (company, product, description, original and
# internal name) says install/setup/update/patch/upgrade, needs an
# administrator: CreateProcess refuses it with ERROR_ELEVATION_REQUIRED (740)
# for a standard user and ShellExecuteEx hands it to the elevation broker
# (SG_ELEVATE, a stand-in that records the command). Not when its manifest
# (inside, or beside it as PROGRAM.manifest) states any level, not for a
# 64-bit program, not for one of Wine's own (builtin) programs, not onto a
# service's window station, not with __COMPAT_LAYER=RunAsInvoker, not with
# the policy EnableInstallerDetection = 0, not inside a program the broker
# started. Mutant SG_MUTANT_NO_INSTALLER_DETECTION (kernelbase) fails the 740s.
#
# This user owns the prefix; SG_OTHER (default sgconf, in SG_GROUP) is the
# standard user. Needs passwordless `sudo -u $SG_OTHER` and both mingw compilers.
#   WINE=... test/instdetect-gate.sh
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
WINE=${WINE:-/opt/wine-sg/bin/wine}
WINESERVER=${WINESERVER:-$(dirname "$WINE")/wineserver}
[ -x "$WINESERVER" ] || WINESERVER=$(dirname "$WINE")/server/wineserver
SG_OTHER=${SG_OTHER:-sgconf}
SG_GROUP=${SG_GROUP:-sgconfgrp}
MINGW=${MINGW:-x86_64-w64-mingw32-gcc}
MINGW32=${MINGW32:-i686-w64-mingw32-gcc}
WINDRES32=${WINDRES32:-i686-w64-mingw32-windres}
WINDRES=${WINDRES:-x86_64-w64-mingw32-windres}
fails=0
pass() { printf 'PASS %s\n' "$*"; }
fail() { printf 'FAIL %s\n' "$*"; fails=$((fails + 1)); }

id "$SG_OTHER" >/dev/null 2>&1 && sudo -n -u "$SG_OTHER" true 2>/dev/null || { echo "SKIP: no $SG_OTHER/sudo"; exit 77; }
for c in "$MINGW" "$MINGW32" "$WINDRES" "$WINDRES32"; do command -v "$c" >/dev/null || { echo "SKIP: no $c"; exit 77; }; done
W=$(mktemp -d /var/tmp/instdetect.XXXXXX)
chmod 755 "$W"
PFX=$W/prefix
cleanup() {
    set +e
    WINEPREFIX=$PFX "$WINESERVER" -k 2>/dev/null
    sleep 1
    chmod -R u+w "$W" 2>/dev/null
    sudo -n rm -rf "$W"
}
trap cleanup EXIT

"$MINGW" -O2 -o "$W/probe.exe" "$HERE/instdetect-probe.c" -lshell32 || { echo "FAIL build probe"; exit 1; }
# build32 NAME [FIELD VALUE] [LEVEL]: a 32-bit program, a version resource with FIELD=VALUE, a manifest with LEVEL
build32() {
    local name=$1 field=${2:-} value=${3:-} level=${4:-} res=
    : > "$W/$name.rc"
    if [ -n "$field" ]; then
        cat >> "$W/$name.rc" <<EOR
1 VERSIONINFO
FILEVERSION 1,0,0,0
BEGIN
  BLOCK "StringFileInfo"
  BEGIN
    BLOCK "040904b0"
    BEGIN
      VALUE "CompanyName", "Gate Works"
      VALUE "$field", "$value"
    END
  END
  BLOCK "VarFileInfo"
  BEGIN
    VALUE "Translation", 0x409, 1200
  END
END
EOR
    fi
    if [ -n "$level" ]; then
        cat > "$W/$name.manifest" <<EOM
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
<trustInfo xmlns="urn:schemas-microsoft-com:asm.v2"><security><requestedPrivileges>
<requestedExecutionLevel level="$level" uiAccess="false"/>
</requestedPrivileges></security></trustInfo></assembly>
EOM
        printf '1 24 "%s"\n' "$W/$name.manifest" >> "$W/$name.rc"
    fi
    if [ -s "$W/$name.rc" ]; then "$WINDRES32" "$W/$name.rc" -O coff -o "$W/$name.res" || { echo "FAIL build $name"; exit 1; }; res="$W/$name.res"; fi
    "$MINGW32" -O2 -o "$W/$name.exe" "$HERE/instdetect-probe.c" $res -lshell32 || { echo "FAIL build $name"; exit 1; }
}
build32 gate-setup                                     # "setup" in its file name
build32 Tool-Installer                                 # OpenSCAD's: "Installer"
build32 tool32                                         # nothing that says so
build32 toolfd FileDescription "Gate Installer"        # its description says so
build32 toolpn ProductName "Gate Updater"              # its product name
build32 toolin InternalName "patchkit"                 # its internal name
build32 toolof OriginalFilename "upgrade.exe"          # its original file name
build32 toolok FileDescription "Gate Tool"             # a version resource that does not
build32 setup-invoker "" "" asInvoker                  # a manifest that states a level
build32 setup-highest "" "" highestAvailable
build32 setup-extman                                   # no manifest inside, one beside it
cp "$W/setup-invoker.manifest" "$W/setup-extman.exe.manifest"
"$MINGW" -O2 -o "$W/setup64.exe" "$HERE/instdetect-probe.c" -lshell32 || exit 1   # a 64-bit one
# one of Wine's own: the builtin mark after the DOS header
cp "$W/tool32.exe" "$W/builtin-setup.exe"
printf 'Wine builtin DLL' | dd of="$W/builtin-setup.exe" bs=1 seek=64 conv=notrunc 2>/dev/null
chmod 755 "$W"/*.exe
mkdir -m 777 "$W/out"
cat > "$W/elevate" <<EOS
#!/bin/sh
ready=""
if [ "\$1" = --ready ]; then ready=\$2; shift 2; fi
printf '%s\n' "\$*" >> "$W/out/elevate.log"
[ -n "\$ready" ] && printf 0 > "\$ready"
exit 0
EOS
printf 'ready\n' > "$W/elevate.features"
chmod 755 "$W/elevate"

mkdir "$PFX"
chgrp "$SG_GROUP" "$PFX"
chmod 2770 "$PFX"
touch "$PFX/.sg-system-prefix"
export WINEPREFIX=$PFX WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY
sg "$SG_GROUP" -c "umask 002; '$WINESERVER' -p"
sg "$SG_GROUP" -c "umask 002; '$WINE' wineboot -i" >/dev/null 2>&1
chmod -R g+rwX "$PFX" 2>/dev/null

other() {
    sudo -n -u "$SG_OTHER" env WINEPREFIX="$PFX" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" \
        HOME=/var/tmp SG_ELEVATE="$W/elevate" SG_ELEVATE_FEATURES="$W/elevate.features" "$@" 2>/dev/null |
        tr -d '\r' | grep -E '^(CREATE|SHELL) '
}
zp() { printf 'Z:%s' "${1//\//\\}"; }
create() { other "$@" "$WINE" "$W/probe.exe" create "$(zp "$W/$PROG")"; }
asks()  { local out; PROG=$1; shift; out=$(create "${@:-env}"); [ "$out" = "CREATE err 740" ]; }
runs()  { local out; PROG=$1; shift; out=$(create "${@:-env}"); [ "$out" = "CREATE ok 7" ]; }
check() { # asks|runs PROGRAM what [env...]
    local want=$1 prog=$2 what=$3; shift 3
    if "$want" "$prog" "$@"; then pass "$what ($prog)"; else fail "$what ($prog): $(PROG=$prog create "${@:-env}")"; fi
}

check asks gate-setup.exe     "a 32-bit program with \"setup\" in its name and no level: ERROR_ELEVATION_REQUIRED"
check asks Tool-Installer.exe "...\"Installer\" (OpenSCAD's installer), any case"
check asks toolfd.exe         "...its version resource's FileDescription says Installer"
check asks toolpn.exe         "...its ProductName says Updater"
check asks toolin.exe         "...its InternalName says patch"
check asks toolof.exe         "...its OriginalFilename says upgrade"
check runs tool32.exe         "a 32-bit program with nothing that says installer runs"
check runs toolok.exe         "...nor a version resource that does not say so"
check runs setup-invoker.exe  "a manifest that states asInvoker: no detection, it runs"
check runs setup-highest.exe  "...highestAvailable (a standard user): runs"
check runs setup-extman.exe   "...a level stated in PROGRAM.manifest beside it: runs"
check runs setup64.exe        "a 64-bit program named setup runs"
out=$(other "$WINE" "$W/probe.exe" create "$(zp "$W/builtin-setup.exe")")
[ "$out" != "CREATE err 740" ] && pass "one of Wine's own (builtin) programs is never taken for an installer ($out)" \
    || fail "builtin: '$out'"
check runs gate-setup.exe     "with __COMPAT_LAYER=RunAsInvoker it runs" env __COMPAT_LAYER=RunAsInvoker
check runs gate-setup.exe     "inside a program the broker started it runs" env SG_IN_BROKER=1
out=$(other "$WINE" "$W/probe.exe" createsvc "$(zp "$W/gate-setup.exe")")
[ "$out" != "CREATE err 740" ] && pass "started onto a service's window station: not interactive, not asked ($out)" \
    || fail "service window station: '$out'"
rm -f "$W/out/elevate.log"
out=$(other "$WINE" "$W/probe.exe" shell "$(zp "$W/Tool-Installer.exe")")
sleep 1
log=$(cat "$W/out/elevate.log" 2>/dev/null)
[ "$out" = "SHELL ok" ] && grep -q 'Tool-Installer.exe' <<<"$log" \
    && pass "ShellExecuteEx (a double-click) hands it to the elevation broker ($log)" || fail "ShellExecuteEx: '$out', broker saw '$log'"
# the policy (User Account Control: Detect application installations and prompt for elevation)
sg "$SG_GROUP" -c "umask 002; '$WINE' reg add 'HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System' /v EnableInstallerDetection /t REG_DWORD /d 0 /f" >/dev/null 2>&1
check runs gate-setup.exe     "EnableInstallerDetection = 0: it runs"
sg "$SG_GROUP" -c "umask 002; '$WINE' reg add 'HKLM\\Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System' /v EnableInstallerDetection /t REG_DWORD /d 1 /f" >/dev/null 2>&1
check asks gate-setup.exe     "EnableInstallerDetection = 1: asked again"
echo "instdetect-gate: $fails failure(s)"
exit $((fails > 0))
