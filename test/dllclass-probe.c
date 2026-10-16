/* dllclass-gate.sh's probe (5527): DllGetClassObject of DLLs that implement
 * no class must fail with CLASS_E_CLASSNOTAVAILABLE and leave a NULL pointer. */
#include <windows.h>
#include <stdio.h>

static const GUID unknown_clsid = {0x12345678, 0x1234, 0x5678, {1, 2, 3, 4, 5, 6, 7, 8}};
static const GUID iid_classfactory = {0x00000001, 0, 0, {0xc0, 0, 0, 0, 0, 0, 0, 0x46}};

static int bad;

int main(void)
{
    static const char *dlls[] = { "olepro32.dll", "dpnhpast.dll" };
    unsigned int i;

    for (i = 0; i < sizeof(dlls) / sizeof(*dlls); i++)
    {
        HRESULT (WINAPI *get)(const GUID *, const GUID *, void **);
        HMODULE mod = LoadLibraryA(dlls[i]);
        void *ptr = (void *)0xdeadbeef;
        HRESULT hr;

        get = mod ? (void *)GetProcAddress(mod, "DllGetClassObject") : NULL;
        if (!get) { printf("FAIL %s: no DllGetClassObject\n", dlls[i]); bad = 1; continue; }
        hr = get(&unknown_clsid, &iid_classfactory, &ptr);
        if (hr == (HRESULT)0x80040111 && !ptr) printf("ok %s\n", dlls[i]);
        else { printf("FAIL %s: hr %08lx ptr %p\n", dlls[i], hr, ptr); bad = 1; }
    }
    printf(bad ? "dllclass-probe: FAILED\n" : "dllclass-probe: all ok\n");
    return bad;
}
