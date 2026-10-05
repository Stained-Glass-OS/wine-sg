/* missingrt-gate.sh's plug-in: imports from msvbvm60.dll (the Visual Basic 6
 * runtime) and from sgnosuch.dll (a DLL nobody offers), whichever the gate
 * builds it against. SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
extern void __stdcall sg_runtime_entry(void);
__declspec(dllexport) void __stdcall plugin_entry(void) { sg_runtime_entry(); }
BOOL WINAPI DllMain( HINSTANCE inst, DWORD reason, void *reserved ) { (void)inst; (void)reason; (void)reserved; return TRUE; }
