/* IMFQualityAdvise and IMFQualityAdviseLimits by hand: the mingw headers have
 * the MF_QUALITY_* enums but not the two interfaces. Shared by the evr probes. */
#ifndef SG_EVR_QA_H
#define SG_EVR_QA_H
DEFINE_GUID(IID_IMFQualityAdvise_sg, 0xec15e2e9, 0xe36b, 0x4f7c, 0x87, 0x58, 0x77, 0xd4, 0x52, 0xef, 0x4c, 0xe7);
DEFINE_GUID(IID_IMFQualityAdviseLimits_sg, 0xdfcd8e4d, 0x30b5, 0x4567, 0xac, 0xaa, 0x8e, 0xb5, 0xb7, 0x85, 0x3d, 0xc9);
typedef struct IMFQualityAdvise IMFQualityAdvise;
typedef struct
{
    HRESULT (WINAPI *QueryInterface)(IMFQualityAdvise *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IMFQualityAdvise *);
    ULONG (WINAPI *Release)(IMFQualityAdvise *);
    HRESULT (WINAPI *SetDropMode)(IMFQualityAdvise *, MF_QUALITY_DROP_MODE);
    HRESULT (WINAPI *SetQualityLevel)(IMFQualityAdvise *, MF_QUALITY_LEVEL);
    HRESULT (WINAPI *GetDropMode)(IMFQualityAdvise *, MF_QUALITY_DROP_MODE *);
    HRESULT (WINAPI *GetQualityLevel)(IMFQualityAdvise *, MF_QUALITY_LEVEL *);
    HRESULT (WINAPI *DropTime)(IMFQualityAdvise *, LONGLONG);
} IMFQualityAdviseVtbl;
struct IMFQualityAdvise { const IMFQualityAdviseVtbl *lpVtbl; };
#define IMFQualityAdvise_SetDropMode(p, a) (p)->lpVtbl->SetDropMode(p, a)
#define IMFQualityAdvise_SetQualityLevel(p, a) (p)->lpVtbl->SetQualityLevel(p, a)
#define IMFQualityAdvise_GetDropMode(p, a) (p)->lpVtbl->GetDropMode(p, a)
#define IMFQualityAdvise_GetQualityLevel(p, a) (p)->lpVtbl->GetQualityLevel(p, a)
#define IMFQualityAdvise_DropTime(p, a) (p)->lpVtbl->DropTime(p, a)
#define IMFQualityAdvise_Release(p) (p)->lpVtbl->Release(p)

typedef struct IMFQualityAdviseLimits IMFQualityAdviseLimits;
typedef struct
{
    HRESULT (WINAPI *QueryInterface)(IMFQualityAdviseLimits *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IMFQualityAdviseLimits *);
    ULONG (WINAPI *Release)(IMFQualityAdviseLimits *);
    HRESULT (WINAPI *GetMaximumDropMode)(IMFQualityAdviseLimits *, MF_QUALITY_DROP_MODE *);
    HRESULT (WINAPI *GetMinimumQualityLevel)(IMFQualityAdviseLimits *, MF_QUALITY_LEVEL *);
} IMFQualityAdviseLimitsVtbl;
struct IMFQualityAdviseLimits { const IMFQualityAdviseLimitsVtbl *lpVtbl; };
#define IMFQualityAdviseLimits_GetMaximumDropMode(p, a) (p)->lpVtbl->GetMaximumDropMode(p, a)
#define IMFQualityAdviseLimits_GetMinimumQualityLevel(p, a) (p)->lpVtbl->GetMinimumQualityLevel(p, a)
#define IMFQualityAdviseLimits_Release(p) (p)->lpVtbl->Release(p)
#endif
