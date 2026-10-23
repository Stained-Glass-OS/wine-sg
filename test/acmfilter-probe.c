/* acmfilter-probe: acmFilterChooseW/A (Stained Glass OS wine-sg 6509) with a filter driver of
 * the probe's own (acmDriverAdd, ACM_DRIVERADDF_FUNCTION): one tag, two filters. Prints
 * name=value lines; see test/acmfilter-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <msacm.h>
/* the driver messages ([MS] ACM driver reference: ACMDM_BASE = ACMDM_RESERVED_LOW = DRV_USER + 0x2000) */
#define ACMDM_DRIVER_DETAILS    (DRV_USER + 0x2000 + 10)
#define ACMDM_FILTERTAG_DETAILS (DRV_USER + 0x2000 + 50)
#define ACMDM_FILTER_DETAILS    (DRV_USER + 0x2000 + 51)
#include <stdio.h>

#define IDD_FILTERTAG 101
#define IDD_FILTER    102

struct probe_filter { WAVEFILTER wf; DWORD level; };
static const WCHAR *names[] = { L"Half", L"Quiet" };
static int action;  /* 0: choose the second filter, 1: cancel, 2: report the selection, OK */
static int tags_seen, filters_seen, preselected = -1;

static LRESULT CALLBACK driver_proc( DWORD_PTR id, HDRVR drv, UINT msg, LPARAM p1, LPARAM p2 )
{
    switch (msg)
    {
    case DRV_LOAD: case DRV_FREE: case DRV_OPEN: case DRV_CLOSE: case DRV_ENABLE: case DRV_DISABLE:
        return 1;
    case ACMDM_DRIVER_DETAILS:
    {
        ACMDRIVERDETAILSW *d = (ACMDRIVERDETAILSW *)p1;
        DWORD size = d->cbStruct;
        memset( d, 0, size );
        d->cbStruct = size;
        d->fccType = ACMDRIVERDETAILS_FCCTYPE_AUDIOCODEC;
        d->wMid = 1; d->wPid = 1;
        d->vdwACM = 0x04000000; d->vdwDriver = 0x01000000;
        d->fdwSupport = ACMDRIVERDETAILS_SUPPORTF_FILTER;
        d->cFilterTags = 1;
        lstrcpyW( d->szShortName, L"SG Filter" );
        lstrcpyW( d->szLongName, L"SG test filter" );
        return MMSYSERR_NOERROR;
    }
    case ACMDM_FILTERTAG_DETAILS:
    {
        ACMFILTERTAGDETAILSW *t = (ACMFILTERTAGDETAILSW *)p1;
        if ((p2 & ACM_FILTERTAGDETAILSF_QUERYMASK) == ACM_FILTERTAGDETAILSF_INDEX && t->dwFilterTagIndex) return ACMERR_NOTPOSSIBLE;
        if ((p2 & ACM_FILTERTAGDETAILSF_QUERYMASK) == ACM_FILTERTAGDETAILSF_FILTERTAG && t->dwFilterTag != WAVE_FILTER_VOLUME)
            return ACMERR_NOTPOSSIBLE;
        t->dwFilterTagIndex = 0;
        t->dwFilterTag = WAVE_FILTER_VOLUME;
        t->cbFilterSize = sizeof(struct probe_filter);
        t->fdwSupport = ACMDRIVERDETAILS_SUPPORTF_FILTER;
        t->cStandardFilters = 2;
        lstrcpyW( t->szFilterTag, L"SG Volume" );
        return MMSYSERR_NOERROR;
    }
    case ACMDM_FILTER_DETAILS:
    {
        ACMFILTERDETAILSW *f = (ACMFILTERDETAILSW *)p1;
        struct probe_filter *pf = (struct probe_filter *)f->pwfltr;
        if ((p2 & ACM_FILTERDETAILSF_QUERYMASK) != ACM_FILTERDETAILSF_INDEX || f->dwFilterIndex > 1) return ACMERR_NOTPOSSIBLE;
        pf->wf.cbStruct = sizeof(*pf);
        pf->wf.dwFilterTag = WAVE_FILTER_VOLUME;
        pf->level = f->dwFilterIndex ? 0x1000 : 0x8000;
        f->fdwSupport = ACMDRIVERDETAILS_SUPPORTF_FILTER;
        lstrcpyW( f->szFilter, names[f->dwFilterIndex] );
        return MMSYSERR_NOERROR;
    }
    }
    return msg >= DRV_USER ? MMSYSERR_NOTSUPPORTED : DefDriverProc( id, drv, msg, p1, p2 );
}

