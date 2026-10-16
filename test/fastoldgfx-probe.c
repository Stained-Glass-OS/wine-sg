/* ddraw, d3d9, d3d10, d3dcompiler_43: the old-graphics smoke probe (raw vtables) */
#define COBJMACROS
#include <windows.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define VT(obj) (*(void ***)(obj))
#define CALL(obj, idx, ret, ...) (((ret (WINAPI *)(void *, ...))VT(obj)[idx])(obj, ##__VA_ARGS__))

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ---- d3d10: profiles of no device ---- */
    {
        HMODULE m = LoadLibraryA("d3d10.dll");
        const char *(WINAPI *vs)(void *) = (void *)GetProcAddress(m, "D3D10GetVertexShaderProfile");
        CHECK(vs && !vs(NULL), "D3D10GetVertexShaderProfile(NULL)");
    }

    /* ---- d3d9: device palettes, GPU priority is Ex only ---- */
    {
        HMODULE m = LoadLibraryA("d3d9.dll");
        void *(WINAPI *create)(UINT) = (void *)GetProcAddress(m, "Direct3DCreate9");
        void *d3d = create ? create(32) : NULL;
        CHECK(d3d != NULL, "Direct3DCreate9");
        if (d3d)
        {
            struct { UINT AdapterOrdinal; int dummy; } unused;
            DWORD params[64] = {0};   /* D3DPRESENT_PARAMETERS: Windowed is the 8th member on both ABIs */
            HWND hwnd = CreateWindowA("static", "d3d9", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, NULL, NULL, NULL, NULL);
            void *device = NULL;
            HRESULT hr;
            PALETTEENTRY pal[256], out[256];
            UINT cur = 99, i;
            (void)unused;
            /* D3DPRESENT_PARAMETERS layout: BackBufferWidth, Height, Format, Count, MultiSampleType, Quality,
             * SwapEffect, hDeviceWindow (pointer), Windowed, ... use a 32-bit-safe build of the struct */
            struct
            {
                UINT w, h, fmt, count, ms, msq, swap;
                HWND window;
                BOOL windowed, auto_ds;
                UINT ds_fmt, flags, refresh, interval;
            } pp = { 64, 64, 22 /* X8R8G8B8 */, 1, 0, 0, 1 /* DISCARD */, hwnd, TRUE, FALSE, 0, 0, 0, 0 };
            (void)params;
            hr = CALL(d3d, 16, HRESULT, 0, 1 /* HAL */, hwnd, 0x20 /* SOFTWARE_VERTEXPROCESSING */, &pp, &device);
            if (FAILED(hr) || !device)
                printf("SKIP d3d9 device (hr %lx)\n", hr);
            else
            {
                for (i = 0; i < 256; i++) { pal[i].peRed = i; pal[i].peGreen = 255 - i; pal[i].peBlue = i / 2; pal[i].peFlags = 0xff; }
                /* IDirect3DDevice9: SetPaletteEntries 71, GetPaletteEntries 72, SetCurrentTexturePalette 73, Get 74 */
                hr = CALL(device, 72, HRESULT, 3, out);
                CHECK(hr == 0x8876086c, "GetPaletteEntries before Set: %lx", hr);
                hr = CALL(device, 71, HRESULT, 3, pal);
                CHECK(hr == S_OK, "SetPaletteEntries %lx", hr);
                memset(out, 0, sizeof(out));
                hr = CALL(device, 72, HRESULT, 3, out);
                CHECK(hr == S_OK && !memcmp(pal, out, sizeof(pal)), "GetPaletteEntries %lx", hr);
                hr = CALL(device, 73, HRESULT, 5);
                CHECK(hr == S_OK, "SetCurrentTexturePalette %lx", hr);
                hr = CALL(device, 74, HRESULT, &cur);
                CHECK(hr == S_OK && cur == 5, "GetCurrentTexturePalette %lx %u", hr, cur);
                CALL(device, 2, ULONG);
            }
            DestroyWindow(hwnd);
            CALL(d3d, 2, ULONG);
        }
    }

    /* ---- ddraw: WaitForVerticalBlank blocks about a frame ---- */
    {
        HMODULE m = LoadLibraryA("ddraw.dll");
        HRESULT (WINAPI *create)(void *, void **, void *) = (void *)GetProcAddress(m, "DirectDrawCreate");
        void *dd = NULL;
        HRESULT hr = create ? create(NULL, &dd, NULL) : E_FAIL;
        if (FAILED(hr) || !dd)
            printf("SKIP ddraw (hr %lx)\n", hr);
        else
        {
            LARGE_INTEGER q0, q1, qf;
            DWORD t0, t1;
            int i;
            /* IDirectDraw: WaitForVerticalBlank is slot 22 */
            hr = CALL(dd, 22, HRESULT, 2 /* DDWAITVB_BLOCKBEGINEVENT */, NULL);
            CHECK(hr == 0x887600a5 || hr == 0x80004001 || FAILED(hr), "BLOCKBEGINEVENT %lx", hr);
            hr = CALL(dd, 22, HRESULT, 0x10, NULL);
            CHECK(FAILED(hr), "invalid flags %lx", hr);
            QueryPerformanceFrequency(&qf);
            QueryPerformanceCounter(&q0);
            for (i = 0; i < 10; i++)
            {
                hr = CALL(dd, 22, HRESULT, 1 /* DDWAITVB_BLOCKBEGIN */, NULL);
                CHECK(hr == S_OK, "WaitForVerticalBlank %lx", hr);
            }
            QueryPerformanceCounter(&q1);
            t0 = 0;
            t1 = (DWORD)((q1.QuadPart - q0.QuadPart) * 1000 / qf.QuadPart);
            CHECK(t1 - t0 >= 100 && t1 - t0 < 400, "10 waits took %lu ms", t1 - t0);
            hr = CALL(dd, 22, HRESULT, 4 /* DDWAITVB_BLOCKEND */, NULL);
            CHECK(hr == S_OK, "BLOCKEND %lx", hr);
            CALL(dd, 2, ULONG);
        }
    }

    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
