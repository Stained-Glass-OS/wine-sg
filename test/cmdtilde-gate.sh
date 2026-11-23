#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# cmd's ~s shortens every name of a path that exists, the last one too (patches/sg/8418).
# %~fsi of "C:\Program Files\QGIS 3.44.15" gave "C:\PROG~FBU\QGIS 3.44.15": the last name stayed
# long and kept its space. OSGeo4W's o4w_env.bat sets its root that way and then runs
#   for %%f in ("%OSGEO4W_ROOT%\etc\ini\*.bat") do call "%%f"
# which, with a space in the root, called nothing: QGIS 3.44 started without Qt on its PATH
# ("Could not load qgis_app.dll").
#   WINE=/opt/wine-sg/bin/wine test/cmdtilde-gate.sh
# Mutant: SG_MUTANT_TILDE_S_LONG_NAME (programs/cmd/batch.c).
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
T=$(mktemp -d /var/tmp/sg-cmdtilde.XXXXXX)
trap 'WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINESERVER WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d"
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
C="$WINEPREFIX/drive_c"
mkdir -p "$C/Program Files/Gate App 3.44.15/etc/ini" "$C/Program Files/Gate App 3.44.15/bin"
printf 'set GATE_INI=called\r\n' > "$C/Program Files/Gate App 3.44.15/etc/ini/gate.bat"
printf '@echo off\r\npushd %%~dp0\r\ncd ..\r\nfor %%%%i in ("%%CD%%") do set ROOT=%%%%~fsi\r\nfor %%%%f in ("%%ROOT%%\\etc\\ini\\*.bat") do call "%%%%f"\r\npopd\r\n' \
    > "$C/Program Files/Gate App 3.44.15/bin/env.bat"
cat > "$C/gate.bat" <<'BAT'
@echo off
for %%i in ("C:\Program Files\Gate App 3.44.15") do echo dir_s=[%%~si]
for %%i in ("C:\Program Files\Gate App 3.44.15") do echo dir_fs=[%%~fsi]
for %%i in ("C:\Program Files\Gate App 3.44.15\etc\ini\gate.bat") do echo file_fs=[%%~fsi]
for %%i in ("C:\Program Files\Gate App 3.44.15\no such file.txt") do echo missing_fs=[%%~fsi]
call "C:\Program Files\Gate App 3.44.15\bin\env.bat"
echo ini=[%GATE_INI%] root=[%ROOT%]
BAT
sed -i 's/$/\r/' "$C/gate.bat"
OUT=$(timeout -s KILL 120 "$WINE" cmd /c 'C:\gate.bat' 2>&1 | tr -d '\r')
printf "%s\n" "$OUT" | sed 's/^/      /'
v() { printf "%s\n" "$OUT" | sed -n "s/^$1=\[\(.*\)\]\$/\1/p" | head -1; }
case "$(v dir_s)" in *' '*|'') fail "%~si of a folder kept a long name: $(v dir_s)";; *'~'*) pass "%~si of a folder is all short names ($(v dir_s))";; *) fail "dir_s=$(v dir_s)";; esac
case "$(v dir_fs)" in C:\\*' '*|'') fail "%~fsi of a folder kept a space: $(v dir_fs)";; C:\\*) pass "%~fsi of a folder has no space ($(v dir_fs))";; *) fail "dir_fs=$(v dir_fs)";; esac
case "$(v file_fs)" in *' '*|'') fail "%~fsi of a file: $(v file_fs)";; *'\gate.bat') pass "%~fsi of a file keeps its own short name ($(v file_fs))";; *) fail "file_fs=$(v file_fs)";; esac
case "$(v missing_fs)" in *'\no such file.txt') pass "a name that does not exist stays as written after a short folder ($(v missing_fs))";; *) fail "missing_fs=$(v missing_fs)";; esac
printf "%s\n" "$OUT" | grep -q '^ini=\[called\]' && pass "an OSGeo4W-style env script calls its ini scripts" || fail "the env script called nothing: $(printf "%s\n" "$OUT" | grep '^ini=')"
echo
if [ "$RC" = 0 ]; then echo "RESULT: PASS"; else echo "RESULT: FAIL"; fi
exit "$RC"
