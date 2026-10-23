#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# msxml3: the Schema Object Model (6053) -- IXMLDOMSchemaCollection2::getSchema and getDeclaration
# gave E_NOTIMPL. From script (JScript through IDispatch), over an XSD added to an MSXML 6 schema cache:
#  - the schema: its item type, target namespace, version, global elements, types, attributes;
#  - a simple type: its base type, enumeration, maxLength, isValid (the built-in type's lexical space,
#    the enumeration, the length);
#  - a complex type: its content model (a sequence of three particles), a particle's type, minOccurs,
#    maxOccurs (unbounded -1), item type, scope, namespace; its attributes (use, default); an anonymous
#    complex type's content type;
#  - getDeclaration of an instance's element (through its parent's type) and attribute; an unknown
#    namespace's schema is refused.
#   WINE=/opt/wine-sg/bin/wine test/somschema-gate.sh   (mutants SG_MUTANT_SOM_ITEMTYPE, SG_MUTANT_SOM_NO_TYPE,
#   SG_MUTANT_SOM_NO_PARTICLES, SG_MUTANT_SOM_DECL_GLOBAL_ONLY)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }

[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
T=$(mktemp -d /var/tmp/sg-somschema.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=${GATE_DEBUG:--all} WINEDLLOVERRIDES="mscoree,mshtml=;winemenubuilder.exe=d" DISPLAY=
# shellcheck disable=SC2317  # invoked via trap
cleanup() {
    "$WINESERVER" -k 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT INT TERM
mkdir -p "$WINEPREFIX"
timeout -s KILL 300 "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
C="$WINEPREFIX/drive_c"
cp "$HERE/somschema.js" "$C/"
out=$(cd "$C" && timeout -s KILL 120 "$WINE" cscript //nologo 'C:\somschema.js' 2>/dev/null </dev/null | tr -d '\r')
printf '%s\n' "$out" | sed 's/^/      /'
expect() {   # expect KEY VALUE WHAT
    if printf '%s\n' "$out" | grep -qxF "$1=$2"; then pass "$3"
    else fail "$3: wanted '$1=$2', got '$(printf '%s\n' "$out" | grep "^$1=" || echo none)'"; fi
}
expect xsd_parse 0 "the schema document parses"
expect getschema 1 "getSchema gives the namespace's schema"
expect schema "1000 urn:sg-som 1.2" "the schema: SOMITEM_SCHEMA, its target namespace and version"
expect elements "1 order" "its global elements"
expect types "2 2400" "its types; ItemType is SOMITEM_COMPLEXTYPE"
expect color "2200 string 2 green 5" "a simple type: base string, two enumerations, maxLength 5"
expect valid "true false false" "isValid: an enumerated value, one not enumerated, one too long"
expect model "4103 3" "a complex type's content model is a sequence of three particles"
expect qty "qty int 0 -1 2113" "a particle's name, type, minOccurs, maxOccurs (unbounded) and type item"
expect attrs "2 id 2 none" "the complex type's attributes: required, defaulted"
expect scope "ItemType urn:sg-som" "a local element's scope and (qualified) namespace"
expect anon "2400 2" "an anonymous complex type, element only"
expect global_attr "lang language" "a global attribute of a built-in type"
expect decl "title ItemType string" "getDeclaration finds an instance element's local declaration"
expect adecl "id ID 2" "and an attribute's, through its element's type"
case "$(printf '%s\n' "$out" | grep '^nosuch=')" in nosuch=error*) pass "an unknown namespace's schema is refused" ;; *) fail "unknown namespace: $(printf '%s\n' "$out" | grep '^nosuch=')" ;; esac
expect done 1 "the script ran to the end"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
