# Windows printer drivers on Stained Glass OS

Wine-sg runs printer makers' Windows drivers: the driver package installs
from its INF, programs print through it (64-bit in process, 32-bit through
splwow64, our 64-bit host), and Linux programs print through it via
sg-session's `sgwindrv` CUPS backend. The core drivers Windows ships are
ours, written from Microsoft's public driver documentation (clean room):

| Windows core            | Ours                                             |
|-------------------------|--------------------------------------------------|
| UNIDRV.DLL / UNIDRVUI.DLL (GPD printers) | `dlls/unidrv`, `dlls/unidrvui` (0921-0927, 1022) |
| PSCRIPT5.DLL / PS5UI.DLL (PPD printers)  | `dlls/pscript5`, `dlls/ps5ui` -> `wineps.drv` (1021) |
| winprint (EMF print processor)           | `dlls/winprint` (0922, 1020-1021)                  |
| GDI engine services (Eng*, XLATEOBJ...)  | `dlls/gdi32/umpd.c`, `engblt.c` (0920, 1020)       |
| spooler router (spoolss)                 | `dlls/spoolss` -> winspool (1020)                  |

## The corpus

Real packages from the makers and the Microsoft Update Catalog. They are
not redistributable, so gates use drivers of our own; these are manual
results. To reproduce: unpack a package (`cabextract`) into a directory and

    WINE=/opt/wine-sg/bin/wine tools/printer-corpus/run.sh PACKAGE_DIR [MODEL]

`run.sh` installs the package in a scratch prefix, adds a printer on a file
port, prints our test page (`tools/printer-corpus/sgprint.c`: text in three
fonts, colour bars, a grey ramp, lines, a circle, a picture) from a 64-bit
and a 32-bit program, and checks the output with `check.py`: the language
(PostScript, PCL 5, PCL XL, ZPL, ESC/P-R, Brother raster), whether it parses
(ghostscript, GhostPCL's gpcl6, our ZPL reader), and how well the rendered
first page matches the page as GDI draws it (1.00 = the same ink).
`imports.py` lists what a package's DLLs import that Wine lacks;
`pclxl.py` lists a PCL XL stream's operators.

Status 2026-10-06 (wine-sg 1020-1022):

| Package (source)                         | Arch | Core / kind            | Installs | 64-bit print | 32-bit print | Output valid | Renders | Notes |
|------------------------------------------|------|------------------------|----------|--------------|--------------|--------------|---------|-------|
| HP Universal Printing PCL 6 (catalog, 2013) | x64 | Unidrv + HP vector plug-in | yes | yes | (host) | PCL XL, gpcl6 clean | 0.98 | needed 1022 (macros, 512 features, plug-in blits) |
| Brother BR-Script3 HL-3070CW (catalog)   | x64  | PScript (PPD only)     | yes      | yes          | yes          | PostScript, gs clean | 0.97 | PJL + all PPD options sent |
| Zebra ZDesigner 8.6 (catalog)            | x64  | own UMPD               | yes      | yes          | yes          | ZPL (^GFA Z64) | 0.99 (label) | |
| Brother HL-L2310D (catalog)              | x64  | own UMPD               | yes      | yes          | no (host)    | Brother PCL (mode 1030 raster) | not checked | gpcl6 does not read Brother's compression |
| Lexmark Universal v2 (catalog)           | x64  | own driver             | yes      | yes          | yes          | PCL 5 | 0.00 | page renders wrong: open |
| Brother QL-800 (catalog)                 | x64  | own UMPD               | yes      | yes          | no           | Brother QL raster | not checked | |
| Zebra ZDesigner 5.x (catalog)            | x64  | own UMPD               | yes      | no           | no           | - | - | open |
| Epson XP-230 (catalog)                   | x64  | own UMPD (ESC/P-R)     | yes      | no           | no           | - | - | UI DLL crashes in DocumentProperties (e_yaskpce) |
| Samsung Universal Print Driver 3         | x64  | own driver + PP + LM   | yes      | no           | no           | - | - | open |
| Kyocera KX (TASKalfa 3510i)              | x64  | own driver             | yes      | no           | no           | - | - | open |
| x86-only packages (Xerox GPD 2009 x86, Dell PS, Sharp PS, OKI PS 2009, Lexmark E120, Canon BJ, HP UPD x86) | x86 | | refused on 64-bit, as Windows does | | | | | |

Test sources: catalog.update.microsoft.com (search, download the cab,
`cabextract`).
