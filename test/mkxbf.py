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
#   TestStyles.xbf  Grid.Resources compiled (a dictionary section): a brush "Accent", a Style "BoxStyle"
#                   for Border (setters: Width/Height inline, Background from {StaticResource Accent}),
#                   an implicit Style for TextBlock (FontSize 40); a Border with Style="{StaticResource
#                   BoxStyle}" (x:Name Styled), a Border with Background="{StaticResource Accent}", one
#                   from a missing resource, a TextBlock (x:Name Label), a Button whose ControlTemplate (a
#                   template section) is a Border x:Name Frame with {TemplateBinding Background}
#   BadDict.xbf     a dictionary entry past its section
#   BadSetter.xbf   an unknown setter kind
#   BadTemplate.xbf a template's section past the table
#   LoopSection.xbf a deferred element whose section refers to itself
#   AppDict.xbf     a ResourceDictionary root as newer compilers write it (dictionary reference kind 10):
#                   "Plain" (#FFFFFF00), and "Cond" chosen by conditional XAML between #FFFF00FF
#                   (IsApiContractPresent) and #FFFF0000 (IsApiContractNotPresent); its brushes' type
#                   given by XML namespace (type flag 2), as compiled App.xaml files have it
#   TemplConn.xbf   a Page whose Button's compiled ControlTemplate has an x:ConnectionId of its own (2,
#                   its x:Bind scope) and one (5) on its root Border ({TemplateBinding Background}) and members and an object of framework IDs
#                   newer than our table (dropped, not fatal)
#   Deferred.xbf    a Grid with a deferred element (x:Load; section reference kind 9, as newer compilers
#                   write it) left unrealized, and a Border after it (x:Name After)
#   DataList.xbf    a Page with a ListView whose compiled ItemTemplate (a DataTemplate section) is a Border
#                   with x:ConnectionId 7 (a compiled x:Bind data template's root)
#   ThemeDict.xbf   a ResourceDictionary whose ResourceDictionary.ThemeDictionaries (owner named, as
#                   compilers write it) holds a "Default" dictionary (keyed item, node 0x0A) with
#                   "ThemeBrush" (#FF00FFFF)
#   CondDict.xbf    a ResourceDictionary with a SolidColorBrush "CondBrush" whose Color is set in two conditional
#                   XAML blocks (nodes 0x26/0x27): #FFFF0000 under IsApiContractNotPresent, #FF00FF00 under
#                   IsApiContractPresent
#   ColorDict.xbf   a ResourceDictionary with a literal Color "DictColor" (#FF0080FF; node 0x15, as
#                   compiled <Color x:Key> entries are) and a SolidColorBrush "DictBrush" whose Color is
#                   {StaticResource DictColor}
#   DeferLoad.xbf   a Grid with an x:Load element as compilers write it: a DeferredElement object (0x82EA)
#                   holding a section reference of kind 9; its section is a Border x:Name Lazy (Background
#                   #FFFF0000); a Border x:Name After follows (realized when found by name, patches/sg/6177)
#   NestedRoot.xbf  a Grid whose first child is written as a root object (node 0x17, as compilers write
#                   objects of other namespaces), x:Name Inner, then a Border x:Name Tail
#   NamedBrush.xbf  a Grid whose Background is a SolidColorBrush with an x:Name (objects without a Name
#                   member are named in markup only), then a Border x:Name Tail2
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
T_SCB, T_STYLE, T_CONTROLTEMPLATE, T_PRESENTER, T_BUTTON = 0x8109, 0x8072, 0x80BF, 0x8128, 0x821C
P_RESOURCES, P_STYLE, P_SCB_COLOR, P_SETTER_VALUE, P_CONTROL_TEMPLATE, P_BORDER_CHILD = 0x8194, 0x8195, 0x81DC, 0x81DB, 0x8230, 0x81FA

def v7(v):
    out = b''
    while True:
        b = v & 0x7f
        v >>= 7
        if v: out += u8(b | 0x80)
        else: return out + u8(b)

