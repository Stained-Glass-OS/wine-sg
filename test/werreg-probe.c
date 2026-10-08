/* Error reporting registrations (patches/sg/1607): WerRegisterMemoryBlock,
 * WerRegisterFile, WerRegisterRuntimeExceptionModule and their Unregister
 * functions, WerSetFlags / WerGetFlags. The stubs failed with E_NOTIMPL.
 *
 *  - registering succeeds, twice is refused (ERROR_ALREADY_EXISTS);
 *  - unregistering what was registered succeeds, again: ERROR_NOT_FOUND;
 *  - bad arguments: E_INVALIDARG (a block over 64 KB, a bad file type);
 *  - at most 16 runtime exception modules;
 *  - WerGetFlags reads back WerSetFlags.
 */
#include <windows.h>
#include <stdio.h>

typedef HRESULT (WINAPI *reg_block_t)(void *, DWORD);
typedef HRESULT (WINAPI *unreg_block_t)(void *);
typedef HRESULT (WINAPI *reg_file_t)(const WCHAR *, int, DWORD);
typedef HRESULT (WINAPI *unreg_file_t)(const WCHAR *);
typedef HRESULT (WINAPI *reg_module_t)(const WCHAR *, void *);
typedef HRESULT (WINAPI *set_flags_t)(DWORD);
typedef HRESULT (WINAPI *get_flags_t)(HANDLE, DWORD *);

static int failures;

static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok) failures++;
}

int main(void)
{
    HMODULE k32 = GetModuleHandleA("kernel32.dll");
    reg_block_t pWerRegisterMemoryBlock = (void *)GetProcAddress(k32, "WerRegisterMemoryBlock");
    unreg_block_t pWerUnregisterMemoryBlock = (void *)GetProcAddress(k32, "WerUnregisterMemoryBlock");
    reg_file_t pWerRegisterFile = (void *)GetProcAddress(k32, "WerRegisterFile");
    unreg_file_t pWerUnregisterFile = (void *)GetProcAddress(k32, "WerUnregisterFile");
    reg_module_t pWerRegisterRuntimeExceptionModule = (void *)GetProcAddress(k32, "WerRegisterRuntimeExceptionModule");
    reg_module_t pWerUnregisterRuntimeExceptionModule = (void *)GetProcAddress(k32, "WerUnregisterRuntimeExceptionModule");
    set_flags_t pWerSetFlags = (void *)GetProcAddress(k32, "WerSetFlags");
    get_flags_t pWerGetFlags = (void *)GetProcAddress(k32, "WerGetFlags");
    static char block[4096], big[70000];
    HRESULT hr;
    DWORD flags;
    int i, ok;

    hr = pWerRegisterMemoryBlock(block, sizeof(block));
    printf("WerRegisterMemoryBlock: %#lx\n", hr);
    check(hr == S_OK, "WerRegisterMemoryBlock succeeds");
    hr = pWerRegisterMemoryBlock(block, sizeof(block));
    check(hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS), "... and refuses the same block twice");
    hr = pWerRegisterMemoryBlock(big, sizeof(big));
    check(hr == E_INVALIDARG, "a block over 64 KB is refused");
    hr = pWerUnregisterMemoryBlock(block);
    check(hr == S_OK, "WerUnregisterMemoryBlock succeeds");
    hr = pWerUnregisterMemoryBlock(block);
    check(hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND), "... and then it is not found");

    hr = pWerRegisterFile(L"C:\\windows\\temp\\app.log", 2, 0);
    check(hr == S_OK, "WerRegisterFile succeeds");
    hr = pWerRegisterFile(L"C:\\windows\\temp\\app2.log", 7, 0);
    check(hr == E_INVALIDARG, "a bad file type is refused");
    hr = pWerUnregisterFile(L"C:\\WINDOWS\\temp\\APP.log");
    check(hr == S_OK, "WerUnregisterFile finds it whatever the case");
    hr = pWerUnregisterFile(L"C:\\windows\\temp\\app.log");
    check(hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND), "... and then it is not found");

    for (i = 0, ok = 1; i < 16; i++) ok = ok && pWerRegisterRuntimeExceptionModule(L"C:\\x\\handler.dll", (void *)(ULONG_PTR)(i + 1)) == S_OK;
    check(ok, "16 runtime exception modules are registered");
    hr = pWerRegisterRuntimeExceptionModule(L"C:\\x\\handler.dll", (void *)100);
    check(hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), "a 17th is refused");
    hr = pWerUnregisterRuntimeExceptionModule(L"C:\\x\\handler.dll", (void *)1);
    check(hr == S_OK, "WerUnregisterRuntimeExceptionModule succeeds");
    hr = pWerUnregisterRuntimeExceptionModule(L"C:\\x\\handler.dll", (void *)1);
    check(hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND), "... and then it is not found");

    pWerSetFlags(4);
    flags = 0;
    hr = pWerGetFlags(GetCurrentProcess(), &flags);
    check(hr == S_OK && flags == 4, "WerGetFlags reads back WerSetFlags");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
