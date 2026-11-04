#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Icon overlay handlers (patch 6245): registered under ShellIconOverlayIdentifiers in sorted order, at
# most 15; an item's overlay from the first handler by priority that marks it; SHGetIconOverlayIndex;
# the file view's items carry the overlay.  6248: a handler that cannot be made (its DLL missing) takes
# no slot, so the one after it is the 15th; COM gives the loader's error (0x8007007e missing,
# 0x800700c1 a 32-bit DLL in a 64-bit process), not E_ACCESSDENIED.  6251: the handlers are asked on a
# worker thread; a handler that takes 5 s ("Slow", *.ovs) leaves the view answering, and the item gets
# its overlay when the answer comes (OneDrive's IsMemberOf blocked Explorer's window thread, qa4 637).
# test/overlayid-dll.c (the handlers), test/overlayid-probe.c.
# Mutants (dlls/shell32): SG_MUTANT_NO_OVERLAY_HANDLERS, SG_MUTANT_FAILED_SLOT (iconcache.c),
# SG_MUTANT_OVERLAY_ON_CALLER (iconcache.c), SG_MUTANT_NO_FOLDER_OVERLAY (shfldr_fs.c),
# SG_MUTANT_VIEW_NO_OVERLAYS, SG_MUTANT_VIEW_OVERLAY_SYNC (shlview.c);
# (dlls/combase) SG_MUTANT_LOAD_ACCESSDENIED (apartment.c).
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: mingw-w64 not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY
T=$(mktemp -d /var/tmp/sg-overlayid.XXXXXX)
export WINEPREFIX="$T/pfx" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
printf 'LIBRARY overlayid.dll\nEXPORTS\n    DllGetClassObject PRIVATE\n    DllCanUnloadNow PRIVATE\n    last_path\n' > "$T/h.def"
"$MINGW" -O2 -municode -shared -o "$T/overlayid.dll" "$HERE/overlayid-dll.c" "$T/h.def" -lole32 -luuid -lshlwapi \
    || { echo "FAIL  handler did not build"; exit 1; }
command -v i686-w64-mingw32-gcc >/dev/null || { echo "SKIP: i686 mingw-w64 not installed"; exit 77; }
i686-w64-mingw32-gcc -O2 -municode -shared -o "$T/overlay32.dll" "$HERE/overlayid-dll.c" "$T/h.def" -lole32 -luuid -lshlwapi \
    || { echo "FAIL  32-bit handler did not build"; exit 1; }
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/overlayid-probe.c" -lshell32 -lole32 -luuid -lcomctl32 -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/overlayid.dll" "$T/overlay32.dll" "$T/probe.exe" "$C/"
mkdir -p "$C/ovtest"; : > "$C/ovtest/x.ova"; : > "$C/ovtest/y.ovb"; : > "$C/ovtest/z.txt"; : > "$C/ovtest/w.ovc"; : > "$C/ovtest/v.ovs"
{
  printf 'Windows Registry Editor Version 5.00\n\n'
  for c in A B C F D E S; do
    d=overlayid.dll
    case $c in A) g=0A;; B) g=0B;; C) g=0C;; S) g=10;; F) g=0F;; D) g=0D; d=overlay32.dll;; E) g=0E; d=missing.dll;; esac
    printf '[HKEY_LOCAL_MACHINE\\Software\\Classes\\CLSID\\{7B1D0E11-2B6D-4C11-9A01-0E57A1100E%s}\\InprocServer32]\n@="C:\\\\%s"\n"ThreadingModel"="Apartment"\n\n' "$g" "$d"
  done
  K='HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Explorer\ShellIconOverlayIdentifiers'
  printf '[%s\\    OvA]\n@="{7B1D0E11-2B6D-4C11-9A01-0E57A1100E0A}"\n\n' "$K"
  for n in 01 02 03 04 05 06 07 08 09 10 11 12; do
    printf '[%s\\   Fill%s]\n@="{7B1D0E11-2B6D-4C11-9A01-0E57A1100E0F}"\n\n' "$K" "$n"
  done
  printf '[%s\\  Broken]\n@="{7B1D0E11-2B6D-4C11-9A01-0E57A1100E0E}"\n\n' "$K"
  printf '[%s\\ Slow]\n@="{7B1D0E11-2B6D-4C11-9A01-0E57A1100E10}"\n\n' "$K"
  printf '[%s\\OvB]\n@="{7B1D0E11-2B6D-4C11-9A01-0E57A1100E0B}"\n\n' "$K"
  printf '[%s\\OvC]\n@="{7B1D0E11-2B6D-4C11-9A01-0E57A1100E0C}"\n\n' "$K"
} > "$T/reg.reg"
"$WINE" regedit /S 'Z:'"$(printf '%s' "$T/reg.reg" | tr '/' '\\')" >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/run.sh" <<EOS
#!/bin/sh
cd "$C"
timeout -s KILL 180 "$WINE" probe.exe 2>/dev/null </dev/null | tr -d '\r' > "$T/probe.out"
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
cat "$T/probe.out"
grep -qx 'RESULT: PASS' "$T/probe.out" && exit 0
exit 1
