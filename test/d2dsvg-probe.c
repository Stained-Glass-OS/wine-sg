/* d2dsvg-probe: Direct2D SVG documents (ID2D1DeviceContext5::CreateSvgDocument
 * and DrawSvgDocument, the ID2D1Svg* interfaces).
 *
 * Office's sign-in pane creates an empty SVG document and ends in a fail-fast
 * when that fails; this probe checks what the pane and other programs use:
 *
 *  - CreateSvgDocument with no stream: an empty document whose root is svg;
 *  - building a tree (CreateChild, AppendChild, RemoveChild, GetNextChild),
 *    string and typed attributes (FLOAT, COLOR, VIEWBOX) and their text;
 *  - attribute objects (ID2D1SvgPaint) that stay live: changing the paint
 *    changes the element;
 *  - CreateSvgDocument from XML, FindElementById, Serialize, Deserialize;
 *  - path data (commands + segment data) and its geometry;
 *  - DrawSvgDocument on a WIC bitmap render target: rect, circle, path,
 *    viewBox scaling, group transforms, fill="none" with a stroke, use,
 *    display="none", and a linear gradient, read back as pixels.
 *
 * The SVG interfaces are called through vtable slots in the order Windows
 * lays them out (overloads of one name sit together, last declared first),
 * not through a header, so the probe checks that order too.
 *
 * Prints name=value lines; see test/d2dsvg-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <d2d1_1.h>
#include <wincodec.h>
#include <objidl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SIZE 64
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/* shlwapi's memory stream (its header does not build as C here) */
IStream * WINAPI SHCreateMemStream(const BYTE *data, UINT size);

DEFINE_GUID(IID_DC5, 0x7836d248, 0x68cc, 0x4df6, 0xb9, 0xe8, 0xde, 0x99, 0x1b, 0xf6, 0x2e, 0xb7);
DEFINE_GUID(IID_SvgPaint, 0xd59bab0a, 0x68a2, 0x455b, 0xa5, 0xdc, 0x9e, 0xb2, 0x85, 0x4e, 0x24, 0x90);
DEFINE_GUID(IID_SvgPathData, 0xc095e4f4, 0xbb98, 0x43d6, 0x97, 0x45, 0x4d, 0x1b, 0x84, 0xec, 0x98, 0x88);
DEFINE_GUID(IID_SvgElement, 0xac7b67a6, 0x183e, 0x49c1, 0xa8, 0x23, 0x0e, 0xbe, 0x40, 0xb0, 0xdb, 0x29);
DEFINE_GUID(IID_SvgDocument, 0x86b88e4d, 0xafa4, 0x4d7b, 0x88, 0xe4, 0x68, 0xa5, 0x1c, 0x4a, 0x0a, 0xec);

#define VT(obj, n) ((*(void ***)(obj))[n])
typedef void *PUNK;

/* ID2D1DeviceContext5 */
#define DC5_CREATE_SVG 115
#define DC5_DRAW_SVG   116
/* IUnknown / ID2D1Resource */
#define QI 0
#define RELEASE 2
/* ID2D1SvgElement */
#define EL_GET_TAG_NAME 5
#define EL_GET_PARENT 8
#define EL_HAS_CHILDREN 9
#define EL_GET_FIRST_CHILD 10
#define EL_GET_NEXT_CHILD 13
#define EL_APPEND_CHILD 15
#define EL_REMOVE_CHILD 17
#define EL_CREATE_CHILD 18
#define EL_IS_SPECIFIED 19
#define EL_SPECIFIED_COUNT 20
#define EL_SET_ATTR_OBJ 27
#define EL_SET_ATTR_POD 28
#define EL_SET_ATTR_STR 29
#define EL_GET_ATTR_IID 30
#define EL_GET_ATTR_POD 31
#define EL_GET_ATTR_STR 32
#define EL_GET_ATTR_LEN 33
/* ID2D1SvgDocument */
#define DOC_GET_VIEWPORT 5
#define DOC_GET_ROOT 7
#define DOC_FIND_BY_ID 8
#define DOC_SERIALIZE 9
#define DOC_DESERIALIZE 10
#define DOC_CREATE_PAINT 11
#define DOC_CREATE_PATH_DATA 14
/* ID2D1SvgAttribute / ID2D1SvgPaint / ID2D1SvgPathData */
#define ATTR_GET_ELEMENT 4
#define PAINT_SET_COLOR 8
#define PAINT_GET_PAINT_TYPE 7
#define PATH_GET_COMMANDS_COUNT 13
#define PATH_CREATE_GEOMETRY 14

