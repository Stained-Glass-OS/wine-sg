#!/usr/bin/env python3
"""Write small Office Open XML documents for test/office-docs-gate.sh.

    mkdocs.py DIR

DIR/src.docx  a heading, a paragraph with the marker Zebra4217 (in Calibri,
              which an office suite substitutes metric-compatibly) and a table
DIR/src.xlsx  item/qty/price/total with formulas (D2=B2*C2, D4=SUM(D2:D3))
DIR/src.pptx  one slide whose title reads "Slide Quokka9031"

Written by hand with zipfile -- no office suite needed on the build host --
and deliberately minimal: what a program has to handle to open a real file.
"""
import os
import sys
import zipfile

W = "http://schemas.openxmlformats.org/wordprocessingml/2006/main"
R = "http://schemas.openxmlformats.org/officeDocument/2006/relationships"
PKG = "http://schemas.openxmlformats.org/package/2006/relationships"
CT = "http://schemas.openxmlformats.org/package/2006/content-types"
S = "http://schemas.openxmlformats.org/spreadsheetml/2006/main"
P = "http://schemas.openxmlformats.org/presentationml/2006/main"
A = "http://schemas.openxmlformats.org/drawingml/2006/main"
OFFDOC = "http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument"
XML = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'


def rels(items):
    out = [XML, f'<Relationships xmlns="{PKG}">']
    for rid, typ, target in items:
        out.append(f'<Relationship Id="{rid}" Type="{typ}" Target="{target}"/>')
    out.append("</Relationships>")
    return "".join(out)


def types(overrides):
    out = [XML, f'<Types xmlns="{CT}">',
           '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>',
           '<Default Extension="xml" ContentType="application/xml"/>']
    for part, ct in overrides:
        out.append(f'<Override PartName="{part}" ContentType="{ct}"/>')
    out.append("</Types>")
    return "".join(out)


def write(path, parts):
    with zipfile.ZipFile(path, "w", zipfile.ZIP_DEFLATED) as z:
        for name, data in parts:
            z.writestr(name, data)


def docx(path):
    def run(text, font="Calibri", bold=False, size=22):
        b = "<w:b/>" if bold else ""
        return (f'<w:r><w:rPr><w:rFonts w:ascii="{font}" w:hAnsi="{font}"/>{b}'
                f'<w:sz w:val="{size}"/></w:rPr><w:t xml:space="preserve">{text}</w:t></w:r>')

    def cell(text):
        return f'<w:tc><w:tcPr><w:tcW w:w="2000" w:type="dxa"/></w:tcPr><w:p>{run(text)}</w:p></w:tc>'

    border = "".join(f'<w:{s} w:val="single" w:sz="4" w:space="0" w:color="000000"/>'
                     for s in ("top", "left", "bottom", "right", "insideH", "insideV"))
    body = (f'<w:p>{run("Stained Glass Office Test", bold=True, size=36)}</w:p>'
            f'<w:p>{run("The quick brown fox jumps over the lazy dog. Zebra4217")}</w:p>'
            f'<w:tbl><w:tblPr><w:tblBorders>{border}</w:tblBorders></w:tblPr>'
            f'<w:tblGrid><w:gridCol w:w="2000"/><w:gridCol w:w="2000"/></w:tblGrid>'
            f'<w:tr>{cell("alpha")}{cell("beta")}</w:tr><w:tr>{cell("gamma")}{cell("delta")}</w:tr></w:tbl>'
            f'<w:p/><w:sectPr><w:pgSz w:w="12240" w:h="15840"/>'
            f'<w:pgMar w:top="1440" w:right="1440" w:bottom="1440" w:left="1440"/></w:sectPr>')
    write(path, [
        ("[Content_Types].xml", types([("/word/document.xml",
            "application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml")])),
        ("_rels/.rels", rels([("rId1", OFFDOC, "word/document.xml")])),
        ("word/document.xml", f'{XML}<w:document xmlns:w="{W}" xmlns:r="{R}"><w:body>{body}</w:body></w:document>'),
    ])


