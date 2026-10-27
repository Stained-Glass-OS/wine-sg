#!/usr/bin/env python3
# mkxbf.py DIR: writes compiled XAML (XBF v2) fixtures for test/xamlxbf-gate.sh -- ours, laid out per
# the public description of the format in XbfAnalyzer (Apache-2.0): a good page and broken ones.
#   TestGrid.xbf    Grid (Background #FFFF0000, RowDefinitions 40/*) with a Border (Grid.Row 1,
#                   Width 100, Height 50, HorizontalAlignment Left, VerticalAlignment Top, Margin
#                   10,20,0,0, Background #FF0000FF, x:Name Box) and a TextBlock (Text, FontSize 40)
#   TestPage.xbf    Page (Background) whose content is a TextBlock with x:ConnectionId 2
#   Trunc.xbf       TestGrid cut in the middle of its nodes
#   BadString.xbf   a string index past the string table
#   BadSection.xbf  a node section reaching past the end of the file
#   BadNode.xbf     an unknown node
import struct, sys, os

def u8(v): return struct.pack('<B', v)
def u16(v): return struct.pack('<H', v)
def u32(v): return struct.pack('<I', v)
def f32(v): return struct.pack('<f', v)
def s16(s): return u32(len(s)) + s.encode('utf-16-le')

PRESENTATION = 'http://schemas.microsoft.com/winfx/2006/xaml/presentation'

# framework type and property IDs (xbf_ids.txt), with the framework bit
T_GRID, T_BORDER, T_TEXTBLOCK, T_PAGE, T_ROWDEF = 0x81BA, 0x811E, 0x818C, 0x820D, 0x806A
P_PANEL_BACKGROUND, P_BORDER_BACKGROUND, P_WIDTH, P_HEIGHT, P_MARGIN = 0x8287, 0x81F7, 0x8199, 0x8189, 0x818D
P_HALIGN, P_VALIGN, P_ROW, P_TEXT, P_FONTSIZE, P_CONTROL_BACKGROUND = 0x818A, 0x8198, 0x835C, 0x8320, 0x8310, 0x821D
P_ROWDEFS, P_ROWDEF_HEIGHT = 0x835D, 0x80E3

def v_color(argb): return u8(0x08) + bytes([argb & 0xff, (argb >> 8) & 0xff, (argb >> 16) & 0xff, argb >> 24])
def v_float(f): return u8(0x03) + f32(f)
def v_int(i): return u8(0x04) + u32(i)
def v_str(index): return u8(0x05) + u16(index)
def v_thickness(l, t, r, b): return u8(0x06) + f32(l) + f32(t) + f32(r) + f32(b)
def v_gridlength(unit, v): return u8(0x07) + u32(unit) + f32(v)
def v_enum(eid, v): return u8(0x0B) + u16(eid) + u32(v)

def prop(pid, value): return u8(0x1A) + u16(pid) + value

def xbf(strings, nodes, sections=None):
    out = b'XBF\0' + u32(0) + u32(0) + u32(2) + u32(1) + b'\0' * 48 + b'\0' * 64
    out += u32(len(strings)) + b''.join(s16(s) + u16(0) for s in strings)
    out += u32(0)                      # assemblies
    out += u32(0)                      # type namespaces
    out += u32(0)                      # types
    out += u32(0)                      # properties
    out += u32(1) + u32(0)             # XML namespaces: strings[0]
    if sections is None: sections = [(0, len(nodes))]
    out += u32(len(sections)) + b''.join(u32(a) + u32(b) for a, b in sections)
    return out + nodes

def grid():
    strings = [PRESENTATION, 'Box', 'XXXX']
    n = u8(0x12) + u16(0) + s16('')
    n += u8(0x17) + u16(T_GRID)
    n += prop(P_PANEL_BACKGROUND, v_color(0xFFFF0000))
    n += u8(0x13) + u16(P_ROWDEFS)
    n += u8(0x14) + u16(T_ROWDEF) + prop(P_ROWDEF_HEIGHT, v_gridlength(1, 40)) + u8(0x21) + u8(0x08)
    n += u8(0x14) + u16(T_ROWDEF) + prop(P_ROWDEF_HEIGHT, v_gridlength(2, 1)) + u8(0x21) + u8(0x08)
    n += u8(0x02)
    n += u8(0x14) + u16(T_BORDER)
    n += prop(P_ROW, v_int(1)) + prop(P_WIDTH, v_float(100)) + prop(P_HEIGHT, v_float(50))
    n += prop(P_HALIGN, v_enum(0x0263, 0)) + prop(P_VALIGN, v_enum(0x0264, 0))
    n += prop(P_MARGIN, v_thickness(10, 20, 0, 0)) + prop(P_BORDER_BACKGROUND, v_color(0xFF0000FF))
    n += u8(0x0D) + v_str(1)
    n += u8(0x21) + u8(0x08)
    n += u8(0x14) + u16(T_TEXTBLOCK) + prop(P_TEXT, v_str(2)) + prop(P_FONTSIZE, v_float(40)) + prop(P_ROW, v_int(1))
    n += prop(P_HALIGN, v_enum(0x0263, 0)) + prop(P_VALIGN, v_enum(0x0264, 0)) + prop(P_MARGIN, v_thickness(150, 100, 0, 0))
    n += u8(0x21) + u8(0x08)
    n += u8(0x21)
    return strings, n

def page():
    strings = [PRESENTATION, 'Hi']
    n = u8(0x12) + u16(0) + s16('')
    n += u8(0x17) + u16(T_PAGE) + prop(P_CONTROL_BACKGROUND, v_color(0xFF008000))
    n += u8(0x14) + u16(T_TEXTBLOCK) + prop(P_TEXT, v_str(1)) + u8(0x0C) + v_int(2) + u8(0x21) + u8(0x08)
    n += u8(0x21)
    return strings, n

d = sys.argv[1]
def write(name, data):
    with open(os.path.join(d, name), 'wb') as f: f.write(data)

s, n = grid()
good = xbf(s, n)
write('TestGrid.xbf', good)
s2, n2 = page()
write('TestPage.xbf', xbf(s2, n2))
write('Trunc.xbf', good[:len(good) - len(n) // 2])
write('BadString.xbf', xbf(s, n.replace(u8(0x0D) + v_str(1), u8(0x0D) + v_str(999))))
write('BadSection.xbf', xbf(s, n, [(0, len(n) + 4096)]))
write('BadNode.xbf', xbf(s, n.replace(u8(0x21) + u8(0x08), u8(0x7E) + u8(0x08), 1)))
