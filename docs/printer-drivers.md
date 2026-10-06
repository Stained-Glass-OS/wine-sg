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
| document, printer and driver events (DrvDocumentEvent, DrvPrinterEvent, DrvDriverEvent) | gdi32 `dc.c`, winspool (1032, 1035, 1040) |
| a 64-bit driver's configuration DLL for 32-bit programs | winspool -> splwow64 (1032) |
| monitors' MONITORINIT and registry functions (MONITORREG) | `dlls/localspl` (1034) |
| v4 PostScript class drivers (manifest.ini) | winspool `install.c` (1037) |
| print schema (driver settings, custom size) | `dlls/prntvpt` (1221, 1031, 1036) |
| winprint (EMF print processor)           | `dlls/winprint` (0922, 1020-1021)                  |
| GDI engine services (Eng*, XLATEOBJ...)  | `dlls/gdi32/umpd.c`, `engblt.c` (0920, 1020, 1033) |
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

Status 2026-10-06 (wine-sg 1020-1041). "Renders" is check.py's match of the first page against the page as drawn, PCL through GhostPCL (gpcl6, built from source for testing: see check.py). "Prefs" is the driver's
Printing Preferences (DocumentProperties with a prompt) through our CPSUI.

| Package (source)                         | Arch | Core / kind            | Installs | 64-bit print | 32-bit print | Output valid | Renders | Prefs | Notes |
|------------------------------------------|------|------------------------|----------|--------------|--------------|--------------|---------|-------|-------|
| HP Universal Printing PCL 6 (catalog)    | x64  | Unidrv + HP plug-ins   | yes      | yes          | yes          | PCL XL, gpcl6 clean | 0.98 | HP's own (asks for the printer's address: dynamic mode) | HP's print processor (hpcpp160) and language monitor run (1029-1030); through HP's processor 0.96 (1041: it was 0.41, the page stretched over the sheet) |
| Lexmark Universal v2 (catalog)           | x64  | Unidrv + plug-ins      | yes      | yes          | yes          | PCL 5 + HP-GL/2 | 0.98 | ours (Layout, Paper/Quality, Advanced) | its print processor (LMUD1O4C) prints its pages since 1041 (they were blank), last page first and about 0.4 in to the right of where winprint puts them: open (0.15); its UI plug-in's own pages fail inside it (E_OUTOFMEMORY at PROPSHEETUI_REASON_INIT, after it asks for IPrintOemDriverUI and makes no further helper calls; its CommonUIProp adds no options): open |
| Toshiba e-STUDIO PS3 (catalog)           | x64  | PScript (PPD)          | yes      | yes          | yes          | PostScript, gs clean | 0.98 | ours | |
| Xerox Global Print Driver PS (catalog)   | x64  | PScript (PPD)          | yes      | yes          | yes          | PostScript, gs clean | 0.98 | ours | |
| Ricoh Aficio SP C420DN PS (catalog)      | x64  | PScript + plug-ins     | yes      | yes          | yes          | PostScript, gs clean | 0.97 | ours + Ricoh's Job/Log page | render plug-in's job code in the PostScript; its print processor runs (1030) |
| DYMO LabelWriter 550 (DYMO Connect's package) | x64 | Unidrv + DYMO plug-ins + LM | yes | yes (DYMO agent's QA) | - | DYMO raster | label | ours (density, quality in Advanced) | custom size listed; density/quality in print tickets (1031) |
| Brother HL-3070CW BR-Script3 (catalog)   | x64  | PScript (PPD only)     | yes (1025) | yes        | yes          | PostScript, gs clean | 0.97 | ours (Chinese PPD strings) | INF lists models only for x86 |
| Zebra ZDesigner 8.6 (catalog)            | x64  | own UMPD               | yes      | yes          | yes          | ZPL (^GFA Z64) | label | Zebra's own | |
| Brother HL-L2310D (catalog)              | x64  | own UMPD               | yes      | yes          | yes (1032)   | Brother PCL (mode 1030 raster) | not readable | Brother's own (full), also from 32-bit programs through our host (1032) | gpcl6 does not read Brother's raster compression (gpcl6 reads it as PCL 5 and draws nothing) |
| Brother QL-800 (catalog)                 | x64  | own UMPD               | yes      | yes          | yes (1032)   | Brother QL raster | not checked | Brother's own (1039; the corpus package is Brother's Arabic one, so its pages are in Arabic) | its printer event (1032) writes its timeouts; its driver needs its own settings (merged, 1032) |
| OKI B930 PS (catalog)                    | x64  | own PS driver (Monotype) | yes (1028) | yes (1035) | -          | PostScript, gs clean | page as drawn | not tried | prepares its pages in its document events (1035) |
| Zebra ZDesigner 8.6 on a network port    | x64  | own UMPD + Zebra LM    | yes      | yes (RAW 9100) | -          | ZPL | - | - | its language monitor runs (1034: it keeps its MONITORINIT) |
| Zebra ZDesigner 5.x (catalog)            | x64  | own UMPD               | yes      | yes (1033)   | not tried    | ZPL | label | not tried | needed EngComputeGlyphSet (1033) |
| Epson XP-230 (catalog)                   | x64  | own UMPD (ESC/P-R)     | yes      | no           | no           | - | - | crashes in E_YASKPCE | its driver event (1040) makes its HKLM\Software\EPSON keys; the crash wants HKLM\Software\EPSON\STM3\Driver\<model> (Profile), made only by Epson's full setup bundle, not in the catalog package: open |
| Samsung Universal Print Driver 3         | x64  | own driver + PP + LM   | yes      | yes (1038)   | not tried    | PCL XL, gpcl6 clean | 0.98 | not tried | its driver needed the job's datatype (GetJob, 1038); its print processor gives up without reading the job, so winprint prints it (1038) |
| Kyocera KX (TASKalfa 3510i)              | x64  | own driver + PJL LM    | yes      | yes (1034)   | not tried    | PRESCRIBE + PCL XL 3.0 | - | not tried | gpcl6 stops at its PCL XL 3.0 operator 0x4e (it reads XL 2.1); the stream's structure is sound (pclxl.py: pages of images) |
| v4 PostScript class (Konica Minolta, HP inbox PS) | arm64 in the corpus | PScript (PPD) via manifest | yes (1037, own x64 fixture) | yes (fixture) | - | PostScript | - | ours | the corpus packages are ARM64-only |
| v4 XPS-filter packages (Canon inbox, Epson inbox, HP Smart Universal) | x64 | GPD + XPS filters | refused (1037) | - | - | - | - | - | need the XPSDrv filter pipeline: see "v4 drivers" |
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
  core. The installer reads the model's manifest (1037) and installs it on
  our PostScript driver with its PPD; it prints from it (gate: our own x64
  v4 package; the Konica and HP packages in the corpus are ARM64-only).
