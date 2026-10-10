/* dcomp (patches/sg/2654): the animation, transform, rectangle clip and effect group objects of the version 1
 * device, and the visual setters that take them. Vtable slots are called by index (the layout of the IDL). */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>

static int failures;

static void check(int ok, const char *fmt, ...)
{
    char buf[256];
    va_list args;

    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s  %s\n", ok ? "PASS" : "FAIL", buf);
    if (!ok) failures++;
}

typedef HRESULT (WINAPI *PFN_CREATE)(void *, REFIID, void **);
typedef HRESULT (STDMETHODCALLTYPE *f_out)(void *, void **);
typedef HRESULT (STDMETHODCALLTYPE *f_ptr)(void *, void *);
typedef HRESULT (STDMETHODCALLTYPE *f_float)(void *, float);
typedef HRESULT (STDMETHODCALLTYPE *f_qi)(void *, REFIID, void **);
typedef ULONG (STDMETHODCALLTYPE *f_ref)(void *);
typedef HRESULT (STDMETHODCALLTYPE *f_int2f)(void *, int, int, float);
typedef HRESULT (STDMETHODCALLTYPE *f_int2p)(void *, int, int, void *);
typedef HRESULT (STDMETHODCALLTYPE *f_cubic)(void *, double, float, float, float, float);
typedef HRESULT (STDMETHODCALLTYPE *f_dbl2)(void *, double, double);
typedef HRESULT (STDMETHODCALLTYPE *f_end)(void *, double, float);
typedef HRESULT (STDMETHODCALLTYPE *f_group)(void *, void **, UINT, void **);
typedef HRESULT (STDMETHODCALLTYPE *f_void)(void *);

#define SLOT(obj, i) ((*(void ***)(obj))[i])
#define QI(o, iid, out) ((f_qi)SLOT(o, 0))(o, iid, out)
#define ADDREF(o) ((f_ref)SLOT(o, 1))(o)
#define RELEASE(o) ((f_ref)SLOT(o, 2))(o)

static const GUID IID_Transform = { 0xfd55faa7, 0x37e0, 0x4c20, { 0x95, 0xd2, 0x9b, 0xe4, 0x5b, 0xc3, 0x3f, 0x55 } };
static const GUID IID_Translate = { 0x06791122, 0xc6f0, 0x417d, { 0x83, 0x23, 0x26, 0x9e, 0x98, 0x7f, 0x59, 0x54 } };
static const GUID IID_Rotate = { 0x641ed83c, 0xae96, 0x46c5, { 0x90, 0xdc, 0x32, 0x77, 0x4c, 0xc5, 0xc6, 0xd5 } };
static const GUID IID_Clip = { 0x64ac3703, 0x9d3f, 0x45ec, { 0xa1, 0x09, 0x7c, 0xac, 0x0e, 0x7a, 0x13, 0xa7 } };
static const GUID IID_Effect = { 0xec81b08f, 0xbfcb, 0x4e8d, { 0xb1, 0x93, 0xa9, 0x15, 0x58, 0x79, 0x99, 0xe8 } };
static const GUID IID_Animation = { 0xcbfd91d9, 0x51b2, 0x45e4, { 0xb3, 0xde, 0xd1, 0x9c, 0xcf, 0xb8, 0x63, 0xc5 } };
static const GUID IID_Device = { 0xc37ea93a, 0xe7aa, 0x450d, { 0xb1, 0x6f, 0x97, 0x46, 0xcb, 0x04, 0x07, 0xf3 } };

/* an object of another implementation */
static HRESULT STDMETHODCALLTYPE foreign_qi(void *i, REFIID r, void **o) { *o = NULL; return E_NOINTERFACE; }
static ULONG STDMETHODCALLTYPE foreign_ref(void *i) { return 2; }
static void *foreign_vtbl[3] = { foreign_qi, foreign_ref, foreign_ref };
static void *foreign_obj = foreign_vtbl;

static LONG refcount_of(void *o)
{
    ADDREF(o);
    return RELEASE(o);
}

