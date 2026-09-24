#!/usr/bin/env python3
# Write a small Package Resource Index (resources.pri) of string resources:
# the fixture for test/resources-gate.sh. Needs no Microsoft tool (makepri is
# one), so the gate runs anywhere.
#
#   mkpri.py OUT.pri
#
# The resources it writes, and what the gate expects of them:
#
#   Resources/Greeting      en-US "Hello" / de-DE "Hallo"      (inline strings)
#   Resources/Farewell      neutral "Goodbye"                 (a data item)
#   Resources/Nested/Deep   neutral "Deep value"
#   custom/Title            neutral "Custom map"
#   custom/Utf8             neutral "Grüße" stored as UTF-8
#
# Layout as documented by independent readers of the format (see
# dlls/windows.applicationmodel/pri.c in the patched tree).
#
# SPDX-License-Identifier: LGPL-2.1-or-later
import struct
import sys

STRING, UTF8 = 0, 4
LANGUAGE = 0


def u16(v): return struct.pack('<H', v)
def u32(v): return struct.pack('<I', v)
def utf16z(s): return s.encode('utf-16-le') + b'\0\0'
def pad8(b): return b + b'\0' * (-len(b) % 8)


def section(ident, content):
    content = pad8(content)
    length = 32 + len(content) + 8
    return ident + u32(0) + u16(0) + u16(0) + u32(length) + u32(0) + content + u32(0xdef5fade) + u32(length)


# ---- the tree: scopes and items ------------------------------------------
scopes = ['', 'Resources', 'Nested', 'custom']           # index 0 is the root
scope_parent = [0, 0, 1, 0]
items = [('Greeting', 1), ('Farewell', 1), ('Deep', 2), ('Title', 3), ('Utf8', 3)]


