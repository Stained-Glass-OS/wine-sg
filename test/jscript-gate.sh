#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# What installer script custom actions need (patches/sg/0407), found in the
# OpenVPN MSI: its JScript actions call GetObject("winmgmts:...") (Wine's
# JScript did not implement GetObject) and
# FileSystemObject.GetAbsolutePathName("") (Wine failed on an empty path;
# Windows gives the current directory). Each failure aborted the install.
#
#   WINE=/opt/wine-sg/bin/wine test/jscript-gate.sh
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-jscript.XXXXXX)
mkdir -p "$T/home"
export HOME="$T/home" XDG_CONFIG_HOME="$T/home/.config" XDG_DATA_HOME="$T/home/.local/share" XDG_DESKTOP_DIR="$T/home/Desktop"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cat > "$WINEPREFIX/drive_c/t.js" <<'JS'
try {
    var wmi = GetObject("winmgmts://./root/cimv2");
    var svcs = wmi.ExecQuery("Select * from Win32_Service where Name = 'eventlog'");
    WScript.Echo("services=" + svcs.Count);
} catch (e) { WScript.Echo("wmi-error=" + e.number); }
try {
    var os = GetObject("winmgmts:").ExecQuery("Select * from Win32_OperatingSystem");
    WScript.Echo("os=" + os.Count);
} catch (e) { WScript.Echo("os-error=" + e.number); }
var fso = new ActiveXObject("Scripting.FileSystemObject");
var shell = new ActiveXObject("WScript.Shell");
try { WScript.Echo("empty=" + (fso.GetAbsolutePathName("").toLowerCase() == shell.CurrentDirectory.toLowerCase())); }
catch (e) { WScript.Echo("empty-error=" + e.number); }
try { GetObject("not a moniker ::"); WScript.Echo("bad=no-error"); } catch (e) { WScript.Echo("bad=error"); }
JS
out=$(cd "$WINEPREFIX/drive_c" && timeout -s KILL 120 "$WINE" cscript //nologo 'C:\t.js' 2>/dev/null | tr -d '\r')
echo "$out"
case "$out" in *services=1*) pass "GetObject(\"winmgmts://./root/cimv2\") and a WMI query" ;; *) fail "WMI through GetObject" ;; esac
case "$out" in *os=1*) pass "GetObject(\"winmgmts:\")" ;; *) fail "winmgmts: moniker" ;; esac
case "$out" in *empty=true*) pass "GetAbsolutePathName(\"\") is the current directory" ;; *) fail "empty path" ;; esac
case "$out" in *bad=error*) pass "a name that is no moniker is an error" ;; *) fail "bad moniker" ;; esac
exit $RC
