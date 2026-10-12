/* dcomp (patches/sg/2672): the three-dimensional transforms of the version 1 device (translate, scale, rotate, matrix,
 * group), their validation, and their use by an effect group. Vtable slots are called by index. */
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

static const GUID IID_T3D = { 0x71185722, 0x246b, 0x41f2, { 0xaa, 0xd1, 0x04, 0x43, 0xf7, 0xf4, 0xbf, 0xc2 } };
static const GUID IID_Trans3D = { 0x91636d4b, 0x9ba1, 0x4532, { 0xaa, 0xf7, 0xe3, 0x34, 0x49, 0x94, 0xd7, 0x88 } };
static const GUID IID_Rot3D = { 0xd8f5b23f, 0xd429, 0x4a91, { 0xb5, 0x5a, 0xd2, 0xf4, 0x5b, 0x75, 0x18, 0x5f } };

int main(void)
{
    HMODULE dcomp = LoadLibraryA("dcomp.dll");
    PFN_CREATE create = dcomp ? (PFN_CREATE)GetProcAddress(dcomp, "DCompositionCreateDevice") : NULL;
    void *dev = NULL, *tr = NULL, *sc = NULL, *ro = NULL, *ma = NULL, *group = NULL, *effect = NULL, *anim = NULL, *tmp = NULL;
    float nan = nanf("");
    HRESULT hr;

    if (!create || FAILED(create(NULL, &IID_Device, &dev))) { printf("FAIL  no device\nRESULT: FAIL\n"); return 1; }
    check(SUCCEEDED(((f_out)SLOT(dev, 18))(dev, &tr)), "CreateTranslateTransform3D");
    check(SUCCEEDED(((f_out)SLOT(dev, 19))(dev, &sc)), "CreateScaleTransform3D");
    check(SUCCEEDED(((f_out)SLOT(dev, 20))(dev, &ro)), "CreateRotateTransform3D");
    check(SUCCEEDED(((f_out)SLOT(dev, 21))(dev, &ma)), "CreateMatrixTransform3D");
    ((f_out)SLOT(dev, 23))(dev, &effect);
    ((f_out)SLOT(dev, 25))(dev, &anim);
    if (!tr || !sc || !ro || !ma || !effect || !anim) { printf("RESULT: FAIL\n"); return 1; }

    check(QI(tr, &IID_Trans3D, &tmp) == S_OK && tmp == tr, "translate3d: its own interface");
    if (tmp) RELEASE(tmp);
    tmp = NULL;
    check(QI(tr, &IID_T3D, &tmp) == S_OK, "translate3d: IDCompositionTransform3D");
    if (tmp) RELEASE(tmp);
    tmp = NULL;
    check(QI(tr, &IID_Rot3D, &tmp) == E_NOINTERFACE && !tmp, "translate3d: not a rotation");

    check(((f_float)SLOT(tr, 3))(tr, 1.0f) == S_OK && ((f_float)SLOT(tr, 5))(tr, 2.0f) == S_OK
          && ((f_float)SLOT(tr, 7))(tr, 3.0f) == S_OK, "translate3d: offsets x, y, z");
    check(((f_float)SLOT(tr, 7))(tr, nan) == E_INVALIDARG, "translate3d: SetOffsetZ(NaN) is E_INVALIDARG");
    check(((f_ptr)SLOT(tr, 8))(tr, anim) == S_OK, "translate3d: SetOffsetZAnimation");
    check(((f_ptr)SLOT(tr, 8))(tr, &foreign_obj) == E_INVALIDARG, "translate3d: a foreign animation is E_INVALIDARG");
    check(((f_float)SLOT(sc, 9))(sc, 2.0f) == S_OK && ((f_float)SLOT(sc, 13))(sc, 1.0f) == S_OK, "scale3d: Z and center Z");
    check(((f_float)SLOT(ro, 3))(ro, 45.0f) == S_OK && ((f_float)SLOT(ro, 9))(ro, 1.0f) == S_OK
          && ((f_float)SLOT(ro, 15))(ro, 0.5f) == S_OK, "rotate3d: angle, axis Z, center Z");
    check(((f_float)SLOT(ro, 3))(ro, nan) == E_INVALIDARG, "rotate3d: SetAngle(NaN) is E_INVALIDARG");

    check(((f_int2f)SLOT(ma, 4))(ma, 3, 3, 2.0f) == S_OK, "matrix3d: SetMatrixElement(3, 3)");
    check(((f_int2f)SLOT(ma, 4))(ma, 4, 0, 2.0f) == E_INVALIDARG, "matrix3d: row 4 is E_INVALIDARG");
    check(((f_int2f)SLOT(ma, 4))(ma, 0, 4, 2.0f) == E_INVALIDARG, "matrix3d: column 4 is E_INVALIDARG");
    check(((f_int2p)SLOT(ma, 5))(ma, 2, 3, anim) == S_OK, "matrix3d: SetMatrixElementAnimation");
    {
        float m[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

        check(((f_ptr)SLOT(ma, 3))(ma, m) == S_OK, "matrix3d: SetMatrix");
        m[15] = nan;
        check(((f_ptr)SLOT(ma, 3))(ma, m) == E_INVALIDARG, "matrix3d: SetMatrix with NaN is E_INVALIDARG");
        check(((f_ptr)SLOT(ma, 3))(ma, NULL) == E_INVALIDARG, "matrix3d: SetMatrix(NULL) is E_INVALIDARG");
    }

    {
        void *two[2] = { tr, ro }, *bad[2] = { tr, &foreign_obj };

        check(((f_group)SLOT(dev, 22))(dev, NULL, 0, &group) == E_INVALIDARG, "CreateTransform3DGroup(0 elements) is E_INVALIDARG");
        check(((f_group)SLOT(dev, 22))(dev, bad, 2, &group) == E_INVALIDARG, "CreateTransform3DGroup(foreign) is E_INVALIDARG");
        hr = ((f_group)SLOT(dev, 22))(dev, two, 2, &group);
        check(hr == S_OK && group, "CreateTransform3DGroup (%#lx)", hr);
    }
    check(((f_ptr)SLOT(effect, 5))(effect, ma) == S_OK, "an effect group takes a 3D matrix transform");
    check(((f_ptr)SLOT(effect, 5))(effect, group) == S_OK, "and a group of 3D transforms");

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
