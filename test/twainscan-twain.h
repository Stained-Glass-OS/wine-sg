/* The few TWAIN 1.x definitions twainscan-probe.c uses (the published TWAIN
 * interface: data groups, messages, capabilities and the structures packed on
 * 2-byte boundaries), so the probe builds without a TWAIN SDK. */
#ifndef SG_TWAINSCAN_TWAIN_H
#define SG_TWAINSCAN_TWAIN_H
#include <windows.h>

#pragma pack(push, 2)
typedef char TW_STR32[34];
typedef unsigned short TW_UINT16;
typedef short TW_INT16;
typedef LONG TW_INT32;
typedef ULONG TW_UINT32;
typedef unsigned short TW_BOOL;
typedef void *TW_MEMREF;
typedef struct { TW_INT16 Whole; TW_UINT16 Frac; } TW_FIX32;
typedef struct { TW_UINT16 MajorNum, MinorNum, Language, Country; TW_STR32 Info; } TW_VERSION;
typedef struct
{
    TW_UINT32 Id;
    TW_VERSION Version;
    TW_UINT16 ProtocolMajor, ProtocolMinor;
    TW_UINT32 SupportedGroups;
    TW_STR32 Manufacturer, ProductFamily, ProductName;
} TW_IDENTITY, *pTW_IDENTITY;
typedef struct { TW_UINT16 ItemType; TW_UINT32 Item; } TW_ONEVALUE, *pTW_ONEVALUE;
typedef struct { TW_UINT16 Cap, ConType; HANDLE hContainer; } TW_CAPABILITY;
typedef struct { TW_BOOL ShowUI, ModalUI; HANDLE hParent; } TW_USERINTERFACE;
typedef struct { TW_MEMREF pEvent; TW_UINT16 TWMessage; } TW_EVENT;
typedef struct
{
    TW_FIX32 XResolution, YResolution;
    TW_INT32 ImageWidth, ImageLength;
    TW_INT16 SamplesPerPixel, BitsPerSample[8], BitsPerPixel;
    TW_BOOL Planar;
    TW_INT16 PixelType;
    TW_UINT16 Compression;
} TW_IMAGEINFO;
typedef struct { TW_UINT16 Count; TW_UINT32 EOJ; } TW_PENDINGXFERS;
#pragma pack(pop)

typedef TW_UINT16 (WINAPI *DSMENTRYPROC)(pTW_IDENTITY, pTW_IDENTITY, TW_UINT32, TW_UINT16, TW_UINT16, TW_MEMREF);

#define TWON_PROTOCOLMINOR 8
#define TWON_PROTOCOLMAJOR 1
#define TWON_ONEVALUE      5
#define TWTY_UINT16        0x0004
#define TWTY_FIX32         0x0007
#define TWPT_RGB           2
#define DG_CONTROL         0x0001
#define DG_IMAGE           0x0002
#define DAT_CAPABILITY     0x0001
#define DAT_EVENT          0x0002
#define DAT_IDENTITY       0x0003
#define DAT_PARENT         0x0004
#define DAT_PENDINGXFERS   0x0005
#define DAT_USERINTERFACE  0x0009
#define DAT_IMAGEINFO      0x0101
#define DAT_IMAGENATIVEXFER 0x0104
#define MSG_NULL           0x0000
#define MSG_GET            0x0001
#define MSG_GETFIRST       0x0004
#define MSG_GETNEXT        0x0005
#define MSG_SET            0x0006
#define MSG_XFERREADY      0x0101
#define MSG_CLOSEDSREQ     0x0102
#define MSG_OPENDSM        0x0301
#define MSG_CLOSEDSM       0x0302
#define MSG_OPENDS         0x0401
#define MSG_CLOSEDS        0x0402
#define MSG_DISABLEDS      0x0501
#define MSG_ENABLEDS       0x0502
#define MSG_PROCESSEVENT   0x0601
#define MSG_ENDXFER        0x0701
#define TWRC_SUCCESS       0
#define TWRC_CHECKSTATUS   2
#define TWRC_DSEVENT       4
#define TWRC_XFERDONE      6
#define ICAP_PIXELTYPE     0x0101
#define ICAP_XRESOLUTION   0x1118
#define ICAP_YRESOLUTION   0x1119
#endif
