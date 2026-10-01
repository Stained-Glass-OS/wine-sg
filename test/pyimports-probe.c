/* pywin32's imports (wine-sg 0621): looked up by name as a UPX-packed
 * module's own loader does, then used. */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <stdio.h>

static const GUID iid_extract_iconw = { 0x000214fa, 0, 0, { 0xc0, 0, 0, 0, 0, 0, 0, 0x46 } };
static const GUID iid_default_extract_icon_init = { 0x41ded17d, 0xd6b3, 0x4261, { 0x99, 0x7d, 0x88, 0xc6, 0x0e, 0x4b, 0x1d, 0x58 } };

/* IExtractIconW as it is laid out (mingw's C declaration has no IUnknown slots) */
typedef struct extract_iconw extract_iconw;
struct extract_iconw_vtbl
{
    HRESULT (WINAPI *QueryInterface)(extract_iconw *, REFIID, void **);
    ULONG (WINAPI *AddRef)(extract_iconw *);
    ULONG (WINAPI *Release)(extract_iconw *);
    HRESULT (WINAPI *GetIconLocation)(extract_iconw *, UINT, LPWSTR, UINT, int *, UINT *);
    HRESULT (WINAPI *Extract)(extract_iconw *, LPCWSTR, UINT, HICON *, HICON *, UINT);
};
struct extract_iconw { const struct extract_iconw_vtbl *lpVtbl; };

static const struct { const char *dll, *name; } exports[] =
{
    { "ole32", "CoSetCancelObject" }, { "ole32", "CoGetCancelObject" }, { "ole32", "CoTestCancel" }, { "ole32", "CoCancelCall" },
    { "kernel32", "CopyFileTransactedW" }, { "kernel32", "CreateSymbolicLinkTransactedW" },
    { "kernel32", "FindFirstFileNameTransactedW" }, { "kernel32", "FindFirstStreamTransactedW" },
    { "kernel32", "GetFullPathNameTransactedA" }, { "kernel32", "GetFullPathNameTransactedW" },
    { "kernel32", "GetLongPathNameTransactedW" }, { "kernel32", "SetFileAttributesTransactedW" },
    { "kernel32", "GetCompressedFileSizeTransactedW" },
    { "advapi32", "AddUsersToEncryptedFile" }, { "advapi32", "DuplicateEncryptionInfoFile" },
    { "advapi32", "EncryptionDisable" }, { "advapi32", "FreeEncryptionCertificateHashList" },
    { "advapi32", "LogonUserExW" }, { "advapi32", "QueryRecoveryAgentsOnEncryptedFile" },
    { "advapi32", "QueryUsersOnEncryptedFile" }, { "advapi32", "RemoveUsersFromEncryptedFile" },
    { "shell32", "AssocCreateForClasses" }, { "shell32", "SHCreateDefaultExtractIcon" },
};

/* a cancel object */
struct canceller { ICancelMethodCalls iface; LONG ref; ULONG cancelled; };
static HRESULT WINAPI c_qi(ICancelMethodCalls *i, REFIID riid, void **o)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_ICancelMethodCalls)) { *o = i; ICancelMethodCalls_AddRef(i); return S_OK; }
    *o = NULL; return E_NOINTERFACE;
}
static ULONG WINAPI c_addref(ICancelMethodCalls *i) { return InterlockedIncrement(&((struct canceller *)i)->ref); }
static ULONG WINAPI c_release(ICancelMethodCalls *i) { return InterlockedDecrement(&((struct canceller *)i)->ref); }
static HRESULT WINAPI c_cancel(ICancelMethodCalls *i, ULONG s) { ((struct canceller *)i)->cancelled = s + 1; return S_OK; }
static HRESULT WINAPI c_test(ICancelMethodCalls *i) { return ((struct canceller *)i)->cancelled ? RPC_E_CALL_CANCELED : RPC_S_CALLPENDING; }
static ICancelMethodCallsVtbl c_vtbl = { c_qi, c_addref, c_release, c_cancel, c_test };

typedef HRESULT (WINAPI *setcancel_fn)(IUnknown *);
typedef HRESULT (WINAPI *getcancel_fn)(DWORD, REFIID, void **);
typedef HRESULT (WINAPI *testcancel_fn)(void);
typedef HRESULT (WINAPI *cancelcall_fn)(DWORD, ULONG);
static cancelcall_fn cancelcall;
static DWORD main_tid;
static DWORD WINAPI canceller_thread(void *arg) { return cancelcall(main_tid, 7); }