typedef HRESULT (WINAPI *create_svg_t)(PUNK, IStream *, D2D1_SIZE_F, PUNK *);
typedef void (WINAPI *draw_svg_t)(PUNK, PUNK);
typedef ULONG (WINAPI *release_t)(PUNK);
typedef HRESULT (WINAPI *qi_t)(PUNK, REFIID, void **);
typedef void (WINAPI *get_elem_t)(PUNK, PUNK *);
typedef HRESULT (WINAPI *get_tag_t)(PUNK, WCHAR *, UINT32);
typedef BOOL (WINAPI *has_children_t)(PUNK);
typedef HRESULT (WINAPI *elem_elem_t)(PUNK, PUNK);
typedef HRESULT (WINAPI *next_child_t)(PUNK, PUNK, PUNK *);
typedef HRESULT (WINAPI *create_child_t)(PUNK, const WCHAR *, PUNK *);
typedef BOOL (WINAPI *is_specified_t)(PUNK, const WCHAR *, BOOL *);
typedef UINT32 (WINAPI *count_t)(PUNK);
typedef HRESULT (WINAPI *set_obj_t)(PUNK, const WCHAR *, PUNK);
typedef HRESULT (WINAPI *set_pod_t)(PUNK, const WCHAR *, UINT32, const void *, UINT32);
typedef HRESULT (WINAPI *set_str_t)(PUNK, const WCHAR *, UINT32, const WCHAR *);
typedef HRESULT (WINAPI *get_iid_t)(PUNK, const WCHAR *, REFIID, void **);
typedef HRESULT (WINAPI *get_pod_t)(PUNK, const WCHAR *, UINT32, void *, UINT32);
typedef HRESULT (WINAPI *get_str_t)(PUNK, const WCHAR *, UINT32, WCHAR *, UINT32);
typedef HRESULT (WINAPI *get_len_t)(PUNK, const WCHAR *, UINT32, UINT32 *);
typedef D2D1_SIZE_F *(WINAPI *get_viewport_t)(PUNK, D2D1_SIZE_F *);
typedef HRESULT (WINAPI *find_t)(PUNK, const WCHAR *, PUNK *);
typedef HRESULT (WINAPI *serialize_t)(PUNK, IStream *, PUNK);
typedef HRESULT (WINAPI *deserialize_t)(PUNK, IStream *, PUNK *);
typedef HRESULT (WINAPI *create_paint_t)(PUNK, UINT32, const D2D1_COLOR_F *, const WCHAR *, PUNK *);
typedef HRESULT (WINAPI *create_path_t)(PUNK, const FLOAT *, UINT32, const UINT32 *, UINT32, PUNK *);
typedef HRESULT (WINAPI *paint_set_color_t)(PUNK, const D2D1_COLOR_F *);
typedef UINT32 (WINAPI *paint_type_t)(PUNK);
typedef HRESULT (WINAPI *path_geometry_t)(PUNK, UINT32, ID2D1PathGeometry **);

#define CALL(type, obj, slot, ...) ((type)VT(obj, slot))(obj, ##__VA_ARGS__)

static IWICImagingFactory *wic;
static ID2D1Factory1 *factory;
static IWICBitmap *wic_bitmap;
static ID2D1RenderTarget *rt;
static PUNK dc5;

static void release(PUNK p)
{
    if (p) CALL(release_t, p, RELEASE);
}

static HRESULT begin(void)
{
    static const D2D1_COLOR_F white = { 1, 1, 1, 1 };
    D2D1_RENDER_TARGET_PROPERTIES props = { D2D1_RENDER_TARGET_TYPE_DEFAULT,
            { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }, 96.0f, 96.0f,
            D2D1_RENDER_TARGET_USAGE_NONE, D2D1_FEATURE_LEVEL_DEFAULT };
    HRESULT hr;

    release(dc5); dc5 = NULL;
    if (rt) { ID2D1RenderTarget_Release(rt); rt = NULL; }
    if (wic_bitmap) { IWICBitmap_Release(wic_bitmap); wic_bitmap = NULL; }
    if (FAILED(hr = IWICImagingFactory_CreateBitmap(wic, SIZE, SIZE, &GUID_WICPixelFormat32bppPBGRA,
            WICBitmapCacheOnDemand, &wic_bitmap)))
        return hr;
    if (FAILED(hr = ID2D1Factory_CreateWicBitmapRenderTarget((ID2D1Factory *)factory, wic_bitmap, &props, &rt)))
        return hr;
    if (FAILED(hr = ID2D1RenderTarget_QueryInterface(rt, &IID_DC5, &dc5)))
        return hr;
    ID2D1RenderTarget_BeginDraw(rt);
    ID2D1RenderTarget_Clear(rt, &white);
    return S_OK;
}

