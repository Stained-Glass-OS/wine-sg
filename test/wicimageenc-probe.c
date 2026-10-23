/* wicimageenc-probe: IWICImagingFactory2::CreateImageEncoder (Stained Glass OS wine-sg 6043): a
 * Direct2D bitmap written as a PNG frame, whole and by parameters (an area of it), read back by WIC;
 * a frame thumbnail and an encoder thumbnail go to the frame/encoder. */
#define COBJMACROS
#include <windows.h>
#include <d3d11.h>
#include <d2d1_1.h>
#include <wincodec.h>
#include <stdio.h>

static const GUID my_iid_dxgidevice = {0x54ec77fa, 0x1377, 0x44e6, {0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c}};
static const GUID my_clsid_wicfactory2 = {0x317d06e8, 0x5f24, 0x433d, {0xbd, 0xf7, 0x79, 0xce, 0x68, 0xd8, 0xab, 0xc2}};
static const GUID my_iid_wicfactory2 = {0x7b816b45, 0x1996, 0x4476, {0xb1, 0x32, 0xde, 0x9e, 0x24, 0x7c, 0x8a, 0xf0}};
static const GUID my_container_png = {0x1b7cfaf4, 0x713f, 0x473c, {0xbb, 0xcd, 0x61, 0x37, 0x42, 0x5f, 0xae, 0xaf}};
static const GUID my_pf_bgra = {0x6fddc324, 0x4e03, 0x4bfe, {0xb1, 0x85, 0x3d, 0x77, 0x76, 0x8d, 0xc9, 0x0f}};

static int failures;

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %ld, %#lx)\n", what, got, got ); failures++; }
}

/* the PNG in the stream decoded: its size and the BGRA pixel at (x, y) */
static DWORD decode_pixel( IWICImagingFactory2 *wic, IStream *stream, UINT x, UINT y, UINT *width, UINT *height )
{
    IWICBitmapDecoder *decoder = NULL;
    IWICBitmapFrameDecode *frame = NULL;
    IWICFormatConverter *conv = NULL;
    LARGE_INTEGER zero = {{0}};
    WICRect rect = { x, y, 1, 1 };
    DWORD pixel = 0xdeadbeef;

    *width = *height = 0;
    IStream_Seek( stream, zero, STREAM_SEEK_SET, NULL );
    if (FAILED(IWICImagingFactory2_CreateDecoderFromStream( wic, stream, NULL, WICDecodeMetadataCacheOnDemand, &decoder )))
        return pixel;
    IWICBitmapDecoder_GetFrame( decoder, 0, &frame );
    if (frame)
    {
        IWICBitmapFrameDecode_GetSize( frame, width, height );
        IWICImagingFactory2_CreateFormatConverter( wic, &conv );
        IWICFormatConverter_Initialize( conv, (IWICBitmapSource *)frame, &my_pf_bgra, WICBitmapDitherTypeNone, NULL,
                                        0.0, WICBitmapPaletteTypeCustom );
        IWICFormatConverter_CopyPixels( conv, &rect, 4, 4, (BYTE *)&pixel );
        IWICFormatConverter_Release( conv );
        IWICBitmapFrameDecode_Release( frame );
    }
    IWICBitmapDecoder_Release( decoder );
    return pixel;
}

static HRESULT write_png( IWICImagingFactory2 *wic, IWICImageEncoder *ie, ID2D1Image *image,
                          const WICImageParameters *params, IStream **out, HRESULT *thumb_hr )
{
    IWICBitmapEncoder *encoder = NULL;
    IWICBitmapFrameEncode *frame = NULL;
    HRESULT hr;

    CreateStreamOnHGlobal( NULL, TRUE, out );
    IWICImagingFactory2_CreateEncoder( wic, &my_container_png, NULL, &encoder );
    IWICBitmapEncoder_Initialize( encoder, *out, WICBitmapEncoderNoCache );
    IWICBitmapEncoder_CreateNewFrame( encoder, &frame, NULL );
    IWICBitmapFrameEncode_Initialize( frame, NULL );
    hr = IWICImageEncoder_WriteFrame( ie, image, frame, params );
    if (thumb_hr) *thumb_hr = IWICImageEncoder_WriteThumbnail( ie, image, encoder, params );
    IWICBitmapFrameEncode_Commit( frame );
    IWICBitmapEncoder_Commit( encoder );
    IWICBitmapFrameEncode_Release( frame );
    IWICBitmapEncoder_Release( encoder );
    return hr;
}