int main(void)
{
    HMODULE dcomp = LoadLibraryA("dcomp.dll");
    PFN_CREATE create = dcomp ? (PFN_CREATE)GetProcAddress(dcomp, "DCompositionCreateDevice") : NULL;
    void *dev = NULL, *translate = NULL, *scale = NULL, *rotate = NULL, *skew = NULL, *matrix = NULL;
    void *anim = NULL, *clip = NULL, *effect = NULL, *group = NULL, *visual = NULL, *tmp = NULL;
    float nan = nanf(""), inf = INFINITY;
    HRESULT hr;
    LONG before;

    if (!create || FAILED(create(NULL, &IID_Device, &dev))) { printf("FAIL  no device\nRESULT: FAIL\n"); return 1; }

    check(SUCCEEDED(((f_out)SLOT(dev, 12))(dev, &translate)), "CreateTranslateTransform");
    check(SUCCEEDED(((f_out)SLOT(dev, 13))(dev, &scale)), "CreateScaleTransform");
    check(SUCCEEDED(((f_out)SLOT(dev, 14))(dev, &rotate)), "CreateRotateTransform");
    check(SUCCEEDED(((f_out)SLOT(dev, 15))(dev, &skew)), "CreateSkewTransform");
    check(SUCCEEDED(((f_out)SLOT(dev, 16))(dev, &matrix)), "CreateMatrixTransform");
    check(SUCCEEDED(((f_out)SLOT(dev, 23))(dev, &effect)), "CreateEffectGroup");
    check(SUCCEEDED(((f_out)SLOT(dev, 24))(dev, &clip)), "CreateRectangleClip");
    check(SUCCEEDED(((f_out)SLOT(dev, 25))(dev, &anim)), "CreateAnimation");
    check(SUCCEEDED(((f_out)SLOT(dev, 7))(dev, &visual)), "CreateVisual");
    check(((f_out)SLOT(dev, 12))(dev, NULL) == E_INVALIDARG, "CreateTranslateTransform(NULL) is E_INVALIDARG");
    if (!translate || !scale || !rotate || !skew || !matrix || !effect || !clip || !anim || !visual)
    {
        printf("RESULT: FAIL\n");
        return 1;
    }

    /* interfaces */
    check(QI(translate, &IID_Translate, &tmp) == S_OK && tmp == translate, "translate: its own interface");
    if (tmp) RELEASE(tmp);
    tmp = NULL;
    check(QI(translate, &IID_Transform, &tmp) == S_OK, "translate: IDCompositionTransform");
    if (tmp) RELEASE(tmp);
    tmp = NULL;
    check(QI(translate, &IID_Rotate, &tmp) == E_NOINTERFACE && !tmp, "translate: not a rotate transform");
    tmp = NULL;
    check(QI(clip, &IID_Clip, &tmp) == S_OK, "clip: IDCompositionClip");
    if (tmp) RELEASE(tmp);
    tmp = NULL;
    check(QI(effect, &IID_Effect, &tmp) == S_OK, "effect group: IDCompositionEffect");
    if (tmp) RELEASE(tmp);
    tmp = NULL;
    check(QI(anim, &IID_Animation, &tmp) == S_OK, "animation: its interface");
    if (tmp) RELEASE(tmp);

    /* numbers */
    check(((f_float)SLOT(translate, 3))(translate, 12.5f) == S_OK, "translate: SetOffsetX");
    check(((f_float)SLOT(translate, 3))(translate, nan) == E_INVALIDARG, "translate: SetOffsetX(NaN) is E_INVALIDARG");
    check(((f_float)SLOT(translate, 5))(translate, inf) == E_INVALIDARG, "translate: SetOffsetY(inf) is E_INVALIDARG");
    check(((f_float)SLOT(scale, 9))(scale, 3.0f) == S_OK, "scale: SetCenterY");
    check(((f_float)SLOT(rotate, 3))(rotate, 90.0f) == S_OK, "rotate: SetAngle");
    check(((f_float)SLOT(clip, 3))(clip, 1.0f) == S_OK && ((f_float)SLOT(clip, 9))(clip, 8.0f) == S_OK, "clip: SetLeft/SetBottom");
    check(((f_float)SLOT(clip, 3))(clip, nan) == E_INVALIDARG, "clip: SetLeft(NaN) is E_INVALIDARG");
    check(((f_float)SLOT(effect, 3))(effect, 0.5f) == S_OK, "effect group: SetOpacity");
    check(((f_float)SLOT(effect, 3))(effect, nan) == E_INVALIDARG, "effect group: SetOpacity(NaN) is E_INVALIDARG");

    /* animations */
    check(((f_ptr)SLOT(translate, 4))(translate, NULL) == E_INVALIDARG, "SetOffsetXAnimation(NULL) is E_INVALIDARG");
    check(((f_ptr)SLOT(translate, 4))(translate, &foreign_obj) == E_INVALIDARG, "SetOffsetXAnimation(foreign) is E_INVALIDARG");
    before = refcount_of(anim);
    check(((f_ptr)SLOT(translate, 4))(translate, anim) == S_OK, "SetOffsetXAnimation(animation)");
    check(refcount_of(anim) == before + 1, "the transform holds the animation (%ld -> %ld)", before, refcount_of(anim));
    check(((f_float)SLOT(translate, 3))(translate, 1.0f) == S_OK && refcount_of(anim) == before,
          "SetOffsetX releases the animation");
    check(((f_cubic)SLOT(anim, 5))(anim, 0.0, 1.0f, 2.0f, 0.0f, 0.0f) == S_OK, "AddCubic at 0");
    check(((f_cubic)SLOT(anim, 5))(anim, 1.0, 0.0f, 0.0f, 0.0f, 0.0f) == S_OK, "AddCubic at 1");
    check(((f_cubic)SLOT(anim, 5))(anim, 0.5, 0.0f, 0.0f, 0.0f, 0.0f) == E_INVALIDARG, "AddCubic before the last segment is E_INVALIDARG");
    check(((f_cubic)SLOT(anim, 5))(anim, -1.0, 0.0f, 0.0f, 0.0f, 0.0f) == E_INVALIDARG, "AddCubic at a negative offset is E_INVALIDARG");
    check(((f_cubic)SLOT(anim, 5))(anim, 2.0, nan, 0.0f, 0.0f, 0.0f) == E_INVALIDARG, "AddCubic with NaN is E_INVALIDARG");
    check(((f_cubic)SLOT(anim, 6))(anim, 2.0, 0.0f, 1.0f, 2.0f, 0.0f) == S_OK, "AddSinusoidal");
    check(((f_cubic)SLOT(anim, 6))(anim, 3.0, 0.0f, 1.0f, -2.0f, 0.0f) == E_INVALIDARG, "AddSinusoidal with a negative frequency is E_INVALIDARG");
    check(((f_dbl2)SLOT(anim, 7))(anim, 3.0, 0.0) == E_INVALIDARG, "AddRepeat with no duration is E_INVALIDARG");
    check(((f_dbl2)SLOT(anim, 7))(anim, 3.0, 1.0) == S_OK, "AddRepeat");
    check(((f_end)SLOT(anim, 8))(anim, 4.0, 5.0f) == S_OK, "End");
    check(((f_cubic)SLOT(anim, 5))(anim, 5.0, 0.0f, 0.0f, 0.0f, 0.0f) == E_INVALIDARG, "AddCubic after End is E_INVALIDARG");
    check(((f_void)SLOT(anim, 3))(anim) == S_OK, "Reset");
    check(((f_cubic)SLOT(anim, 5))(anim, 0.0, 1.0f, 0.0f, 0.0f, 0.0f) == S_OK, "AddCubic after Reset");

    /* the matrix transform */
    check(((f_int2f)SLOT(matrix, 4))(matrix, 2, 1, 7.0f) == S_OK, "SetMatrixElement(2, 1)");
    check(((f_int2f)SLOT(matrix, 4))(matrix, 3, 0, 7.0f) == E_INVALIDARG, "SetMatrixElement(3, 0) is E_INVALIDARG");
    check(((f_int2f)SLOT(matrix, 4))(matrix, 0, 2, 7.0f) == E_INVALIDARG, "SetMatrixElement(0, 2) is E_INVALIDARG");
    check(((f_int2f)SLOT(matrix, 4))(matrix, -1, 0, 7.0f) == E_INVALIDARG, "SetMatrixElement(-1, 0) is E_INVALIDARG");
    check(((f_int2p)SLOT(matrix, 5))(matrix, 1, 1, anim) == S_OK, "SetMatrixElementAnimation");
    check(((f_int2p)SLOT(matrix, 5))(matrix, 5, 1, anim) == E_INVALIDARG, "SetMatrixElementAnimation row 5 is E_INVALIDARG");
    check(((f_ptr)SLOT(matrix, 3))(matrix, NULL) == E_INVALIDARG, "SetMatrix(NULL) is E_INVALIDARG");
    {
        float m[6] = { 1, 0, 0, 1, 4, 5 };
        check(((f_ptr)SLOT(matrix, 3))(matrix, m) == S_OK, "SetMatrix");
        m[4] = nan;
        check(((f_ptr)SLOT(matrix, 3))(matrix, m) == E_INVALIDARG, "SetMatrix with NaN is E_INVALIDARG");
    }

    /* a group */
    {
        void *two[2] = { translate, rotate }, *bad[2] = { translate, &foreign_obj };

        check(((f_group)SLOT(dev, 17))(dev, NULL, 0, &group) == E_INVALIDARG, "CreateTransformGroup(0 elements) is E_INVALIDARG");
        check(((f_group)SLOT(dev, 17))(dev, bad, 2, &group) == E_INVALIDARG, "CreateTransformGroup(foreign) is E_INVALIDARG");
        before = refcount_of(translate);
        check(((f_group)SLOT(dev, 17))(dev, two, 2, &group) == S_OK && group, "CreateTransformGroup");
        check(refcount_of(translate) == before + 1, "the group holds its transforms");
    }

    /* the visual */
    before = refcount_of(rotate);
    check(((f_ptr)SLOT(visual, 7))(visual, rotate) == S_OK, "visual: SetTransformObject");
    check(refcount_of(rotate) == before + 1, "the visual holds its transform (%ld -> %ld)", before, refcount_of(rotate));
    check(((f_ptr)SLOT(visual, 7))(visual, &foreign_obj) == E_INVALIDARG, "visual: SetTransformObject(foreign) is E_INVALIDARG");
    check(((f_ptr)SLOT(visual, 7))(visual, NULL) == S_OK && refcount_of(rotate) == before, "visual: SetTransformObject(NULL) releases it");
    check(((f_ptr)SLOT(visual, 8))(visual, NULL) == E_INVALIDARG, "visual: SetTransform(NULL) is E_INVALIDARG");
    {
        float m[6] = { 1, 0, 0, 1, 4, 5 }, r[4] = { 0, 0, 10, 10 };

        check(((f_ptr)SLOT(visual, 8))(visual, m) == S_OK, "visual: SetTransform(matrix)");
        m[0] = inf;
        check(((f_ptr)SLOT(visual, 8))(visual, m) == E_INVALIDARG, "visual: SetTransform(inf) is E_INVALIDARG");
        check(((f_ptr)SLOT(visual, 14))(visual, r) == S_OK, "visual: SetClip(rect)");
        check(((f_ptr)SLOT(visual, 14))(visual, NULL) == E_INVALIDARG, "visual: SetClip(NULL) is E_INVALIDARG");
    }
    check(((f_ptr)SLOT(visual, 13))(visual, clip) == S_OK, "visual: SetClipObject");
    check(((f_ptr)SLOT(visual, 13))(visual, &foreign_obj) == E_INVALIDARG, "visual: SetClipObject(foreign) is E_INVALIDARG");
    check(((f_ptr)SLOT(visual, 10))(visual, effect) == S_OK, "visual: SetEffect(effect group)");
    check(((f_ptr)SLOT(visual, 10))(visual, translate) == S_OK, "visual: SetEffect(transform)");
    check(((f_ptr)SLOT(visual, 10))(visual, &foreign_obj) == E_INVALIDARG, "visual: SetEffect(foreign) is E_INVALIDARG");
    check(((f_ptr)SLOT(effect, 5))(effect, translate) == S_OK, "effect group: SetTransform3D(transform)");
    check(((f_ptr)SLOT(effect, 5))(effect, &foreign_obj) == E_INVALIDARG, "effect group: SetTransform3D(foreign) is E_INVALIDARG");
    check(((f_float)SLOT(visual, 4))(visual, 3.0f) == S_OK, "visual: SetOffsetX");
    check(((f_float)SLOT(visual, 4))(visual, nan) == E_INVALIDARG, "visual: SetOffsetX(NaN) is E_INVALIDARG");
    check(((f_ptr)SLOT(visual, 3))(visual, NULL) == E_INVALIDARG, "visual: SetOffsetXAnimation(NULL) is E_INVALIDARG");
    before = refcount_of(anim);
    check(((f_ptr)SLOT(visual, 5))(visual, anim) == S_OK && refcount_of(anim) == before + 1, "visual: SetOffsetYAnimation holds it");
    hr = ((f_void)SLOT(dev, 3))(dev);
    check(hr == S_OK, "Commit with animated visuals");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
