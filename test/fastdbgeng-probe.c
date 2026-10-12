/* dbgeng batch (patches/sg/3008), run by test/fastdbgeng-gate.sh: engine state that needs no target. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <initguid.h>
#include <dbgeng.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECK(e) check((e), #e)

int main(void)
{
    IDebugClient *client;
    IDebugControl *control;
    IDebugSymbols *symbols;
    char buf[128], buf2[32];
    ULONG n, v;
    HRESULT hr;

    hr = DebugCreate(&IID_IDebugClient, (void **)&client);
    CHECK(hr == S_OK);
    if (hr != S_OK) return 1;
    CHECK(client->lpVtbl->QueryInterface(client, &IID_IDebugControl, (void **)&control) == S_OK);
    CHECK(client->lpVtbl->QueryInterface(client, &IID_IDebugSymbols, (void **)&symbols) == S_OK);

    CHECK(control->lpVtbl->GetRadix(control, &v) == S_OK && v == 16);
    CHECK(control->lpVtbl->SetRadix(control, 10) == S_OK);
    CHECK(control->lpVtbl->GetRadix(control, &v) == S_OK && v == 10);

    CHECK(control->lpVtbl->GetProcessorTypeNames(control, IMAGE_FILE_MACHINE_AMD64, buf, sizeof(buf), &n, buf2, sizeof(buf2), &n) == S_OK);
    printf("# processor name '%s'\n", buf);
    CHECK(buf[0] != 0);

    CHECK(symbols->lpVtbl->SetSymbolPath(symbols, "c:\\syms") == S_OK);
    CHECK(symbols->lpVtbl->GetSymbolPath(symbols, buf, sizeof(buf), &n) == S_OK && !strcmp(buf, "c:\\syms"));
    CHECK(symbols->lpVtbl->SetImagePath(symbols, "c:\\img") == S_OK);
    CHECK(symbols->lpVtbl->GetImagePath(symbols, buf, sizeof(buf), &n) == S_OK && !strcmp(buf, "c:\\img"));

    CHECK(control->lpVtbl->IsPointer64Bit(control) == E_UNEXPECTED);

    symbols->lpVtbl->Release(symbols);
    control->lpVtbl->Release(control);
    client->lpVtbl->Release(client);
    printf("%d failure(s)\n", failures);
    return failures != 0;
}