int main( void )
{
    static const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    D2D1_BITMAP_PROPERTIES1 props = { { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }, 96.0f, 96.0f };
    D2D1_CREATION_PROPERTIES cp = { D2D1_THREADING_MODE_SINGLE_THREADED };
    WICImageParameters params = { { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED }, 96.0f, 96.0f,
                                  8.0f, 16.0f, 16, 8 };
    D2D1_SIZE_U size = { 32, 16 };
    ID3D11Device *d3d = NULL;
    IDXGIDevice *dxgi = NULL;
    ID2D1Device *device = NULL;
    ID2D1DeviceContext *dc = NULL;
    ID2D1Bitmap1 *bitmap = NULL;
    IWICImagingFactory2 *wic = NULL;
    IWICImageEncoder *ie = NULL;
    IStream *stream;
    DWORD pixels[32 * 16], pixel;
    UINT width, height, x, y;
    HRESULT hr, thumb_hr;

    CoInitializeEx( NULL, COINIT_MULTITHREADED );
    hr = D3D11CreateDevice( NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 3,
                            D3D11_SDK_VERSION, &d3d, NULL, NULL );
    if (FAILED(hr))
        hr = D3D11CreateDevice( NULL, D3D_DRIVER_TYPE_WARP, NULL, D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 3,
                                D3D11_SDK_VERSION, &d3d, NULL, NULL );
    check( SUCCEEDED(hr), "D3D11CreateDevice", hr );
    if (FAILED(hr)) goto done;
    ID3D11Device_QueryInterface( d3d, &my_iid_dxgidevice, (void **)&dxgi );
    hr = D2D1CreateDevice( dxgi, &cp, &device );
    check( hr == S_OK, "D2D1CreateDevice", hr );
    if (FAILED(hr)) goto done;
    ID2D1Device_CreateDeviceContext( device, D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &dc );

    /* a bitmap of four quadrants: left half red, right half blue; the lower half green on the right */
    for (y = 0; y < 16; y++)
        for (x = 0; x < 32; x++)
            pixels[y * 32 + x] = x < 16 ? 0xffff0000 : y < 8 ? 0xff0000ff : 0xff00ff00;
    hr = ID2D1DeviceContext_CreateBitmap( dc, size, pixels, 32 * 4, &props, &bitmap );
    check( hr == S_OK, "a 32x16 Direct2D bitmap", hr );
    if (FAILED(hr)) goto done;

    hr = CoCreateInstance( &my_clsid_wicfactory2, NULL, CLSCTX_INPROC_SERVER, &my_iid_wicfactory2, (void **)&wic );
    check( hr == S_OK, "WIC imaging factory 2", hr );
    if (FAILED(hr)) goto done;
    hr = IWICImagingFactory2_CreateImageEncoder( wic, device, &ie );
    check( hr == S_OK && ie, "CreateImageEncoder", hr );
    if (!ie) goto done;
    hr = IWICImagingFactory2_CreateImageEncoder( wic, NULL, (IWICImageEncoder **)&stream );
    check( hr == E_INVALIDARG, "CreateImageEncoder without a device: E_INVALIDARG", hr );

    hr = write_png( wic, ie, (ID2D1Image *)bitmap, NULL, &stream, &thumb_hr );
    check( hr == S_OK, "WriteFrame of the whole bitmap", hr );
    pixel = decode_pixel( wic, stream, 2, 2, &width, &height );
    check( width == 32 && height == 16, "the PNG has the bitmap's size", width * 1000 + height );
    check( pixel == 0xffff0000, "its left half is red", pixel );
    pixel = decode_pixel( wic, stream, 30, 2, &width, &height );
    check( pixel == 0xff0000ff, "its upper right quarter blue", pixel );
    pixel = decode_pixel( wic, stream, 30, 14, &width, &height );
    check( pixel == 0xff00ff00, "its lower right quarter green", pixel );
    check( thumb_hr == WINCODEC_ERR_UNSUPPORTEDOPERATION, "WriteThumbnail: PNG has no thumbnail (the encoder's answer)", thumb_hr );
    IStream_Release( stream );

    /* by parameters: the 16x8 area at (16, 8) -- the green quarter */
    hr = write_png( wic, ie, (ID2D1Image *)bitmap, &params, &stream, NULL );
    check( hr == S_OK, "WriteFrame with parameters (an area)", hr );
    pixel = decode_pixel( wic, stream, 4, 4, &width, &height );
    check( width == 16 && height == 8, "the PNG has the parameters' size", width * 1000 + height );
    check( pixel == 0xff00ff00, "and the area's pixels", pixel );
    IStream_Release( stream );

    hr = IWICImageEncoder_WriteFrame( ie, NULL, NULL, NULL );
    check( hr == E_INVALIDARG, "WriteFrame of nothing: E_INVALIDARG", hr );

done:
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
