#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# Direct2D SVG documents (patches/sg/0532), under Xvfb: test/d2dsvg-probe.c
# builds and draws SVG documents on a WIC bitmap render target and reads the
# pixels back.  CreateSvgDocument and DrawSvgDocument were stubs (E_NOTIMPL),
# and Office's sign-in pane ends in a fail-fast when an empty document cannot
# be created.
#
#  - an empty document (no stream) has an svg root; tree operations;
#  - string and typed attributes, live attribute objects (ID2D1SvgPaint,
#    ID2D1SvgPathData) in Windows' vtable order;
#  - documents from XML, FindElementById, Serialize and Deserialize;
#  - drawing: rect, circle, path, viewBox scaling, group transform, stroke,
#    fill="none", use, defs, display="none", a linear gradient.
#
#   WINE=/opt/wine-sg/bin/wine test/d2dsvg-gate.sh
#   WINESERVER=... when it is not beside $WINE (a build tree)
set -u
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
DPY="${D2DSVG_DPY:-178}"
RC=0; XP=""
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

[ "$DPY" = 0 ] && { echo "refusing display :0"; exit 2; }
for need in Xvfb xdpyinfo "$MINGW"; do command -v "$need" >/dev/null || { echo "SKIP: $need missing"; exit 77; }; done
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }

T=$(mktemp -d /var/tmp/sg-d2dsvg.XXXXXX)
cleanup() {
    WINEPREFIX="$T/prefix" "$WINESERVER" -k 2>/dev/null
    [ -n "$XP" ] && kill "$XP" 2>/dev/null
    rm -f "/tmp/.X${DPY}-lock"; rm -rf "$T"
}
trap cleanup EXIT INT TERM

TMPDIR=/var/tmp "$MINGW" -O2 -o "$T/d2dsvg-probe.exe" "$HERE/d2dsvg-probe.c" \
    -ld2d1 -lwindowscodecs -lole32 -lshlwapi -luuid -ldxguid || { fail "probe did not build"; exit 1; }

Xvfb ":$DPY" -screen 0 1024x768x24 -nolisten tcp >/dev/null 2>&1 & XP=$!
mkdir -p "$T/home" "$T/prefix"
export DISPLAY=":$DPY" HOME="$T/home" WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" WINESERVER
i=0; while ! xdpyinfo >/dev/null 2>&1 && [ $i -lt 20 ]; do sleep 0.25; i=$((i + 1)); done
DISPLAY= timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w

out=$(timeout -s KILL 120 "$WINE" "$T/d2dsvg-probe.exe" 2>/dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
check() {
    if printf '%s\n' "$out" | grep -Eq "^$1=1( |$)"; then pass "$2"
    else fail "$2 ($(printf '%s\n' "$out" | grep "^$1=" || echo none))"; fi
}
printf '%s\n' "$out" | grep -q '^no_' && { fail "no Direct2D target: $out"; exit 1; }
check empty_doc              "CreateSvgDocument without a stream makes a document"
check viewport               "GetViewportSize gives the size it was made with"
check empty_root_svg         "the empty document's root is an svg element"
check empty_root_no_children "with no children"
check iid_element            "an element answers IID_ID2D1SvgElement"
check create_child           "CreateChild makes a child element"
check set_float              "SetAttributeValue FLOAT"
check set_color              "SetAttributeValue COLOR"
check set_pod_bad_size       "a typed value of the wrong size is E_INVALIDARG"
check color_as_text          "a COLOR reads back as SVG text (#FF0000)"
check text_as_float          "SVG text reads back as a FLOAT"
check attr_length            "GetAttributeValueLength"
check is_specified           "IsAttributeSpecified"
check specified_count        "GetSpecifiedAttributeCount"
check parent                 "GetParent"
check draw_rect_inside       "DrawSvgDocument draws a built rect"
check draw_rect_outside      "and only there"
check get_paint              "GetAttributeValue gives an ID2D1SvgPaint for fill"
check paint_element          "the paint knows its element"
check paint_live_text        "changing the paint changes the attribute's text"
check paint_live_draw        "and what is drawn"
check set_paint_object       "a paint made by the document set on an element is drawn"
check next_child             "GetNextChild"
check remove_child           "RemoveChild"
check append_child           "AppendChild"
check serialize              "Serialize writes the tree as SVG"
check deserialize            "Deserialize reads it back"
check xml_doc                "CreateSvgDocument from XML"
check xml_circle             "a circle, scaled by the viewBox"
check xml_circle_outside     "and not outside it"
check xml_path_in_group      "a path in a translated group"
check xml_path_outside       "and not outside it"
check xml_stroke             "a stroke"
check xml_fill_none          "fill=none leaves the inside"
check xml_use                "use draws the referenced element at its x,y"
check xml_defs_not_drawn     "definitions are not drawn"
check xml_gradient           "a linear gradient fill goes from red to blue"
check find_by_id             "FindElementById"
check path_data              "path data parses into commands"
check path_geometry          "and makes the geometry"
check path_data_text         "path data set as an object reads back as text"
check bad_xml_fails          "malformed XML fails and gives no document"
check done                       "the probe ran to the end"
[ $RC = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit $RC
