/* wine-sg 9703: CoGetSystemSecurityPermissions (combase, ole32; Dependencies delay-imports it). The
 * machine-wide COM launch/access permissions and limits: the HKLM\Software\Microsoft\Ole value when set,
 * else the system default; a self-relative descriptor freed with LocalFree. */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <sddl.h>
#include <stdio.h>

static int failures;
#define CHECK(c, ...) do { if (c) printf("PASS  "); else { printf("FAIL  "); failures++; } printf(__VA_ARGS__); printf("\n"); } while (0)
typedef HRESULT (WINAPI *get_t)(int, PSECURITY_DESCRIPTOR *);

static char *sddl_of(PSECURITY_DESCRIPTOR sd)
{
    char *s = NULL;
    ConvertSecurityDescriptorToStringSecurityDescriptorA(sd, SDDL_REVISION_1,
        OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION, &s, NULL);
    return s ? s : "(none)";
}

int main(void)
{
    get_t fn = (get_t)GetProcAddress(LoadLibraryA("ole32.dll"), "CoGetSystemSecurityPermissions");
    get_t fn2 = (get_t)GetProcAddress(LoadLibraryA("combase.dll"), "CoGetSystemSecurityPermissions");
    static const char *want[4] = { ";;;IU)", ";;;PS)", ";;;WD)", ";;;AN)" };
    PSECURITY_DESCRIPTOR sd, custom;
    SECURITY_DESCRIPTOR_CONTROL ctl;
    DWORD rev;
    HRESULT hr;
    HKEY key;
    int t;

    CHECK(fn && fn2, "ole32 and combase export CoGetSystemSecurityPermissions");
    if (!fn) { printf("RESULT: FAIL\n"); return 1; }
    RegCreateKeyExA(HKEY_LOCAL_MACHINE, "Software\\Microsoft\\Ole", 0, NULL, 0, KEY_ALL_ACCESS, NULL, &key, NULL);
    for (t = 0; t < 4; t++) RegDeleteValueA(key, (const char *[]){ "DefaultLaunchPermission", "DefaultAccessPermission",
                                                                 "MachineLaunchRestriction", "MachineAccessRestriction" }[t]);
    for (t = 0; t < 4; t++)
    {
        char *s;
        sd = NULL;
        hr = fn(t, &sd);
        s = sd ? sddl_of(sd) : "";
        CHECK(hr == S_OK && sd && IsValidSecurityDescriptor(sd) && GetSecurityDescriptorControl(sd, &ctl, &rev) &&
              (ctl & SE_SELF_RELATIVE) && (ctl & SE_DACL_PRESENT) && strstr(s, want[t]) && strstr(s, "O:BA"),
              "type %d default: hr %#lx, %s", t, hr, s);
        if (sd) CHECK(LocalFree(sd) == NULL, "type %d frees with LocalFree", t);
    }
    hr = fn(4, &sd);
    CHECK(hr == E_INVALIDARG, "type 4: E_INVALIDARG (got %#lx)", hr);
    hr = fn(0, NULL);
    CHECK(hr == E_INVALIDARG, "NULL out pointer: E_INVALIDARG (got %#lx)", hr);

    ConvertStringSecurityDescriptorToSecurityDescriptorA("O:SYG:SYD:(A;;0x7;;;WD)", SDDL_REVISION_1, &custom, NULL);
    RegSetValueExA(key, "DefaultAccessPermission", 0, REG_BINARY, custom, GetSecurityDescriptorLength(custom));
    sd = NULL;
    hr = fn2(SD_ACCESSPERMISSIONS, &sd);
    CHECK(hr == S_OK && sd && !strcmp(sddl_of(sd), "O:SYG:SYD:(A;;CCDCLC;;;WD)") ,
          "the machine's DefaultAccessPermission value is what comes back: %s", sd ? sddl_of(sd) : "");
    if (sd) LocalFree(sd);
    RegSetValueExA(key, "DefaultLaunchPermission", 0, REG_BINARY, (const BYTE *)"junk", 4);
    sd = NULL;
    hr = fn2(SD_LAUNCHPERMISSIONS, &sd);
    CHECK(hr == S_OK && sd && strstr(sddl_of(sd), ";;;IU)"), "a value that is no descriptor: the default (%s)", sd ? sddl_of(sd) : "");
    RegDeleteValueA(key, "DefaultAccessPermission");
    RegDeleteValueA(key, "DefaultLaunchPermission");
    RegCloseKey(key);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
