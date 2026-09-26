#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# powershell.exe's exit status (patches/sg/0302).
#
# Wine's powershell.exe is a stub that runs nothing -- and used to exit 0
# for every script, which told electron-builder installers (Joplin,
# Obsidian, and most Electron applications' NSIS setups) "yes, Get-CimInstance
# exists" and then "yes, the application is running": they gave up with
# exit code 2 and installed nothing. A script it cannot run now exits 1, as
# a failed PowerShell command does, and callers take their fallback path
# (electron-builder's is tasklist | findstr, which works).
#
#   WINE=/opt/wine-sg/bin/wine test/powershell-gate.sh
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
export WINESERVER
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-powershell.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
# A scratch HOME and no menu/desktop integration: a prefix links its Desktop,
# Documents, Downloads... to $HOME's, and installers write shortcuts there.
export HOME="$T/home" XDG_CONFIG_HOME="$T/home/.config" XDG_DATA_HOME="$T/home/.local/share" \
    XDG_DESKTOP_DIR="$T/home/Desktop"
mkdir -p "$HOME/Desktop"
export WINEDLLOVERRIDES="winemenubuilder.exe=d${WINEDLLOVERRIDES:+;$WINEDLLOVERRIDES}"
export WINEPREFIX="$T/prefix" WINEDEBUG=-all
"$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
PS='C:\windows\system32\WindowsPowerShell\v1.0\powershell.exe'

rc() { "$WINE" "$PS" "$@" < /dev/null > /dev/null 2>&1; echo $?; }

# electron-builder's two probes, verbatim
r=$(rc -C "if (Get-Command Get-CimInstance -ErrorAction SilentlyContinue) { exit 0 } else { exit 1 }")
[ "$r" = 1 ] && pass "a script the stub cannot run fails (electron-builder's Get-CimInstance probe: $r)" || fail "Get-CimInstance probe exited $r, not 1"
r=$(rc -C "if ((Get-CimInstance -ClassName Win32_Process | ? {\$_.Path -and \$_.Path.StartsWith('C:\\x')}).Count -gt 0) { exit 0 } else { exit 1 }")
[ "$r" = 1 ] && pass "\"is the application running?\" is not answered yes ($r)" || fail "running-app probe exited $r"
r=$(rc -NoProfile -ExecutionPolicy Bypass -File 'C:\setup.ps1')
[ "$r" = 1 ] && pass "-File with -ExecutionPolicy's value skipped: fails ($r)" || fail "-File exited $r"
r=$(rc "Get-ChildItem")
[ "$r" = 1 ] && pass "a positional command fails ($r)" || fail "positional command exited $r"
r=$(rc -Command "exit 3")
[ "$r" = 3 ] && pass "a plain 'exit 3' is honoured ($r)" || fail "'exit 3' exited $r"
r=$(rc -NoLogo)
[ "$r" = 0 ] && pass "no script at all still exits 0 ($r)" || fail "no script exited $r"
r=$(echo exit | "$WINE" "$PS" -Command - > /dev/null 2>&1; echo $?)
[ "$r" = 0 ] && pass "-Command - reading 'exit' from stdin exits 0 ($r)" || fail "-Command - exited $r"

[ $RC = 0 ] && echo "powershell-gate: all passed" || echo "powershell-gate: FAILURES"
exit $RC
