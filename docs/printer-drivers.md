# Windows printer drivers on Stained Glass OS

Wine-sg runs printer makers' Windows drivers: the driver package installs
from its INF, programs print through it (64-bit in process, 32-bit through
splwow64, our 64-bit host), and Linux programs print through it via
sg-session's `sgwindrv` CUPS backend. The core drivers Windows ships are
ours, written from Microsoft's public driver documentation (clean room):

| Windows core            | Ours                                             |
|-------------------------|--------------------------------------------------|
| UNIDRV.DLL / UNIDRVUI.DLL (GPD printers) | `dlls/unidrv`, `dlls/unidrvui` (0921-0927, 1022, 1023, 1027) |
| PSCRIPT5.DLL / PS5UI.DLL (PPD printers)  | `dlls/pscript5`, `dlls/ps5ui` -> `wineps.drv` (1021, 1024, 1027) |
| COMPSTUI.DLL (the drivers' property sheets) | `dlls/compstui` (1026) |
| render plug-ins (IPrintOemUni, IPrintOemPS/PS2) | unidrv `oem.c` (0923, 1022), wineps `oemps.c` (1024) |
| UI plug-ins (IPrintOemUI/UI2, IPrintCoreUI2) | `dlls/unidrvui/drvui.c`, shared with wineps (1027) |
| TCPMON.DLL (Standard TCP/IP Port: RAW, LPR; IPP by URL) | `dlls/tcpmon` (1029) |
| language monitors, port status | `dlls/localspl` (1029), winspool SetPort/GetPrinter |
| makers' print processors (GdiPlayPageEMF...) | gdi32 `dc.c`, localspl (1030) |
| print schema (driver settings, custom size) | `dlls/prntvpt` (1221, 1031) |
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

Status 2026-10-06 (wine-sg 1020-1031). "Prefs" is the driver's
Printing Preferences (DocumentProperties with a prompt) through our CPSUI.

| Package (source)                         | Arch | Core / kind            | Installs | 64-bit print | 32-bit print | Output valid | Renders | Prefs | Notes |
|------------------------------------------|------|------------------------|----------|--------------|--------------|--------------|---------|-------|-------|
| HP Universal Printing PCL 6 (catalog)    | x64  | Unidrv + HP plug-ins   | yes      | yes          | yes          | PCL XL, gpcl6 clean | 0.98 | HP's own (asks for the printer's address: dynamic mode) | HP's print processor (hpcpp160) and language monitor run (1029-1030) |
| Lexmark Universal v2 (catalog)           | x64  | Unidrv + plug-ins      | yes      | yes          | yes          | PCL 5 + HP-GL/2 | 0.98 | ours (Layout, Paper/Quality, Advanced) | its print processor runs (1030); its UI plug-in's own pages fail inside it (E_OUTOFMEMORY): open |
| Toshiba e-STUDIO PS3 (catalog)           | x64  | PScript (PPD)          | yes      | yes          | yes          | PostScript, gs clean | 0.98 | ours | |
| Xerox Global Print Driver PS (catalog)   | x64  | PScript (PPD)          | yes      | yes          | yes          | PostScript, gs clean | 0.98 | ours | |
| Ricoh Aficio SP C420DN PS (catalog)      | x64  | PScript + plug-ins     | yes      | yes          | yes          | PostScript, gs clean | 0.97 | ours + Ricoh's Job/Log page | render plug-in's job code in the PostScript; its print processor runs (1030) |
| DYMO LabelWriter 550 (DYMO Connect's package) | x64 | Unidrv + DYMO plug-ins + LM | yes | yes (DYMO agent's QA) | - | DYMO raster | label | ours (density, quality in Advanced) | custom size listed; density/quality in print tickets (1031) |
| Brother HL-3070CW BR-Script3 (catalog)   | x64  | PScript (PPD only)     | yes (1025) | yes        | yes          | PostScript, gs clean | 0.97 | ours (Chinese PPD strings) | INF lists models only for x86 |
| Zebra ZDesigner 8.6 (catalog)            | x64  | own UMPD               | yes      | yes          | yes          | ZPL (^GFA Z64) | label | Zebra's own | |
| Brother HL-L2310D (catalog)              | x64  | own UMPD               | yes      | yes          | no (host)    | Brother PCL (mode 1030 raster) | not checked | Brother's own (full) | gpcl6 does not read Brother's compression |
| Brother QL-800 (catalog)                 | x64  | own UMPD               | yes      | yes          | no           | Brother QL raster | not checked | not tried | |
| OKI B930 PS (catalog)                    | x64  | own PS driver (Monotype) | yes (1028) | header only | -          | PostScript header, no pages | - | not tried | pages not emitted: open |
| Zebra ZDesigner 8.6 on a network port    | x64  | own UMPD + Zebra LM    | yes      | yes (RAW 9100) | -          | ZPL | - | - | its language monitor fails starting a job; the job goes past it (1029) |
| Zebra ZDesigner 5.x (catalog)            | x64  | own UMPD               | yes      | no           | no           | - | - | - | open |
| Epson XP-230 (catalog)                   | x64  | own UMPD (ESC/P-R)     | yes      | no           | no           | - | - | crashes in E_YASKPCE (wants Epson Status Monitor's registry, made by Epson's installer) | open |
| Samsung Universal Print Driver 3         | x64  | own driver + PP + LM   | yes      | no           | no           | - | - | - | open |
| Kyocera KX (TASKalfa 3510i)              | x64  | own driver             | yes      | no           | no           | - | - | - | open |
| v4 packages (Konica Minolta, HP inbox PS, Canon inbox, Epson inbox, HP Smart Universal) | x64 | v4 (XPS, manifest.ini) | no | - | - | - | - | - | not supported yet: see "v4 drivers" |
| x86-only packages (Xerox GPD 2009 x86, Dell PS, Sharp PS, OKI PS 2009, Lexmark E120, Canon BJ, HP UPD x86) | x86 | | refused on 64-bit, as Windows does | | | | | | |

Test sources: catalog.update.microsoft.com (search, download the cab,
`cabextract`).

## v4 drivers

A v4 package (INF `ClassVer=4.0`) installs no driver DLL of its own: each
model's `*-manifest.ini` names a data file (PPD or GPD), the class driver
(`DriverFile=PSCRIPT5.DLL`, or none for a GPD) and a
`*-PipelineConfig.xml` of XPS print filters that turn the job into the
printer's language. Two kinds are in the corpus:

- PostScript class (Konica Minolta's, HP's inbox PS): the pipeline is only
  Microsoft's "v3 hosting filter", which renders through the PostScript
  core -- our PostScript driver could print these from their PPD. Not done
  yet: the installer does not read manifests (and the Konica and HP
  packages in the corpus are ARM64-only).
- GPD with the maker's filters (Epson's and Canon's inbox drivers): the
  filters (EP0*.DLL, CNN08CL2FX.DLL) write ESC/P-R or UFR II from the XPS
  document; they need the XPSDrv filter pipeline (IPrintPipelineFilter,
  IXpsDocumentProvider/Consumer, the XPS object model), which Wine does not
  have.

## Network printers

A Windows driver prints to a printer on the network through the Standard
TCP/IP Port monitor (`tcpmon.dll`, 1029): RAW on port 9100 or LPR, ports
made through its Xcv interface (as makers' installers and our Settings do),
or to an IPP printer by naming the port by its URL (`ipp://host/ipp/print`).
Jobs go through the driver's language monitor; what it reports (SetPort) is
the printer's status. WSD ports are not done.
