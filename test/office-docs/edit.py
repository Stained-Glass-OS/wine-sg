# Open a spreadsheet through UNO (LibreOffice's Python, a named-pipe bridge to
# soffice), set B2 to 10, recalculate, save as .xlsx: office-docs-gate.sh.
#   python.exe edit.py SRC.xlsx DST.xlsx
import sys, time, subprocess, os
import uno
from com.sun.star.beans import PropertyValue
def pv(n, v):
    p = PropertyValue(); p.Name = n; p.Value = v; return p
src, dst = sys.argv[1], sys.argv[2]
prog = r"C:\Program Files\LibreOffice\program"
soffice = os.path.join(prog, "soffice.exe")
proc = subprocess.Popen([soffice, "--headless", "--norestore", "--nologo", "--accept=pipe,name=sgdocs;urp;StarOffice.ComponentContext"])
ctx = uno.getComponentContext()
resolver = ctx.ServiceManager.createInstanceWithContext("com.sun.star.bridge.UnoUrlResolver", ctx)
for i in range(120):
    try:
        rctx = resolver.resolve("uno:pipe,name=sgdocs;urp;StarOffice.ComponentContext"); break
    except Exception as e:
        time.sleep(1)
else:
    print("connect failed"); sys.exit(2)
desktop = rctx.ServiceManager.createInstanceWithContext("com.sun.star.frame.Desktop", rctx)
doc = desktop.loadComponentFromURL(uno.systemPathToFileUrl(src), "_blank", 0, (pv("Hidden", True),))
sheet = doc.Sheets.getByIndex(0)
print("before", sheet.getCellRangeByName("D4").getValue())
sheet.getCellRangeByName("B2").setValue(10)
doc.calculateAll()
print("after", sheet.getCellRangeByName("D4").getValue())
doc.storeToURL(uno.systemPathToFileUrl(dst), (pv("FilterName", "Calc MS Excel 2007 XML"),))
doc.close(True)
try:
    desktop.terminate()
except Exception:
    pass
proc.wait(60)
print("saved", os.path.getsize(dst))