def hschema():
    unique, name = 'ms-appx://sg-test/', 'sg-test'
    # nodes: scopes first, then items; node i of a scope is scope i
    nodes = [('s', i) for i in range(len(scopes))] + [('i', i) for i in range(len(items))]
    names = b''
    offsets = []
    for kind, i in nodes:
        n = scopes[i] if kind == 's' else items[i][0]
        offsets.append(len(names) // 2)
        names += utf16z(n) if n else b''
    unicode_len = len(names) // 2

    def full_len(kind, i):
        if kind == 's':
            if i == 0: return 0
            return len(scopes[i]) + (full_len('s', scope_parent[i]) + 1 if scope_parent[i] else 0)
        return len(items[i][0]) + 1 + full_len('s', items[i][1])

    node_bytes = b''
    for n, (kind, i) in enumerate(nodes):
        parent = scope_parent[i] if kind == 's' else items[i][1]   # scopes are nodes 0..
        label = scopes[i] if kind == 's' else items[i][0]
        flags = 0x10 if kind == 's' else 0
        node_bytes += (u16(parent) + u16(full_len(kind, i)) + u16(ord(label[0].upper()) if label else 0) +
                       bytes([len(label), flags]) + u16(offsets[n]) + u16(i))
    scope_ex = b''
    for s in range(len(scopes)):
        children = [n for n, (k, i) in enumerate(nodes) if n and
                    ((k == 's' and scope_parent[i] == s and i != s) or (k == 'i' and items[i][1] == s))]
        scope_ex += u16(s) + u16(len(children)) + u16(children[0] if children else 0) + u16(0)
    item_order = b''.join(u16(len(scopes) + i) for i in range(len(items)))
    body = node_bytes + scope_ex + item_order + names
    hnames_hdr_len = 2 + 2 + 4 * 5
    total_hnames = hnames_hdr_len + len(body)
    total_hnames += -total_hnames % 8
    hnames = (u16(64) + u16(0) + u32(len(nodes)) + u32(len(scopes)) + u32(len(items)) +
              u32(unicode_len) + u32(total_hnames) + body)
    head = (u16(1) + u16(len(unique) + 1) + u16(len(name) + 1) + u16(0) + b'[def_hnames]   \0' +
            u16(1) + u16(0) + u32(0) + u32(0) + u32(len(scopes)) + u32(len(items)) +
            utf16z(unique) + utf16z(name) + u16(0))
    return head + hnames


# ---- decisions: sets of qualifiers, and which sets each item chooses among --
qualifier_values = ['EN-US', 'DE-DE']
# qualifier sets: 0 neutral, 1 en-US, 2 de-DE
sets = [[], [0], [1]]
# decisions: 0 = {en-US, de-DE}, 1 = {neutral}
decisions = [[1, 2], [0]]


def decn_info():
    data = b''
    value_offs = []
    for v in qualifier_values:
        value_offs.append(len(data) // 2)
        data += utf16z(v)
    index = []
    set_ranges = []
    for s in sets:
        set_ranges.append((len(index), len(s)))
        index += s
    dec_ranges = []
    for d in decisions:
        dec_ranges.append((len(index), len(d)))
        index += d
    out = (u16(len(qualifier_values)) + u16(len(qualifier_values)) + u16(len(sets)) + u16(len(decisions)) +
           u16(len(index)) + u16(len(data) // 2))
    out += b''.join(u16(f) + u16(c) for f, c in dec_ranges)
    out += b''.join(u16(f) + u16(c) for f, c in set_ranges)
    # qualifiers: distinct index, priority, fallback score (x1000); en-US is
    # the file's default language, so it carries the fallback score
    out += u16(0) + u16(700) + u16(1000) + u16(0)
    out += u16(1) + u16(700) + u16(0) + u16(0)
    for i in range(len(qualifier_values)):
        out += u16(0) + u16(LANGUAGE) + u16(0) + u16(0) + u32(value_offs[i])
    out += b''.join(u16(i) for i in index)
    return out + data


# ---- the values ----------------------------------------------------------
# per item: (decision, [(value type, bytes, inline?)] in the decision's set order)
values = [
    (0, [(STRING, utf16z('Hello'), True), (STRING, utf16z('Hallo'), True)]),
    (1, [(STRING, utf16z('Goodbye'), False)]),
    (1, [(STRING, utf16z('Deep value'), True)]),
    (1, [(STRING, utf16z('Custom map'), True)]),
    (1, [(UTF8, 'Grüße'.encode('utf-8') + b'\0', True)]),
]
SEC_DESC, SEC_SCHEMA, SEC_DECN, SEC_MAP, SEC_DATA = range(5)


def res_map():
    types = [STRING, UTF8]
    strings = b''
    data_items = []
    cands = b''
    infos = b''
    first = 0
    for item, (dec, cs) in enumerate(values):
        infos += u16(dec) + u16(first)
        for vt, b, inline in cs:
            if inline:
                cands += bytes([0, types.index(vt)]) + u16(len(b)) + u32(len(strings))
                strings += b
            else:
                cands += bytes([1, types.index(vt)]) + u16(0) + u16(len(data_items)) + u16(SEC_DATA)
                data_items.append(b)
            first += 1
    i2g = b''.join(u16(i) + u16(i) for i in range(len(values)))   # group >= count: one item per info
    out = (u16(0) + u16(0) + u16(SEC_SCHEMA) + u16(0) + u16(SEC_DECN) + u16(len(types)) +
           u16(len(values)) + u16(0) + u32(len(values)) + u32(first) + u32(len(strings)) + u32(0))
    out += b''.join(u32(4) + u32(t) for t in types)
    return out + i2g + infos + cands + strings, data_items


def data_item(items):
    table = b''
    data = b''
    for b in items:
        table += u16(len(data)) + u16(len(b))
        data += b
    return u32(0) + u16(len(items)) + u16(0) + u32(len(data)) + table + data


def pri_descriptor():
    return (u16(1) + u16(0xffff) + u16(0) + u16(1) + u16(1) + u16(1) + u16(SEC_MAP) + u16(0) + u16(1) + u16(0) +
            u16(SEC_SCHEMA) + u16(SEC_DECN) + u16(SEC_MAP) + u16(SEC_DATA))


def main():
    map_content, items_data = res_map()
    secs = [
        (b'[mrm_pridescex]\0', pri_descriptor()),
        (b'[mrm_hschemaex] ', hschema()),
        (b'[mrm_decn_info]\0', decn_info()),
        (b'[mrm_res_map2_]\0', map_content),
        (b'[mrm_dataitem] \0', data_item(items_data)),
    ]
    blobs = [section(i, c) for i, c in secs]
    toc_off = 32
    start = toc_off + 32 * len(blobs)
    toc = b''
    off = 0
    for (ident, _), b in zip(secs, blobs):
        toc += ident + u16(0) + u16(0) + u32(0) + u32(off) + u32(len(b))
        off += len(b)
    total = start + off + 16
    head = b'mrm_pri2' + u16(0) + u16(1) + u32(total) + u32(toc_off) + u32(start) + u16(len(blobs)) + u16(0xffff) + u32(0)
    out = head + toc + b''.join(blobs) + u32(0xdefffade) + u32(total) + b'mrm_pri2'
    assert len(out) == total
    with open(sys.argv[1], 'wb') as f:
        f.write(out)


if __name__ == '__main__':
    main()