def v_color(argb): return u8(0x08) + bytes([argb & 0xff, (argb >> 8) & 0xff, (argb >> 16) & 0xff, argb >> 24])
def v_float(f): return u8(0x03) + f32(f)
def v_int(i): return u8(0x04) + u32(i)
def v_str(index): return u8(0x05) + u16(index)
def v_thickness(l, t, r, b): return u8(0x06) + f32(l) + f32(t) + f32(r) + f32(b)
def v_gridlength(unit, v): return u8(0x07) + u32(unit) + f32(v)
def v_enum(eid, v): return u8(0x0B) + u16(eid) + u32(v)

def prop(pid, value): return u8(0x1A) + u16(pid) + value

def xbf(strings, nodes, sections=None, types=()):
    out = b'XBF\0' + u32(0) + u32(0) + u32(2) + u32(1) + b'\0' * 48 + b'\0' * 64
    out += u32(len(strings)) + b''.join(s16(s) + u16(0) for s in strings)
    out += u32(0)                      # assemblies
    out += u32(0)                      # type namespaces
    out += u32(len(types)) + b''.join(u32(f) + u32(ns) + u32(name) for f, ns, name in types)   # types
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

# strings of the styles file
S_ACCENT, S_BOXSTYLE, S_STYLED, S_LABEL, S_TEXT, S_FRAME, S_NOPE = 1, 2, 3, 4, 5, 6, 7

def styles(bad=None):
    strings = [PRESENTATION, 'Accent', 'BoxStyle', 'Styled', 'Label', 'XXXX', 'Frame', 'Nope']
    left_top = prop(P_HALIGN, v_enum(0x0263, 0)) + prop(P_VALIGN, v_enum(0x0264, 0))
    # section 1: the dictionary's resources
    brush = u8(0x14) + u16(T_SCB) + prop(P_SCB_COLOR, v_color(0xFF00FF00)) + u8(0x21)
    if bad == 'setter': setters = v7(1) + u8(0x55) + u16(P_WIDTH)
    else:
        setters = v7(3) + u8(0x30) + u16(P_WIDTH) + v_float(100) + u8(0x30) + u16(P_HEIGHT) + v_float(50)
        setters += u8(0x11) + u16(P_BORDER_BACKGROUND) + v7(0)
    box_style = u8(0x14) + u16(T_STYLE) + u8(0x1D) + u16(0) + u16(T_BORDER) + u8(0x0F) + v7(3) + u16(0) + v7(2) + setters + u8(0x21)
    implicit = u8(0x14) + u16(T_STYLE) + u8(0x1D) + u16(0) + u16(T_TEXTBLOCK)
    implicit += u8(0x0F) + v7(3) + u16(0) + v7(2) + v7(1) + u8(0x30) + u16(P_FONTSIZE) + v_float(40) + u8(0x21)
    s1 = brush + box_style + implicit
    # section 2: the Button's template
    s2 = u8(0x17) + u16(T_BORDER) + u8(0x1F) + u16(P_BORDER_BACKGROUND) + u16(P_CONTROL_BACKGROUND) + u8(0x0D) + v_str(S_FRAME)
    s2 += u8(0x14) + u16(T_PRESENTER) + u8(0x21) + u8(0x07) + u16(P_BORDER_CHILD) + u8(0x21)
    # section 3: the setters' resource values
    s3 = u8(0x1E) + u16(P_SETTER_VALUE) + v_str(S_ACCENT)
    # section 0: the page
    dict_ref = u8(0x0F) + v7(1) + u16(0) + v7(7)
    style_offset = len(brush) + (4096 if bad == 'dict' else 0)
    dict_ref += v7(2) + u16(S_ACCENT) + v7(0) + u16(S_BOXSTYLE) + v7(style_offset)   # keyed
    dict_ref += v7(0)                                                                 # (keys of styles)
    dict_ref += v7(1) + u16(T_TEXTBLOCK) + v7(len(brush) + len(box_style))           # implicit styles
    dict_ref += v7(0)
    n = u8(0x12) + u16(0) + s16('')
    n += u8(0x17) + u16(T_GRID) + prop(P_PANEL_BACKGROUND, v_color(0xFFFFFFFF))
    n += u8(0x13) + u16(P_RESOURCES) + dict_ref + u8(0x02)
    n += u8(0x14) + u16(T_BORDER) + u8(0x1E) + u16(P_STYLE) + v_str(S_BOXSTYLE) + left_top
    n += prop(P_MARGIN, v_thickness(10, 10, 0, 0)) + u8(0x0D) + v_str(S_STYLED) + u8(0x21) + u8(0x08)
    n += u8(0x14) + u16(T_BORDER) + u8(0x1E) + u16(P_BORDER_BACKGROUND) + v_str(S_ACCENT) + left_top
    n += prop(P_WIDTH, v_float(40)) + prop(P_HEIGHT, v_float(40)) + prop(P_MARGIN, v_thickness(200, 10, 0, 0)) + u8(0x21) + u8(0x08)
    n += u8(0x14) + u16(T_BORDER) + u8(0x1E) + u16(P_BORDER_BACKGROUND) + v_str(S_NOPE) + prop(P_WIDTH, v_float(10)) + u8(0x21) + u8(0x08)
    n += u8(0x14) + u16(T_TEXTBLOCK) + prop(P_TEXT, v_str(S_TEXT)) + left_top + prop(P_MARGIN, v_thickness(150, 100, 0, 0))
    n += u8(0x0D) + v_str(S_LABEL) + u8(0x21) + u8(0x08)
    template_section = 99 if bad == 'template' else 2
    n += u8(0x14) + u16(T_BUTTON) + prop(P_CONTROL_BACKGROUND, v_color(0xFF0000FF)) + left_top
    n += prop(P_WIDTH, v_float(80)) + prop(P_HEIGHT, v_float(40)) + prop(P_MARGIN, v_thickness(10, 120, 0, 0))
    n += u8(0x14) + u16(T_CONTROLTEMPLATE) + u8(0x11) + u16(0) + v7(template_section) + v7(0) + v7(0) + u8(0x21)
    n += u8(0x07) + u16(P_CONTROL_TEMPLATE) + u8(0x21) + u8(0x08)
    n += u8(0x21)
    nodes, sections, pos = b'', [], 0
    for sec in (n, s1, s2, s3):
        sections.append((pos, pos + len(sec)))
        nodes += sec
        pos += len(sec)
    return strings, nodes, sections

