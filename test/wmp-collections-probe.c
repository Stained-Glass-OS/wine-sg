/* wmp's collection and application objects (patches/sg/2946-2949): the media
 * collection, playlist collection, CD-ROM collection, DVD and player
 * application objects of IWMPPlayer4, and the network buffering/download
 * progress, with the answers of a control that has no media library, no
 * playlist store, no optical drive and no separate player application.
 * Run by test/wmp-collections-gate.sh.
 *
 *   wmp-collections-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <wmp.h>
#include <stdio.h>
#include <string.h>

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define NS_E_DVD_CANNOT_JUMP ((HRESULT)0xc00d116f)

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[320]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static BOOL bstr_is(BSTR b, const WCHAR *s) { return b && !wcscmp(b, s); }

/* every collection object: IUnknown/IDispatch identity, type information, dispatch by name */
static void test_dispatch(IUnknown *obj, const IID *iid, const char *name, const WCHAR *member, DISPID expect)
{
    IDispatch *disp = NULL;
    IUnknown *same = NULL, *other = NULL;
    ITypeInfo *ti;
    TYPEATTR *attr;
    UINT count = 9;
    DISPID id = 0;
    HRESULT hr;
    WCHAR *names[1];

    hr = IUnknown_QueryInterface(obj, iid, (void **)&same);
    CHECKF(hr == S_OK && same == obj, "%s: QueryInterface(own iid) returns itself (%08lx)", name, hr);
    if (same) IUnknown_Release(same);
    hr = IUnknown_QueryInterface(obj, &IID_IDispatch, (void **)&disp);
    CHECKF(hr == S_OK && disp == (IDispatch *)obj, "%s: QueryInterface(IDispatch) (%08lx)", name, hr);
    hr = IUnknown_QueryInterface(obj, &IID_IStream, (void **)&other);
    CHECKF(hr == E_NOINTERFACE && !other, "%s: QueryInterface(IStream) = E_NOINTERFACE (%08lx)", name, hr);
    if (!disp) return;
    hr = IDispatch_GetTypeInfoCount(disp, &count);
    CHECKF(hr == S_OK && count == 1, "%s: GetTypeInfoCount = 1 (%08lx %u)", name, hr, count);
    ti = NULL;
    hr = IDispatch_GetTypeInfo(disp, 0, 0, &ti);
    CHECKF(hr == S_OK && ti, "%s: GetTypeInfo(0) (%08lx)", name, hr);
    if (ti)
    {
        attr = NULL;
        hr = ITypeInfo_GetTypeAttr(ti, &attr);
        CHECKF(hr == S_OK && attr && IsEqualGUID(&attr->guid, iid), "%s: type information describes the interface", name);
        if (attr) ITypeInfo_ReleaseTypeAttr(ti, attr);
        ITypeInfo_Release(ti);
    }
    hr = IDispatch_GetTypeInfo(disp, 1, 0, &ti);
    CHECKF(hr == DISP_E_BADINDEX, "%s: GetTypeInfo(1) = DISP_E_BADINDEX (%08lx)", name, hr);
    names[0] = (WCHAR *)member;
    hr = IDispatch_GetIDsOfNames(disp, &IID_NULL, names, 1, 0, &id);
    CHECKF(hr == S_OK && id == expect, "%s: GetIDsOfNames(%ls) = %ld (%08lx %ld)", name, member, (long)expect, hr, (long)id);
    IDispatch_Release(disp);
}

