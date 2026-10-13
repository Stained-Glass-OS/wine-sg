/* gphoto2.ds: triplets the source does not support answer TWCC_BADPROTOCOL; sequence errors; audio (patch 3617) */
#include <windows.h>
#define DECLSPEC_EXPORT __declspec(dllexport)
#include <stdio.h>
typedef unsigned short TW_UINT16;
typedef unsigned int TW_UINT32;
typedef void *TW_MEMREF;
/* the source only reads the identity at MSG_OPENDS: room enough, the Id first */
typedef struct { TW_UINT32 Id; char rest[1020]; } TW_IDENTITY, *pTW_IDENTITY;
typedef struct { TW_UINT16 ConditionCode; TW_UINT16 Reserved; } TW_STATUS;
#define DG_CONTROL 0x0001L
#define DG_IMAGE 0x0002L
#define DG_AUDIO 0x0004L
#define DAT_STATUS 0x0008
#define DAT_JPEGCOMPRESSION 0x0109
#define DAT_PALETTE8 0x010a
#define DAT_GRAYRESPONSE 0x0107
#define DAT_RGBRESPONSE 0x0108
#define DAT_CIECOLOR 0x0106
#define DAT_EXTIMAGEINFO 0x010b
#define DAT_CUSTOMDSDATA 0x000c
#define DAT_FILESYSTEM 0x000e
#define DAT_PASSTHRU 0x000f
#define DAT_AUDIOFILEXFER 0x0201
#define DAT_XFERGROUP 0x000a
#define DAT_SETUPFILEXFER 0x0007
#define DAT_CAPABILITY 0x0001
#define DAT_IMAGELAYOUT 0x0102
#define DAT_IMAGEFILEXFER 0x0105
#define DAT_USERINTERFACE 0x0009
#define MSG_GET 0x0001
#define MSG_GETDEFAULT 0x0003
#define MSG_RESET 0x0007
#define MSG_SET 0x0006
#define MSG_CHANGEDIRECTORY 0x0801
#define MSG_CREATEDIRECTORY 0x0802
#define MSG_DELETE 0x0803
#define MSG_FORMATMEDIA 0x0804
#define MSG_GETCLOSE 0x0805
#define MSG_GETFIRSTFILE 0x0806
#define MSG_GETINFO 0x0807
#define MSG_GETNEXTFILE 0x0808
#define MSG_RENAME 0x0809
#define MSG_PASSTHRU 0x0901
#define MSG_QUERYSUPPORT 0x0008
#define MSG_ENABLEDSUIONLY 0x0503
#define TWRC_FAILURE 1
#define TWCC_BADPROTOCOL 9
#define TWCC_SEQERROR 11

static int f;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); f++; } } while (0)
typedef TW_UINT16 (WINAPI *entry_t)(pTW_IDENTITY, TW_UINT32, TW_UINT16, TW_UINT16, TW_MEMREF);
static entry_t entry;
static TW_IDENTITY app;

static TW_UINT16 cc(void)
{
    TW_STATUS st = {0};
    entry(&app, DG_CONTROL, DAT_STATUS, MSG_GET, &st);
    return st.ConditionCode;
}

static void unsupported(TW_UINT32 dg, TW_UINT16 dat, TW_UINT16 msg, const char *name)
{
    char buf[512] = {0};
    TW_UINT16 rc = entry(&app, dg, dat, msg, buf);
    if (rc != TWRC_FAILURE || cc() != TWCC_BADPROTOCOL) { printf("FAIL %s rc=%d\n", name, rc); f++; }
}

static void seqerror(TW_UINT32 dg, TW_UINT16 dat, TW_UINT16 msg, const char *name)
{
    char buf[512] = {0};
    TW_UINT16 rc = entry(&app, dg, dat, msg, buf);
    if (rc != TWRC_FAILURE || cc() != TWCC_SEQERROR) { printf("FAIL %s rc=%d\n", name, rc); f++; }
}

