/* hidpdata-probe: HidP_SetData, HidP_UsageListDifference and HidP_GetExtendedAttributes of hid.dll (Stained Glass
 * OS wine-sg 8239; test/hidpdata-gate.sh), on preparsed data made from a report descriptor by hidparse.sys's
 * HidP_GetCollectionDescription. They were spec stubs.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <winternl.h>
#include <hidsdi.h>
#include <hidpi.h>
#include <stdio.h>

typedef struct { USAGE UsagePage, Usage; UCHAR CollectionNumber, Reserved[15]; USHORT InputLength, OutputLength,
                 FeatureLength, PreparsedDataLength; PHIDP_PREPARSED_DATA PreparsedData; } COLDESC;
typedef struct { COLDESC *CollectionDesc; ULONG CollectionDescLength; void *ReportIDs; ULONG ReportIDsLength;
                 ULONG Dbg[8]; } DEVDESC;
typedef struct { UCHAR NumGlobalUnknowns, Reserved[3]; void *GlobalUnknowns; ULONG Data[1]; } EXTATTR;

static int failures;
static void check( int ok, const char *what, long got )
{
    printf( "%s  %s (%#lx)\n", ok ? "PASS" : "FAIL", what, got );
    if (!ok) failures++;
}

/* a gamepad-like input report: 8 buttons (page 9, usages 1-8) and X, Y (page 1 usages 0x30, 0x31) of 8 bits */
static const BYTE descriptor[] =
{
    0x05, 0x01, 0x09, 0x05, 0xa1, 0x01,             /* Usage Page (Generic Desktop), Usage (Game Pad), Collection (Application) */
    0x05, 0x09, 0x19, 0x01, 0x29, 0x08,             /* Usage Page (Button), Usage Minimum 1, Usage Maximum 8 */
    0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x81, 0x02,   /* Logical 0-1, size 1, count 8, Input (Var) */
    0x05, 0x01, 0x09, 0x30, 0x09, 0x31,             /* Usage Page (Generic Desktop), X, Y */
    0x15, 0x00, 0x26, 0xff, 0x00, 0x75, 0x08, 0x95, 0x02, 0x81, 0x02,   /* Logical 0-255, size 8, count 2, Input (Var) */
    0xc0
};

