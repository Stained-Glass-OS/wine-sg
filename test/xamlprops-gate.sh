#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Windows.UI.Xaml property system (patches/sg/6131): DependencyProperty.Register/RegisterAttached,
# PropertyMetadata (default value, default factory, change callback), a DependencyObject's values
# (GetValue, SetValue, ClearValue, ReadLocalValue, UnsetValue, null as a value) and change
# callbacks, and a DependencyObject derived by an application. There was none: every XAML element
# is built on it. test/xamlprops-probe.c.
#   WINE=/opt/wine-sg/bin/wine test/xamlprops-gate.sh
# Mutant (dlls/windows.ui.xaml/dependency.c): SG_MUTANT_DP_NO_LOCAL_VALUE.
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW missing"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-xamlprops.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/xamlprops-probe.exe" "$HERE/xamlprops-probe.c" -lruntimeobject -luuid || { echo "FAIL  the probe did not build"; exit 1; }
mkdir -p "$T/home" "$T/prefix"
export HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
unset DISPLAY WAYLAND_DISPLAY
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
rm -f "$WINEPREFIX/dosdevices/z:"
cp "$T/xamlprops-probe.exe" "$WINEPREFIX/drive_c/"
out=$(cd "$WINEPREFIX/drive_c" && WINE_SG_XAML=1 timeout -s KILL 120 "$WINE" xamlprops-probe.exe 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
RC=0
chk() { if printf '%s\n' "$out" | grep -qx "$1"; then printf 'PASS  %s\n' "$2"; else printf 'FAIL  %s\n' "$2"; RC=1; fi; }
chk 'unset=1' "DependencyProperty.UnsetValue, one object"
chk 'metadata=00000000' "PropertyMetadata.Create with a default value and a change callback"
chk 'register=00000000' "DependencyProperty.Register"
chk 'getmetadata=1' "the property's metadata"
chk 'object=00000000' "a DependencyObject"
chk 'default=42' "an unset property's value is its metadata's default"
chk 'local_unset=1' "and it has no local value (UnsetValue)"
chk 'set=00000000 value=7 local=7' "SetValue: the value and the local value"
chk 'changed=1 old=42 new=7 dp=1 sender=1' "the change callback gets the object, the property, old and new value"
chk 'same_value_quiet=1' "an equal value is not a change"
chk 'registered=00000000 calls=1 dp=1 sender=1' "RegisterPropertyChangedCallback: called on a change"
chk 'unregistered=1' "not after UnregisterPropertyChangedCallback"
chk 'cleared=42 unset=1 changed=1 old=9 new=42' "ClearValue: back to the default, a change"
chk 'set_unset_clears=1' "setting UnsetValue clears"
chk 'null_is_a_value=1' "null is a value"
chk 'made_default=101,101,102 calls=2' "a default from a factory, made once per object"
chk 'attached_default_null=1' "RegisterAttached: no metadata, no default"
chk 'attached=3 other=-1000' "an attached property set on one object"
chk 'aggregated=1' "a DependencyObject derived by an application (aggregated)"
chk 'aggregated_value=11 sender_outer=1' "its values; callbacks get the application's object"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