int main(void)
{
    HMODULE dsm = LoadLibraryA("twain_32.dll"), ds = LoadLibraryA("gphoto2.ds");
    setvbuf(stdout, NULL, _IONBF, 0);
    CHECK(dsm && ds);
    entry = ds ? (entry_t)GetProcAddress(ds, "DS_Entry") : NULL;
    CHECK(entry);
    if (!entry) { puts("RESULT: FAIL"); return 1; }
    app.Id = 1;

#define U(dg, dat, msg) unsupported(dg, dat, msg, #dat "/" #msg)
    U(DG_IMAGE, DAT_JPEGCOMPRESSION, MSG_GET);
    U(DG_IMAGE, DAT_JPEGCOMPRESSION, MSG_GETDEFAULT);
    U(DG_IMAGE, DAT_JPEGCOMPRESSION, MSG_RESET);
    U(DG_IMAGE, DAT_JPEGCOMPRESSION, MSG_SET);
    U(DG_IMAGE, DAT_PALETTE8, MSG_GET);
    U(DG_IMAGE, DAT_PALETTE8, MSG_SET);
    U(DG_IMAGE, DAT_PALETTE8, MSG_RESET);
    U(DG_IMAGE, DAT_PALETTE8, MSG_GETDEFAULT);
    U(DG_IMAGE, DAT_GRAYRESPONSE, MSG_SET);
    U(DG_IMAGE, DAT_GRAYRESPONSE, MSG_RESET);
    U(DG_IMAGE, DAT_RGBRESPONSE, MSG_SET);
    U(DG_IMAGE, DAT_RGBRESPONSE, MSG_RESET);
    U(DG_IMAGE, DAT_CIECOLOR, MSG_GET);
    U(DG_IMAGE, DAT_EXTIMAGEINFO, MSG_GET);
    U(DG_CONTROL, DAT_CUSTOMDSDATA, MSG_GET);
    U(DG_CONTROL, DAT_CUSTOMDSDATA, MSG_SET);
    U(DG_CONTROL, DAT_FILESYSTEM, MSG_CHANGEDIRECTORY);
    U(DG_CONTROL, DAT_FILESYSTEM, MSG_CREATEDIRECTORY);
    U(DG_CONTROL, DAT_FILESYSTEM, MSG_DELETE);
    U(DG_CONTROL, DAT_FILESYSTEM, MSG_FORMATMEDIA);
    U(DG_CONTROL, DAT_FILESYSTEM, MSG_GETCLOSE);
    U(DG_CONTROL, DAT_FILESYSTEM, MSG_GETFIRSTFILE);
    U(DG_CONTROL, DAT_FILESYSTEM, MSG_GETINFO);
    U(DG_CONTROL, DAT_FILESYSTEM, MSG_GETNEXTFILE);
    U(DG_CONTROL, DAT_FILESYSTEM, MSG_RENAME);
    U(DG_CONTROL, DAT_PASSTHRU, MSG_PASSTHRU);
    U(DG_AUDIO, DAT_AUDIOFILEXFER, MSG_GET);
    /* nothing is open: the triplets that need a state say so */
    seqerror(DG_CONTROL, DAT_XFERGROUP, MSG_GET, "XFERGROUP/GET");
    seqerror(DG_CONTROL, DAT_XFERGROUP, MSG_SET, "XFERGROUP/SET");
    seqerror(DG_CONTROL, DAT_SETUPFILEXFER, MSG_GET, "SETUPFILEXFER/GET");
    seqerror(DG_CONTROL, DAT_SETUPFILEXFER, MSG_SET, "SETUPFILEXFER/SET");
    seqerror(DG_CONTROL, DAT_SETUPFILEXFER, MSG_RESET, "SETUPFILEXFER/RESET");
    seqerror(DG_CONTROL, DAT_CAPABILITY, MSG_QUERYSUPPORT, "CAPABILITY/QUERYSUPPORT");
    seqerror(DG_IMAGE, DAT_IMAGELAYOUT, MSG_GET, "IMAGELAYOUT/GET");
    seqerror(DG_IMAGE, DAT_IMAGELAYOUT, MSG_SET, "IMAGELAYOUT/SET");
    seqerror(DG_IMAGE, DAT_IMAGEFILEXFER, MSG_GET, "IMAGEFILEXFER/GET");
    seqerror(DG_CONTROL, DAT_USERINTERFACE, MSG_ENABLEDSUIONLY, "ENABLEDSUIONLY");
    puts(f ? "RESULT: FAIL" : "RESULT: PASS");
    return f;
}
