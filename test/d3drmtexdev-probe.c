/* d3drm (patches/sg/2662): the decal scale, transparent colour and cache options of a texture; the update
 * callbacks, buffer count, shades and texture quality of a device. */
#define COBJMACROS
#include <windows.h>
#include <d3drm.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static int failures;

static void check(int ok, const char *fmt, ...)
{
    char buf[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s  %s\n", ok ? "PASS" : "FAIL", buf);
    if (!ok) failures++;
}

static const GUID my_CLSID_Device = { 0x4fa3568e, 0x623f, 0x11cf, { 0xac, 0x4a, 0x00, 0x00, 0xc0, 0x38, 0x25, 0xa1 } };
static const GUID my_CLSID_Texture = { 0x4fa35695, 0x623f, 0x11cf, { 0xac, 0x4a, 0x00, 0x00, 0xc0, 0x38, 0x25, 0xa1 } };
static const GUID my_IID_Device3 = { 0x549f498b, 0xbfeb, 0x11d1, { 0x8e, 0xd8, 0x00, 0xa0, 0xc9, 0x67, 0xa4, 0x82 } };
static const GUID my_IID_Texture3 = { 0xff6b7f73, 0xa40e, 0x11d1, { 0x91, 0xf9, 0x00, 0x00, 0xf8, 0x75, 0x8e, 0x66 } };
static const GUID my_IID_Drm3 = { 0x4516ec83, 0x8f20, 0x11d0, { 0x9b, 0x6d, 0x00, 0x00, 0xc0, 0x78, 0x1b, 0xc3 } };
#define BADVALUE ((HRESULT)0x88760316)
#define NOTFOUND ((HRESULT)0x88760311)

static int calls;
static void __cdecl update_cb(IDirect3DRMDevice *device, void *ctx, int count, D3DRECT *rects)
{
    calls += count;
}

int main(void)
{
    HMODULE mod = LoadLibraryA("d3drm.dll");
    HRESULT (WINAPI *create)(IDirect3DRM **) = mod ? (void *)GetProcAddress(mod, "Direct3DRMCreate") : NULL;
    IDirect3DRM *drm1 = NULL;
    IDirect3DRM3 *drm = NULL;
    IDirect3DRMDevice3 *dev = NULL;
    IDirect3DRMTexture3 *tex = NULL;
    LONG importance;
    DWORD flags;
    HRESULT hr;

    if (!create || FAILED(create(&drm1))) { printf("FAIL  no Direct3DRM\nRESULT: FAIL\n"); return 1; }
    IDirect3DRM_QueryInterface(drm1, &my_IID_Drm3, (void **)&drm);
    hr = IDirect3DRM3_CreateObject(drm, &my_CLSID_Texture, NULL, &my_IID_Texture3, (void **)&tex);
    check(hr == S_OK && tex, "a texture (%#lx)", hr);
    hr = IDirect3DRM3_CreateObject(drm, &my_CLSID_Device, NULL, &my_IID_Device3, (void **)&dev);
    check(hr == S_OK && dev, "a device (%#lx)", hr);
    if (!tex || !dev) goto done;

    check(IDirect3DRMTexture3_SetDecalScale(tex, 3) == S_OK && IDirect3DRMTexture3_GetDecalScale(tex) == 3, "decal scale");
    check(IDirect3DRMTexture3_SetDecalTransparentColor(tex, 0xff00ff00) == S_OK
          && IDirect3DRMTexture3_GetDecalTransparentColor(tex) == 0xff00ff00, "decal transparent colour");
    check(IDirect3DRMTexture3_SetCacheOptions(tex, -5, 0x11) == S_OK, "SetCacheOptions");
    hr = IDirect3DRMTexture3_GetCacheOptions(tex, &importance, &flags);
    check(hr == S_OK && importance == -5 && flags == 0x11, "GetCacheOptions (%ld %#lx)", importance, flags);
    check(IDirect3DRMTexture3_GetCacheOptions(tex, NULL, &flags) == BADVALUE, "GetCacheOptions(NULL) is D3DRMERR_BADVALUE");
    check(IDirect3DRMTexture3_Changed(tex, 1, 0, NULL) == S_OK, "Changed");
    check(IDirect3DRMTexture3_Changed(tex, 1, 2, NULL) == BADVALUE, "Changed with rectangles missing is D3DRMERR_BADVALUE");

    check(IDirect3DRMDevice3_GetBufferCount(dev) == 1, "one buffer at first");
    check(IDirect3DRMDevice3_SetBufferCount(dev, 2) == S_OK && IDirect3DRMDevice3_GetBufferCount(dev) == 2, "buffer count");
    check(IDirect3DRMDevice3_SetBufferCount(dev, 0) == BADVALUE, "no buffers is D3DRMERR_BADVALUE");
    check(IDirect3DRMDevice3_SetShades(dev, 64) == S_OK && IDirect3DRMDevice3_GetShades(dev) == 64, "shades");
    check(IDirect3DRMDevice3_SetShades(dev, 0) == BADVALUE, "no shades is D3DRMERR_BADVALUE");
    check(IDirect3DRMDevice3_SetTextureQuality(dev, D3DRMTEXTURE_LINEAR) == S_OK
          && IDirect3DRMDevice3_GetTextureQuality(dev) == D3DRMTEXTURE_LINEAR, "texture quality");
    check(IDirect3DRMDevice3_SetTextureQuality(dev, 40) == BADVALUE, "an unknown texture quality is D3DRMERR_BADVALUE");
    check(IDirect3DRMDevice3_GetTrianglesDrawn(dev) == 0, "no triangles drawn");

    check(IDirect3DRMDevice3_AddUpdateCallback(dev, update_cb, NULL) == S_OK, "AddUpdateCallback");
    check(IDirect3DRMDevice3_AddUpdateCallback(dev, update_cb, NULL) == S_OK, "adding it again is accepted");
    hr = IDirect3DRMDevice3_Update(dev);
    check(hr == S_OK && calls == 1, "Update runs the callback once (%d)", calls);
    check(IDirect3DRMDevice3_DeleteUpdateCallback(dev, update_cb, NULL) == S_OK, "DeleteUpdateCallback");
    check(IDirect3DRMDevice3_DeleteUpdateCallback(dev, update_cb, NULL) == NOTFOUND, "deleting it again is D3DRMERR_NOTFOUND");
    calls = 0;
    IDirect3DRMDevice3_Update(dev);
    check(calls == 0, "no callback runs after it was deleted");

done:
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