- GPD with the maker's filters (Epson's and Canon's inbox drivers): the
  filters (EP0*.DLL, CNN08CL2FX.DLL) write ESC/P-R or UFR II from the XPS
  document; they need the XPSDrv filter pipeline (IPrintPipelineFilter,
  IXpsDocumentProvider/Consumer, the XPS object model), which Wine does not
  have. The installer refuses them (ERROR_UNKNOWN_PRINTER_DRIVER, 1037).

## Network printers

A Windows driver prints to a printer on the network through the Standard
TCP/IP Port monitor (`tcpmon.dll`, 1029): RAW on port 9100 or LPR, ports
made through its Xcv interface (as makers' installers and our Settings do),
or to an IPP printer by naming the port by its URL (`ipp://host/ipp/print`).
Jobs go through the driver's language monitor; what it reports (SetPort) is
the printer's status. WSD ports are not done: a WSD port needs WS-Discovery
to find the printer and the WS-Print SOAP service to send it jobs (and a
monitor, WSDMON, built on both); not cheap, left for later. A printer that
speaks IPP (nearly every network printer that speaks WSD) is reached by
its ipp:// URL instead.

## 32-bit programs

A maker's 64-bit driver serves 32-bit programs as on 64-bit Windows: the
program cannot load the driver's DLLs, so our 64-bit print host
(`splwow64`) does the work (1032):

- the page: the program spools EMF; the host renders it through the driver
  (`splwow64 render`), holding a job of its own on the printer so a driver
  that looks its job up (GetJob) finds it;
- the device's measurements for the program's DC (`splwow64 caps`);
- the settings and the dialogs: DocumentProperties (with Printing
  preferences), DeviceCapabilities and PrinterProperties are answered by
  the host (`splwow64 devprops`, `devcaps`, `printerprops`), where the
  configuration DLL and its makers' plug-ins load; the dialogs are the
  maker's own (Brother's, from a 32-bit program).