static void test_string_collection(IWMPStringCollection *sc, const char *name)
{
    LONG n = 9;
    BSTR b = (BSTR)1;
    HRESULT hr;

    hr = IWMPStringCollection_get_count(sc, &n);
    CHECKF(hr == S_OK && n == 0, "%s: count = 0 (%08lx %ld)", name, hr, n);
    hr = IWMPStringCollection_get_count(sc, NULL);
    CHECKF(hr == E_POINTER, "%s: count(NULL) = E_POINTER (%08lx)", name, hr);
    hr = IWMPStringCollection_Item(sc, 0, &b);
    CHECKF(hr == E_INVALIDARG && !b, "%s: Item(0) = E_INVALIDARG, NULL (%08lx)", name, hr);
    hr = IWMPStringCollection_Item(sc, 0, NULL);
    CHECKF(hr == E_POINTER, "%s: Item(0, NULL) = E_POINTER (%08lx)", name, hr);
    test_dispatch((IUnknown *)sc, &IID_IWMPStringCollection, name, L"count", 0x191);
}

static void test_playlist_array(IWMPPlaylistArray *pa, const char *name)
{
    LONG n = 9;
    IWMPPlaylist *pl = (IWMPPlaylist *)1;
    HRESULT hr;

    hr = IWMPPlaylistArray_get_count(pa, &n);
    CHECKF(hr == S_OK && n == 0, "%s: count = 0 (%08lx %ld)", name, hr, n);
    hr = IWMPPlaylistArray_get_count(pa, NULL);
    CHECKF(hr == E_POINTER, "%s: count(NULL) = E_POINTER (%08lx)", name, hr);
    hr = IWMPPlaylistArray_Item(pa, 0, &pl);
    CHECKF(hr == E_INVALIDARG && !pl, "%s: Item(0) = E_INVALIDARG, NULL (%08lx)", name, hr);
    hr = IWMPPlaylistArray_Item(pa, -1, NULL);
    CHECKF(hr == E_POINTER, "%s: Item(NULL) = E_POINTER (%08lx)", name, hr);
    test_dispatch((IUnknown *)pa, &IID_IWMPPlaylistArray, name, L"Item", 0x1f6);
}

/* an empty playlist: no items, an empty name */
static void check_empty_playlist(IWMPPlaylist *pl, const char *what)
{
    LONG n = 9;
    BSTR b = NULL;
    HRESULT hr;

    CHECKF(pl != NULL, "%s: playlist returned", what);
    if (!pl) return;
    hr = IWMPPlaylist_get_count(pl, &n);
    CHECKF(hr == S_OK && n == 0, "%s: playlist count = 0 (%08lx %ld)", what, hr, n);
    hr = IWMPPlaylist_get_name(pl, &b);
    CHECKF(hr == S_OK && bstr_is(b, L""), "%s: playlist name is empty (%08lx)", what, hr);
    SysFreeString(b);
    IWMPPlaylist_Release(pl);
}