int main( void )
{
    HMODULE hid = LoadLibraryA( "hid.dll" ), parse = LoadLibraryA( "C:\\windows\\system32\\drivers\\hidparse.sys" );
    NTSTATUS (WINAPI *pGetColDesc)(const BYTE *, ULONG, int, DEVDESC *) = parse ? (void *)GetProcAddress( parse, "HidP_GetCollectionDescription" ) : NULL;
    NTSTATUS (WINAPI *pSetData)(HIDP_REPORT_TYPE, HIDP_DATA *, ULONG *, PHIDP_PREPARSED_DATA, char *, ULONG) = (void *)GetProcAddress( hid, "HidP_SetData" );
    NTSTATUS (WINAPI *pGetData)(HIDP_REPORT_TYPE, HIDP_DATA *, ULONG *, PHIDP_PREPARSED_DATA, char *, ULONG) = (void *)GetProcAddress( hid, "HidP_GetData" );
    NTSTATUS (WINAPI *pDiff)(USAGE *, USAGE *, USAGE *, USAGE *, ULONG) = (void *)GetProcAddress( hid, "HidP_UsageListDifference" );
    NTSTATUS (WINAPI *pExt)(HIDP_REPORT_TYPE, USHORT, PHIDP_PREPARSED_DATA, EXTATTR *, ULONG *) = (void *)GetProcAddress( hid, "HidP_GetExtendedAttributes" );
    USAGE prev[5] = { 1, 2, 5, 0, 0 }, cur[5] = { 2, 3, 5, 7, 0 }, brk[5], make[5];
    DEVDESC desc = { 0 };
    HIDP_DATA data[4], got[16];
    ULONG len, i;
    char report[8];
    EXTATTR attr;
    NTSTATUS status;
    PHIDP_PREPARSED_DATA pp;
    int x = -1, y = -1, b3 = 0, b5 = 0;

    status = pDiff( prev, cur, brk, make, 5 );
    check( status == HIDP_STATUS_SUCCESS && brk[0] == 1 && !brk[1] && make[0] == 3 && make[1] == 7 && !make[2],
           "HidP_UsageListDifference: 1 broken, 3 and 7 made", status );

    check( pGetColDesc != NULL, "hidparse.sys HidP_GetCollectionDescription", 0 );
    if (!pGetColDesc) goto done;
    status = pGetColDesc( descriptor, sizeof(descriptor), 0 /* NonPagedPool */, &desc );
    check( status == HIDP_STATUS_SUCCESS && desc.CollectionDescLength == 1, "preparsed data from the descriptor", status );
    if (status != HIDP_STATUS_SUCCESS) goto done;
    pp = desc.CollectionDesc[0].PreparsedData;
    printf( "      input report %u bytes\n", desc.CollectionDesc[0].InputLength );

    /* data indices: buttons 0-7, X 8, Y 9 (the order of the main items) */
    memset( report, 0, sizeof(report) );
    data[0].DataIndex = 2; data[0].On = TRUE;          /* button 3 */
    data[1].DataIndex = 4; data[1].On = TRUE;          /* button 5 */
    data[2].DataIndex = 8; data[2].RawValue = 0x40;    /* X */
    data[3].DataIndex = 9; data[3].RawValue = 0xc0;    /* Y */
    len = 4;
    status = pSetData( HidP_Input, data, &len, pp, report, desc.CollectionDesc[0].InputLength );
    printf( "      report %02x %02x %02x %02x\n", (BYTE)report[0], (BYTE)report[1], (BYTE)report[2], (BYTE)report[3] );
    check( status == HIDP_STATUS_SUCCESS && len == 4, "HidP_SetData of two buttons and two values", status );
    len = 16;
    status = pGetData( HidP_Input, got, &len, pp, report, desc.CollectionDesc[0].InputLength );
    for (i = 0; i < len; i++)
    {
        if (got[i].DataIndex == 2) b3 = got[i].On;
        if (got[i].DataIndex == 4) b5 = got[i].On;
        if (got[i].DataIndex == 8) x = got[i].RawValue;
        if (got[i].DataIndex == 9) y = got[i].RawValue;
    }
    check( status == HIDP_STATUS_SUCCESS && b3 && b5 && x == 0x40 && y == 0xc0, "HidP_GetData reads them back", status );

    data[0].On = FALSE;
    len = 1;
    status = pSetData( HidP_Input, data, &len, pp, report, desc.CollectionDesc[0].InputLength );
    check( status == HIDP_STATUS_SUCCESS && !(report[1] & 0x04) && (report[1] & 0x10), "a button set off is cleared", (BYTE)report[1] );

    data[0].DataIndex = 77;
    len = 1;
    status = pSetData( HidP_Input, data, &len, pp, report, desc.CollectionDesc[0].InputLength );
    check( status == HIDP_STATUS_DATA_INDEX_NOT_FOUND && len == 0, "an unknown data index: HIDP_STATUS_DATA_INDEX_NOT_FOUND", status );

    len = sizeof(attr);
    memset( &attr, 0xcc, sizeof(attr) );
    status = pExt( HidP_Input, 8, pp, &attr, &len );
    check( status == HIDP_STATUS_SUCCESS && attr.NumGlobalUnknowns == 0, "HidP_GetExtendedAttributes: no unknown items", status );
    status = pExt( HidP_Input, 77, pp, &attr, &len );
    check( status == HIDP_STATUS_DATA_INDEX_NOT_FOUND, "of an unknown index", status );
done:
    printf( "RESULT: %s\n", failures ? "FAIL" : "PASS" );
    return failures != 0;
}
