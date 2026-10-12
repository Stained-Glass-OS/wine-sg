/* d3dx9_36, fast mode (patch 3501): D3DXSHMultiply5/6 (against D3DXSHMultiply4
 * and the identity), the frame helpers, and the new resource loaders'
 * argument checks. */
#include <windows.h>
#include <d3dx9.h>
#include <stdio.h>
#include <math.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}

typedef float *(WINAPI *shmul_t)(float *, const float *, const float *);

static void test_sh(void)
{
    HMODULE mod = LoadLibraryA("d3dx9_36.dll");
    shmul_t D3DXSHMultiply5 = (shmul_t)GetProcAddress(mod, "D3DXSHMultiply5");
    shmul_t D3DXSHMultiply6 = (shmul_t)GetProcAddress(mod, "D3DXSHMultiply6");

    float a[36], b[36], o4[16], o5[25], o6[36], c[36];
    int i, ok;

    for (i = 0; i < 36; i++)
    {
        a[i] = i < 16 ? 0.1f * (i + 1) - 0.7f : 0.0f;
        b[i] = i < 16 ? 0.9f - 0.07f * i : 0.0f;
    }
    check(D3DXSHMultiply5 && D3DXSHMultiply6, "SHMultiply5 and 6 are exported");
    if (!D3DXSHMultiply5 || !D3DXSHMultiply6) return;
    D3DXSHMultiply4(o4, a, b);
    D3DXSHMultiply5(o5, a, b);
    D3DXSHMultiply6(o6, a, b);
    ok = 1;
    for (i = 0; i < 16; i++) if (fabsf(o5[i] - o4[i]) > 1e-4f) ok = 0;
    check(ok, "SHMultiply5 matches SHMultiply4 on order-4 inputs");
    ok = 1;
    for (i = 0; i < 16; i++) if (fabsf(o6[i] - o4[i]) > 1e-4f) ok = 0;
    check(ok, "SHMultiply6 matches SHMultiply4 on order-4 inputs");

    /* the constant function 1 is 1/0.28209479 in the first coefficient: multiplying leaves b alone */
    memset(c, 0, sizeof(c));
    c[0] = 1.0f / 0.28209479f;
    for (i = 0; i < 36; i++) b[i] = 0.01f * (i + 1);
    D3DXSHMultiply6(o6, c, b);
    ok = 1;
    for (i = 0; i < 36; i++) if (fabsf(o6[i] - b[i]) > 1e-4f) ok = 0;
    check(ok, "multiplying by the constant 1 keeps the function (order 6)");
    D3DXSHMultiply5(o5, c, b);
    ok = 1;
    for (i = 0; i < 25; i++) if (fabsf(o5[i] - b[i]) > 1e-4f) ok = 0;
    check(ok, "multiplying by the constant 1 keeps the function (order 5)");
}

static void test_frames(void)
{
    D3DXFRAME root, a, b, c, d;
    D3DXVECTOR3 center;
    float radius;
    HRESULT hr;

    memset(&root, 0, sizeof(root)); memset(&a, 0, sizeof(a)); memset(&b, 0, sizeof(b));
    memset(&c, 0, sizeof(c)); memset(&d, 0, sizeof(d));
    root.Name = (char *)"root"; a.Name = (char *)"a"; c.Name = (char *)"c";

    hr = D3DXFrameAppendChild(&root, &a);
    check(hr == D3D_OK && root.pFrameFirstChild == &a, "first child");
    hr = D3DXFrameAppendChild(&root, &b);
    check(hr == D3D_OK && a.pFrameSibling == &b, "second child is a sibling of the first");
    hr = D3DXFrameAppendChild(&a, &c);
    check(hr == D3D_OK && a.pFrameFirstChild == &c, "grandchild");
    hr = D3DXFrameAppendChild(&root, &d);
    check(hr == D3D_OK && b.pFrameSibling == &d, "third child at the end");
    hr = D3DXFrameAppendChild(NULL, &a);
    check(hr == D3DERR_INVALIDCALL, "null parent");
    check(D3DXFrameNumNamedMatrices(&root) == 3, "named matrices: root, a, c");
    check(D3DXFrameNumNamedMatrices(NULL) == 0, "no frame, no matrices");

    hr = D3DXFrameCalculateBoundingSphere(&root, &center, &radius);
    check(hr == D3D_OK && radius == 0.0f, "empty hierarchy has an empty sphere");
    hr = D3DXFrameCalculateBoundingSphere(NULL, &center, &radius);
    check(hr == D3DERR_INVALIDCALL, "sphere argument check");
}

static void test_resources(void)
{
    IDirect3DCubeTexture9 *cube = NULL;
    IDirect3DVolumeTexture9 *vol = NULL;
    HRESULT hr;

    hr = D3DXCreateCubeTextureFromResourceA(NULL, NULL, "none", &cube);
    check(hr == D3DERR_INVALIDCALL, "cube from resource, no device");
    hr = D3DXCreateVolumeTextureFromResourceW(NULL, NULL, L"none", &vol);
    check(hr == D3DERR_INVALIDCALL, "volume from resource, no device");
    hr = D3DXLoadVolumeFromResourceA(NULL, NULL, NULL, NULL, "none", NULL, D3DX_DEFAULT, 0, NULL);
    check(hr == D3DERR_INVALIDCALL, "load volume from resource, no volume");
}

int main(void)
{
    test_sh();
    test_frames();
    test_resources();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
