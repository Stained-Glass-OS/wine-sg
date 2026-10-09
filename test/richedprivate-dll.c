/* stand-in for a program's own riched20.dll (Office ships one with exports the system's lacks) */
#include <windows.h>
__declspec(dllexport) int WINAPI SgPrivateRichEdit(void) { return 42; }
BOOL WINAPI DllMain(HINSTANCE h, DWORD r, void *p) { return TRUE; }
