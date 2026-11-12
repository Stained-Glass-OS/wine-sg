#!/bin/sh
# gen.sh OBJ MONO_SRC OUT.idl: regenerate patch 7743's dlls/mscorlib.tlb/mscorlib_tlb.idl.
#   OBJ:      a built Wine object tree (its wine runs Wine Mono's mcs and the generator)
#   MONO_SRC: a Wine Mono source tree (for the MIT reference assembly source of mscorlib)
# The type list and COM attributes come from the reference source; members and computed GUIDs come
# from Wine Mono's corlib by reflection. Runs in a scratch prefix and HOME, never the real ones.
set -e
OBJ=$(realpath "$1"); MONO=$(realpath "$2"); OUT=$(realpath "$3")
HERE=$(dirname "$(realpath "$0")")
W=$(mktemp -d /var/tmp/mscorlib-tlb.XXXXXX)
export HOME=$W/home WINEPREFIX=$W/pfx WINEDEBUG=-all WINEDLLOVERRIDES=winemenubuilder.exe=d
unset DISPLAY WAYLAND_DISPLAY
mkdir -p $HOME
python3 "$HERE/extract-guids.py" "$MONO/mono/external/binary-reference-assemblies/src/v4.7.2/mscorlib.cs" > $W/guids.txt
cp "$HERE/tlbgen.cs" $W/
$OBJ/wine wineboot -i >/dev/null 2>&1; $OBJ/server/wineserver -w
MCS=$(ls $WINEPREFIX/drive_c/windows/mono/mono-2.0/lib/mono/4.5/mcs.exe /usr/share/wine/mono/wine-mono-*/lib/mono/4.5/mcs.exe 2>/dev/null | head -1)
cd $W
$OBJ/wine "$MCS" -nologo -out:tlbgen.exe tlbgen.cs
$OBJ/wine tlbgen.exe guids.txt > "$OUT"
$OBJ/server/wineserver -k || true
rm -rf $W
