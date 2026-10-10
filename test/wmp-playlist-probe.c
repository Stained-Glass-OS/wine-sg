/* wmp's media and playlist objects and the playlist side of the control:
 * IWMPPlaylist (items, insert/append/remove/move/clear, attributes,
 * isIdentical), IWMPMedia (attributes, markers, durationString, isIdentical,
 * isMemberOf, read-only items), IWMPPlayer4::put_currentPlaylist and
 * IWMPControls next/previous/currentItem/playItem, run by
 * test/wmp-playlist-gate.sh (patches/sg/2852).
 *
 *   wmp-playlist-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <wmp.h>
#include <stdio.h>
#include <string.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define NS_S_COMMAND_NOT_AVAILABLE ((HRESULT)0x000d1105)

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[320]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static BOOL bstr_is(BSTR b, const WCHAR *s) { return b ? !wcscmp(b, s) : !*s; }
static LONG refs_of(IUnknown *u) { IUnknown_AddRef(u); return IUnknown_Release(u); }

static void pump(DWORD ms)
{
    DWORD end = GetTickCount() + ms;
    MSG msg;
    while (GetTickCount() < end)
    {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
        Sleep(10);
    }
}

static void write_wave(const WCHAR *path, unsigned int seconds)
{
    static const BYTE hdr[] = {'R','I','F','F',0,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,1,0,0x40,0x1f,0,0,0x40,0x1f,0,0,1,0,8,0,'d','a','t','a',0,0,0,0};
    BYTE h[sizeof(hdr)];
    DWORD size = 8000 * seconds, w, n;
    BYTE *data = malloc(size);
    HANDLE f;

    memcpy(h, hdr, sizeof(h));
    n = size + 36; memcpy(h + 4, &n, 4);
    memcpy(h + 40, &size, 4);
    memset(data, 0x80, size);
    f = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    WriteFile(f, h, sizeof(h), &w, NULL);
    WriteFile(f, data, size, &w, NULL);
    CloseHandle(f);
    free(data);
}

static IWMPMedia *new_media(IWMPPlayer4 *player, const WCHAR *url)
{
    IWMPMedia *m = NULL;
    BSTR b = SysAllocString(url);
    IWMPPlayer4_newMedia(player, b, &m);
    SysFreeString(b);
    return m;
}

static BOOL same_item(IWMPMedia *a, IWMPMedia *b)
{
    VARIANT_BOOL vb = VARIANT_FALSE;
    return a && b && SUCCEEDED(IWMPMedia_get_isIdentical(a, b, &vb)) && vb == VARIANT_TRUE;
}

static BSTR info(IWMPMedia *m, const WCHAR *name)
{
    BSTR n = SysAllocString(name), v = NULL;
    IWMPMedia_getItemInfo(m, n, &v);
    SysFreeString(n);
    return v;
}

static void test_media(IWMPPlayer4 *player)
{
    IWMPMedia *m, *same, *other, *dupe;
    IWMPPlaylist *pl, *pl2;
    VARIANT_BOOL vb;
    BSTR b, n, v, name;
    LONG l;
    DOUBLE d;
    HRESULT hr;
    unsigned int i;

    m = new_media(player, L"C:\\music\\song.mp3");
    same = new_media(player, L"c:\\MUSIC\\song.mp3");
    other = new_media(player, L"C:\\music\\other.mp3");
    dupe = m;

    /* identity */
    vb = 5; hr = IWMPMedia_get_isIdentical(m, m, &vb);
    CHECKF(hr == S_OK && vb == VARIANT_TRUE, "media is identical to itself (%08lx %d)", hr, vb);
    vb = 5; hr = IWMPMedia_get_isIdentical(m, same, &vb);
    CHECKF(hr == S_OK && vb == VARIANT_TRUE, "media of the same address is identical (%08lx %d)", hr, vb);
    vb = 5; hr = IWMPMedia_get_isIdentical(m, other, &vb);
    CHECKF(hr == S_OK && vb == VARIANT_FALSE, "media of another address is not identical (%08lx %d)", hr, vb);
    vb = 5; hr = IWMPMedia_get_isIdentical(m, NULL, &vb);
    CHECKF(hr == E_INVALIDARG && vb == VARIANT_FALSE, "isIdentical(NULL) = E_INVALIDARG (%08lx)", hr);
    hr = IWMPMedia_get_isIdentical(m, other, NULL);
    CHECKF(hr == E_POINTER, "isIdentical(.., NULL) = E_POINTER (%08lx)", hr);

    /* attributes: the built-ins first, then the ones that were set */
    l = 0; hr = IWMPMedia_get_attributeCount(m, &l);
    CHECKF(hr == S_OK && l == 3, "three built-in attributes (%08lx %ld)", hr, l);
    {
        static const WCHAR *builtin[] = {L"SourceURL", L"Title", L"Duration"};
        for (i = 0; i < 3; i++)
        {
            b = NULL; hr = IWMPMedia_getAttributeName(m, i, &b);
            CHECKF(hr == S_OK && bstr_is(b, builtin[i]), "attribute %u is %ls (%08lx)", i, builtin[i], hr);
            SysFreeString(b);
        }
    }
    b = info(m, L"SourceURL");
    CHECKF(bstr_is(b, L"C:\\music\\song.mp3"), "SourceURL attribute");
    SysFreeString(b);
    b = info(m, L"title");
    CHECKF(bstr_is(b, L"song"), "Title attribute (case-insensitive name)");
    SysFreeString(b);
    b = info(m, L"Duration");
    CHECKF(bstr_is(b, L"0"), "Duration attribute of an unplayed item is 0");
    SysFreeString(b);
    b = info(m, L"NoSuchAttribute");
    CHECKF(bstr_is(b, L""), "an unknown attribute is empty");
    SysFreeString(b);

    n = SysAllocString(L"Artist"); v = SysAllocString(L"Somebody");
    hr = IWMPMedia_setItemInfo(m, n, v);
    CHECKF(hr == S_OK, "setItemInfo(Artist) (%08lx)", hr);
    SysFreeString(v);
    b = info(m, L"ARTIST");
    CHECKF(bstr_is(b, L"Somebody"), "Artist reads back");
    SysFreeString(b);
    IWMPMedia_get_attributeCount(m, &l);
    CHECKF(l == 4, "a new attribute is counted (%ld)", l);
    b = NULL; IWMPMedia_getAttributeName(m, 3, &b);
    CHECKF(bstr_is(b, L"Artist"), "the new attribute is listed after the built-ins");
    SysFreeString(b);
    v = SysAllocString(L"Somebody Else");
    IWMPMedia_setItemInfo(m, n, v);
    SysFreeString(v);
    IWMPMedia_get_attributeCount(m, &l);
    b = info(m, L"Artist");
    CHECKF(l == 4 && bstr_is(b, L"Somebody Else"), "setting an attribute again replaces it (%ld)", l);
    SysFreeString(b);
    hr = IWMPMedia_setItemInfo(m, n, NULL);
    b = info(m, L"Artist");
    CHECKF(hr == S_OK && bstr_is(b, L""), "a NULL value is the empty string");
    SysFreeString(b);
    SysFreeString(n);
    /* other items do not see it */
    b = info(same, L"Artist");
    CHECKF(bstr_is(b, L""), "attributes belong to the item");
    SysFreeString(b);
    hr = IWMPMedia_getAttributeName(m, 4, &b);
    CHECKF(hr == E_INVALIDARG && !b, "attribute index past the end = E_INVALIDARG (%08lx)", hr);
    hr = IWMPMedia_getAttributeName(m, -1, &b);
    CHECKF(hr == E_INVALIDARG, "attribute index -1 = E_INVALIDARG (%08lx)", hr);
    hr = IWMPMedia_getAttributeName(m, 0, NULL);
    CHECKF(hr == E_POINTER, "getAttributeName(NULL) = E_POINTER");
    hr = IWMPMedia_get_attributeCount(m, NULL);
    CHECKF(hr == E_POINTER, "attributeCount(NULL) = E_POINTER");

    /* Title is the name */
    n = SysAllocString(L"Title"); v = SysAllocString(L"A Better Name");
    hr = IWMPMedia_setItemInfo(m, n, v);
    CHECKF(hr == S_OK, "setItemInfo(Title) (%08lx)", hr);
    b = NULL; IWMPMedia_get_name(m, &b);
    CHECKF(bstr_is(b, L"A Better Name"), "Title sets the name");
    SysFreeString(b);
    SysFreeString(n); SysFreeString(v);
    name = SysAllocString(L"Renamed");
    IWMPMedia_put_name(m, name);
    SysFreeString(name);
    b = info(m, L"Title");
    CHECKF(bstr_is(b, L"Renamed"), "the name is the Title attribute");
    SysFreeString(b);
    IWMPMedia_get_attributeCount(m, &l);
    CHECKF(l == 4, "Title is not counted twice (%ld)", l);

    /* read-only items */
    {
        static const struct { const WCHAR *name; VARIANT_BOOL ro; } items[] = {
            {L"SourceURL", VARIANT_TRUE}, {L"duration", VARIANT_TRUE}, {L"Title", VARIANT_FALSE}, {L"Artist", VARIANT_FALSE}, {L"Unknown", VARIANT_FALSE}};
        for (i = 0; i < ARRAY_SIZE(items); i++)
        {
            n = SysAllocString(items[i].name);
            vb = 5; hr = IWMPMedia_isReadOnlyItem(m, n, &vb);
            CHECKF(hr == S_OK && vb == items[i].ro, "isReadOnlyItem(%ls) = %d (%08lx %d)", items[i].name, items[i].ro, hr, vb);
            SysFreeString(n);
        }
        n = SysAllocString(L"SourceURL"); v = SysAllocString(L"C:\\x.mp3");
        hr = IWMPMedia_setItemInfo(m, n, v);
        CHECKF(hr == E_ACCESSDENIED, "setItemInfo(SourceURL) = E_ACCESSDENIED (%08lx)", hr);
        SysFreeString(n);
        n = SysAllocString(L"Duration");
        hr = IWMPMedia_setItemInfo(m, n, v);
        CHECKF(hr == E_ACCESSDENIED, "setItemInfo(Duration) = E_ACCESSDENIED (%08lx)", hr);
        SysFreeString(n); SysFreeString(v);
        b = info(m, L"SourceURL");
        CHECKF(bstr_is(b, L"C:\\music\\song.mp3"), "the refused SourceURL is unchanged");
        SysFreeString(b);
        hr = IWMPMedia_setItemInfo(m, NULL, NULL);
        CHECKF(hr == E_INVALIDARG, "setItemInfo(NULL) = E_INVALIDARG (%08lx)", hr);
        hr = IWMPMedia_isReadOnlyItem(m, NULL, &vb);
        CHECKF(hr == E_INVALIDARG, "isReadOnlyItem(NULL) = E_INVALIDARG (%08lx)", hr);
        hr = IWMPMedia_getItemInfo(m, NULL, &b);
        CHECKF(hr == E_INVALIDARG, "getItemInfo(NULL) = E_INVALIDARG (%08lx)", hr);
    }
    b = (BSTR)1; hr = IWMPMedia_getItemInfoByAtom(m, 1, &b);
    CHECKF(hr == E_INVALIDARG && !b, "getItemInfoByAtom: there are no atoms without a media collection (%08lx)", hr);

    /* markers */
    l = 5; hr = IWMPMedia_get_markerCount(m, &l);
    CHECKF(hr == S_OK && l == 0, "markerCount = 0 (%08lx %ld)", hr, l);
    d = 5.0; hr = IWMPMedia_getMarkerTime(m, 1, &d);
    CHECKF(hr == E_INVALIDARG && d == 0.0, "getMarkerTime(1) = E_INVALIDARG (%08lx)", hr);
    b = (BSTR)1; hr = IWMPMedia_getMarkerName(m, 1, &b);
    CHECKF(hr == E_INVALIDARG && !b, "getMarkerName(1) = E_INVALIDARG (%08lx)", hr);
    CHECKF(IWMPMedia_get_markerCount(m, NULL) == E_POINTER, "markerCount(NULL) = E_POINTER");
    CHECKF(IWMPMedia_getMarkerTime(m, 1, NULL) == E_POINTER, "getMarkerTime(NULL) = E_POINTER");
    CHECKF(IWMPMedia_getMarkerName(m, 1, NULL) == E_POINTER, "getMarkerName(NULL) = E_POINTER");

    /* picture size and duration */
    l = 5; hr = IWMPMedia_get_imageSourceWidth(m, &l);
    CHECKF(hr == S_OK && l == 0, "imageSourceWidth = 0 (%08lx %ld)", hr, l);
    l = 5; hr = IWMPMedia_get_imageSourceHeight(m, &l);
    CHECKF(hr == S_OK && l == 0, "imageSourceHeight = 0 (%08lx %ld)", hr, l);
    CHECKF(IWMPMedia_get_imageSourceWidth(m, NULL) == E_POINTER, "imageSourceWidth(NULL) = E_POINTER");
    b = NULL; hr = IWMPMedia_get_durationString(m, &b);
    CHECKF(hr == S_OK && bstr_is(b, L"00:00"), "durationString of an unplayed item is 00:00 (%08lx)", hr);
    SysFreeString(b);
    CHECKF(IWMPMedia_get_durationString(m, NULL) == E_POINTER, "durationString(NULL) = E_POINTER");
    CHECKF(IWMPMedia_get_duration(m, NULL) == E_POINTER, "duration(NULL) = E_POINTER");

    /* membership */
    n = SysAllocString(L"list");
    IWMPPlayer4_newPlaylist(player, n, NULL, &pl);
    IWMPPlayer4_newPlaylist(player, n, NULL, &pl2);
    SysFreeString(n);
    IWMPPlaylist_appendItem(pl, m);
    vb = 5; hr = IWMPMedia_isMemberOf(m, pl, &vb);
    CHECKF(hr == S_OK && vb == VARIANT_TRUE, "isMemberOf(its playlist) = TRUE (%08lx %d)", hr, vb);
    vb = 5; hr = IWMPMedia_isMemberOf(m, pl2, &vb);
    CHECKF(hr == S_OK && vb == VARIANT_FALSE, "isMemberOf(another playlist) = FALSE (%08lx %d)", hr, vb);
    vb = 5; hr = IWMPMedia_isMemberOf(other, pl, &vb);
    CHECKF(hr == S_OK && vb == VARIANT_FALSE, "isMemberOf: another item is not a member (%08lx %d)", hr, vb);
    vb = 5; hr = IWMPMedia_isMemberOf(same, pl, &vb);
    CHECKF(hr == S_OK && vb == VARIANT_FALSE, "isMemberOf: membership is by item, not by address (%08lx %d)", hr, vb);
    hr = IWMPMedia_isMemberOf(m, NULL, &vb);
    CHECKF(hr == E_INVALIDARG, "isMemberOf(NULL) = E_INVALIDARG (%08lx)", hr);
    hr = IWMPMedia_isMemberOf(m, pl, NULL);
    CHECKF(hr == E_POINTER, "isMemberOf(.., NULL) = E_POINTER (%08lx)", hr);
    IWMPPlaylist_Release(pl2);
    IWMPPlaylist_Release(pl);

    (void)dupe;
    IWMPMedia_Release(other);
    IWMPMedia_Release(same);
    IWMPMedia_Release(m);
}

