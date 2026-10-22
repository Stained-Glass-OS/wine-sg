/* gdipprop-probe: GDI+ GdipSetPropertyItem / GdipRemovePropertyItem, and the properties saved in
 * a JPEG, which are read back as one list with the EXIF and GPS IFDs' tags (Stained Glass OS wine-sg 6019).   gdipprop-probe.exe DIR */
#include <windows.h>
#include <stdio.h>

typedef int GpStatus;
typedef void GpImage;
typedef struct { ULONG_PTR GdiplusVersion; void *DebugEventCallback; BOOL SuppressBackgroundThread, SuppressExternalCodecs; } GdiplusStartupInput;
typedef struct { ULONG id; ULONG length; WORD type; void *value; } PropertyItem;
typedef struct
{
    CLSID Clsid; GUID FormatID; const WCHAR *CodecName, *DllName, *FormatDescription, *FilenameExtension, *MimeType;
    DWORD Flags, Version, SigCount, SigSize; const BYTE *SigPattern, *SigMask;
} ImageCodecInfo;

GpStatus WINAPI GdiplusStartup(ULONG_PTR *, const GdiplusStartupInput *, void *);
GpStatus WINAPI GdipCreateBitmapFromScan0(INT, INT, INT, INT, BYTE *, GpImage **);
GpStatus WINAPI GdipSetPropertyItem(GpImage *, const PropertyItem *);
GpStatus WINAPI GdipRemovePropertyItem(GpImage *, ULONG);
GpStatus WINAPI GdipGetPropertyCount(GpImage *, UINT *);
GpStatus WINAPI GdipGetPropertyItemSize(GpImage *, ULONG, UINT *);
GpStatus WINAPI GdipGetPropertyItem(GpImage *, ULONG, UINT, PropertyItem *);
GpStatus WINAPI GdipGetImageEncodersSize(UINT *, UINT *);
GpStatus WINAPI GdipGetImageEncoders(UINT, UINT, ImageCodecInfo *);
GpStatus WINAPI GdipSaveImageToFile(GpImage *, const WCHAR *, const CLSID *, void *);
GpStatus WINAPI GdipLoadImageFromFile(const WCHAR *, GpImage **);
GpStatus WINAPI GdipDisposeImage(GpImage *);

#define PixelFormat24bppRGB 0x21808
#define PropertyNotFound 19
#define TYPE_ASCII 2
#define TYPE_SHORT 3
#define TYPE_RATIONAL 5

static int failures;

static void check( int ok, const char *what, long got )
{
    if (ok) printf( "ok    %s\n", what );
    else { printf( "FAIL  %s (got %#lx)\n", what, got ); failures++; }
}

static GpStatus set_ascii( GpImage *img, ULONG id, const char *text )
{
    PropertyItem item = { id, (ULONG)strlen( text ) + 1, TYPE_ASCII, (void *)text };
    return GdipSetPropertyItem( img, &item );
}

static GpStatus set_short( GpImage *img, ULONG id, USHORT v )
{
    PropertyItem item = { id, 2, TYPE_SHORT, &v };
    return GdipSetPropertyItem( img, &item );
}

/* the property's value, or NULL */
static PropertyItem *get( GpImage *img, ULONG id )
{
    UINT size = 0;
    PropertyItem *item;

    if (GdipGetPropertyItemSize( img, id, &size ) || !size) return NULL;
    item = calloc( 1, size );
    if (GdipGetPropertyItem( img, id, size, item )) { free( item ); return NULL; }
    return item;
}

static BOOL has_ascii( GpImage *img, ULONG id, const char *text )
{
    PropertyItem *item = get( img, id );
    BOOL ret = item && item->type == TYPE_ASCII && !strcmp( item->value, text );
    free( item );
    return ret;
}

static BOOL has_short( GpImage *img, ULONG id, USHORT v )
{
    PropertyItem *item = get( img, id );
    BOOL ret = item && item->type == TYPE_SHORT && item->length >= 2 && *(USHORT *)item->value == v;
    free( item );
    return ret;
}

static BOOL encoder( const WCHAR *mime, CLSID *clsid )
{
    UINT n = 0, size = 0, i;
    ImageCodecInfo *codecs;
    BOOL found = FALSE;

    GdipGetImageEncodersSize( &n, &size );
    codecs = malloc( size );
    GdipGetImageEncoders( n, size, codecs );
    for (i = 0; i < n; i++)
        if (!lstrcmpW( codecs[i].MimeType, mime )) { *clsid = codecs[i].Clsid; found = TRUE; }
    free( codecs );
    return found;
}