def appdict():
    strings = [PRESENTATION, 'Plain', 'Cond', 'SolidColorBrush', 'Windows.Foundation.UniversalApiContract,13']
    types = [(2, 0, 3)]                # SolidColorBrush in XML namespace 0
    def brush(argb): return u8(0x14) + u16(0) + prop(P_SCB_COLOR, v_color(argb)) + u8(0x21)
    plain, present, absent = brush(0xFFFFFF00), brush(0xFFFF00FF), brush(0xFFFF0000)
    s1 = plain + present + absent
    a, b = len(plain), len(plain) + len(present)
    ref = u8(0x0F) + v7(1) + u16(0) + v7(10)
    ref += v7(1) + u16(1) + v7(0)                       # keyed: Plain
    ref += v7(0) + v7(0)                                # (keys of styles), implicit styles
    ref += v7(1) + u16(2) + v7(2) + v7(b) + v7(a)       # Cond: two alternatives
    ref += v7(0)
    ref += v7(2) + v7(a) + v7(1) + u16(0x8369) + u16(4) + v7(b) + v7(1) + u16(0x836a) + u16(4)
    n = u8(0x12) + u16(0) + s16('') + u8(0x17) + u16(0x8173) + ref + u8(0x21)
    return strings, n + s1, [(0, len(n)), (len(n), len(n) + len(s1))], types

def themedict():
    strings = [PRESENTATION, 'ThemeBrush', 'Default']
    brush = u8(0x14) + u16(T_SCB) + prop(P_SCB_COLOR, v_color(0xFF00FFFF)) + u8(0x21)
    ref = u8(0x0F) + v7(1) + u16(0) + v7(7) + v7(1) + u16(1) + v7(0) + v7(0) + v7(0) + v7(0)
    n = u8(0x12) + u16(0) + s16('') + u8(0x17) + u16(0x8173)
    n += u8(0x13) + u16(0x82B1) + u8(0x14) + u16(0x8173) + ref + u8(0x21) + u8(0x0A) + v_str(2) + u8(0x02)
    n += u8(0x21)
    return strings, n + brush, [(0, len(n)), (len(n), len(n) + len(brush))]