typedef DWORD (WINAPI *fullpath_fn)(LPCWSTR, DWORD, LPWSTR, LPWSTR *, HANDLE);
typedef BOOL (WINAPI *copytx_fn)(LPCWSTR, LPCWSTR, LPPROGRESS_ROUTINE, LPVOID, LPBOOL, DWORD, HANDLE);
typedef DWORD (WINAPI *queryusers_fn)(LPCWSTR, void **);
typedef BOOL (WINAPI *encdisable_fn)(LPCWSTR, BOOL);
typedef HRESULT (WINAPI *assocclasses_fn)(const ASSOCIATIONELEMENT *, ULONG, REFIID, void **);
typedef HRESULT (WINAPI *defextract_fn)(REFIID, void **);

int main(void)
{
    unsigned i, found = 0;
    HMODULE ole32 = LoadLibraryA("ole32"), k32 = LoadLibraryA("kernel32"), adv = LoadLibraryA("advapi32"), sh = LoadLibraryA("shell32");
    char missing[512] = "";
    struct canceller c = { { &c_vtbl }, 1, 0 };
    ICancelMethodCalls *got = NULL;
    HANDLE thread;
    DWORD code;
    HRESULT hr;

    for (i = 0; i < ARRAYSIZE(exports); i++)
        if (GetProcAddress(GetModuleHandleA(exports[i].dll), exports[i].name)) found++;
        else { strcat(missing, " "); strcat(missing, exports[i].name); }
    printf("exports=%u/%u%s\n", found, (unsigned)ARRAYSIZE(exports), missing);
    if (found != ARRAYSIZE(exports)) return 1;

    /* COM cancel objects: this thread's, reached from another thread too */
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    main_tid = GetCurrentThreadId();
    cancelcall = (cancelcall_fn)GetProcAddress(ole32, "CoCancelCall");
    hr = ((getcancel_fn)GetProcAddress(ole32, "CoGetCancelObject"))(0, &IID_ICancelMethodCalls, (void **)&got);
    printf("cancel-none=%#lx\n", hr);
    hr = ((setcancel_fn)GetProcAddress(ole32, "CoSetCancelObject"))((IUnknown *)&c.iface);
    printf("cancel-set=%#lx ref=%ld\n", hr, c.ref);
    printf("cancel-pending=%#lx\n", ((testcancel_fn)GetProcAddress(ole32, "CoTestCancel"))());
    thread = CreateThread(NULL, 0, canceller_thread, NULL, 0, NULL);
    WaitForSingleObject(thread, 5000);
    GetExitCodeThread(thread, &code);
    printf("cancel-call=%#lx seconds=%lu\n", code, c.cancelled ? c.cancelled - 1 : 999);
    printf("cancel-test=%#lx\n", ((testcancel_fn)GetProcAddress(ole32, "CoTestCancel"))());
    ((setcancel_fn)GetProcAddress(ole32, "CoSetCancelObject"))(NULL);
    printf("cancel-cleared=%#lx ref=%ld\n", ((testcancel_fn)GetProcAddress(ole32, "CoTestCancel"))(), c.ref);

    /* transacted file calls do the plain call */
    {
        WCHAR a[MAX_PATH], b[MAX_PATH], src[MAX_PATH], dst[MAX_PATH];
        GetFullPathNameW(L"x\\y.txt", MAX_PATH, a, NULL);
        ((fullpath_fn)GetProcAddress(k32, "GetFullPathNameTransactedW"))(L"x\\y.txt", MAX_PATH, b, NULL, NULL);
        printf("tx-fullpath=%s\n", !wcscmp(a, b) ? "same" : "differs");
        GetTempPathW(MAX_PATH, a);
        swprintf(src, MAX_PATH, L"%lssg-tx-src.txt", a);
        swprintf(dst, MAX_PATH, L"%lssg-tx-dst.txt", a);
        CloseHandle(CreateFileW(src, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL));
        DeleteFileW(dst);
        {
            BOOL copied = ((copytx_fn)GetProcAddress(k32, "CopyFileTransactedW"))(src, dst, NULL, NULL, NULL, 0, NULL);
            printf("tx-copy=%d exists=%d\n", copied, GetFileAttributesW(dst) != INVALID_FILE_ATTRIBUTES);
        }

        /* EFS: nothing is encrypted */
        {
            void *list = (void *)1;
            DWORD e1 = ((queryusers_fn)GetProcAddress(adv, "QueryUsersOnEncryptedFile"))(src, &list);
            DWORD e2 = ((queryusers_fn)GetProcAddress(adv, "QueryUsersOnEncryptedFile"))(L"C:\\no-such-file.sg", &list);
            WCHAR dir[MAX_PATH], ini[MAX_PATH], v[8];
            printf("efs-users=%lu list=%p missing=%lu\n", e1, list, e2);
            swprintf(dir, MAX_PATH, L"%lssg-efs-dir", a);
            CreateDirectoryW(dir, NULL);
            swprintf(ini, MAX_PATH, L"%ls\\Desktop.ini", dir);
            ((encdisable_fn)GetProcAddress(adv, "EncryptionDisable"))(dir, TRUE);
            GetPrivateProfileStringW(L"Encryption", L"Disable", L"", v, 8, ini);
            printf("efs-disable=%ls\n", v);
        }
    }

    /* the classes' associations, in order */
    {
        HKEY key;
        WCHAR out[MAX_PATH];
        DWORD len = MAX_PATH;
        IQueryAssociations *qa = NULL;
        ASSOCIATIONELEMENT two[2] = { { ASSOCCLASS_PROGID_STR, NULL, L"SG.Missing" }, { ASSOCCLASS_PROGID_STR, NULL, L"SG.TestProg" } };
        assocclasses_fn create = (assocclasses_fn)GetProcAddress(sh, "AssocCreateForClasses");

        RegCreateKeyExW(HKEY_CLASSES_ROOT, L"SG.TestProg\\shell\\open\\command", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
        RegSetValueExW(key, NULL, 0, REG_SZ, (BYTE *)L"sgtest.exe \"%1\"", sizeof(L"sgtest.exe \"%1\""));
        RegCloseKey(key);
        hr = create(two, 2, &IID_IQueryAssociations, (void **)&qa);
        out[0] = 0;
        if (SUCCEEDED(hr)) hr = qa->lpVtbl->GetString(qa, 0, ASSOCSTR_COMMAND, L"open", out, &len);
        printf("assoc-second=%#lx %ls\n", hr, out);
        if (qa) qa->lpVtbl->Release(qa);
        qa = NULL; len = MAX_PATH;
        hr = create(two, 1, &IID_IQueryAssociations, (void **)&qa);
        if (SUCCEEDED(hr)) hr = qa->lpVtbl->GetString(qa, 0, ASSOCSTR_COMMAND, L"open", out, &len);
        printf("assoc-missing=%s\n", FAILED(hr) ? "fails" : "found");
        if (qa) qa->lpVtbl->Release(qa);
    }

    /* a default extract-icon object */
    {
        IDefaultExtractIconInit *init = NULL;
        extract_iconw *icon = NULL;
        WCHAR file[MAX_PATH];
        int index = -1;
        UINT flags = 0;

        hr = ((defextract_fn)GetProcAddress(sh, "SHCreateDefaultExtractIcon"))(&iid_default_extract_icon_init, (void **)&init);
        printf("icon-create=%#lx\n", hr);
        if (SUCCEEDED(hr))
        {
            IDefaultExtractIconInit_SetFlags(init, GIL_PERCLASS);
            IDefaultExtractIconInit_SetNormalIcon(init, L"C:\\normal.dll", 3);
            IDefaultExtractIconInit_SetOpenIcon(init, L"C:\\open.dll", 4);
            IDefaultExtractIconInit_QueryInterface(init, &iid_extract_iconw, (void **)&icon);
            icon->lpVtbl->GetIconLocation(icon, 0, file, MAX_PATH, &index, &flags);
            printf("icon-normal=%ls,%d flags=%#x\n", file, index, flags);
            icon->lpVtbl->GetIconLocation(icon, GIL_OPENICON, file, MAX_PATH, &index, &flags);
            printf("icon-open=%ls,%d\n", file, index);
            icon->lpVtbl->GetIconLocation(icon, GIL_FORSHORTCUT, file, MAX_PATH, &index, &flags);
            printf("icon-shortcut=%ls,%d\n", file, index);
            icon->lpVtbl->Release(icon);
            IDefaultExtractIconInit_Release(init);
        }
    }
    return 0;
}