static UINT CALLBACK hook( HWND dialog, UINT msg, WPARAM wparam, LPARAM lparam )
{
    if (msg != WM_INITDIALOG) return 0;
    tags_seen = SendDlgItemMessageW( dialog, IDD_FILTERTAG, CB_GETCOUNT, 0, 0 );
    filters_seen = SendDlgItemMessageW( dialog, IDD_FILTER, CB_GETCOUNT, 0, 0 );
    preselected = SendDlgItemMessageW( dialog, IDD_FILTER, CB_GETCURSEL, 0, 0 );
    if (action == 0) SendDlgItemMessageW( dialog, IDD_FILTER, CB_SETCURSEL, 1, 0 );
    PostMessageW( dialog, WM_COMMAND, action == 1 ? IDCANCEL : IDOK, 0 );
    return 0;
}

int main( void )
{
    HACMDRIVERID id;
    ACMFILTERCHOOSEW c;
    ACMFILTERCHOOSEA ca;
    struct probe_filter pf;
    MMRESULT ret;

    ret = acmDriverAddW( &id, GetModuleHandleW( NULL ), (LPARAM)driver_proc, 0, ACM_DRIVERADDF_FUNCTION | ACM_DRIVERADDF_LOCAL );
    printf( "driver_added=%d %u\n", ret == MMSYSERR_NOERROR, ret );

    memset( &c, 0, sizeof(c) );
    c.cbStruct = sizeof(c);
    c.fdwStyle = ACMFILTERCHOOSE_STYLEF_ENABLEHOOK;
    c.pwfltr = &pf.wf;
    c.cbwfltr = sizeof(pf);
    c.pfnHook = hook;
    memset( &pf, 0, sizeof(pf) );
    action = 0;
    ret = acmFilterChooseW( &c );
    printf( "lists=%d tags %d filters %d\n", tags_seen == 1 && filters_seen == 2, tags_seen, filters_seen );
    printf( "chosen=%d ret %u tag %#lx level %#lx names %ls/%ls\n", ret == MMSYSERR_NOERROR &&
            pf.wf.dwFilterTag == WAVE_FILTER_VOLUME && pf.level == 0x1000 && !lstrcmpW( c.szFilter, L"Quiet" ) &&
            !lstrcmpW( c.szFilterTag, L"SG Volume" ), ret, pf.wf.dwFilterTag, pf.level, c.szFilterTag, c.szFilter );

    /* the chosen one comes back selected */
    c.fdwStyle |= ACMFILTERCHOOSE_STYLEF_INITTOFILTERSTRUCT;
    action = 2;
    ret = acmFilterChooseW( &c );
    printf( "init_to_struct=%d selected %d ret %u level %#lx\n", preselected == 1 && ret == MMSYSERR_NOERROR && pf.level == 0x1000,
            preselected, ret, pf.level );

    action = 1;
    ret = acmFilterChooseW( &c );
    printf( "canceled=%d ret %u\n", ret == ACMERR_CANCELED, ret );

    c.cbwfltr = 4;
    action = 0;
    ret = acmFilterChooseW( &c );
    printf( "too_small=%d ret %u size %lu\n", ret == ACMERR_NOTPOSSIBLE && c.cbwfltr == sizeof(pf), ret, c.cbwfltr );

    memset( &ca, 0, sizeof(ca) );
    ca.cbStruct = sizeof(ca);
    ca.fdwStyle = ACMFILTERCHOOSE_STYLEF_ENABLEHOOK;
    ca.pwfltr = &pf.wf;
    ca.cbwfltr = sizeof(pf);
    ca.pfnHook = (ACMFILTERCHOOSEHOOKPROCA)hook;
    ret = acmFilterChooseA( &ca );
    printf( "ansi=%d ret %u %s/%s\n", ret == MMSYSERR_NOERROR && !strcmp( ca.szFilter, "Quiet" ), ret, ca.szFilterTag, ca.szFilter );

    memset( &c, 0, sizeof(c) );
    printf( "invalid=%d %u\n", acmFilterChooseW( &c ) == MMSYSERR_INVALPARAM, acmFilterChooseW( &c ) );
    acmDriverRemove( id, 0 );
    printf( "done=1\n" );
    return 0;
}