def colordict():
    strings = [PRESENTATION, 'DictColor', 'DictBrush']
    color = u8(0x15) + u16(0x802A) + v_color(0xFF0080FF) + u8(0x21)
    brush = u8(0x14) + u16(T_SCB) + u8(0x1E) + u16(P_SCB_COLOR) + v_str(1) + u8(0x21)
    s1 = color + brush
    ref = u8(0x0F) + v7(1) + u16(0) + v7(7) + v7(2) + u16(1) + v7(0) + u16(2) + v7(len(color)) + v7(0) + v7(0) + v7(0)
    n = u8(0x12) + u16(0) + s16('') + u8(0x17) + u16(0x8173) + ref + u8(0x21)
    return strings, n + s1, [(0, len(n)), (len(n), len(n) + len(s1))]

def conddict():
    strings = [PRESENTATION, 'CondBrush', 'Windows.Foundation.UniversalApiContract,5']
    brush = u8(0x14) + u16(T_SCB)
    brush += u8(0x26) + u16(0x836a) + u16(2) + prop(P_SCB_COLOR, v_color(0xFFFF0000)) + u8(0x27)
    brush += u8(0x26) + u16(0x8369) + u16(2) + prop(P_SCB_COLOR, v_color(0xFF00FF00)) + u8(0x27)
    brush += u8(0x21)
    ref = u8(0x0F) + v7(1) + u16(0) + v7(7) + v7(1) + u16(1) + v7(0) + v7(0) + v7(0) + v7(0)
    n = u8(0x12) + u16(0) + s16('') + u8(0x17) + u16(0x8173) + ref + u8(0x21)
    return strings, n + brush, [(0, len(n)), (len(n), len(n) + len(brush))]

def templconn():
    strings = [PRESENTATION]
    left_top = prop(P_HALIGN, v_enum(0x0263, 0)) + prop(P_VALIGN, v_enum(0x0264, 0))
    n = u8(0x12) + u16(0) + s16('') + u8(0x17) + u16(T_PAGE)
    n += u8(0x14) + u16(T_BUTTON) + prop(P_CONTROL_BACKGROUND, v_color(0xFF0000FF)) + left_top
    n += prop(P_WIDTH, v_float(80)) + prop(P_HEIGHT, v_float(40)) + prop(P_MARGIN, v_thickness(10, 10, 0, 0))
    n += u8(0x14) + u16(T_CONTROLTEMPLATE) + u8(0x0C) + v_int(2) + u8(0x11) + u16(0) + v7(1) + v7(0) + v7(0) + u8(0x21)
    n += u8(0x07) + u16(P_CONTROL_TEMPLATE) + prop(0x8FFD, v_float(2)) + u8(0x21) + u8(0x08)
    n += u8(0x21)
    s1 = u8(0x17) + u16(T_BORDER) + u8(0x1F) + u16(P_BORDER_BACKGROUND) + u16(P_CONTROL_BACKGROUND) + u8(0x0C) + v_int(5)
    s1 += u8(0x14) + u16(0x83FF) + prop(0x8FFF, v_float(1)) + u8(0x21) + u8(0x07) + u16(P_BORDER_CHILD)
    s1 += prop(0x8FFE, v_int(3)) + u8(0x21)
    return strings, n + s1, [(0, len(n)), (len(n), len(n) + len(s1))]

def deferred():
    strings = [PRESENTATION, 'Lazy', 'After']
    s1 = u8(0x14) + u16(T_BORDER) + u8(0x0D) + v_str(1) + u8(0x21)
    n = u8(0x12) + u16(0) + s16('') + u8(0x17) + u16(T_GRID)
    n += u8(0x0F) + v7(1) + u16(0) + v7(9) + u16(1) + v7(0) + v7(1)
    n += u8(0x14) + u16(T_BORDER) + u8(0x0D) + v_str(2) + u8(0x21) + u8(0x08) + u8(0x21)
    return strings, n + s1, [(0, len(n)), (len(n), len(n) + len(s1))]

