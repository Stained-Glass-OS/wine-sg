/* wicfast-probe: WIC's fast (in-place) metadata encoder (Stained Glass OS
 * wine-sg 6014): created for JPEG and TIFF, refused for PNG; its query writer;
 * Commit reports that the file keeps no padding for in-place metadata. */
#define COBJMACROS
#include <windows.h>
#include <wincodec.h>
#include <stdio.h>

#ifndef WINCODEC_ERR_TOOMUCHMETADATA
#define WINCODEC_ERR_TOOMUCHMETADATA ((HRESULT)0x88982f52)
#endif
#ifndef WINCODEC_ERR_UNSUPPORTEDOPERATION
#define WINCODEC_ERR_UNSUPPORTEDOPERATION ((HRESULT)0x88982f81)
#endif
#ifndef WINCODEC_ERR_WRONGSTATE
#define WINCODEC_ERR_WRONGSTATE ((HRESULT)0x88982f04)
#endif

static int failures;

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %#lx)\n", what, got ); failures++; }
}

static IWICImagingFactory *factory;

/* a 4x4 image in the given container, decoded again */
static IWICBitmapDecoder *make_image( const GUID *container )
{
    IWICBitmapEncoder *enc;
    IWICBitmapFrameEncode *frame;
    IWICBitmapDecoder *dec = NULL;
    IStream *stream;
    BYTE pixels[4 * 4 * 3] = {0};
    WICPixelFormatGUID fmt = GUID_WICPixelFormat24bppBGR;
    LARGE_INTEGER zero = {{0}};

    CreateStreamOnHGlobal( NULL, TRUE, &stream );
    if (FAILED(IWICImagingFactory_CreateEncoder( factory, container, NULL, &enc ))) return NULL;
    IWICBitmapEncoder_Initialize( enc, stream, WICBitmapEncoderNoCache );
    IWICBitmapEncoder_CreateNewFrame( enc, &frame, NULL );
    IWICBitmapFrameEncode_Initialize( frame, NULL );
    IWICBitmapFrameEncode_SetSize( frame, 4, 4 );
    IWICBitmapFrameEncode_SetPixelFormat( frame, &fmt );
    IWICBitmapFrameEncode_WritePixels( frame, 4, 12, sizeof(pixels), pixels );
    IWICBitmapFrameEncode_Commit( frame );
    IWICBitmapEncoder_Commit( enc );
    IWICBitmapFrameEncode_Release( frame );
    IWICBitmapEncoder_Release( enc );
    IStream_Seek( stream, zero, STREAM_SEEK_SET, NULL );
    IWICImagingFactory_CreateDecoderFromStream( factory, stream, NULL, WICDecodeMetadataCacheOnDemand, &dec );
    IStream_Release( stream );
    return dec;
}

int main( void )
{
    IWICFastMetadataEncoder *fast = NULL;
    IWICMetadataQueryWriter *writer = NULL;
    IWICBitmapDecoder *jpeg, *png, *tiff;
    IWICBitmapFrameDecode *frame;
    HRESULT hr;

    CoInitialize( NULL );
    CoCreateInstance( &CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, (void **)&factory );
    jpeg = make_image( &GUID_ContainerFormatJpeg );
    png = make_image( &GUID_ContainerFormatPng );
    tiff = make_image( &GUID_ContainerFormatTiff );
    check( jpeg && png && tiff, "JPEG, PNG and TIFF images", 0 );
    if (!jpeg || !png || !tiff) { printf( "RESULT: FAIL\n" ); return 1; }

    hr = IWICImagingFactory_CreateFastMetadataEncoderFromDecoder( factory, jpeg, &fast );
    check( hr == S_OK && fast, "CreateFastMetadataEncoderFromDecoder (JPEG)", hr );
    if (fast)
    {
        hr = IWICFastMetadataEncoder_GetMetadataQueryWriter( fast, &writer );
        check( hr == S_OK && writer, "its query writer", hr );
        if (writer) IWICMetadataQueryWriter_Release( writer );
        hr = IWICFastMetadataEncoder_Commit( fast );
        check( hr == WINCODEC_ERR_TOOMUCHMETADATA, "Commit: no padding in the file (WINCODEC_ERR_TOOMUCHMETADATA)", hr );
        hr = IWICFastMetadataEncoder_Commit( fast );
        check( hr == WINCODEC_ERR_WRONGSTATE, "a second Commit: WINCODEC_ERR_WRONGSTATE", hr );
        IWICFastMetadataEncoder_Release( fast );
    }
    fast = NULL;
    hr = IWICImagingFactory_CreateFastMetadataEncoderFromDecoder( factory, tiff, &fast );
    check( hr == S_OK && fast, "CreateFastMetadataEncoderFromDecoder (TIFF)", hr );
    if (fast) IWICFastMetadataEncoder_Release( fast );
    fast = (void *)1;
    hr = IWICImagingFactory_CreateFastMetadataEncoderFromDecoder( factory, png, &fast );
    check( hr == WINCODEC_ERR_UNSUPPORTEDOPERATION && !fast, "PNG has no in-place metadata: WINCODEC_ERR_UNSUPPORTEDOPERATION", hr );

    IWICBitmapDecoder_GetFrame( jpeg, 0, &frame );
    fast = NULL;
    hr = IWICImagingFactory_CreateFastMetadataEncoderFromFrameDecode( factory, frame, &fast );
    check( hr == S_OK && fast, "CreateFastMetadataEncoderFromFrameDecode (JPEG frame)", hr );
    if (fast)
    {
        hr = IWICFastMetadataEncoder_GetMetadataQueryWriter( fast, &writer );
        check( hr == S_OK && writer, "the frame's query writer", hr );
        if (writer) IWICMetadataQueryWriter_Release( writer );
        IWICFastMetadataEncoder_Release( fast );
    }
    IWICBitmapFrameDecode_Release( frame );
    check( IWICImagingFactory_CreateFastMetadataEncoderFromDecoder( factory, NULL, &fast ) == E_INVALIDARG,
           "no decoder: E_INVALIDARG", 0 );

    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
