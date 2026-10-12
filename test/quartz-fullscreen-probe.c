/* Video Renderer / VMR7 / VMR9 IVideoWindow full screen mode: renderers return
 * E_NOTIMPL, the filter graph maps it to OAFALSE / S_FALSE. */
#define COBJMACROS
#include <windows.h>
#include <dshow.h>
#include <stdio.h>

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL  %s: ", name); printf(__VA_ARGS__); printf("\n"); } } while (0)

struct row { const char *name; const CLSID *clsid; };

int main(void)
{
    static const struct row rows[] = {
        {"VideoRenderer", &CLSID_VideoRenderer},
        {"VMR7", &CLSID_VideoMixingRenderer},
        {"VMR9", &CLSID_VideoMixingRenderer9},
    };
    unsigned i;
    CoInitialize(NULL);
    for (i = 0; i < sizeof(rows)/sizeof(rows[0]); i++)
    {
        const char *name = rows[i].name;
        IBaseFilter *filter = NULL;
        IVideoWindow *w = NULL, *gw = NULL;
        IFilterGraph2 *graph = NULL;
        LONG v;
        HRESULT hr;

        hr = CoCreateInstance(rows[i].clsid, NULL, CLSCTX_INPROC_SERVER, &IID_IBaseFilter, (void **)&filter);
        CHECK(hr == S_OK, "create hr %#lx", hr);
        if (FAILED(hr)) continue;
        hr = IBaseFilter_QueryInterface(filter, &IID_IVideoWindow, (void **)&w);
        CHECK(hr == S_OK, "QI hr %#lx", hr);
        if (FAILED(hr)) { IBaseFilter_Release(filter); continue; }

        v = 0x1234;
        hr = IVideoWindow_get_FullScreenMode(w, &v);
        CHECK(hr == E_NOTIMPL, "renderer get hr %#lx", hr);
        hr = IVideoWindow_put_FullScreenMode(w, OATRUE);
        CHECK(hr == E_NOTIMPL, "renderer put(TRUE) hr %#lx", hr);
        hr = IVideoWindow_put_FullScreenMode(w, OAFALSE);
        CHECK(hr == E_NOTIMPL, "renderer put(FALSE) hr %#lx", hr);
        v = 0x1234;
        hr = IVideoWindow_get_FullScreenMode(w, &v);
        CHECK(hr == E_NOTIMPL && v != OATRUE, "renderer get after put hr %#lx v %ld", hr, v);

        hr = CoCreateInstance(&CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER, &IID_IFilterGraph2, (void **)&graph);
        CHECK(hr == S_OK, "graph hr %#lx", hr);
        if (SUCCEEDED(hr))
        {
            hr = IFilterGraph2_AddFilter(graph, filter, NULL);
            CHECK(hr == S_OK, "AddFilter hr %#lx", hr);
            hr = IFilterGraph2_QueryInterface(graph, &IID_IVideoWindow, (void **)&gw);
            CHECK(hr == S_OK, "graph QI hr %#lx", hr);
            if (gw)
            {
                v = 0x1234;
                hr = IVideoWindow_get_FullScreenMode(gw, &v);
                CHECK(hr == S_OK && v == OAFALSE, "graph get hr %#lx v %ld", hr, v);
                hr = IVideoWindow_put_FullScreenMode(gw, OAFALSE);
                CHECK(hr == S_FALSE, "graph put(FALSE) hr %#lx", hr);
                hr = IVideoWindow_put_FullScreenMode(gw, OATRUE);
                CHECK(hr == E_NOTIMPL, "graph put(TRUE) hr %#lx", hr);
                IVideoWindow_Release(gw);
            }
            IFilterGraph2_RemoveFilter(graph, filter);
            IFilterGraph2_Release(graph);
        }
        IVideoWindow_Release(w);
        IBaseFilter_Release(filter);
        printf("done  %s\n", name);
    }
    printf(failures ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", failures);
    return failures != 0;
}