static void test_playlist(IWMPPlayer4 *player)
{
    IWMPMedia *m[3], *item, *extra;
    IWMPPlaylist *pl, *pl2;
    BSTR n, b, v;
    VARIANT_BOOL vb;
    LONG l, r0;
    HRESULT hr;
    unsigned int i;

    n = SysAllocString(L"Mix");
    hr = IWMPPlayer4_newPlaylist(player, n, NULL, &pl);
    CHECKF(hr == S_OK && pl, "newPlaylist (%08lx)", hr);
    IWMPPlayer4_newPlaylist(player, n, NULL, &pl2);
    SysFreeString(n);
    m[0] = new_media(player, L"C:\\m\\a.mp3");
    m[1] = new_media(player, L"C:\\m\\b.mp3");
    m[2] = new_media(player, L"C:\\m\\c.mp3");
    extra = new_media(player, L"C:\\m\\d.mp3");

    l = 5; IWMPPlaylist_get_count(pl, &l);
    CHECKF(l == 0, "a new playlist is empty (%ld)", l);
    hr = IWMPPlaylist_get_Item(pl, 0, &item);
    CHECKF(hr == E_INVALIDARG && !item, "Item(0) of an empty playlist = E_INVALIDARG (%08lx)", hr);

    r0 = refs_of((IUnknown *)m[0]);
    hr = IWMPPlaylist_appendItem(pl, m[0]);
    CHECKF(hr == S_OK, "appendItem (%08lx)", hr);
    CHECKF(refs_of((IUnknown *)m[0]) == r0 + 1, "the playlist holds the item");
    IWMPPlaylist_appendItem(pl, m[2]);
    hr = IWMPPlaylist_insertItem(pl, 1, m[1]);
    CHECKF(hr == S_OK, "insertItem(1) (%08lx)", hr);
    IWMPPlaylist_get_count(pl, &l);
    CHECKF(l == 3, "three items (%ld)", l);
    for (i = 0; i < 3; i++)
    {
        item = NULL; hr = IWMPPlaylist_get_Item(pl, i, &item);
        CHECKF(hr == S_OK && item == m[i], "item %u is the one inserted at that place (%08lx)", i, hr);
        if (item) IWMPMedia_Release(item);
    }
    hr = IWMPPlaylist_insertItem(pl, 3, extra);
    CHECKF(hr == S_OK, "insertItem at the end (%08lx)", hr);
    hr = IWMPPlaylist_insertItem(pl, 0, extra);
    CHECKF(hr == S_OK, "insertItem at the start (%08lx)", hr);
    item = NULL; IWMPPlaylist_get_Item(pl, 0, &item);
    CHECKF(item == extra, "the item inserted at 0 is first");
    if (item) IWMPMedia_Release(item);
    item = NULL; IWMPPlaylist_get_Item(pl, 4, &item);
    CHECKF(item == extra, "the item inserted at the end is last");
    if (item) IWMPMedia_Release(item);
    hr = IWMPPlaylist_insertItem(pl, 6, extra);
    CHECKF(hr == E_INVALIDARG, "insertItem past the end = E_INVALIDARG (%08lx)", hr);
    hr = IWMPPlaylist_insertItem(pl, -1, extra);
    CHECKF(hr == E_INVALIDARG, "insertItem(-1) = E_INVALIDARG (%08lx)", hr);
    hr = IWMPPlaylist_insertItem(pl, 0, NULL);
    CHECKF(hr == E_POINTER, "insertItem(NULL) = E_POINTER (%08lx)", hr);
    hr = IWMPPlaylist_appendItem(pl, NULL);
    CHECKF(hr == E_POINTER, "appendItem(NULL) = E_POINTER (%08lx)", hr);
    hr = IWMPPlaylist_get_Item(pl, 5, &item);
    CHECKF(hr == E_INVALIDARG && !item, "Item(count) = E_INVALIDARG (%08lx)", hr);
    hr = IWMPPlaylist_get_Item(pl, -1, &item);
    CHECKF(hr == E_INVALIDARG, "Item(-1) = E_INVALIDARG (%08lx)", hr);
    hr = IWMPPlaylist_get_Item(pl, 0, NULL);
    CHECKF(hr == E_POINTER, "Item(.., NULL) = E_POINTER (%08lx)", hr);

    /* remove the two extras again: [x a b c x] */
    r0 = refs_of((IUnknown *)extra);
    hr = IWMPPlaylist_removeItem(pl, extra);
    CHECKF(hr == S_OK, "removeItem (%08lx)", hr);
    CHECKF(refs_of((IUnknown *)extra) == r0 - 1, "removeItem releases the item (%ld vs %ld)", refs_of((IUnknown *)extra), r0);
    IWMPPlaylist_removeItem(pl, extra);
    IWMPPlaylist_get_count(pl, &l);
    CHECKF(l == 3, "three items again (%ld)", l);
    hr = IWMPPlaylist_removeItem(pl, extra);
    CHECKF(hr == E_INVALIDARG, "removeItem of a non-member = E_INVALIDARG (%08lx)", hr);
    hr = IWMPPlaylist_removeItem(pl, NULL);
    CHECKF(hr == E_POINTER, "removeItem(NULL) = E_POINTER (%08lx)", hr);
    for (i = 0; i < 3; i++)
    {
        item = NULL; IWMPPlaylist_get_Item(pl, i, &item);
        CHECKF(item == m[i], "item %u is back to its place after removing the extras", i);
        if (item) IWMPMedia_Release(item);
    }

    /* moveItem: [a b c] */
    hr = IWMPPlaylist_moveItem(pl, 0, 2);
    CHECKF(hr == S_OK, "moveItem(0, 2) (%08lx)", hr);
    {
        IWMPMedia *expect[3] = {m[1], m[2], m[0]};
        for (i = 0; i < 3; i++)
        {
            item = NULL; IWMPPlaylist_get_Item(pl, i, &item);
            CHECKF(item == expect[i], "after moving 0 to 2: item %u", i);
            if (item) IWMPMedia_Release(item);
        }
    }
    hr = IWMPPlaylist_moveItem(pl, 2, 0);
    CHECKF(hr == S_OK, "moveItem(2, 0) (%08lx)", hr);
    for (i = 0; i < 3; i++)
    {
        item = NULL; IWMPPlaylist_get_Item(pl, i, &item);
        CHECKF(item == m[i], "after moving 2 to 0: item %u is back", i);
        if (item) IWMPMedia_Release(item);
    }
    hr = IWMPPlaylist_moveItem(pl, 1, 1);
    CHECKF(hr == S_OK, "moveItem(1, 1) (%08lx)", hr);
    item = NULL; IWMPPlaylist_get_Item(pl, 1, &item);
    CHECKF(item == m[1], "moving an item onto itself changes nothing");
    if (item) IWMPMedia_Release(item);
    CHECKF(IWMPPlaylist_moveItem(pl, 3, 0) == E_INVALIDARG, "moveItem(3, 0) = E_INVALIDARG");
    CHECKF(IWMPPlaylist_moveItem(pl, 0, 3) == E_INVALIDARG, "moveItem(0, 3) = E_INVALIDARG");
    CHECKF(IWMPPlaylist_moveItem(pl, -1, 0) == E_INVALIDARG, "moveItem(-1, 0) = E_INVALIDARG");

    /* attributes */
    l = 0; hr = IWMPPlaylist_get_attributeCount(pl, &l);
    CHECKF(hr == S_OK && l == 2, "two built-in playlist attributes (%08lx %ld)", hr, l);
    b = NULL; IWMPPlaylist_get_attributeName(pl, 0, &b);
    CHECKF(bstr_is(b, L"Title"), "attribute 0 is Title");
    SysFreeString(b);
    b = NULL; IWMPPlaylist_get_attributeName(pl, 1, &b);
    CHECKF(bstr_is(b, L"ItemCount"), "attribute 1 is ItemCount");
    SysFreeString(b);
    n = SysAllocString(L"ItemCount"); b = NULL;
    IWMPPlaylist_getItemInfo(pl, n, &b);
    CHECKF(bstr_is(b, L"3"), "ItemCount = 3");
    SysFreeString(b);
    v = SysAllocString(L"9");
    hr = IWMPPlaylist_setItemInfo(pl, n, v);
    CHECKF(hr == E_ACCESSDENIED, "setItemInfo(ItemCount) = E_ACCESSDENIED (%08lx)", hr);
    SysFreeString(v); SysFreeString(n);
    n = SysAllocString(L"title"); b = NULL;
    IWMPPlaylist_getItemInfo(pl, n, &b);
    CHECKF(bstr_is(b, L"Mix"), "Title is the name (case-insensitive)");
    SysFreeString(b);
    v = SysAllocString(L"Renamed Mix");
    hr = IWMPPlaylist_setItemInfo(pl, n, v);
    SysFreeString(v); SysFreeString(n);
    b = NULL; IWMPPlaylist_get_name(pl, &b);
    CHECKF(hr == S_OK && bstr_is(b, L"Renamed Mix"), "setItemInfo(Title) sets the name (%08lx)", hr);
    SysFreeString(b);
    n = SysAllocString(L"Owner"); v = SysAllocString(L"me");
    hr = IWMPPlaylist_setItemInfo(pl, n, v);
    CHECKF(hr == S_OK, "setItemInfo(Owner) (%08lx)", hr);
    b = NULL; IWMPPlaylist_getItemInfo(pl, n, &b);
    CHECKF(bstr_is(b, L"me"), "Owner reads back");
    SysFreeString(b);
    IWMPPlaylist_get_attributeCount(pl, &l);
    CHECKF(l == 3, "Owner is counted (%ld)", l);
    b = NULL; IWMPPlaylist_get_attributeName(pl, 2, &b);
    CHECKF(bstr_is(b, L"Owner"), "Owner is listed");
    SysFreeString(b);
    b = (BSTR)1; hr = IWMPPlaylist_get_attributeName(pl, 3, &b);
    CHECKF(hr == E_INVALIDARG && !b, "attribute 3 = E_INVALIDARG (%08lx)", hr);
    b = NULL; IWMPPlaylist_getItemInfo(pl2, n, &b);
    CHECKF(bstr_is(b, L""), "attributes belong to the playlist");
    SysFreeString(b);
    SysFreeString(n); SysFreeString(v);
    CHECKF(IWMPPlaylist_getItemInfo(pl, NULL, &b) == E_INVALIDARG, "getItemInfo(NULL) = E_INVALIDARG");
    CHECKF(IWMPPlaylist_setItemInfo(pl, NULL, NULL) == E_INVALIDARG, "setItemInfo(NULL) = E_INVALIDARG");
    CHECKF(IWMPPlaylist_get_attributeCount(pl, NULL) == E_POINTER, "attributeCount(NULL) = E_POINTER");

    /* identity */
    vb = 5; hr = IWMPPlaylist_get_isIdentical(pl, pl, &vb);
    CHECKF(hr == S_OK && vb == VARIANT_TRUE, "playlist is identical to itself (%08lx %d)", hr, vb);
    vb = 5; hr = IWMPPlaylist_get_isIdentical(pl, pl2, &vb);
    CHECKF(hr == S_OK && vb == VARIANT_FALSE, "two playlists are not identical (%08lx %d)", hr, vb);
    CHECKF(IWMPPlaylist_get_isIdentical(pl, NULL, &vb) == E_INVALIDARG, "isIdentical(NULL) = E_INVALIDARG");
    CHECKF(IWMPPlaylist_get_isIdentical(pl, pl2, NULL) == E_POINTER, "isIdentical(.., NULL) = E_POINTER");

    /* clear releases everything */
    r0 = refs_of((IUnknown *)m[1]);
    hr = IWMPPlaylist_clear(pl);
    CHECKF(hr == S_OK, "clear (%08lx)", hr);
    IWMPPlaylist_get_count(pl, &l);
    CHECKF(l == 0, "empty after clear (%ld)", l);
    CHECKF(refs_of((IUnknown *)m[1]) == r0 - 1, "clear releases the items");
    hr = IWMPPlaylist_clear(pl);
    CHECKF(hr == S_OK, "clear of an empty playlist (%08lx)", hr);
    /* the playlist can be refilled, and its items go with it */
    IWMPPlaylist_appendItem(pl, m[1]);
    r0 = refs_of((IUnknown *)m[1]);
    IWMPPlaylist_Release(pl);
    CHECKF(refs_of((IUnknown *)m[1]) == r0 - 1, "releasing the playlist releases its items");

    IWMPPlaylist_Release(pl2);
    for (i = 0; i < 3; i++) IWMPMedia_Release(m[i]);
    IWMPMedia_Release(extra);
}