def xlsx(path):
    rows = [
        [("s", 0), ("s", 1), ("s", 2), ("s", 3)],
        [("s", 4), ("n", "3"), ("n", "1.25"), ("f", "B2*C2")],
        [("s", 5), ("n", "4"), ("n", "2.5"), ("f", "B3*C3")],
        [("s", 6), None, None, ("f", "SUM(D2:D3)")],
    ]
    strings = ["item", "qty", "price", "total", "apple", "pear", "sum"]
    xrows = []
    for i, row in enumerate(rows, 1):
        cells = []
        for j, c in enumerate(row):
            if c is None:
                continue
            ref = "ABCD"[j] + str(i)
            kind, v = c
            if kind == "s":
                cells.append(f'<c r="{ref}" t="s"><v>{v}</v></c>')
            elif kind == "n":
                cells.append(f'<c r="{ref}"><v>{v}</v></c>')
            else:   # a formula with no cached value: the reader must calculate
                cells.append(f'<c r="{ref}"><f>{v}</f></c>')
        xrows.append(f'<row r="{i}">{"".join(cells)}</row>')
    sst = "".join(f"<si><t>{s}</t></si>" for s in strings)
    write(path, [
        ("[Content_Types].xml", types([
            ("/xl/workbook.xml", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"),
            ("/xl/worksheets/sheet1.xml", "application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"),
            ("/xl/sharedStrings.xml", "application/vnd.openxmlformats-officedocument.spreadsheetml.sharedStrings+xml")])),
        ("_rels/.rels", rels([("rId1", OFFDOC, "xl/workbook.xml")])),
        ("xl/workbook.xml", f'{XML}<workbook xmlns="{S}" xmlns:r="{R}"><sheets>'
                            f'<sheet name="Sheet1" sheetId="1" r:id="rId1"/></sheets>'
                            f'<calcPr fullCalcOnLoad="1"/></workbook>'),
        ("xl/_rels/workbook.xml.rels", rels([
            ("rId1", R + "/worksheet", "worksheets/sheet1.xml"),
            ("rId2", R + "/sharedStrings", "sharedStrings.xml")])),
        ("xl/sharedStrings.xml", f'{XML}<sst xmlns="{S}" count="{len(strings)}" uniqueCount="{len(strings)}">{sst}</sst>'),
        ("xl/worksheets/sheet1.xml", f'{XML}<worksheet xmlns="{S}"><sheetData>{"".join(xrows)}</sheetData></worksheet>'),
    ])


def pptx(path):
    theme = (f'{XML}<a:theme xmlns:a="{A}" name="T"><a:themeElements>'
             '<a:clrScheme name="T">'
             + "".join(f'<a:{n}><a:srgbClr val="{v}"/></a:{n}>' for n, v in (
                 ("dk1", "000000"), ("lt1", "FFFFFF"), ("dk2", "1F497D"), ("lt2", "EEECE1"),
                 ("accent1", "4F81BD"), ("accent2", "C0504D"), ("accent3", "9BBB59"), ("accent4", "8064A2"),
                 ("accent5", "4BACC6"), ("accent6", "F79646"), ("hlink", "0000FF"), ("folHlink", "800080")))
             + '</a:clrScheme><a:fontScheme name="T"><a:majorFont><a:latin typeface="Calibri"/><a:ea typeface=""/>'
             '<a:cs typeface=""/></a:majorFont><a:minorFont><a:latin typeface="Calibri"/><a:ea typeface=""/>'
             '<a:cs typeface=""/></a:minorFont></a:fontScheme><a:fmtScheme name="T">'
             '<a:fillStyleLst>' + '<a:solidFill><a:schemeClr val="phClr"/></a:solidFill>' * 3 + '</a:fillStyleLst>'
             '<a:lnStyleLst>' + '<a:ln w="9525"><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:ln>' * 3
             + '</a:lnStyleLst><a:effectStyleLst>' + '<a:effectStyle><a:effectLst/></a:effectStyle>' * 3
             + '</a:effectStyleLst><a:bgFillStyleLst>' + '<a:solidFill><a:schemeClr val="phClr"/></a:solidFill>' * 3
             + '</a:bgFillStyleLst></a:fmtScheme></a:themeElements></a:theme>')
    spTree_empty = ('<p:cSld><p:spTree><p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>'
                    '<p:grpSpPr/></p:spTree></p:cSld>')
    ns = f'xmlns:a="{A}" xmlns:r="{R}" xmlns:p="{P}"'
    master = (f'{XML}<p:sldMaster {ns}>{spTree_empty}'
              '<p:clrMap bg1="lt1" tx1="dk1" bg2="lt2" tx2="dk2" accent1="accent1" accent2="accent2" '
              'accent3="accent3" accent4="accent4" accent5="accent5" accent6="accent6" hlink="hlink" folHlink="folHlink"/>'
              '<p:sldLayoutIdLst><p:sldLayoutId id="2147483649" r:id="rId1"/></p:sldLayoutIdLst></p:sldMaster>')
    layout = f'{XML}<p:sldLayout {ns} type="blank">{spTree_empty}<p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr></p:sldLayout>'
    slide = (f'{XML}<p:sld {ns}><p:cSld><p:spTree><p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/>'
             '</p:nvGrpSpPr><p:grpSpPr/><p:sp><p:nvSpPr><p:cNvPr id="2" name="Title"/><p:cNvSpPr txBox="1"/><p:nvPr/>'
             '</p:nvSpPr><p:spPr><a:xfrm><a:off x="914400" y="914400"/><a:ext cx="7315200" cy="1371600"/></a:xfrm>'
             '<a:prstGeom prst="rect"><a:avLst/></a:prstGeom></p:spPr><p:txBody><a:bodyPr/><a:lstStyle/>'
             '<a:p><a:r><a:rPr lang="en-US" sz="4000"/><a:t>Slide Quokka9031</a:t></a:r></a:p></p:txBody></p:sp>'
             '</p:spTree></p:cSld><p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr></p:sld>')
    pres = (f'{XML}<p:presentation {ns}><p:sldMasterIdLst><p:sldMasterId id="2147483648" r:id="rId1"/>'
            '</p:sldMasterIdLst><p:sldIdLst><p:sldId id="256" r:id="rId2"/></p:sldIdLst>'
            '<p:sldSz cx="9144000" cy="6858000"/><p:notesSz cx="6858000" cy="9144000"/></p:presentation>')
    PT = "application/vnd.openxmlformats-officedocument.presentationml."
    write(path, [
        ("[Content_Types].xml", types([
            ("/ppt/presentation.xml", PT + "presentation.main+xml"),
            ("/ppt/slideMasters/slideMaster1.xml", PT + "slideMaster+xml"),
            ("/ppt/slideLayouts/slideLayout1.xml", PT + "slideLayout+xml"),
            ("/ppt/slides/slide1.xml", PT + "slide+xml"),
            ("/ppt/theme/theme1.xml", "application/vnd.openxmlformats-officedocument.theme+xml")])),
        ("_rels/.rels", rels([("rId1", OFFDOC, "ppt/presentation.xml")])),
        ("ppt/presentation.xml", pres),
        ("ppt/_rels/presentation.xml.rels", rels([
            ("rId1", R + "/slideMaster", "slideMasters/slideMaster1.xml"),
            ("rId2", R + "/slide", "slides/slide1.xml"),
            ("rId3", R + "/theme", "theme/theme1.xml")])),
        ("ppt/slideMasters/slideMaster1.xml", master),
        ("ppt/slideMasters/_rels/slideMaster1.xml.rels", rels([
            ("rId1", R + "/slideLayout", "../slideLayouts/slideLayout1.xml"),
            ("rId2", R + "/theme", "../theme/theme1.xml")])),
        ("ppt/slideLayouts/slideLayout1.xml", layout),
        ("ppt/slideLayouts/_rels/slideLayout1.xml.rels", rels([
            ("rId1", R + "/slideMaster", "../slideMasters/slideMaster1.xml")])),
        ("ppt/slides/slide1.xml", slide),
        ("ppt/slides/_rels/slide1.xml.rels", rels([
            ("rId1", R + "/slideLayout", "../slideLayouts/slideLayout1.xml")])),
        ("ppt/theme/theme1.xml", theme),
    ])


if __name__ == "__main__":
    d = sys.argv[1]
    os.makedirs(d, exist_ok=True)
    docx(os.path.join(d, "src.docx"))
    xlsx(os.path.join(d, "src.xlsx"))
    pptx(os.path.join(d, "src.pptx"))
