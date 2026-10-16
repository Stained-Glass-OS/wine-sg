#!/bin/sh
. "$(dirname "$0")/scratch-home.sh"
# mshtml: DOM nodes and collections (removeNode/swapNode/replaceNode, Node3, CharacterData, attributes).
#
#   WINE=/opt/wine-sg/bin/wine test/fastmshtmldom-gate.sh   (mutants SG_MUTANT_MSHTML_DOM_NOSWAP, SG_MUTANT_ATTRMIRROR)
set -u
HERE=$(CDPATH='' cd -- "$(dirname -- "$0")" && pwd)
WINE="${WINE:-/opt/wine-sg/bin/wine}"
WINESERVER="${WINESERVER:-$(dirname "$WINE")/wineserver}"
[ -x "$WINESERVER" ] || WINESERVER="$(dirname "$WINE")/server/wineserver"
MINGW="${MINGW:-x86_64-w64-mingw32-gcc}"
command -v "$MINGW" >/dev/null || { echo "SKIP: $MINGW not installed"; exit 77; }
command -v xvfb-run >/dev/null || { echo "SKIP: needs xvfb-run"; exit 77; }
[ -x "$WINE" ] || { echo "SKIP: no wine at $WINE"; exit 77; }
RC=0
pass() { printf 'PASS  %s\n' "$*"; }
fail() { printf 'FAIL  %s\n' "$*"; RC=1; }
T=$(mktemp -d /var/tmp/sg-fastmshtmldom.XXXXXX)
export WINEPREFIX="$T/prefix" WINEDEBUG=-all WINEDLLOVERRIDES="winemenubuilder.exe=d" WINESERVER DISPLAY=
trap '"$WINESERVER" -k 2>/dev/null; rm -rf "$T"' EXIT INT TERM
TMPDIR=/var/tmp "$MINGW" -O1 -o "$T/probe.exe" "$HERE/fastmshtmldom-probe.c" -lole32 -loleaut32 -luuid || { fail "probe did not build"; exit 1; }
timeout -s KILL 300 env DISPLAY= "$WINE" wineboot -i >/dev/null 2>&1
"$WINESERVER" -w
cat > "$T/run.sh" <<EOS
#!/bin/sh
timeout -s KILL 120 "$WINE" "$T/probe.exe" >"$T/o" 2>/dev/null </dev/null
EOS
chmod +x "$T/run.sh"
timeout -s KILL 300 xvfb-run -a -s '-screen 0 1024x768x24' "$T/run.sh"
tr -d '\r' < "$T/o" > "$T/o2"; mv "$T/o2" "$T/o"
v() { sed -n "s/^$1=//p" "$T/o" | head -1; }
w() { sed -n "s/^$1 //p" "$T/o" | head -1; }
grep -q "^DONE" "$T/o" || { fail "the probe did not finish"; head -30 "$T/o"; echo "RESULT: FAIL"; exit 1; }
[ "$(v MODE)" = 11 ] || fail "document mode: $(v MODE)"
[ "$(v RM0)" = "<b>1</b><i>2</i>" ] && [ "$(v RM1)" = "[]" ] && pass "removeNode(false) keeps the children in the parent; removeNode(true) takes them along" || fail "removeNode: $(v RM0) $(v RM1)"
[ "$(v SW0)" = '<p id="s3">3</p><p id="s2">2</p><p id="s1">1</p>' ] && [ "$(v SW1)" = '<p id="s3">3</p><p id="s1">1</p><p id="s2">2</p>' ] && pass "swapNode swaps distant and adjacent siblings" || fail "swapNode: $(v SW0) $(v SW1)"
[ "$(v RP0)" = "<em></em> r1" ] && pass "replaceNode puts the new node in place and returns the old" || fail "replaceNode: $(v RP0)"
[ "$(v IB)" = "<u>0</u><s></s>" ] && pass "insertBefore with an undefined reference appends" || fail "insertBefore: $(v IB)"
[ "$(v NV)" = 42 ] && pass "nodeValue accepts a number" || fail "nodeValue: $(v NV)"
[ "$(v EQ)" = "true false false" ] && [ "$(v SAME)" = "true false" ] && pass "isEqualNode and isSameNode" || fail "equal: $(v EQ) $(v SAME)"
[ "$(v LOCAL)" = "div null" ] && [ "$(v NSLOOKUP)" = "object boolean object" ] && pass "localName, prefix, lookupNamespaceURI/lookupPrefix/isDefaultNamespace answer" || fail "node3: $(v LOCAL) $(v NSLOOKUP)"
[ "$(v HASATTR)" = "true false" ] && [ "$(v NORM0)" = 2 ] && [ "$(v NORM1)" = "1 ab" ] && pass "hasAttributes; normalize merges text nodes" || fail "hasAttributes/normalize: $(v HASATTR) $(v NORM0) $(v NORM1)"
[ "$(v CD0)" = heXXllo ] && [ "$(v CD1)" = hXllo ] && [ "$(v CD2)" = "JXllo 5 Xl" ] && [ "$(v CD3)" = "JXllo!" ] && pass "CharacterData insertData, deleteData, replaceData, substringData, length" || fail "chardata: $(grep '^CD' "$T/o" | tr '\n' ' ')"
[ "$(v SAN)" = v ] && [ "$(v SAN2)" = "5 string" ] && [ "$(v SAN3)" = "later later" ] && [ "$(v SNI)" = baz ] && pass "document mode 11: setAttributeNode and setNamedItem make getAttribute return the value, also after the node's value changes" || fail "setAttributeNode: $(grep '^SAN\|^SNI' "$T/o" | tr '\n' ' ')"
[ "$(v SAN4 | cut -d' ' -f1-2)" = "one two" ] && pass "setAttributeNode over an attribute made by setAttribute returns that attribute" || fail "replace: $(v SAN4)"
[ "$(v RNI)" = "baz null" ] && [ "$(v RAN)" = "parentElement= false" ] && pass "attributes.removeNamedItem and removeAttributeNode return the detached attribute" || fail "remove attr: $(v RNI) $(v RAN)"
[ "$(v ATTR)" = "2 null null false" ] && pass "an attribute node: nodeType 2, no siblings" || fail "attr: $(v ATTR)"
[ "$(w ISDIS_DEEP)" = "00000000 -1" ] && [ "$(w ISDIS_E1)" = "00000000 0" ] && [ "$(w DIS_DEEP)" = 0 ] && pass "isDisabled looks at the ancestors, disabled does not" || fail "isDisabled: $(grep ISDIS "$T/o" | tr '\n' ' ')"
[ "$(w CANHTML_DIV)" = "00000000 -1" ] && [ "$(w CANHTML_BR)" = "00000000 0" ] && pass "canHaveHTML: div yes, br no" || fail "canHaveHTML"
[ "$(w HIDEFOCUS)" = "00000000 -1 0" ] && pass "hideFocus reflects" || fail "hideFocus: $(w HIDEFOCUS)"
[ "$(w RSV)" = "00000000 4" ] && [ "$(w PTE)" = "00000000 BODY" ] && pass "readyStateValue (complete = 4); parentTextEdit finds BODY" || fail "rsv/pte: $(w RSV) $(w PTE)"
[ "$(w CLONE_NODES)" = "00000000 2 2" ] && [ "$(w CLONE_ATTRS)" = "00000000 1" ] && pass "enumerators of childNodes and attributes clone with their position" || fail "clone: $(w CLONE_NODES) $(w CLONE_ATTRS)"
[ "$(v SS0)" = "Sty text/css false screen st1 2" ] && [ "$(v SS1)" = "p.a div.b" ] && [ "$(v SS2)" = true ] && pass "style sheet: title, type, disabled, media, owner, rules' selectorText" || fail "stylesheet: $(grep '^SS[0-4]=' "$T/o" | tr '\n' ' ')"
[ "$(v SS3)" = "1 div.b" ] && [ "$(v SS4)" = 0 ] && pass "deleteRule and removeRule remove rules" || fail "delete rules: $(v SS3) $(v SS4)"
[ "$(w SSID)" = "00000000 9 st1" ] && [ "$(w CLONE_SHEETS)" = 00000000 ] && pass "styleSheets.item by id; its enumerator clones" || fail "sheets: $(w SSID) $(w CLONE_SHEETS)"
[ "$(v STY0)" = "12px 7px 12 7" ] && pass "style pixelRight/pixelBottom set and read the right/bottom properties" || fail "pixelRight: $(v STY0)"
[ "$(v STY2)" = "3 right important," ] && pass "style length, item and getPropertyPriority" || fail "style decl: $(v STY2)"
[ "$(v ANC0)" = "http://b.example:8080/p/q?x=1#h" ] && [ "$(v ANC1)" = "http://b.example:99/p/q?x=1#h" ] && [ "$(v ANC2)" = "http://b.example:99/z?x=1#h" ] && [ "$(v ANC3)" = "https://b.example:99/z?x=1#h" ] && [ "$(v ANC4)" = "https://b.example:99/z?x=1#k" ] \
    && pass "anchor hostname, port, pathname, protocol and hash setters change the href" || fail "anchor: $(grep '^ANC' "$T/o" | tr '\n' ' ')"
[ "$(v ARE0)" = "a.example 81 http: /x/y" ] && [ "$(v ARE1)" = "c.example:81 #q" ] && pass "area host parts read and set through its href" || fail "area: $(v ARE0) / $(v ARE1)"
[ "$(w INSEL)" = "00000000 00000000" ] && [ "$(w INRS)" = "00000000 complete" ] && [ "$(w IMGRS)" = "00000000 complete" ] && [ "$(w LNKRS)" = "00000000 complete" ] && pass "onselect of input; readyState of input, img and link" || fail "ready states: $(grep 'INSEL\|RS ' "$T/o" | tr '\n' ' ')"
[ "$(w CLONE_FORM)" = "00000000 1" ] && [ "$(w CLONE_SELECT)" = "00000000 1" ] && pass "enumerators of a form and a select clone with their position" || fail "form/select clone: $(w CLONE_FORM) $(w CLONE_SELECT)"
[ "$RC" = 0 ] && echo "RESULT: PASS" || echo "RESULT: FAIL"
exit "$RC"
