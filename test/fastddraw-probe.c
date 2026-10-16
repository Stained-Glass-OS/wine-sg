/* ddraw: flippable texture surfaces and DuplicateSurface's refusals (patch 3308), run by test/fastddraw-gate.sh */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <ddraw.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[300]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)

static void set_pixel(IDirectDrawSurface4 *s, DWORD value)
{
    DDSURFACEDESC2 d = { sizeof(d) };
    if (SUCCEEDED(IDirectDrawSurface4_Lock(s, NULL, &d, DDLOCK_WAIT, NULL)))
    {
        *(DWORD *)d.lpSurface = value;
        IDirectDrawSurface4_Unlock(s, NULL);
    }
}

static DWORD get_pixel(IDirectDrawSurface4 *s)
{
    DDSURFACEDESC2 d = { sizeof(d) };
    DWORD v = 0xdeadbeef;
    if (SUCCEEDED(IDirectDrawSurface4_Lock(s, NULL, &d, DDLOCK_WAIT | DDLOCK_READONLY, NULL)))
    {
        v = *(DWORD *)d.lpSurface;
        IDirectDrawSurface4_Unlock(s, NULL);
    }
    return v;
}

int main(void)
{
    IDirectDraw *dd1 = NULL;
    IDirectDraw4 *dd4 = NULL;
    IDirectDraw7 *dd7 = NULL;
    IDirectDrawSurface4 *front = NULL, *back1 = NULL, *back2 = NULL, *back3 = NULL, *s = NULL, *dup = NULL;
    DDSURFACEDESC2 desc;
    DDSCAPS2 caps = { DDSCAPS_FLIP };
    HWND window;
    HRESULT hr;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    window = CreateWindowA("static", "sgddraw", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 200, NULL, NULL, NULL, NULL);
    hr = DirectDrawCreate(NULL, &dd1, NULL);
    if (FAILED(hr)) { check(0, "DirectDrawCreate"); return 1; }
    IDirectDraw_QueryInterface(dd1, &IID_IDirectDraw4, (void **)&dd4);
    IDirectDraw_QueryInterface(dd1, &IID_IDirectDraw7, (void **)&dd7);
    hr = IDirectDraw4_SetCooperativeLevel(dd4, window, DDSCL_NORMAL);
    CHECKF(hr == DD_OK, "cooperative level (%#lx)", hr);

    /* a flippable texture of three back buffers */
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_BACKBUFFERCOUNT;
    desc.ddsCaps.dwCaps = DDSCAPS_COMPLEX | DDSCAPS_FLIP | DDSCAPS_TEXTURE;
    desc.dwWidth = 64;
    desc.dwHeight = 64;
    desc.dwBackBufferCount = 3;
    hr = IDirectDraw4_CreateSurface(dd4, &desc, &front, NULL);
    CHECKF(hr == DD_OK && front, "a flippable texture can be created (%#lx)", hr);
    if (FAILED(hr)) goto errors;

    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    IDirectDrawSurface4_GetSurfaceDesc(front, &desc);
    CHECKF((desc.ddsCaps.dwCaps & (DDSCAPS_FRONTBUFFER | DDSCAPS_COMPLEX | DDSCAPS_FLIP | DDSCAPS_TEXTURE))
            == (DDSCAPS_FRONTBUFFER | DDSCAPS_COMPLEX | DDSCAPS_FLIP | DDSCAPS_TEXTURE), "the front buffer's caps %#lx", desc.ddsCaps.dwCaps);

    hr = IDirectDrawSurface4_GetAttachedSurface(front, &caps, &back1);
    CHECKF(hr == DD_OK, "first back buffer (%#lx)", hr);
    hr = back1 ? IDirectDrawSurface4_GetAttachedSurface(back1, &caps, &back2) : E_FAIL;
    CHECKF(hr == DD_OK, "second back buffer (%#lx)", hr);
    hr = back2 ? IDirectDrawSurface4_GetAttachedSurface(back2, &caps, &back3) : E_FAIL;
    CHECKF(hr == DD_OK, "third back buffer (%#lx)", hr);
    hr = back3 ? IDirectDrawSurface4_GetAttachedSurface(back3, &caps, &s) : E_FAIL;
    CHECKF(hr == DD_OK && s == front, "and the chain returns to the front buffer (%#lx)", hr);
    if (s) IDirectDrawSurface4_Release(s);

    if (back1)
    {
        memset(&desc, 0, sizeof(desc));
        desc.dwSize = sizeof(desc);
        IDirectDrawSurface4_GetSurfaceDesc(back1, &desc);
        CHECKF((desc.ddsCaps.dwCaps & (DDSCAPS_BACKBUFFER | DDSCAPS_TEXTURE)) == (DDSCAPS_BACKBUFFER | DDSCAPS_TEXTURE), "the back buffer is a texture too (%#lx)", desc.ddsCaps.dwCaps);
    }

    /* each surface of the chain holds its own pixels, and a flip rotates them */
    if (front && back1 && back2 && back3)
    {
        set_pixel(front, 0x11111111);
        set_pixel(back1, 0x22222222);
        set_pixel(back2, 0x33333333);
        set_pixel(back3, 0x44444444);
        CHECKF(get_pixel(front) == 0x11111111 && get_pixel(back1) == 0x22222222 && get_pixel(back3) == 0x44444444, "the surfaces do not share their memory");
        hr = IDirectDrawSurface4_Flip(front, NULL, DDFLIP_WAIT);
        CHECKF(hr == DD_OK, "Flip (%#lx)", hr);
        CHECKF(get_pixel(front) == 0x22222222, "after a flip the front buffer has the first back buffer's pixels (%#lx)", get_pixel(front));
        CHECKF(get_pixel(back1) == 0x33333333 && get_pixel(back2) == 0x44444444 && get_pixel(back3) == 0x11111111,
                "and the others moved up the chain (%#lx %#lx %#lx)", get_pixel(back1), get_pixel(back2), get_pixel(back3));
        hr = IDirectDrawSurface4_Flip(front, back2, DDFLIP_WAIT);
        CHECKF(hr == DD_OK && get_pixel(front) == 0x44444444, "Flip to a given back buffer (%#lx, %#lx)", hr, get_pixel(front));
    }
    if (back3) IDirectDrawSurface4_Release(back3);
    if (back2) IDirectDrawSurface4_Release(back2);
    if (back1) IDirectDrawSurface4_Release(back1);
    IDirectDrawSurface4_Release(front);

    /* a flippable texture needs back buffers and the complex flag, as any flippable surface */
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_BACKBUFFERCOUNT;
    desc.ddsCaps.dwCaps = DDSCAPS_FLIP | DDSCAPS_TEXTURE;
    desc.dwWidth = 64;
    desc.dwHeight = 64;
    desc.dwBackBufferCount = 1;
    hr = IDirectDraw4_CreateSurface(dd4, &desc, &s, NULL);
    CHECKF(hr == DDERR_INVALIDCAPS, "flip and texture without complex is refused (%#lx)", hr);
    desc.ddsCaps.dwCaps |= DDSCAPS_COMPLEX;
    desc.dwBackBufferCount = 0;
    hr = IDirectDraw4_CreateSurface(dd4, &desc, &s, NULL);
    CHECKF(hr == DDERR_INVALIDCAPS, "and without a back buffer count (%#lx)", hr);

errors:
    /* DuplicateSurface */
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
    desc.dwWidth = 32;
    desc.dwHeight = 32;
    hr = IDirectDraw4_CreateSurface(dd4, &desc, &s, NULL);
    CHECKF(hr == DD_OK, "an offscreen surface (%#lx)", hr);
    if (s)
    {
        dup = (IDirectDrawSurface4 *)0x1234;
        hr = IDirectDraw4_DuplicateSurface(dd4, s, &dup);
        CHECKF(hr == DD_OK && dup && dup != s, "an ordinary surface is duplicated (%#lx)", hr);
        if (hr == DD_OK && dup)
        {
            memset(&desc, 0, sizeof(desc));
            desc.dwSize = sizeof(desc);
            IDirectDrawSurface4_GetSurfaceDesc(dup, &desc);
            CHECKF(desc.dwWidth == 32 && desc.dwHeight == 32, "of the same size (%lux%lu)", desc.dwWidth, desc.dwHeight);
            IDirectDrawSurface4_Release(dup);
        }
        hr = IDirectDraw4_DuplicateSurface(dd4, s, NULL);
        CHECKF(hr == DDERR_INVALIDPARAMS, "no destination (%#lx)", hr);
        dup = (IDirectDrawSurface4 *)0x1234;
        hr = IDirectDraw4_DuplicateSurface(dd4, NULL, &dup);
        CHECKF(hr == DDERR_INVALIDOBJECT && !dup, "no source (%#lx)", hr);
        IDirectDrawSurface4_Release(s);
    }

    /* a 3-D surface */
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    desc.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_3DDEVICE;
    desc.dwWidth = 32;
    desc.dwHeight = 32;
    hr = IDirectDraw4_CreateSurface(dd4, &desc, &s, NULL);
    if (SUCCEEDED(hr))
    {
        hr = IDirectDraw4_DuplicateSurface(dd4, s, &dup);
        CHECKF(hr == DDERR_CANTDUPLICATE, "a 3-D surface cannot be duplicated (%#lx)", hr);
        IDirectDrawSurface4_Release(s);
    }
    else printf("NOTE  no 3-D offscreen surface here (%#lx)\n", hr);

    /* the primary surface */
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS;
    desc.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
    hr = IDirectDraw4_CreateSurface(dd4, &desc, &s, NULL);
    CHECKF(hr == DD_OK, "a primary surface (%#lx)", hr);
    if (s)
    {
        hr = IDirectDraw4_DuplicateSurface(dd4, s, &dup);
        CHECKF(hr == DDERR_CANTDUPLICATE, "the primary surface cannot be duplicated (%#lx)", hr);
        IDirectDrawSurface4_Release(s);
    }

    /* an implicitly created surface: a mipmap level */
    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_MIPMAPCOUNT;
    desc.ddsCaps.dwCaps = DDSCAPS_TEXTURE | DDSCAPS_MIPMAP | DDSCAPS_COMPLEX | DDSCAPS_SYSTEMMEMORY;
    desc.dwWidth = 64;
    desc.dwHeight = 64;
    desc.dwMipMapCount = 3;
    hr = IDirectDraw4_CreateSurface(dd4, &desc, &s, NULL);
    if (SUCCEEDED(hr))
    {
        IDirectDrawSurface4 *level = NULL;
        DDSCAPS2 mip = { DDSCAPS_TEXTURE | DDSCAPS_MIPMAP };

        hr = IDirectDrawSurface4_GetAttachedSurface(s, &mip, &level);
        CHECKF(hr == DD_OK && level, "a mipmap level (%#lx)", hr);
        if (level)
        {
            hr = IDirectDraw4_DuplicateSurface(dd4, level, &dup);
            CHECKF(hr == DDERR_CANTDUPLICATE, "a mipmap level cannot be duplicated (%#lx)", hr);
            IDirectDrawSurface4_Release(level);
        }
        IDirectDrawSurface4_Release(s);
    }

    /* the other interface versions answer alike */
    {
        IDirectDrawSurface *s1 = NULL, *d1 = NULL;
        DDSURFACEDESC d;

        memset(&d, 0, sizeof(d));
        d.dwSize = sizeof(d);
        d.dwFlags = DDSD_CAPS;
        d.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
        hr = IDirectDraw_CreateSurface(dd1, &d, &s1, NULL);
        if (SUCCEEDED(hr))
        {
            hr = IDirectDraw_DuplicateSurface(dd1, s1, &d1);
            CHECKF(hr == DDERR_CANTDUPLICATE, "ddraw 1: the primary surface cannot be duplicated (%#lx)", hr);
            IDirectDrawSurface_Release(s1);
        }
    }

    IDirectDraw7_Release(dd7);
    IDirectDraw4_Release(dd4);
    IDirectDraw_Release(dd1);
    DestroyWindow(window);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