def datalist():
    strings = [PRESENTATION, 'List']
    n = u8(0x12) + u16(0) + s16('') + u8(0x17) + u16(T_PAGE)
    n += u8(0x14) + u16(0x823B) + u8(0x0D) + v_str(1)
    n += u8(0x14) + u16(0x80C2) + u8(0x11) + u16(0) + v7(1) + v7(0) + v7(0) + u8(0x21) + u8(0x07) + u16(0x837B)
    n += u8(0x21) + u8(0x08) + u8(0x21)
    s1 = u8(0x17) + u16(T_BORDER) + u8(0x0C) + v_int(7) + prop(P_HEIGHT, v_float(20)) + u8(0x21)
    return strings, n + s1, [(0, len(n)), (len(n), len(n) + len(s1))]

def loop():
    strings = [PRESENTATION]
    n = u8(0x12) + u16(0) + s16('') + u8(0x17) + u16(T_GRID) + u8(0x0F) + v7(1) + u16(0) + v7(6) + u16(0) + v7(0) + u8(0x21)
    s1 = u8(0x14) + u16(T_GRID) + u8(0x0F) + v7(1) + u16(0) + v7(6) + u16(0) + v7(0) + u8(0x21)
    return strings, n + s1, [(0, len(n)), (len(n), len(n) + len(s1))]

T_DEFERRED = 0x82EA

def deferload():
    strings = [PRESENTATION, 'Lazy', 'After']
    s1 = u8(0x14) + u16(T_BORDER) + u8(0x0D) + v_str(1) + prop(P_BORDER_BACKGROUND, v_color(0xFFFF0000)) + u8(0x21)
    n = u8(0x12) + u16(0) + s16('') + u8(0x17) + u16(T_GRID)
    n += u8(0x14) + u16(T_DEFERRED) + u8(0x0F) + v7(1) + u16(0) + v7(9) + u16(1) + v7(0) + v7(1) + u8(0x21) + u8(0x08)
    n += u8(0x14) + u16(T_BORDER) + u8(0x0D) + v_str(2) + u8(0x21) + u8(0x08) + u8(0x21)
    return strings, n + s1, [(0, len(n)), (len(n), len(n) + len(s1))]

def nestedroot():
    strings = [PRESENTATION, 'Inner', 'Tail']
    n = u8(0x12) + u16(0) + s16('') + u8(0x17) + u16(T_GRID)
    n += u8(0x17) + u16(T_BORDER) + u8(0x0D) + v_str(1) + u8(0x21) + u8(0x08)
    n += u8(0x14) + u16(T_BORDER) + u8(0x0D) + v_str(2) + u8(0x21) + u8(0x08) + u8(0x21)
    return strings, n

def namedbrush():
    strings = [PRESENTATION, 'Fill', 'Tail2']
    n = u8(0x12) + u16(0) + s16('') + u8(0x17) + u16(T_GRID)
    n += u8(0x14) + u16(T_SCB) + u8(0x0D) + v_str(1) + prop(P_SCB_COLOR, v_color(0xFF00FF00)) + u8(0x21) + u8(0x07) + u16(P_PANEL_BACKGROUND)
    n += u8(0x14) + u16(T_BORDER) + u8(0x0D) + v_str(2) + u8(0x21) + u8(0x08) + u8(0x21)
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
for name, bad in (('TestStyles', None), ('BadDict', 'dict'), ('BadSetter', 'setter'), ('BadTemplate', 'template')):
    st, nd, sc = styles(bad)
    write(name + '.xbf', xbf(st, nd, sc))
st, nd, sc = loop()
write('LoopSection.xbf', xbf(st, nd, sc))
st, nd, sc, ty = appdict()
write('AppDict.xbf', xbf(st, nd, sc, ty))
st, nd, sc = themedict()
write('ThemeDict.xbf', xbf(st, nd, sc))
st, nd, sc = templconn()
write('TemplConn.xbf', xbf(st, nd, sc))
st, nd, sc = deferred()
write('Deferred.xbf', xbf(st, nd, sc))
st, nd, sc = datalist()
write('DataList.xbf', xbf(st, nd, sc))
st, nd, sc = colordict()
write('ColorDict.xbf', xbf(st, nd, sc))
st, nd, sc = conddict()
write('CondDict.xbf', xbf(st, nd, sc))
st, nd, sc = deferload()
write('DeferLoad.xbf', xbf(st, nd, sc))
st, nd = nestedroot()
write('NestedRoot.xbf', xbf(st, nd))
st, nd = namedbrush()
write('NamedBrush.xbf', xbf(st, nd))