static void end(void)
{
    ID2D1RenderTarget_EndDraw(rt, NULL, NULL);
}

static DWORD pixel(int x, int y)
{
    WICRect r = { 0, 0, SIZE, SIZE };
    IWICBitmapLock *lock;
    UINT stride, size;
    BYTE *data;
    DWORD v = 0xdeadbeef;

    if (FAILED(IWICBitmap_Lock(wic_bitmap, &r, WICBitmapLockRead, &lock)))
        return v;
    if (SUCCEEDED(IWICBitmapLock_GetStride(lock, &stride))
            && SUCCEEDED(IWICBitmapLock_GetDataPointer(lock, &size, &data)))
        v = *(DWORD *)(data + y * stride + x * 4);
    IWICBitmapLock_Release(lock);
    return v;
}

static BOOL close_to(DWORD a, DWORD b, int tolerance)
{
    int i;
    for (i = 0; i < 32; i += 8)
        if (abs((int)((a >> i) & 0xff) - (int)((b >> i) & 0xff)) > tolerance) return FALSE;
    return TRUE;
}

static void report(const char *name, BOOL ok, DWORD got)
{
    if (ok) printf("%s=1\n", name);
    else printf("%s=0 (0x%08lx)\n", name, got);
}

static IStream *stream_from(const char *text)
{
    return SHCreateMemStream((const BYTE *)text, strlen(text));
}

#define WHITE 0xffffffff
#define RED   0xffff0000
#define GREEN 0xff008000
#define BLUE  0xff0000ff
#define BLACK 0xff000000

static PUNK create_doc(IStream *stream)
{
    D2D1_SIZE_F size = { SIZE, SIZE };
    PUNK doc = NULL;
    HRESULT hr = CALL(create_svg_t, dc5, DC5_CREATE_SVG, stream, size, &doc);
    if (FAILED(hr)) printf("create_hr=%#lx\n", hr);
    return doc;
}

static void draw(PUNK doc)
{
    CALL(draw_svg_t, dc5, DC5_DRAW_SVG, doc);
}