static void test_media_collection(IWMPPlayer4 *player)
{
    IWMPMediaCollection *mc = NULL;
    IWMPPlaylist *pl;
    IWMPMedia *media = NULL, *m2;
    IWMPStringCollection *sc;
    BSTR a, b, url;
    LONG atom;
    VARIANT_BOOL del;
    HRESULT hr;
    unsigned int i;

    hr = IWMPPlayer4_get_mediaCollection(player, &mc);
    CHECKF(hr == S_OK && mc, "mediaCollection (%08lx)", hr);
    if (!mc) return;
    hr = IWMPPlayer4_get_mediaCollection(player, NULL);
    CHECKF(hr == E_POINTER, "mediaCollection(NULL) = E_POINTER (%08lx)", hr);
    test_dispatch((IUnknown *)mc, &IID_IWMPMediaCollection, "mediaCollection", L"getAll", 0x1c5);

    /* getAll and the queries by name/genre/author/album/attribute: empty playlists */
    pl = (IWMPPlaylist *)1; hr = IWMPMediaCollection_getAll(mc, &pl);
    CHECKF(hr == S_OK, "getAll (%08lx)", hr);
    check_empty_playlist(hr == S_OK ? pl : NULL, "getAll");
    hr = IWMPMediaCollection_getAll(mc, NULL);
    CHECKF(hr == E_POINTER, "getAll(NULL) = E_POINTER (%08lx)", hr);
    a = SysAllocString(L"x");
#define QUERY(fn) do { \
    pl = (IWMPPlaylist *)1; hr = IWMPMediaCollection_##fn(mc, a, &pl); \
    CHECKF(hr == S_OK, #fn " (%08lx)", hr); check_empty_playlist(hr == S_OK ? pl : NULL, #fn); \
    pl = (IWMPPlaylist *)1; hr = IWMPMediaCollection_##fn(mc, NULL, &pl); \
    CHECKF(hr == E_INVALIDARG && !pl, #fn "(NULL) = E_INVALIDARG (%08lx)", hr); \
    hr = IWMPMediaCollection_##fn(mc, a, NULL); \
    CHECKF(hr == E_POINTER, #fn "(, NULL) = E_POINTER (%08lx)", hr); \
} while (0)
    QUERY(getByName); QUERY(getByGenre); QUERY(getByAuthor); QUERY(getByAlbum);
#undef QUERY
    b = SysAllocString(L"y");
    pl = (IWMPPlaylist *)1; hr = IWMPMediaCollection_getByAttribute(mc, a, b, &pl);
    CHECKF(hr == S_OK, "getByAttribute (%08lx)", hr);
    check_empty_playlist(hr == S_OK ? pl : NULL, "getByAttribute");
    hr = IWMPMediaCollection_getByAttribute(mc, a, NULL, &pl);
    CHECKF(hr == E_INVALIDARG && !pl, "getByAttribute(, NULL) = E_INVALIDARG (%08lx)", hr);

    /* attribute string collections are empty */
    sc = (IWMPStringCollection *)1; hr = IWMPMediaCollection_getAttributeStringCollection(mc, a, b, &sc);
    CHECKF(hr == S_OK && sc > (IWMPStringCollection *)1, "getAttributeStringCollection (%08lx)", hr);
    if (hr == S_OK && sc) { test_string_collection(sc, "attribute strings"); IWMPStringCollection_Release(sc); }
    hr = IWMPMediaCollection_getAttributeStringCollection(mc, NULL, b, &sc);
    CHECKF(hr == E_INVALIDARG && !sc, "getAttributeStringCollection(NULL) = E_INVALIDARG (%08lx)", hr);
    hr = IWMPMediaCollection_getAttributeStringCollection(mc, a, b, NULL);
    CHECKF(hr == E_POINTER, "getAttributeStringCollection(, NULL) = E_POINTER (%08lx)", hr);

    atom = 9; hr = IWMPMediaCollection_getMediaAtom(mc, a, &atom);
    CHECKF(hr == S_FALSE && atom == 0, "getMediaAtom = S_FALSE, 0 (%08lx %ld)", hr, atom);
    hr = IWMPMediaCollection_getMediaAtom(mc, NULL, &atom);
    CHECKF(hr == E_INVALIDARG, "getMediaAtom(NULL) = E_INVALIDARG (%08lx)", hr);
    SysFreeString(b);
    SysFreeString(a);

    /* add() returns an item for the URL, which is not kept */
    url = SysAllocString(L"c:\\some\\song.mp3");
    hr = IWMPMediaCollection_add(mc, url, &media);
    CHECKF(hr == S_OK && media, "add (%08lx)", hr);
    if (media)
    {
        b = NULL; hr = IWMPMedia_get_sourceURL(media, &b);
        CHECKF(hr == S_OK && bstr_is(b, L"c:\\some\\song.mp3"), "add: sourceURL is the URL (%08lx)", hr);
        SysFreeString(b);
        pl = NULL; hr = IWMPMediaCollection_getAll(mc, &pl);
        check_empty_playlist(pl, "getAll after add");
        del = 5; hr = IWMPMediaCollection_isDeleted(mc, media, &del);
        CHECKF(hr == S_OK && del == VARIANT_FALSE, "isDeleted = FALSE (%08lx %d)", hr, del);
        hr = IWMPMediaCollection_isDeleted(mc, NULL, &del);
        CHECKF(hr == E_POINTER, "isDeleted(NULL item) = E_POINTER (%08lx)", hr);
        hr = IWMPMediaCollection_isDeleted(mc, media, NULL);
        CHECKF(hr == E_POINTER, "isDeleted(, NULL) = E_POINTER (%08lx)", hr);
        hr = IWMPMediaCollection_setDeleted(mc, media, VARIANT_TRUE);
        CHECKF(hr == S_FALSE, "setDeleted = S_FALSE (%08lx)", hr);
        hr = IWMPMediaCollection_setDeleted(mc, NULL, VARIANT_TRUE);
        CHECKF(hr == E_POINTER, "setDeleted(NULL) = E_POINTER (%08lx)", hr);
        hr = IWMPMediaCollection_remove(mc, media, VARIANT_FALSE);
        CHECKF(hr == S_FALSE, "remove of an unlisted item = S_FALSE (%08lx)", hr);
        hr = IWMPMediaCollection_remove(mc, NULL, VARIANT_FALSE);
        CHECKF(hr == E_POINTER, "remove(NULL) = E_POINTER (%08lx)", hr);
        IWMPMedia_Release(media);
    }
    m2 = (IWMPMedia *)1;
    hr = IWMPMediaCollection_add(mc, NULL, &m2);
    CHECKF(hr == E_INVALIDARG && !m2, "add(NULL) = E_INVALIDARG (%08lx)", hr);
    hr = IWMPMediaCollection_add(mc, url, NULL);
    CHECKF(hr == E_POINTER, "add(, NULL) = E_POINTER (%08lx)", hr);
    SysFreeString(url);

    /* the object shares the control's lifetime */
    for (i = 0; i < 3; i++) IWMPMediaCollection_AddRef(mc);
    for (i = 0; i < 3; i++) IWMPMediaCollection_Release(mc);
    IWMPMediaCollection_Release(mc);
}

static void test_playlist_collection(IWMPPlayer4 *player)
{
    IWMPPlaylistCollection *pc = NULL;
    IWMPPlaylistArray *pa;
    IWMPPlaylist *pl, *imp;
    BSTR name;
    VARIANT_BOOL del;
    HRESULT hr;

    hr = IWMPPlayer4_get_playlistCollection(player, &pc);
    CHECKF(hr == S_OK && pc, "playlistCollection (%08lx)", hr);
    if (!pc) return;
    hr = IWMPPlayer4_get_playlistCollection(player, NULL);
    CHECKF(hr == E_POINTER, "playlistCollection(NULL) = E_POINTER (%08lx)", hr);
    test_dispatch((IUnknown *)pc, &IID_IWMPPlaylistCollection, "playlistCollection", L"newPlaylist", 0x228);

    pa = (IWMPPlaylistArray *)1; hr = IWMPPlaylistCollection_getAll(pc, &pa);
    CHECKF(hr == S_OK && pa > (IWMPPlaylistArray *)1, "getAll (%08lx)", hr);
    if (hr == S_OK && pa) { test_playlist_array(pa, "getAll array"); IWMPPlaylistArray_Release(pa); }
    hr = IWMPPlaylistCollection_getAll(pc, NULL);
    CHECKF(hr == E_POINTER, "getAll(NULL) = E_POINTER (%08lx)", hr);
    name = SysAllocString(L"My list");
    pa = (IWMPPlaylistArray *)1; hr = IWMPPlaylistCollection_getByName(pc, name, &pa);
    CHECKF(hr == S_OK && pa > (IWMPPlaylistArray *)1, "getByName (%08lx)", hr);
    if (hr == S_OK && pa) { test_playlist_array(pa, "getByName array"); IWMPPlaylistArray_Release(pa); }
    hr = IWMPPlaylistCollection_getByName(pc, NULL, &pa);
    CHECKF(hr == E_INVALIDARG && !pa, "getByName(NULL) = E_INVALIDARG (%08lx)", hr);
    hr = IWMPPlaylistCollection_getByName(pc, name, NULL);
    CHECKF(hr == E_POINTER, "getByName(, NULL) = E_POINTER (%08lx)", hr);

    pl = NULL; hr = IWMPPlaylistCollection_newPlaylist(pc, name, &pl);
    CHECKF(hr == S_OK && pl, "newPlaylist (%08lx)", hr);
    if (pl)
    {
        BSTR got = NULL; LONG n = 9;
        hr = IWMPPlaylist_get_name(pl, &got);
        CHECKF(hr == S_OK && bstr_is(got, L"My list"), "newPlaylist: name is kept (%08lx)", hr);
        SysFreeString(got);
        hr = IWMPPlaylist_get_count(pl, &n);
        CHECKF(hr == S_OK && n == 0, "newPlaylist: count 0 (%08lx %ld)", hr, n);

        del = 5; hr = IWMPPlaylistCollection_isDeleted(pc, pl, &del);
        CHECKF(hr == S_OK && del == VARIANT_FALSE, "isDeleted = FALSE (%08lx %d)", hr, del);
        hr = IWMPPlaylistCollection_isDeleted(pc, NULL, &del);
        CHECKF(hr == E_POINTER, "isDeleted(NULL item) = E_POINTER (%08lx)", hr);
        hr = IWMPPlaylistCollection_isDeleted(pc, pl, NULL);
        CHECKF(hr == E_POINTER, "isDeleted(, NULL) = E_POINTER (%08lx)", hr);
        hr = IWMPPlaylistCollection_setDeleted(pc, pl, VARIANT_TRUE);
        CHECKF(hr == S_FALSE, "setDeleted = S_FALSE (%08lx)", hr);
        hr = IWMPPlaylistCollection_setDeleted(pc, NULL, VARIANT_TRUE);
        CHECKF(hr == E_POINTER, "setDeleted(NULL) = E_POINTER (%08lx)", hr);
        hr = IWMPPlaylistCollection_remove(pc, pl);
        CHECKF(hr == S_FALSE, "remove of an unlisted playlist = S_FALSE (%08lx)", hr);
        hr = IWMPPlaylistCollection_remove(pc, NULL);
        CHECKF(hr == E_POINTER, "remove(NULL) = E_POINTER (%08lx)", hr);
        imp = NULL; hr = IWMPPlaylistCollection_importPlaylist(pc, pl, &imp);
        CHECKF(hr == S_OK && imp == pl, "importPlaylist returns the playlist (%08lx)", hr);
        if (imp) IWMPPlaylist_Release(imp);
        hr = IWMPPlaylistCollection_importPlaylist(pc, NULL, &imp);
        CHECKF(hr == E_POINTER && !imp, "importPlaylist(NULL) = E_POINTER (%08lx)", hr);
        hr = IWMPPlaylistCollection_importPlaylist(pc, pl, NULL);
        CHECKF(hr == E_POINTER, "importPlaylist(, NULL) = E_POINTER (%08lx)", hr);
        IWMPPlaylist_Release(pl);
    }
    SysFreeString(name);
    name = SysAllocString(L"");
    pl = (IWMPPlaylist *)1; hr = IWMPPlaylistCollection_newPlaylist(pc, name, &pl);
    CHECKF(hr == E_INVALIDARG && !pl, "newPlaylist(empty name) = E_INVALIDARG (%08lx)", hr);
    SysFreeString(name);
    hr = IWMPPlaylistCollection_newPlaylist(pc, NULL, &pl);
    CHECKF(hr == E_INVALIDARG, "newPlaylist(NULL) = E_INVALIDARG (%08lx)", hr);
    hr = IWMPPlaylistCollection_newPlaylist(pc, NULL, NULL);
    CHECKF(hr == E_POINTER, "newPlaylist(, NULL) = E_POINTER (%08lx)", hr);
    IWMPPlaylistCollection_Release(pc);
}

static void test_cdrom_collection(IWMPPlayer4 *player)
{
    IWMPCdromCollection *cc = NULL;
    IWMPCdrom *cd = (IWMPCdrom *)1;
    LONG n = 9;
    BSTR drv;
    HRESULT hr;

    hr = IWMPPlayer4_get_cdromCollection(player, &cc);
    CHECKF(hr == S_OK && cc, "cdromCollection (%08lx)", hr);
    if (!cc) return;
    hr = IWMPPlayer4_get_cdromCollection(player, NULL);
    CHECKF(hr == E_POINTER, "cdromCollection(NULL) = E_POINTER (%08lx)", hr);
    test_dispatch((IUnknown *)cc, &IID_IWMPCdromCollection, "cdromCollection", L"getByDriveSpecifier", 0x12f);
    hr = IWMPCdromCollection_get_count(cc, &n);
    CHECKF(hr == S_OK && n == 0, "cdrom count = 0 (%08lx %ld)", hr, n);
    hr = IWMPCdromCollection_get_count(cc, NULL);
    CHECKF(hr == E_POINTER, "cdrom count(NULL) = E_POINTER (%08lx)", hr);
    hr = IWMPCdromCollection_Item(cc, 0, &cd);
    CHECKF(hr == E_INVALIDARG && !cd, "cdrom Item(0) = E_INVALIDARG, NULL (%08lx)", hr);
    hr = IWMPCdromCollection_Item(cc, 0, NULL);
    CHECKF(hr == E_POINTER, "cdrom Item(, NULL) = E_POINTER (%08lx)", hr);
    drv = SysAllocString(L"D:");
    cd = (IWMPCdrom *)1; hr = IWMPCdromCollection_getByDriveSpecifier(cc, drv, &cd);
    CHECKF(hr == S_FALSE && !cd, "getByDriveSpecifier(D:) = S_FALSE, NULL (%08lx)", hr);
    hr = IWMPCdromCollection_getByDriveSpecifier(cc, drv, NULL);
    CHECKF(hr == E_POINTER, "getByDriveSpecifier(, NULL) = E_POINTER (%08lx)", hr);
    SysFreeString(drv);
    hr = IWMPCdromCollection_getByDriveSpecifier(cc, NULL, &cd);
    CHECKF(hr == E_INVALIDARG && !cd, "getByDriveSpecifier(NULL) = E_INVALIDARG (%08lx)", hr);
    IWMPCdromCollection_Release(cc);
}

static void test_dvd(IWMPPlayer4 *player)
{
    static const WCHAR *items[] = {L"back", L"resume", L"titleMenu", L"topMenu", L"bogus"};
    IWMPDVD *dvd = NULL;
    VARIANT_BOOL v;
    BSTR b, item;
    HRESULT hr;
    unsigned int i;

    hr = IWMPPlayer4_get_dvd(player, &dvd);
    CHECKF(hr == S_OK && dvd, "dvd (%08lx)", hr);
    if (!dvd) return;
    hr = IWMPPlayer4_get_dvd(player, NULL);
    CHECKF(hr == E_POINTER, "dvd(NULL) = E_POINTER (%08lx)", hr);
    test_dispatch((IUnknown *)dvd, &IID_IWMPDVD, "dvd", L"domain", 0x3ea);
    for (i = 0; i < ARRAY_SIZE(items); i++)
    {
        item = SysAllocString(items[i]);
        v = 5; hr = IWMPDVD_get_isAvailable(dvd, item, &v);
        CHECKF(hr == S_OK && v == VARIANT_FALSE, "dvd isAvailable(%ls) = FALSE (%08lx %d)", items[i], hr, v);
        SysFreeString(item);
    }
    hr = IWMPDVD_get_isAvailable(dvd, NULL, &v);
    CHECKF(hr == E_INVALIDARG && v == VARIANT_FALSE, "dvd isAvailable(NULL) = E_INVALIDARG (%08lx)", hr);
    item = SysAllocString(L"back");
    hr = IWMPDVD_get_isAvailable(dvd, item, NULL);
    CHECKF(hr == E_POINTER, "dvd isAvailable(, NULL) = E_POINTER (%08lx)", hr);
    SysFreeString(item);
    b = NULL; hr = IWMPDVD_get_domain(dvd, &b);
    CHECKF(hr == S_OK && bstr_is(b, L"stop"), "dvd domain = stop (%08lx)", hr);
    SysFreeString(b);
    hr = IWMPDVD_get_domain(dvd, NULL);
    CHECKF(hr == E_POINTER, "dvd domain(NULL) = E_POINTER (%08lx)", hr);
    hr = IWMPDVD_topMenu(dvd);
    CHECKF(hr == NS_E_DVD_CANNOT_JUMP, "dvd topMenu = NS_E_DVD_CANNOT_JUMP (%08lx)", hr);
    hr = IWMPDVD_titleMenu(dvd);
    CHECKF(hr == NS_E_DVD_CANNOT_JUMP, "dvd titleMenu = NS_E_DVD_CANNOT_JUMP (%08lx)", hr);
    hr = IWMPDVD_back(dvd);
    CHECKF(hr == S_FALSE, "dvd back = S_FALSE (%08lx)", hr);
    hr = IWMPDVD_resume(dvd);
    CHECKF(hr == S_FALSE, "dvd resume = S_FALSE (%08lx)", hr);
    IWMPDVD_Release(dvd);
}

static void test_player_application(IWMPPlayer4 *player)
{
    IWMPPlayerApplication *app = NULL;
    VARIANT_BOOL v;
    HRESULT hr;

    hr = IWMPPlayer4_get_playerApplication(player, &app);
    CHECKF(hr == S_OK && app, "playerApplication (%08lx)", hr);
    if (!app) return;
    hr = IWMPPlayer4_get_playerApplication(player, NULL);
    CHECKF(hr == E_POINTER, "playerApplication(NULL) = E_POINTER (%08lx)", hr);
    test_dispatch((IUnknown *)app, &IID_IWMPPlayerApplication, "playerApplication", L"playerDocked", 0x44f);
    v = 5; hr = IWMPPlayerApplication_get_playerDocked(app, &v);
    CHECKF(hr == S_OK && v == VARIANT_FALSE, "playerDocked = FALSE (%08lx %d)", hr, v);
    hr = IWMPPlayerApplication_get_playerDocked(app, NULL);
    CHECKF(hr == E_POINTER, "playerDocked(NULL) = E_POINTER (%08lx)", hr);
    v = 5; hr = IWMPPlayerApplication_get_hasDisplay(app, &v);
    CHECKF(hr == S_OK && v == VARIANT_FALSE, "hasDisplay = FALSE (%08lx %d)", hr, v);
    hr = IWMPPlayerApplication_get_hasDisplay(app, NULL);
    CHECKF(hr == E_POINTER, "hasDisplay(NULL) = E_POINTER (%08lx)", hr);
    hr = IWMPPlayerApplication_switchToPlayerApplication(app);
    CHECKF(hr == S_FALSE, "switchToPlayerApplication = S_FALSE (%08lx)", hr);
    hr = IWMPPlayerApplication_switchToControl(app);
    CHECKF(hr == S_FALSE, "switchToControl = S_FALSE (%08lx)", hr);
    IWMPPlayerApplication_Release(app);
}

/* the old IWMPPlayer interface exposes the same objects */
static void test_old_player(IWMPPlayer4 *player4)
{
    IWMPPlayer *p = NULL;
    IWMPMediaCollection *mc = NULL;
    IWMPCdromCollection *cc = NULL;
    HRESULT hr;

    hr = IWMPPlayer4_QueryInterface(player4, &IID_IWMPPlayer, (void **)&p);
    CHECKF(hr == S_OK && p, "QueryInterface(IWMPPlayer) (%08lx)", hr);
    if (!p) return;
    hr = IWMPPlayer_get_mediaCollection(p, &mc);
    CHECKF(hr == S_OK && mc, "IWMPPlayer mediaCollection (%08lx)", hr);
    if (mc) IWMPMediaCollection_Release(mc);
    hr = IWMPPlayer_get_cdromCollection(p, &cc);
    CHECKF(hr == S_OK && cc, "IWMPPlayer cdromCollection (%08lx)", hr);
    if (cc) IWMPCdromCollection_Release(cc);
    IWMPPlayer_Release(p);
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

/* buffering and download progress: idle = 0 with S_FALSE, a loaded local file = 100 */
static void test_progress(IWMPPlayer4 *player)
{
    IWMPNetwork *n = NULL;
    IWMPSettings *settings = NULL;
    IWMPControls *controls = NULL;
    WCHAR path[MAX_PATH];
    BSTR url;
    LONG l;
    HRESULT hr;
    unsigned int i;

    IWMPPlayer4_get_network(player, &n);
    IWMPPlayer4_get_settings(player, &settings);
    IWMPPlayer4_get_controls(player, &controls);
    l = 77; hr = IWMPNetwork_get_bufferingProgress(n, &l);
    CHECKF(hr == S_FALSE && l == 0, "idle bufferingProgress = 0, S_FALSE (%08lx %ld)", hr, l);
    l = 77; hr = IWMPNetwork_get_downloadProgress(n, &l);
    CHECKF(hr == S_FALSE && l == 0, "idle downloadProgress = 0, S_FALSE (%08lx %ld)", hr, l);
    hr = IWMPNetwork_get_bufferingProgress(n, NULL);
    CHECKF(hr == E_POINTER, "bufferingProgress(NULL) = E_POINTER (%08lx)", hr);
    hr = IWMPNetwork_get_downloadProgress(n, NULL);
    CHECKF(hr == E_POINTER, "downloadProgress(NULL) = E_POINTER (%08lx)", hr);

    GetTempPathW(MAX_PATH, path);
    lstrcatW(path, L"wmp-collections-probe.wav");
    write_wave(path, 2);
    url = SysAllocString(path);
    IWMPSettings_put_autoStart(settings, VARIANT_FALSE);
    IWMPPlayer4_put_URL(player, url);
    SysFreeString(url);
    hr = IWMPControls_play(controls);
    for (i = 0; FAILED(hr) && i < 3; i++) { pump(1500); hr = IWMPControls_play(controls); }
    pump(200);
    if (FAILED(hr))
        printf("SKIP  no playback possible here (play = %08lx): loaded-file progress not checked\n", hr);
    else
    {
        l = 0; hr = IWMPNetwork_get_bufferingProgress(n, &l);
        CHECKF(hr == S_OK && l == 100, "loaded file: bufferingProgress = 100, S_OK (%08lx %ld)", hr, l);
        l = 0; hr = IWMPNetwork_get_downloadProgress(n, &l);
        CHECKF(hr == S_OK && l == 100, "loaded file: downloadProgress = 100, S_OK (%08lx %ld)", hr, l);
        IWMPControls_stop(controls);
    }
    DeleteFileW(path);
    IWMPControls_Release(controls);
    IWMPSettings_Release(settings);
    IWMPNetwork_Release(n);
}

int main(void)
{
    IWMPPlayer4 *player = NULL;
    HRESULT hr;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_WindowsMediaPlayer, NULL, CLSCTX_INPROC_SERVER, &IID_IWMPPlayer4, (void **)&player);
    if (FAILED(hr)) { CHECKF(0, "CoCreateInstance(WindowsMediaPlayer) = %08lx", hr); printf("RESULT: FAIL\n"); return 1; }
    test_media_collection(player);
    test_playlist_collection(player);
    test_cdrom_collection(player);
    test_dvd(player);
    test_player_application(player);
    test_old_player(player);
    test_progress(player);
    IWMPPlayer4_Release(player);
    CoUninitialize();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
