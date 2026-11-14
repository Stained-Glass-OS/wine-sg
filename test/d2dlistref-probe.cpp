/* test/d2dlistref-gate.sh's probe (7759): a command list one device context made, drawn into
 * through another context, outlives the program's last Release a while -- Office's Designer
 * releases such a list after closing it and then makes it an effect's input.
 * Prints name=value lines.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <d3d11.h>
#include <d2d1_3.h>
#include <d2d1effects.h>
#include <cstdio>

static const GUID clsid_color_matrix = {0x921f03d6,0x641c,0x47df,{0x85,0x2d,0xb4,0xbb,0x61,0x53,0xae,0x11}};

int main()
{
    ID3D11Device *d3d = nullptr;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr, 0, D3D11_SDK_VERSION, &d3d, nullptr, nullptr);
    if (FAILED(hr)) hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr, 0, D3D11_SDK_VERSION, &d3d, nullptr, nullptr);
    printf("D3D=0x%08lx\n", hr);
    if (FAILED(hr)) return 0;
    IDXGIDevice *dxgi = nullptr;
    d3d->QueryInterface(__uuidof(IDXGIDevice), (void **)&dxgi);
    ID2D1Factory1 *factory = nullptr;
    D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, __uuidof(ID2D1Factory1), nullptr, (void **)&factory);
    ID2D1Device *device = nullptr;
    hr = factory->CreateDevice(dxgi, &device);
    printf("Device=0x%08lx\n", hr);
    if (FAILED(hr)) return 0;
    ID2D1DeviceContext *a = nullptr, *b = nullptr;
    device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &a);
    device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &b);

    /* made by a, drawn into through b, b goes */
    ID2D1CommandList *list = nullptr;
    a->CreateCommandList(&list);
    b->SetTarget(list);
    ID2D1Image *got = nullptr;
    b->GetTarget(&got);
    printf("ForeignTargetReads=%d\n", got != nullptr);
    if (got) got->Release();
    b->BeginDraw();
    b->Clear(D2D1::ColorF(1.0f, 0.0f, 0.0f, 1.0f));
    printf("EndDraw=0x%08lx\n", b->EndDraw());
    b->Release();
    printf("Close=0x%08lx\n", list->Close());
    printf("ForeignLastRelease=%lu\n", list->Release());

    /* the program goes on using it, as Office does: an effect's input, drawn */
    D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    ID2D1Bitmap1 *target = nullptr;
    a->CreateBitmap(D2D1::SizeU(32, 32), nullptr, 0, &props, &target);
    a->SetTarget(target);
    ID2D1Effect *effect = nullptr;
    hr = a->CreateEffect(clsid_color_matrix, &effect);
    printf("Effect=0x%08lx\n", hr);
    if (SUCCEEDED(hr))
    {
        ID2D1Image *output = nullptr;
        effect->SetInput(0, list, TRUE);
        effect->GetOutput(&output);
        a->BeginDraw();
        a->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));
        a->DrawImage(output, nullptr, nullptr, D2D1_INTERPOLATION_MODE_LINEAR, D2D1_COMPOSITE_MODE_SOURCE_OVER);
        printf("DrawInput=0x%08lx\n", a->EndDraw());
        output->Release();
        effect->Release();
    }

    /* set on another context but never drawn into: freed at its last Release (Wine's test) */
    ID2D1DeviceContext *c = nullptr;
    ID2D1CommandList *list3 = nullptr;
    device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &c);
    a->CreateCommandList(&list3);
    c->SetTarget(list3);
    list3->Release();
    printf("UndrawnContextGone=%lu\n", c->Release());

    /* a context with no target draws nothing (it had bound no view and crashed in d3d11) */
    ID2D1DeviceContext *d = nullptr;
    device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &d);
    d->BeginDraw();
    d->Clear(D2D1::ColorF(0.0f, 0.0f, 1.0f, 1.0f));
    d->EndDraw();
    printf("NoTargetDraw=1\n");
    d->Release();

    /* made and drawn into by the same context: its last Release frees it */
    ID2D1CommandList *list2 = nullptr;
    a->CreateCommandList(&list2);
    a->SetTarget(list2);
    a->SetTarget(nullptr);
    list2->Close();
    printf("OwnLastRelease=%lu\n", list2->Release());

    target->Release();
    a->Release();
    device->Release();
    /* the lists the device kept went with it: nothing holds the factory any more */
    printf("FactoryLastRelease=%lu\n", factory->Release());
    printf("Done=1\n");
    return 0;
}
