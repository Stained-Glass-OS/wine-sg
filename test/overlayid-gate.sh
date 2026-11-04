#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Icon overlay handlers (patch 6245): registered under ShellIconOverlayIdentifiers in sorted order, at
# most 15; an item's overlay from the first handler by priority that marks it; SHGetIconOverlayIndex;
# the file view's items carry the overlay.  test/overlayid-dll.c (the handlers), test/overlayid-probe.c.
# Mutants (dlls/shell32): SG_MUTANT_NO_OVERLAY_HANDLERS (iconcache.c), SG_MUTANT_NO_FOLDER_OVERLAY
# (shfldr_fs.c), SG_MUTANT_VIEW_NO_OVERLAYS (shlview.c).
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
"$MINGW" -O2 -o "$T/probe.exe" "$HERE/overlayid-probe.c" -lshell32 -lole32 -luuid -lcomctl32 -luser32 \
    || { echo "FAIL  probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1; "$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$T/overlayid.dll" "$T/probe.exe" "$C/"
mkdir -p "$C/ovtest"; : > "$C/ovtest/x.ova"; : > "$C/ovtest/y.ovb"; : > "$C/ovtest/z.txt"
{
  printf 'Windows Registry Editor Version 5.00\n\n'
  for c in A B F; do
    case $c in A) g=0A;; B) g=0B;; F) g=0F;; esac
    printf '[HKEY_LOCAL_MACHINE\\Software\\Classes\\CLSID\\{7B1D0E11-2B6D-4C11-9A01-0E57A1100E%s}\\InprocServer32]\n@="C:\\\\overlayid.dll"\n"ThreadingModel"="Apartment"\n\n' "$g"
  done
  K='HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Explorer\ShellIconOverlayIdentifiers'
  printf '[%s\\    OvA]\n@="{7B1D0E11-2B6D-4C11-9A01-0E57A1100E0A}"\n\n' "$K"
  for n in 01 02 03 04 05 06 07 08 09 10 11 12 13 14; do
    printf '[%s\\   Fill%s]\n@="{7B1D0E11-2B6D-4C11-9A01-0E57A1100E0F}"\n\n' "$K" "$n"
  done
  printf '[%s\\OvB]\n@="{7B1D0E11-2B6D-4C11-9A01-0E57A1100E0B}"\n\n' "$K"
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