A DEVMODE with the public settings alone (a program's own, or one from a
program that could not reach the driver) is merged into the driver's own
settings before the driver is enabled (1032); makers' drivers expect their
private part (Brother's label driver crashed without it).

## Driver events

- Printer events (1032): a new printer's driver is told so
  (DrvPrinterEvent, PRINTER_EVENT_INITIALIZE) once the printer has its
  settings, and a deleted printer's. Makers' drivers make their registry
  entries there; what their installers (EXEs we do not run) would make is
  still missing (Epson's status monitor keys).
- Document events (1035): GDI tells the configuration DLL of each printing
  call (DrvDocumentEvent): the event filter (QUERYFILTER), CreateDC, ResetDC,
  StartDoc (which may refuse the document), StartPage, EndPage, EndDoc
  (the job is scheduled after ENDDOCPOST, as Windows' spooler prints it
  after GDI is done), AbortDoc and DeleteDC. A driver that answers
  CREATEDCPRE with anything but success hears nothing more of that DC
  (Brother's, HP's). OKI's PostScript driver prepares its pages there.

## Linux programs through Windows drivers

Retested 2026-10-06 (wine-sg 1037) the way sg-session's `sgwindrv`
backend prints a Linux program's job: a PDF page drawn at 300 dpi
(pdftoppm), its PPD from the driver (`splwow64 ppd`), the page printed
through the driver (`splwow64 print`). PostScript: Brother BR-Script3,
Ricoh SP C420DN PS and OKI B930 print the page as drawn (ghostscript);
PCL: HP Universal PCL 6 (PCL XL) and Lexmark Universal (PCL 5) print the
page as drawn (GhostPCL); Kyocera KX and Brother HL-L2310D write their jobs
in languages GhostPCL does not read (PCL XL 3.0, Brother's raster). The gate with our own driver is
test/linuxdrv-gate.sh.

## Makers' installers and their registry

Makers' drivers set themselves up through the events Windows' spooler sends
them: DrvDriverEvent when the driver is installed (1040), DrvPrinterEvent
when a printer is added or deleted (1032). Both run here, so what a driver
makes for itself is made. What a maker's setup program (an EXE outside the
driver package) makes is not: we install driver packages from their INFs
and run none of their programs. Epson's status monitor key
(HKLM\Software\EPSON\STM3\Driver\<model>) is the case in the corpus;
the catalog package has the status monitor's programs as driver files but
no installer for them, so neither running an installer nor making the key
generically (its contents are Epson's own) is possible.

## Makers' print processors

A maker's print processor plays the job's pages itself (GdiPlayPageEMF).
A spooled page's frame is the whole sheet around the printable origin, as
Windows records it, and a processor's rectangle is honoured, or the sheet
when it gives none or an empty one (1041). A processor that gives up on a
job without reading it (Samsung's) does not lose the job: winprint prints
it through the driver (1038). The printproc gate checks that a processor's
page lands where winprint puts it.
