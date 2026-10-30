/* applang-probe: Windows.Globalization.ApplicationLanguages (patches/sg/6206).
 *
 * The class did not exist (CLASS_E_CLASSNOTAVAILABLE): React Native for Windows (OneDrive) asks it
 * for the languages to show itself in and stopped. Checks Languages (the user's UI languages, an
 * override first), ManifestLanguages, PrimaryLanguageOverride (kept, cleared by an empty string, a
 * bad tag refused) and GetLanguagesForUser.
 * Interface identity per Wine's windows.globalization IDL.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <winstring.h>
#include <roapi.h>
#include <inspectable.h>
#include <stdio.h>

DEFINE_GUID(IID_AppLanguagesStatics, 0x75b40847, 0x0a4c, 0x4a92, 0x95, 0x65, 0xfd, 0x63, 0xc9, 0x5f, 0x7a, 0xed);
DEFINE_GUID(IID_AppLanguagesStatics2, 0x1df0de4f, 0x072b, 0x4d7b, 0x8f, 0x06, 0xcb, 0x2d, 0xb4, 0x0f, 0x2b, 0xb5);

typedef struct { void **vtbl; } obj;
#define SLOT(o, n) ((o)->vtbl[n])
#define CALL(o, n, type) ((type)SLOT(o, n))

static HSTRING hs(const WCHAR *s) { HSTRING h; WindowsCreateString(s, lstrlenW(s), &h); return h; }

/* a view's size and its first item */
static UINT32 first_of(obj *view, WCHAR *first)
{
    UINT32 size = 0;
    HSTRING item = NULL;

    first[0] = 0;
    if (!view) return 0;
    CALL(view, 7, HRESULT (WINAPI *)(obj *, UINT32 *))(view, &size);
    if (size && SUCCEEDED(CALL(view, 6, HRESULT (WINAPI *)(obj *, UINT32, HSTRING *))(view, 0, &item)))
        lstrcpynW(first, WindowsGetStringRawBuffer(item, NULL), 64);
    return size;
}

int main(void)
{
    obj *statics = NULL, *statics2 = NULL, *view = NULL;
    WCHAR first[64], ui[64] = L"";
    ULONG count = 0, size = ARRAYSIZE(ui);
    HSTRING str = NULL;
    UINT32 n;
    HRESULT hr;

    RoInitialize(RO_INIT_MULTITHREADED);
    hr = RoGetActivationFactory(hs(L"Windows.Globalization.ApplicationLanguages"), &IID_AppLanguagesStatics, (void **)&statics);
    printf("statics=%08lx\n", hr);
    if (!statics) return 1;
    GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &count, ui, &size);

    CALL(statics, 8, HRESULT (WINAPI *)(obj *, obj **))(statics, &view);           /* Languages */
    n = first_of(view, first);
    printf("languages=%d first_is_ui=%d\n", n > 0, !lstrcmpiW(first, ui));
    view = NULL;
    CALL(statics, 9, HRESULT (WINAPI *)(obj *, obj **))(statics, &view);           /* ManifestLanguages */
    printf("manifest=%d\n", first_of(view, first) > 0);

    hr = CALL(statics, 7, HRESULT (WINAPI *)(obj *, HSTRING))(statics, hs(L"fr-FR")); /* PrimaryLanguageOverride */
    CALL(statics, 6, HRESULT (WINAPI *)(obj *, HSTRING *))(statics, &str);
    view = NULL;
    CALL(statics, 8, HRESULT (WINAPI *)(obj *, obj **))(statics, &view);
    first_of(view, first);
    printf("override=%08lx,%ls,%ls\n", hr, WindowsGetStringRawBuffer(str, NULL), first);
    hr = CALL(statics, 7, HRESULT (WINAPI *)(obj *, HSTRING))(statics, hs(L"not a tag!"));
    printf("bad_tag=%08lx\n", hr);
    CALL(statics, 7, HRESULT (WINAPI *)(obj *, HSTRING))(statics, NULL);           /* cleared */
    str = NULL;
    CALL(statics, 6, HRESULT (WINAPI *)(obj *, HSTRING *))(statics, &str);
    view = NULL;
    CALL(statics, 8, HRESULT (WINAPI *)(obj *, obj **))(statics, &view);
    first_of(view, first);
    printf("cleared=%d,%d\n", WindowsGetStringLen(str) == 0, !lstrcmpiW(first, ui));

    hr = RoGetActivationFactory(hs(L"Windows.Globalization.ApplicationLanguages"), &IID_AppLanguagesStatics2, (void **)&statics2);
    view = NULL;
    if (statics2) CALL(statics2, 6, HRESULT (WINAPI *)(obj *, void *, obj **))(statics2, NULL, &view);
    printf("for_user=%d\n", first_of(view, first) > 0);
    printf("done=1\n");
    return 0;
}
