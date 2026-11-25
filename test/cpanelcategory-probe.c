/* cpanelcategory-probe: the Control Panel's category view {26EE0668-A00A-44D7-9371-BEB064C98683} and
 * FOLDERID_ControlPanelFolder ("::{26EE0668-...}\0", All Control Panel Items), for
 * test/cpanelcategory-gate.sh (Stained Glass OS wine-sg 9860). One line per check:
 *   COCREATE hr              CoCreateInstance of the category view as an IShellFolder
 *   KNOWN hr / KENUM hr n    SHGetKnownFolderIDList(FOLDERID_ControlPanelFolder) and its items
 *   PARSE hr                 SHParseDisplayName("::{26EE0668-...}\0")
 *   NAME hr name             its normal display name
 *   PARSING hr name          its desktop-absolute parsing name
 *   ENUM hr n                enumerating it (BHID_EnumItems)
 *   ITEM hr name             its first item's normal name through IShellItem (the parents bind)
 */
#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <knownfolders.h>
#include <stdio.h>

static const GUID clsid_cat = {0x26ee0668, 0xa00a, 0x44d7, {0x93, 0x71, 0xbe, 0xb0, 0x64, 0xc9, 0x86, 0x83}};
static const GUID fid_cpanel = {0x82a74aeb, 0xaeb4, 0x465c, {0xa0, 0x14, 0xd0, 0x97, 0xee, 0x34, 0x6d, 0x63}};
static const GUID bhid_enum = {0x94f60519, 0x2850, 0x4924, {0xaa, 0x5a, 0xd1, 0x5e, 0x84, 0x86, 0x80, 0x39}};

static void name_of(const char *tag, ITEMIDLIST *pidl, SIGDN how)
{
    IShellItem *item;
    WCHAR *name = NULL;
    HRESULT hr = SHCreateItemFromIDList(pidl, &IID_IShellItem, (void **)&item);
    if (SUCCEEDED(hr))
    {
        hr = IShellItem_GetDisplayName(item, how, &name);
        IShellItem_Release(item);
    }
    printf("%s 0x%08lx %ls\n", tag, hr, name ? name : L"-");
    CoTaskMemFree(name);
}

static int enum_items(const char *tag, ITEMIDLIST *pidl, ITEMIDLIST **first)
{
    IShellItem *item;
    IEnumShellItems *e = NULL;
    int count = 0;
    HRESULT hr = SHCreateItemFromIDList(pidl, &IID_IShellItem, (void **)&item);

    if (SUCCEEDED(hr))
    {
        hr = IShellItem_BindToHandler(item, NULL, &bhid_enum, &IID_IEnumShellItems, (void **)&e);
        if (SUCCEEDED(hr))
        {
            IShellItem *child;
            while (IEnumShellItems_Next(e, 1, &child, NULL) == S_OK)
            {
                count++;
                if (first && !*first) SHGetIDListFromObject((IUnknown *)child, first);
                IShellItem_Release(child);
            }
            IEnumShellItems_Release(e);
        }
        IShellItem_Release(item);
    }
    printf("%s 0x%08lx %d\n", tag, hr, count);
    return count;
}

int main(void)
{
    ITEMIDLIST *parsed = NULL, *known = NULL, *first = NULL;
    IShellFolder *sf = NULL;
    HRESULT hr;

    CoInitialize(NULL);
    hr = CoCreateInstance(&clsid_cat, NULL, CLSCTX_INPROC_SERVER, &IID_IShellFolder, (void **)&sf);
    printf("COCREATE 0x%08lx\n", hr);
    if (sf) IShellFolder_Release(sf);

    hr = SHGetKnownFolderIDList(&fid_cpanel, 0, NULL, &known);
    printf("KNOWN 0x%08lx\n", hr);
    if (known) enum_items("KENUM", known, NULL);

    hr = SHParseDisplayName(L"::{26EE0668-A00A-44D7-9371-BEB064C98683}\\0", NULL, &parsed, 0, NULL);
    printf("PARSE 0x%08lx\n", hr);
    if (!parsed) { printf("END\n"); return 0; }
    name_of("NAME", parsed, SIGDN_NORMALDISPLAY);
    name_of("PARSING", parsed, SIGDN_DESKTOPABSOLUTEPARSING);
    enum_items("ENUM", parsed, &first);
    if (!first) { printf("ITEM none\nEND\n"); return 0; }
    name_of("ITEM", first, SIGDN_NORMALDISPLAY);
    printf("END\n");
    return 0;
}
