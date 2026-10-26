/* conpty-fake: stands in for the conpty.dll a terminal ships (test/conpty-gate.sh).
 * Wine's builtin must be loaded in its place; this one only answers a marker. */
#include <windows.h>
HRESULT WINAPI ConptyCreatePseudoConsole(COORD size, HANDLE in, HANDLE out, DWORD flags, void **ret)
{
    return 0x8000dead;
}
