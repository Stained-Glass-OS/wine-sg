#include <windows.h>
#include <stdio.h>
#define COBJMACROS
#include <ole2.h>
#include <msctf.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL %s:%d ", #c, __LINE__); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static const GUID CLSID_Profiles = { 0x33c53a50, 0xf456, 0x4884, { 0xb0, 0x49, 0x85, 0xfd, 0x64, 0x3e, 0xcf, 0xed } };
static const GUID IID_Mgr = { 0x71c6e74c, 0x0f28, 0x11d8, { 0xa8, 0x2a, 0x00, 0x06, 0x5b, 0x84, 0x43, 0x5c } };
static const GUID CAT_KEYBOARD = { 0x34745c63, 0xb2f0, 0x4784, { 0x8b, 0x67, 0x5e, 0x12, 0xc8, 0x70, 0x1a, 0x31 } };
static const GUID CAT_OTHER = { 0x534c48c1, 0x0607, 0x4098, { 0xa5, 0x21, 0x4f, 0xc8, 0x99, 0xc7, 0x3e, 0x90 } };

int main(void)
{
    ITfInputProcessorProfileMgr *mgr;
    IEnumTfInputProcessorProfiles *en, *clone;
    TF_INPUTPROCESSORPROFILE p, list[16];
    ULONG n, total = 0;
    HKL layouts[16];
    UINT nl, i;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    hr = CoCreateInstance(&CLSID_Profiles, NULL, CLSCTX_INPROC_SERVER, &IID_Mgr, (void **)&mgr);
    if (hr != S_OK) { printf("FAIL create %lx\nRESULT: FAIL\n", hr); return 1; }
    nl = GetKeyboardLayoutList(16, layouts);
    CHECK(nl > 0, "layouts");

    hr = ITfInputProcessorProfileMgr_EnumProfiles(mgr, 0, &en);
    CHECK(hr == S_OK, "enum %lx", hr);
    hr = IEnumTfInputProcessorProfiles_Next(en, 16, list, &total);
    CHECK(total == nl, "total %lu vs %u", total, nl);
    CHECK(hr == (total == 16 ? S_OK : S_FALSE), "next hr %lx", hr);
    for (i = 0; i < total; i++)
    {
        CHECK(list[i].dwProfileType == TF_PROFILETYPE_KEYBOARDLAYOUT, "type %lu", list[i].dwProfileType);
        CHECK(list[i].hkl == layouts[i], "hkl %p vs %p", list[i].hkl, layouts[i]);
        CHECK(list[i].langid == LOWORD((ULONG_PTR)layouts[i]), "langid");
        CHECK(IsEqualGUID(&list[i].catid, &CAT_KEYBOARD), "catid");
        CHECK(list[i].dwFlags & 2, "enabled");
        CHECK(!!(list[i].dwFlags & 1) == (layouts[i] == GetKeyboardLayout(0)), "active flag");
    }
    hr = IEnumTfInputProcessorProfiles_Next(en, 1, &p, &n);
    CHECK(hr == S_FALSE && n == 0, "exhausted %lx %lu", hr, n);
    hr = IEnumTfInputProcessorProfiles_Reset(en);
    CHECK(hr == S_OK, "reset %lx", hr);
    hr = IEnumTfInputProcessorProfiles_Skip(en, 1);
    CHECK(hr == (nl >= 1 ? S_OK : S_FALSE), "skip %lx", hr);
    hr = IEnumTfInputProcessorProfiles_Clone(en, &clone);
    CHECK(hr == S_OK, "clone %lx", hr);
    if (hr == S_OK)
    {
        ULONG left = 0;
        IEnumTfInputProcessorProfiles_Next(clone, 16, list, &left);
        CHECK(left == nl - 1, "clone remaining %lu", left);
        IEnumTfInputProcessorProfiles_Release(clone);
    }
    IEnumTfInputProcessorProfiles_Release(en);

    hr = ITfInputProcessorProfileMgr_EnumProfiles(mgr, 0x1234, &en);
    n = 0;
    IEnumTfInputProcessorProfiles_Next(en, 1, &p, &n);
    CHECK(n == 0, "no profile for unknown langid");
    IEnumTfInputProcessorProfiles_Release(en);

    memset(&p, 0xcc, sizeof(p));
    hr = ITfInputProcessorProfileMgr_GetActiveProfile(mgr, &CAT_KEYBOARD, &p);
    CHECK(hr == S_OK, "active %lx", hr);
    CHECK(p.hkl == GetKeyboardLayout(0) && (p.dwFlags & 1), "active hkl");
    hr = ITfInputProcessorProfileMgr_GetActiveProfile(mgr, &CAT_OTHER, &p);
    CHECK(hr == S_FALSE, "other cat %lx", hr);
    hr = ITfInputProcessorProfileMgr_GetActiveProfile(mgr, &CAT_KEYBOARD, NULL);
    CHECK(hr == E_INVALIDARG, "null %lx", hr);

    hr = ITfInputProcessorProfileMgr_GetProfile(mgr, TF_PROFILETYPE_KEYBOARDLAYOUT, 0, &GUID_NULL, &GUID_NULL, layouts[0], &p);
    CHECK(hr == S_OK && p.hkl == layouts[0], "getprofile %lx", hr);
    hr = ITfInputProcessorProfileMgr_GetProfile(mgr, TF_PROFILETYPE_KEYBOARDLAYOUT, 0, &GUID_NULL, &GUID_NULL, (HKL)(ULONG_PTR)0x12340001, &p);
    CHECK(hr == E_FAIL, "unknown hkl %lx", hr);
    hr = ITfInputProcessorProfileMgr_GetProfile(mgr, 7, 0, &GUID_NULL, &GUID_NULL, layouts[0], &p);
    CHECK(hr == E_INVALIDARG, "bad type %lx", hr);

    ITfInputProcessorProfileMgr_Release(mgr);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
