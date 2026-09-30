/* How long File Explorer takes to show a big folder (patches/sg/0595): starts
 * ARGV[2] (explorer.exe on the folder) and reports when its view holds ARGV[1]
 * items. */
#include <windows.h>
#include <commctrl.h>
#include <stdio.h>
static HWND lv;
static BOOL CALLBACK child(HWND h, LPARAM l) { char c[64], pc[64]; GetClassNameA(h, c, sizeof c); GetClassNameA(GetParent(h), pc, sizeof pc); if (!strcmp(c, "SysListView32") && !strcmp(pc, "SHELLDLL_DefView")) { lv = h; return FALSE; } return TRUE; }
int main(int argc, char **argv) {
  int want = atoi(argv[1]); DWORD t0 = GetTickCount(); STARTUPINFOA si = { sizeof si }; PROCESS_INFORMATION pi;
  CreateProcessA(NULL, argv[2], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
  while (GetTickCount() - t0 < 60000) {
    HWND w = FindWindowA("ExplorerWClass", NULL); lv = NULL; if (w) EnumChildWindows(w, child, 0);
    if (lv && SendMessageA(lv, LVM_GETITEMCOUNT, 0, 0) >= want) { printf("shown=%lu\n", GetTickCount() - t0); fflush(stdout); return 0; }
    Sleep(20); }
  printf("shown=timeout\n"); return 1; }
