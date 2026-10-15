#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef struct { ULONG length; WCHAR *chars; } WS_STRING;
typedef struct { int scheme; } WS_URL;
typedef struct { WS_URL url; WS_STRING host; USHORT port; WS_STRING portAsString; WS_STRING path; WS_STRING query; WS_STRING fragment; } WS_HTTP_URL;
typedef struct { int id; void *value; ULONG valueSize; } WS_HEAP_PROPERTY;
typedef struct { WS_HEAP_PROPERTY *properties; ULONG propertyCount; } WS_HEAP_PROPERTIES;
typedef struct { int id; void *value; ULONG valueSize; } WS_MESSAGE_PROPERTY;
typedef void WS_HEAP;
typedef void WS_MESSAGE;
#define WS_HEAP_PROPERTY_MAX_SIZE 0
#define WS_MESSAGE_PROPERTY_HEAP 1
#define WS_MESSAGE_PROPERTY_HEAP_PROPERTIES 9
#define WS_ENVELOPE_VERSION_SOAP_1_2 2
#define WS_ADDRESSING_VERSION_1_0 2
static HRESULT (WINAPI *WsCreateHeap)(SIZE_T, SIZE_T, const void *, ULONG, WS_HEAP **, void *);
static void (WINAPI *WsFreeHeap)(WS_HEAP *);
static HRESULT (WINAPI *WsGetHeapProperty)(WS_HEAP *, int, void *, ULONG, void *);
static HRESULT (WINAPI *WsDecodeUrl)(const WS_STRING *, ULONG, WS_HEAP *, WS_URL **, void *);
static HRESULT (WINAPI *WsEncodeUrl)(const WS_URL *, ULONG, WS_HEAP *, WS_STRING *, void *);
static HRESULT (WINAPI *WsCreateMessage)(int, int, const WS_MESSAGE_PROPERTY *, ULONG, WS_MESSAGE **, void *);
static void (WINAPI *WsFreeMessage)(WS_MESSAGE *);
static HRESULT (WINAPI *WsGetMessageProperty)(WS_MESSAGE *, int, void *, ULONG, void *);