static void test_control_playlist(IWMPPlayer4 *player, IWMPSettings *settings, IWMPControls *controls)
{
    WCHAR path[3][MAX_PATH];
    IWMPMedia *m[3], *item;
    IWMPPlaylist *pl, *cur;
    WMPPlayState ps;
    VARIANT_BOOL vb;
    BSTR n, b;
    HRESULT hr;
    unsigned int i;
    static const WCHAR *names[] = {L"one", L"two", L"three"};

    IWMPSettings_put_autoStart(settings, VARIANT_FALSE);
    n = SysAllocString(L"queue");
    IWMPPlayer4_newPlaylist(player, n, NULL, &pl);
    SysFreeString(n);
    for (i = 0; i < 3; i++)
    {
        GetTempPathW(MAX_PATH, path[i]);
        lstrcatW(path[i], names[i]);
        lstrcatW(path[i], L".wav");
        write_wave(path[i], 2);
        m[i] = new_media(player, path[i]);
        IWMPPlaylist_appendItem(pl, m[i]);
    }

    hr = IWMPPlayer4_put_currentPlaylist(player, NULL);
    CHECKF(hr == E_POINTER, "put_currentPlaylist(NULL) = E_POINTER (%08lx)", hr);
    hr = IWMPPlayer4_put_currentPlaylist(player, pl);
    CHECKF(hr == S_OK, "put_currentPlaylist (%08lx)", hr);
    cur = NULL; hr = IWMPPlayer4_get_currentPlaylist(player, &cur);
    CHECKF(hr == S_OK && cur == pl, "currentPlaylist is the playlist that was set (%08lx)", hr);
    if (cur) IWMPPlaylist_Release(cur);
    item = NULL; hr = IWMPPlayer4_get_currentMedia(player, &item);
    CHECKF(hr == S_OK && same_item(item, m[0]), "the first item is the current media (%08lx)", hr);
    if (item) IWMPMedia_Release(item);
    IWMPPlayer4_get_playState(player, &ps);
    CHECKF(ps == wmppsReady, "playState = Ready after put_currentPlaylist with autoStart off (%d)", ps);
    item = NULL; hr = IWMPControls_currentItem(controls, &item);
    CHECKF(hr == S_OK && same_item(item, m[0]), "controls.currentItem is the current media (%08lx)", hr);
    if (item) IWMPMedia_Release(item);
    {
        BSTR k = SysAllocString(L"Artist"), v = SysAllocString(L"Band"), t = SysAllocString(L"First Song");
        IWMPMedia_setItemInfo(m[0], k, v);
        IWMPMedia_put_name(m[0], t);
        item = NULL; IWMPPlayer4_get_currentMedia(player, &item);
        b = item ? info(item, L"Artist") : NULL;
        CHECKF(bstr_is(b, L"Band"), "the copy of the current media carries the attributes");
        SysFreeString(b);
        b = NULL; if (item) IWMPMedia_get_name(item, &b);
        CHECKF(bstr_is(b, L"First Song"), "the copy of the current media carries the name");
        SysFreeString(b);
        if (item) IWMPMedia_Release(item);
        SysFreeString(k); SysFreeString(v); SysFreeString(t);
    }

    {
        static const struct { const WCHAR *item; VARIANT_BOOL v[3]; } avail[] = {
            {L"next", {VARIANT_TRUE, VARIANT_TRUE, VARIANT_FALSE}}, {L"previous", {VARIANT_FALSE, VARIANT_TRUE, VARIANT_TRUE}},
            {L"currentItem", {VARIANT_TRUE, VARIANT_TRUE, VARIANT_TRUE}}};
        unsigned int j;
        for (i = 0; i < 3; i++)
        {
            if (i > 0)
            {
                hr = IWMPControls_put_currentItem(controls, m[i]);
                CHECKF(hr == S_OK, "put_currentItem(%u) (%08lx)", i, hr);
            }
            item = NULL; IWMPControls_currentItem(controls, &item);
            CHECKF(same_item(item, m[i]), "current item is %u", i);
            if (item) IWMPMedia_Release(item);
            for (j = 0; j < ARRAY_SIZE(avail); j++)
            {
                n = SysAllocString(avail[j].item);
                vb = 5; hr = IWMPControls_get_isAvailable(controls, n, &vb);
                CHECKF(hr == S_OK && vb == avail[j].v[i], "at item %u: isAvailable(%ls) = %d (%08lx %d)", i, avail[j].item, avail[j].v[i], hr, vb);
                SysFreeString(n);
            }
        }
    }
    hr = IWMPControls_next(controls);
    CHECKF(hr == NS_S_COMMAND_NOT_AVAILABLE, "next at the last item = COMMAND_NOT_AVAILABLE (%08lx)", hr);
    hr = IWMPControls_put_currentItem(controls, m[0]);
    hr = IWMPControls_previous(controls);
    CHECKF(hr == NS_S_COMMAND_NOT_AVAILABLE, "previous at the first item = COMMAND_NOT_AVAILABLE (%08lx)", hr);
    {
        IWMPMedia *stranger = new_media(player, L"C:\\elsewhere\\x.mp3");
        hr = IWMPControls_put_currentItem(controls, stranger);
        CHECKF(hr == E_INVALIDARG, "put_currentItem(not in the playlist) = E_INVALIDARG (%08lx)", hr);
        item = NULL; IWMPControls_currentItem(controls, &item);
        CHECKF(same_item(item, m[0]), "a refused currentItem leaves the old one");
        if (item) IWMPMedia_Release(item);
        IWMPMedia_Release(stranger);
    }
    hr = IWMPControls_put_currentItem(controls, NULL);
    CHECKF(hr == E_POINTER, "put_currentItem(NULL) = E_POINTER (%08lx)", hr);
    hr = IWMPControls_playItem(controls, NULL);
    CHECKF(hr == E_POINTER, "playItem(NULL) = E_POINTER (%08lx)", hr);

    /* next() makes the following item the current one and starts it */
    hr = IWMPControls_next(controls);
    item = NULL; IWMPControls_currentItem(controls, &item);
    CHECKF(same_item(item, m[1]), "next: the second item is current (%08lx)", hr);
    /* the first graph in a fresh prefix can fail while the media framework starts: try again */
    for (i = 0; FAILED(hr) && i < 3; i++)
    {
        pump(1500);
        hr = IWMPControls_playItem(controls, item);
    }
    pump(300);
    if (item) IWMPMedia_Release(item);
    if (SUCCEEDED(hr))
    {
        IWMPPlayer4_get_playState(player, &ps);
        CHECKF(ps == wmppsPlaying, "next: playing (%d)", ps);
        hr = IWMPControls_next(controls);
        pump(300);
        item = NULL; IWMPControls_currentItem(controls, &item);
        CHECKF(same_item(item, m[2]), "next: the third item is current");
        if (item) IWMPMedia_Release(item);
        hr = IWMPControls_previous(controls);
        pump(300);
        item = NULL; IWMPControls_currentItem(controls, &item);
        CHECKF(same_item(item, m[1]), "previous: back to the second item (%08lx)", hr);
        if (item) IWMPMedia_Release(item);
        /* playing: the duration of the item is known now */
        item = NULL; IWMPControls_currentItem(controls, &item);
        b = NULL; IWMPMedia_get_durationString(item, &b);
        CHECKF(bstr_is(b, L"00:02"), "durationString of a played 2 s item = 00:02 (%ls)", b ? b : L"(null)");
        SysFreeString(b);
        b = info(item, L"Duration");
        CHECKF(b && (!wcscmp(b, L"2") || !wcsncmp(b, L"2.", 2) || !wcsncmp(b, L"1.99", 4)), "Duration attribute of a played item (%ls)", b ? b : L"(null)");
        SysFreeString(b);
        IWMPMedia_Release(item);
    }
    else
        printf("SKIP  no playback possible here (next = %08lx)\n", hr);

    IWMPControls_stop(controls);
    IWMPPlayer4_close(player);
    IWMPPlaylist_Release(pl);
    for (i = 0; i < 3; i++) { IWMPMedia_Release(m[i]); DeleteFileW(path[i]); }
}

int main(void)
{
    IWMPPlayer4 *player = NULL;
    IWMPSettings *settings;
    IWMPControls *controls;
    HRESULT hr;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_WindowsMediaPlayer, NULL, CLSCTX_INPROC_SERVER, &IID_IWMPPlayer4, (void **)&player);
    if (FAILED(hr)) { CHECKF(0, "CoCreateInstance(WindowsMediaPlayer) = %08lx", hr); goto out; }
    IWMPPlayer4_get_settings(player, &settings);
    IWMPPlayer4_get_controls(player, &controls);

    test_media(player);
    test_playlist(player);
    test_control_playlist(player, settings, controls);

    IWMPControls_Release(controls);
    IWMPSettings_Release(settings);
    IWMPPlayer4_Release(player);
out:
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
