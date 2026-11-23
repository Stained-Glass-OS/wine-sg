/* commonfav-probe: CSIDL_COMMON_FAVORITES is the caller's own Favorites, as on Windows
 * Vista and later (patches/sg/9102); FOLDERID_Favorites is the same folder. */
#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <wchar.h>

static int fails;
static void check(int ok, const char *what, const WCHAR *got)
{
    printf("%s %s: %ls\n", ok ? "ok  " : "FAIL", what, got ? got : L"(none)");
    if (!ok) fails++;
}

int main(void)
{
    WCHAR fav[MAX_PATH] = {0}, common[MAX_PATH] = {0}, profile[MAX_PATH] = {0};
    WCHAR *known = NULL;
    HRESULT hr;

    hr = SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, SHGFP_TYPE_CURRENT, profile);
    check(hr == S_OK, "CSIDL_PROFILE", profile);
    hr = SHGetFolderPathW(NULL, CSIDL_FAVORITES, NULL, SHGFP_TYPE_CURRENT, fav);
    check(hr == S_OK && !_wcsnicmp(fav, profile, wcslen(profile)), "CSIDL_FAVORITES is in the caller's profile", fav);
    hr = SHGetFolderPathW(NULL, CSIDL_COMMON_FAVORITES, NULL, SHGFP_TYPE_CURRENT, common);
    check(hr == S_OK && !_wcsicmp(common, fav), "CSIDL_COMMON_FAVORITES is the caller's own Favorites", common);
    hr = SHGetFolderPathW(NULL, CSIDL_COMMON_FAVORITES | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, common);
    check(hr == S_OK && !_wcsicmp(common, fav), "...with CSIDL_FLAG_CREATE too", common);
    hr = SHGetKnownFolderPath(&FOLDERID_Favorites, 0, NULL, &known);
    check(hr == S_OK && known && !_wcsicmp(known, fav), "FOLDERID_Favorites", known);
    CoTaskMemFree(known);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
