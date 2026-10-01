#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# The desktop keeps winspool loaded, so the system's printers are loaded once
# per session (patches/sg/0609). winspool loads them when it is first loaded
# and marks that with a named mutex, which lives only while some process holds
# it; short programs that link winspool (GIMP's 130 plug-ins, through GTK)
# each asked CUPS again, a second apiece, and GIMP's first start took minutes.
# With the shell desktop up, the mutex must exist.
#
#   WINE=/opt/wine-sg/bin/wine test/spooler-gate.sh
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
unset DISPLAY WAYLAND_DISPLAY

T=$(mktemp -d /var/tmp/sg-spooler.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
trap '"$WINESERVER" -k 2>/dev/null; [ -n "${KEEP:-}" ] || rm -rf "$T"' EXIT INT TERM
cat > "$T/mutex.c" <<'EOF'
#include <windows.h>
#include <stdio.h>
int main(void)
{
    HANDLE m = OpenMutexW( SYNCHRONIZE, FALSE, L"__WINE_WINSPOOL_MUTEX__" );
    printf( "held=%d\n", m != NULL );
    return 0;
}
EOF
"$MINGW" -O2 -o "$T/mutex.exe" "$T/mutex.c" || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cp "$T/mutex.exe" "$WINEPREFIX/drive_c/"
cat > "$T/session.sh" <<EOF
#!/bin/sh
cd "$WINEPREFIX/drive_c"
"$WINE" explorer /desktop=shell,800x600 > "$T/explorer.out" 2>&1 &
i=0; while [ \$i -lt 60 ]; do
    "$WINE" 'C:\mutex.exe' 2>/dev/null | tr -d '\r' > "$T/state.out"
    grep -q '^held=1' "$T/state.out" && break
    sleep 1; i=\$((i + 1))
done
EOF
chmod +x "$T/session.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 800x600x24' "$T/session.sh"
grep -q '^held=1' "$T/state.out" && pass "the desktop holds winspool: printers load once a session" ||
    fail "with the desktop up no one holds winspool's mutex: every program asks CUPS again"
exit $RC