int main(void)
{
    WCHAR buf[256];
    PUNK doc, root, rect, circle, child, next, paint, el, path, clone;
    D2D1_SIZE_F vp;
    D2D1_COLOR_F c;
    D2D1_RECT_F bounds;
    ID2D1PathGeometry *geometry;
    IStream *stream;
    float f;
    UINT32 len;
    HRESULT hr;
    BOOL ok;

    CoInitialize(NULL);
    if (FAILED(hr = CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
            &IID_IWICImagingFactory, (void **)&wic)))
    {
        printf("no_wic=%#lx\n", hr);
        return 1;
    }
    if (FAILED(hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory1, NULL,
            (void **)&factory)))
    {
        printf("no_d2d=%#lx\n", hr);
        return 1;
    }
    if (FAILED(hr = begin()))
    {
        printf("no_target=%#lx\n", hr);
        return 1;
    }

    /* An empty document: what Office's sign-in pane asks for. */
    doc = create_doc(NULL);
    report("empty_doc", doc != NULL, 0);
    if (!doc) { printf("done=0\n"); return 1; }
    CALL(get_viewport_t, doc, DOC_GET_VIEWPORT, &vp);
    report("viewport", vp.width == SIZE && vp.height == SIZE, (DWORD)vp.width);
    root = NULL;
    CALL(get_elem_t, doc, DOC_GET_ROOT, &root);
    ok = root && SUCCEEDED(CALL(get_tag_t, root, EL_GET_TAG_NAME, buf, ARRAY_SIZE(buf))) && !wcscmp(buf, L"svg");
    report("empty_root_svg", ok, 0);
    report("empty_root_no_children", root && !CALL(has_children_t, root, EL_HAS_CHILDREN), 0);
    child = NULL;
    report("iid_element", root && SUCCEEDED(CALL(qi_t, root, QI, &IID_SvgElement, &child)), 0);
    release(child);

    /* Build a tree. */
    rect = NULL;
    hr = CALL(create_child_t, root, EL_CREATE_CHILD, L"rect", &rect);
    report("create_child", SUCCEEDED(hr) && rect, hr);
    CALL(set_str_t, rect, EL_SET_ATTR_STR, L"x", 0, L"8");
    CALL(set_str_t, rect, EL_SET_ATTR_STR, L"y", 0, L"8");
    f = 16.0f;
    hr = CALL(set_pod_t, rect, EL_SET_ATTR_POD, L"width", 0 /* FLOAT */, &f, sizeof(f));
    report("set_float", SUCCEEDED(hr), hr);
    CALL(set_str_t, rect, EL_SET_ATTR_STR, L"height", 0, L"16");
    c.r = 1; c.g = 0; c.b = 0; c.a = 1;
    hr = CALL(set_pod_t, rect, EL_SET_ATTR_POD, L"fill", 1 /* COLOR */, &c, sizeof(c));
    report("set_color", SUCCEEDED(hr), hr);
    hr = CALL(set_pod_t, rect, EL_SET_ATTR_POD, L"fill", 1, &c, 4);
    report("set_pod_bad_size", hr == E_INVALIDARG, hr);
    hr = CALL(get_str_t, rect, EL_GET_ATTR_STR, L"fill", 0, buf, ARRAY_SIZE(buf));
    report("color_as_text", SUCCEEDED(hr) && !_wcsicmp(buf, L"#FF0000"), hr);
    f = 0.0f;
    hr = CALL(get_pod_t, rect, EL_GET_ATTR_POD, L"x", 0, &f, sizeof(f));
    report("text_as_float", SUCCEEDED(hr) && f == 8.0f, hr);
    len = 0;
    hr = CALL(get_len_t, rect, EL_GET_ATTR_LEN, L"height", 0, &len);
    report("attr_length", SUCCEEDED(hr) && len == 2, len);
    report("is_specified", CALL(is_specified_t, rect, EL_IS_SPECIFIED, L"x", NULL)
            && !CALL(is_specified_t, rect, EL_IS_SPECIFIED, L"stroke", NULL), 0);
    report("specified_count", CALL(count_t, rect, EL_SPECIFIED_COUNT) == 5, CALL(count_t, rect, EL_SPECIFIED_COUNT));
    child = NULL;
    CALL(get_elem_t, rect, EL_GET_PARENT, &child);
    report("parent", child == root, 0);
    release(child);

    draw(doc);
    end();
    report("draw_rect_inside", close_to(pixel(16, 16), RED, 3), pixel(16, 16));
    report("draw_rect_outside", close_to(pixel(30, 30), WHITE, 3), pixel(30, 30));

    /* A live paint: change it and the rect is drawn in the new color. */
    paint = NULL;
    hr = CALL(get_iid_t, rect, EL_GET_ATTR_IID, L"fill", &IID_SvgPaint, &paint);
    report("get_paint", SUCCEEDED(hr) && paint && CALL(paint_type_t, paint, PAINT_GET_PAINT_TYPE) == 1, hr);
    el = NULL;
    if (paint) CALL(get_elem_t, paint, ATTR_GET_ELEMENT, &el);
    report("paint_element", el == rect, 0);
    release(el);
    c.r = 0; c.g = 0; c.b = 1;
    if (paint) CALL(paint_set_color_t, paint, PAINT_SET_COLOR, &c);
    hr = CALL(get_str_t, rect, EL_GET_ATTR_STR, L"fill", 0, buf, ARRAY_SIZE(buf));
    report("paint_live_text", SUCCEEDED(hr) && !_wcsicmp(buf, L"#0000FF"), hr);
    begin();
    draw(doc);
    end();
    report("paint_live_draw", close_to(pixel(16, 16), BLUE, 3), pixel(16, 16));

    /* A paint from the document set on an element. */
    c.r = 0; c.g = 128.0f / 255.0f; c.b = 0;
    release(paint); paint = NULL;
    hr = CALL(create_paint_t, doc, DOC_CREATE_PAINT, 1, &c, NULL, &paint);
    if (SUCCEEDED(hr)) hr = CALL(set_obj_t, rect, EL_SET_ATTR_OBJ, L"fill", paint);
    begin();
    draw(doc);
    end();
    report("set_paint_object", SUCCEEDED(hr) && close_to(pixel(16, 16), GREEN, 3), pixel(16, 16));
    release(paint);

    /* Tree operations. */
    circle = NULL;
    CALL(create_child_t, root, EL_CREATE_CHILD, L"circle", &circle);
    next = NULL;
    hr = CALL(next_child_t, root, EL_GET_NEXT_CHILD, rect, &next);
    report("next_child", SUCCEEDED(hr) && next == circle, hr);
    release(next);
    hr = CALL(elem_elem_t, root, EL_REMOVE_CHILD, rect);
    child = NULL;
    CALL(get_elem_t, root, EL_GET_FIRST_CHILD, &child);
    report("remove_child", SUCCEEDED(hr) && child == circle, hr);
    release(child);
    hr = CALL(elem_elem_t, root, EL_APPEND_CHILD, rect);
    next = NULL;
    CALL(next_child_t, root, EL_GET_NEXT_CHILD, circle, &next);
    report("append_child", SUCCEEDED(hr) && next == rect, hr);
    release(next);

    /* Serialize, then read it back. */
    stream = SHCreateMemStream(NULL, 0);
    hr = CALL(serialize_t, doc, DOC_SERIALIZE, stream, NULL);
    {
        STATSTG st;
        LARGE_INTEGER zero = {{0}};
        char text[1024] = {0};
        ULONG got = 0;

        IStream_Stat(stream, &st, STATFLAG_NONAME);
        IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
        IStream_Read(stream, text, sizeof(text) - 1, &got);
        ok = SUCCEEDED(hr) && strstr(text, "<svg") && strstr(text, "<rect") && strstr(text, "<circle")
                && strstr(text, "width=\"16\"");
        report("serialize", ok, hr);
        if (!ok) printf("serialized: %s\n", text);
        IStream_Seek(stream, zero, STREAM_SEEK_SET, NULL);
        child = NULL;
        hr = CALL(deserialize_t, doc, DOC_DESERIALIZE, stream, &child);
        ok = SUCCEEDED(hr) && child && SUCCEEDED(CALL(get_tag_t, child, EL_GET_TAG_NAME, buf, ARRAY_SIZE(buf)))
                && !wcscmp(buf, L"svg") && CALL(has_children_t, child, EL_HAS_CHILDREN);
        report("deserialize", ok, hr);
        release(child);
    }
    IStream_Release(stream);
    release(circle);
    release(rect);
    release(root);
    release(doc);

    /* A document from XML: viewBox scaling, group transform, path, stroke,
     * use, display none, and a gradient. */
    stream = stream_from(
        "<?xml version=\"1.0\"?>\n"
        "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\""
        " viewBox=\"0 0 32 32\">\n"
        "  <defs>\n"
        "    <linearGradient id=\"lg\" x1=\"0\" y1=\"0\" x2=\"1\" y2=\"0\">\n"
        "      <stop offset=\"0\" stop-color=\"#ff0000\"/>\n"
        "      <stop offset=\"1\" stop-color=\"#0000ff\"/>\n"
        "    </linearGradient>\n"
        "    <rect id=\"tpl\" width=\"4\" height=\"4\" fill=\"blue\"/>\n"
        "  </defs>\n"
        "  <title>probe</title>\n"
        "  <circle id=\"c1\" cx=\"8\" cy=\"8\" r=\"6\" fill=\"#008000\"/>\n"
        "  <g transform=\"translate(16 0)\">\n"
        "    <path id=\"p1\" d=\"M2 2 h12 v12 h-12 z\" fill=\"rgb(255,0,0)\"/>\n"
        "  </g>\n"
        "  <rect x=\"1\" y=\"17\" width=\"14\" height=\"14\" fill=\"none\" stroke=\"black\" stroke-width=\"2\"/>\n"
        "  <use xlink:href=\"#tpl\" x=\"20\" y=\"20\"/>\n"
        "  <rect x=\"16\" y=\"26\" width=\"16\" height=\"6\" fill=\"url(#lg)\"/>\n"
        "  <rect x=\"0\" y=\"0\" width=\"32\" height=\"32\" fill=\"yellow\" display=\"none\"/>\n"
        "</svg>\n");
    begin();
    doc = create_doc(stream);
    IStream_Release(stream);
    report("xml_doc", doc != NULL, 0);
    if (!doc) { end(); printf("done=0\n"); return 1; }
    draw(doc);
    end();
    /* Everything is scaled by 2 (32 -> 64). */
    report("xml_circle", close_to(pixel(16, 16), GREEN, 3), pixel(16, 16));
    report("xml_circle_outside", close_to(pixel(2, 2), WHITE, 3), pixel(2, 2));
    report("xml_path_in_group", close_to(pixel(48, 16), RED, 3), pixel(48, 16));
    report("xml_path_outside", close_to(pixel(34, 16), WHITE, 3), pixel(34, 16));
    report("xml_stroke", close_to(pixel(2, 48), BLACK, 3), pixel(2, 48));
    report("xml_fill_none", close_to(pixel(16, 48), WHITE, 3), pixel(16, 48));
    report("xml_use", close_to(pixel(44, 44), BLUE, 3), pixel(44, 44));
    report("xml_defs_not_drawn", close_to(pixel(3, 3), WHITE, 3), pixel(3, 3));
    {
        DWORD l = pixel(34, 58), r = pixel(62, 58);
        ok = ((l >> 16) & 0xff) > 0xc0 && (l & 0xff) < 0x40 && (r & 0xff) > 0xc0 && ((r >> 16) & 0xff) < 0x40;
        report("xml_gradient", ok, l);
        if (!ok) printf("gradient_right=0x%08lx\n", r);
    }

    el = NULL;
    hr = CALL(find_t, doc, DOC_FIND_BY_ID, L"p1", &el);
    ok = SUCCEEDED(hr) && el && SUCCEEDED(CALL(get_tag_t, el, EL_GET_TAG_NAME, buf, ARRAY_SIZE(buf)))
            && !wcscmp(buf, L"path");
    report("find_by_id", ok, hr);
    path = NULL;
    if (el) hr = CALL(get_iid_t, el, EL_GET_ATTR_IID, L"d", &IID_SvgPathData, &path);
    report("path_data", SUCCEEDED(hr) && path && CALL(count_t, path, PATH_GET_COMMANDS_COUNT) == 5, hr);
    geometry = NULL;
    if (path) hr = CALL(path_geometry_t, path, PATH_CREATE_GEOMETRY, D2D1_FILL_MODE_WINDING, &geometry);
    ok = SUCCEEDED(hr) && geometry && SUCCEEDED(ID2D1PathGeometry_GetBounds(geometry, NULL, &bounds))
            && bounds.left == 2 && bounds.top == 2 && bounds.right == 14 && bounds.bottom == 14;
    report("path_geometry", ok, hr);
    if (geometry) ID2D1PathGeometry_Release(geometry);
    release(path);
    release(el);

    {
        static const FLOAT data[] = { 1, 1, 5, 5 };
        static const UINT32 commands[] = { 1 /* M */, 3 /* L */ };
        path = NULL;
        hr = CALL(create_path_t, doc, DOC_CREATE_PATH_DATA, data, 4, commands, 2, &path);
        clone = NULL;
        el = NULL;
        CALL(find_t, doc, DOC_FIND_BY_ID, L"p1", &el);
        if (SUCCEEDED(hr) && el) hr = CALL(set_obj_t, el, EL_SET_ATTR_OBJ, L"d", path);
        if (SUCCEEDED(hr) && el) hr = CALL(get_str_t, el, EL_GET_ATTR_STR, L"d", 0, buf, ARRAY_SIZE(buf));
        report("path_data_text", SUCCEEDED(hr) && !wcscmp(buf, L"M1 1 L5 5"), hr);
        if (FAILED(hr) || wcscmp(buf, L"M1 1 L5 5")) printf("path_text=%ls\n", buf);
        release(el);
        release(path);
    }

    {
        D2D1_SIZE_F one = { 1, 1 };
        stream = stream_from("<svg><rect></svg>");
        el = NULL;
        hr = CALL(create_svg_t, dc5, DC5_CREATE_SVG, stream, one, &el);
        report("bad_xml_fails", FAILED(hr) && !el, hr);
        release(el);
        IStream_Release(stream);
    }

    release(doc);
    printf("done=1\n");
    return 0;
}