static void round_trip( const WCHAR *dir, const WCHAR *mime, const WCHAR *name, GpImage *img )
{
    WCHAR path[MAX_PATH];
    CLSID clsid;
    GpImage *loaded = NULL;
    GpStatus st;
    char what[128];

    if (!encoder( mime, &clsid )) { check( 0, "an encoder", 0 ); return; }
    swprintf( path, MAX_PATH, L"%ls\\%ls", dir, name );
    st = GdipSaveImageToFile( img, path, &clsid, NULL );
    snprintf( what, sizeof(what), "saved as %ls", name );
    check( st == 0, what, st );
    st = GdipLoadImageFromFile( path, &loaded );
    if (st) { check( 0, "loaded again", st ); return; }
    snprintf( what, sizeof(what), "%ls: ImageDescription kept", name );
    check( has_ascii( loaded, 0x010e, "SG gate image" ), what, 0 );
    snprintf( what, sizeof(what), "%ls: Artist (the new value) kept", name );
    check( has_ascii( loaded, 0x013b, "Stained Glass" ), what, 0 );
    snprintf( what, sizeof(what), "%ls: DateTimeOriginal (EXIF IFD) kept", name );
    check( has_ascii( loaded, 0x9003, "2026:10:22 10:00:00" ), what, 0 );
    snprintf( what, sizeof(what), "%ls: GpsLatitudeRef (GPS IFD) kept", name );
    check( has_ascii( loaded, 0x0001, "N" ), what, 0 );
    snprintf( what, sizeof(what), "%ls: the removed Copyright is not there", name );
    check( !get( loaded, 0x8298 ), what, 0 );
    GdipDisposeImage( loaded );
}

int wmain( int argc, WCHAR **argv )
{
    GdiplusStartupInput in = { 1 };
    ULONG_PTR token;
    GpImage *img = NULL;
    UINT count = 99;
    GpStatus st;

    if (argc < 2) return 2;
    GdiplusStartup( &token, &in, NULL );
    GdipCreateBitmapFromScan0( 32, 32, 0, PixelFormat24bppRGB, NULL, &img );
    if (!img) { printf( "FAIL  no bitmap\nRESULT: FAIL\n" ); return 1; }

    GdipGetPropertyCount( img, &count );
    check( count == 0, "a new bitmap has no properties", count );
    check( set_ascii( img, 0x010e, "SG gate image" ) == 0, "SetPropertyItem ImageDescription", 0 );
    check( set_ascii( img, 0x013b, "Somebody" ) == 0, "SetPropertyItem Artist", 0 );
    check( set_ascii( img, 0x8298, "(c) nobody" ) == 0, "SetPropertyItem Copyright", 0 );
    check( set_ascii( img, 0x9003, "2026:10:22 10:00:00" ) == 0, "SetPropertyItem DateTimeOriginal", 0 );
    check( set_ascii( img, 0x0001, "N" ) == 0, "SetPropertyItem GpsLatitudeRef", 0 );
    check( set_short( img, 0x0112, 6 ) == 0, "SetPropertyItem Orientation", 0 );
    GdipGetPropertyCount( img, &count );
    check( count == 6, "six properties", count );
    check( has_ascii( img, 0x010e, "SG gate image" ) && has_short( img, 0x0112, 6 ), "they read back", 0 );
    check( set_ascii( img, 0x013b, "Stained Glass" ) == 0, "SetPropertyItem Artist again", 0 );
    GdipGetPropertyCount( img, &count );
    check( count == 6 && has_ascii( img, 0x013b, "Stained Glass" ), "the new value replaces the old", count );
    check( GdipRemovePropertyItem( img, 0x8298 ) == 0, "RemovePropertyItem Copyright", 0 );
    GdipGetPropertyCount( img, &count );
    check( count == 5 && !get( img, 0x8298 ) && has_ascii( img, 0x010e, "SG gate image" ),
           "it is gone, the others stay", count );
    st = GdipRemovePropertyItem( img, 0x8298 );
    check( st == PropertyNotFound, "removing it again: PropertyNotFound", st );

    round_trip( argv[1], L"image/jpeg", L"gdipprop.jpg", img );

    GdipDisposeImage( img );
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