#ifndef WS_URL_FLAGS_ZERO_TERMINATE
#define WS_URL_FLAGS_ALLOW_HOST_WILDCARDS 1
#define WS_URL_FLAGS_NO_PATH_COLLAPSE 2
#define WS_URL_FLAGS_ZERO_TERMINATE 4
#endif

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void)
{
    WS_HEAP *heap;
    WS_STRING in, out;
    WS_URL *url;
    WS_HTTP_URL *h;
    HRESULT hr;
    static WCHAR s1[] = L"http://host:81/a/./b/../c?x=1#f";
    static WCHAR s2[] = L"http://host/a/../b";

    {
        HMODULE m = LoadLibraryA("webservices.dll");
#define LOAD(f) f = (void *)GetProcAddress(m, #f)
        LOAD(WsCreateHeap); LOAD(WsFreeHeap); LOAD(WsGetHeapProperty); LOAD(WsDecodeUrl); LOAD(WsEncodeUrl);
        LOAD(WsCreateMessage); LOAD(WsFreeMessage); LOAD(WsGetMessageProperty);
    }
    hr = WsCreateHeap(1 << 16, 0, NULL, 0, &heap, NULL);
    CHECK(hr == S_OK, "heap %lx", hr);

    in.chars = s1; in.length = wcslen(s1);
    hr = WsDecodeUrl(&in, 0, heap, &url, NULL);
    CHECK(hr == S_OK, "decode %lx", hr);
    h = (WS_HTTP_URL *)url;
    CHECK(h->path.length == 4 && !wcsncmp(h->path.chars, L"/a/c", 4), "collapsed path '%.*ls'", (int)h->path.length, h->path.chars);
    CHECK(!wcsncmp(s1, L"http://host:81/a/./b/../c?x=1#f", 31), "caller string untouched");
    CHECK(h->port == 81 && h->query.length == 3 && h->fragment.length == 1, "other parts");

    hr = WsDecodeUrl(&in, WS_URL_FLAGS_NO_PATH_COLLAPSE, heap, &url, NULL);
    h = (WS_HTTP_URL *)url;
    CHECK(hr == S_OK && h->path.length == 11, "uncollapsed path length %u", hr == S_OK ? h->path.length : 0);

    hr = WsDecodeUrl(&in, WS_URL_FLAGS_ZERO_TERMINATE, heap, &url, NULL);
    h = (WS_HTTP_URL *)url;
    CHECK(hr == S_OK && h->host.chars[h->host.length] == 0 && h->path.chars[h->path.length] == 0 &&
          h->query.chars[h->query.length] == 0 && h->fragment.chars[h->fragment.length] == 0, "zero terminated parts");

    hr = WsDecodeUrl(&in, 0x80, heap, &url, NULL);
    CHECK(hr == E_INVALIDARG, "bad flags %lx", hr);

    in.chars = s2; in.length = wcslen(s2);
    hr = WsDecodeUrl(&in, 0, heap, &url, NULL);
    h = (WS_HTTP_URL *)url;
    CHECK(hr == S_OK && h->path.length == 2 && !wcsncmp(h->path.chars, L"/b", 2), "dotdot collapse");

    hr = WsEncodeUrl((const WS_URL *)h, WS_URL_FLAGS_ZERO_TERMINATE, heap, &out, NULL);
    CHECK(hr == S_OK && out.chars[out.length] == 0 && !wcscmp(out.chars, L"http://host/b"), "encode zero terminate");
    hr = WsEncodeUrl((const WS_URL *)h, 0x10, heap, &out, NULL);
    CHECK(hr == E_INVALIDARG, "encode bad flags %lx", hr);

    /* message with heap properties and reader/writer properties */
    {
        WS_MESSAGE *msg;
        WS_HEAP_PROPERTY hp[1];
        WS_HEAP_PROPERTIES props;
        SIZE_T max = 8192, got = 0;
        WS_MESSAGE_PROPERTY mp[1];
        WS_HEAP *mheap = NULL;
        WS_HEAP_PROPERTIES back;

        hp[0].id = WS_HEAP_PROPERTY_MAX_SIZE; hp[0].value = &max; hp[0].valueSize = sizeof(max);
        props.properties = hp; props.propertyCount = 1;
        mp[0].id = WS_MESSAGE_PROPERTY_HEAP_PROPERTIES; mp[0].value = &props; mp[0].valueSize = sizeof(props);
        hr = WsCreateMessage(WS_ENVELOPE_VERSION_SOAP_1_2, WS_ADDRESSING_VERSION_1_0, mp, 1, &msg, NULL);
        CHECK(hr == S_OK, "create message with heap properties %lx", hr);
        if (hr == S_OK)
        {
            hr = WsGetMessageProperty(msg, WS_MESSAGE_PROPERTY_HEAP, &mheap, sizeof(mheap), NULL);
            CHECK(hr == S_OK && mheap, "message heap");
            hr = WsGetHeapProperty(mheap, WS_HEAP_PROPERTY_MAX_SIZE, &got, sizeof(got), NULL);
            CHECK(hr == S_OK && got == 8192, "message heap max size %u", (unsigned)got);
            memset(&back, 0, sizeof(back));
            hr = WsGetMessageProperty(msg, WS_MESSAGE_PROPERTY_HEAP_PROPERTIES, &back, sizeof(back), NULL);
            CHECK(hr == S_OK && back.propertyCount == 1 && back.properties && back.properties != hp &&
                  back.properties[0].id == WS_HEAP_PROPERTY_MAX_SIZE, "properties are kept as a private copy");
            WsFreeMessage(msg);
        }
    }

    WsFreeHeap(heap);
    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
