#define COBJMACROS
#define INITGUID
#include <windows.h>
#include <ole2.h>
#include <oleauto.h>
#include <stdio.h>
#include <msinkaut.h>

static int fails, checks;
static const GUID nullguid;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)
#define OK(e) do { HRESULT h_ = (e); checks++; if (FAILED(h_)) { fails++; printf("FAIL line %d: %s -> %08lx\n", __LINE__, #e, (long)h_); } } while (0)

static const CLSID clsid_inkdisp = {0x937c1a34,0x151d,0x4610,{0x9c,0xa6,0xa8,0xcc,0x9b,0xdb,0x5d,0x83}};

static VARIANT longs(const LONG *v, int n)
{
    VARIANT var;
    SAFEARRAY *sa = SafeArrayCreateVector(VT_I4, 0, n);
    LONG *p;
    SafeArrayAccessData(sa, (void **)&p);
    memcpy(p, v, n * sizeof(LONG));
    SafeArrayUnaccessData(sa);
    VariantInit(&var);
    V_VT(&var) = VT_ARRAY | VT_I4;
    V_ARRAY(&var) = sa;
    return var;
}

static LONG rect_get(IInkRectangle *r, int which)
{
    LONG t, l, b, rr;
    IInkRectangle_GetRectangle(r, &t, &l, &b, &rr);
    return which == 0 ? t : which == 1 ? l : which == 2 ? b : rr;
}

static IInkStrokeDisp *mkstroke(IInkDisp *ink, const LONG *pts, int n)
{
    IInkStrokeDisp *s = NULL;
    VARIANT p = longs(pts, n * 2), d;
    VariantInit(&d);
    OK(IInkDisp_CreateStroke(ink, p, d, &s));
    VariantClear(&p);
    return s;
}

int main(void)
{
    typedef HRESULT (WINAPI *gco)(REFCLSID, REFIID, void **);
    HMODULE mod;
    gco get;
    IClassFactory *cf = NULL;
    IInkDisp *ink = NULL, *ink2 = NULL;
    IInkStrokes *strokes = NULL, *hits = NULL;
    IInkStrokeDisp *s1, *s2, *s3, *tmp;
    IInkRectangle *rect = NULL;
    IInkExtendedProperties *props = NULL;
    IInkExtendedProperty *prop = NULL;
    IInkDrawingAttributes *attrs = NULL;
    IDispatch *disp = NULL;
    ITypeInfo *ti = NULL;
    LONG count, id1, id2, v;
    VARIANT_BOOL vb;
    VARIANT var, data, empty;
    float pt, dist;
    DISPID dispid;
    OLECHAR *name = (OLECHAR *)L"Dirty";
    BSTR bstr;

    OleInitialize(NULL);
    mod = LoadLibraryA("inkobj.dll");
    CHECK(mod != NULL);
    get = (gco)GetProcAddress(mod, "DllGetClassObject");
    CHECK(get != NULL);
    OK(get(&clsid_inkdisp, &IID_IClassFactory, (void **)&cf));
    OK(IClassFactory_CreateInstance(cf, NULL, &IID_IInkDisp, (void **)&ink));
    VariantInit(&empty);

    /* IDispatch through the type library */
    OK(IInkDisp_QueryInterface(ink, &IID_IDispatch, (void **)&disp));
    OK(IDispatch_GetTypeInfo(disp, 0, 0, &ti));
    if (ti) ITypeInfo_Release(ti);
    OK(IDispatch_GetIDsOfNames(disp, &nullguid, &name, 1, 0, &dispid));
    CHECK(dispid != 0);
    IDispatch_Release(disp);

    /* empty ink */
    OK(IInkDisp_get_Strokes(ink, &strokes));
    OK(IInkStrokes_get_Count(strokes, &count));
    CHECK(count == 0);
    IInkStrokes_Release(strokes);
    OK(IInkDisp_GetBoundingBox(ink, IBBM_Default, &rect));
    CHECK(rect_get(rect, 0) == 0 && rect_get(rect, 3) == 0);
    IInkRectangle_Release(rect);

    /* strokes */
    {
        LONG a[] = { 100, 100, 200, 100, 300, 100 };
        LONG b[] = { 1000, 1000, 1100, 1100, 1200, 1000 };
        LONG c[] = { 150, 50, 150, 150 };
        s1 = mkstroke(ink, a, 3);
        s2 = mkstroke(ink, b, 3);
        s3 = mkstroke(ink, c, 2);
    }
    CHECK(s1 && s2 && s3);
    OK(IInkDisp_get_Dirty(ink, &vb));
    CHECK(vb == VARIANT_TRUE);
    OK(IInkStrokeDisp_get_ID(s1, &id1));
    OK(IInkStrokeDisp_get_ID(s2, &id2));
    CHECK(id1 != id2);
    OK(IInkStrokeDisp_get_PacketCount(s1, &v));
    CHECK(v == 3);
    OK(IInkStrokeDisp_get_PacketSize(s1, &v));
    CHECK(v == 2);

    OK(IInkDisp_get_Strokes(ink, &strokes));
    OK(IInkStrokes_get_Count(strokes, &count));
    CHECK(count == 3);
    OK(IInkStrokes_Item(strokes, 1, &tmp));
    OK(IInkStrokeDisp_get_ID(tmp, &v));
    CHECK(v == id2);
    IInkStrokeDisp_Release(tmp);
    CHECK(FAILED(IInkStrokes_Item(strokes, 7, &tmp)));
    OK(strokes->lpVtbl->ToString(strokes, &bstr));
    CHECK(bstr && wcschr(bstr, L' '));
    SysFreeString(bstr);
    IInkStrokes_Release(strokes);

    /* bounding boxes */
    OK(IInkDisp_GetBoundingBox(ink, IBBM_PointsOnly, &rect));
    CHECK(rect_get(rect, 1) == 100 && rect_get(rect, 3) == 1200 && rect_get(rect, 0) == 50 && rect_get(rect, 2) == 1100);
    IInkRectangle_Release(rect);
    OK(IInkStrokeDisp_GetBoundingBox(s1, IBBM_Default, &rect));
    CHECK(rect_get(rect, 1) < 100 && rect_get(rect, 3) > 300); /* pen width included */
    IInkRectangle_Release(rect);

    /* points */
    OK(IInkStrokeDisp_GetPoints(s1, 0, -1, &var));
    CHECK(V_VT(&var) == (VT_ARRAY | VT_I4));
    {
        LONG lb, ub;
        SafeArrayGetLBound(V_ARRAY(&var), 1, &lb);
        SafeArrayGetUBound(V_ARRAY(&var), 1, &ub);
        CHECK(ub - lb + 1 == 6);
    }
    VariantClear(&var);
    CHECK(FAILED(IInkStrokeDisp_GetPoints(s1, 5, 1, &var)));

    /* hit testing */
    OK(IInkDisp_HitTestCircle(ink, 1100, 1050, 80.0f, &hits));
    OK(IInkStrokes_get_Count(hits, &count));
    CHECK(count == 1);
    IInkStrokes_Release(hits);
    OK(IInkDisp_HitTestCircle(ink, 5000, 5000, 80.0f, &hits));
    OK(IInkStrokes_get_Count(hits, &count));
    CHECK(count == 0);
    IInkStrokes_Release(hits);
    OK(IInkDisp_NearestPoint(ink, 148, 60, &pt, &dist, &tmp));
    CHECK(tmp != NULL);
    if (tmp)
    {
        OK(IInkStrokeDisp_get_ID(tmp, &v));
        CHECK(dist >= 0.0f);
        IInkStrokeDisp_Release(tmp);
    }
    OK(IInkStrokeDisp_HitTestCircle(s1, 200, 110, 20.0f, &vb));
    CHECK(vb == VARIANT_TRUE);
    OK(IInkStrokeDisp_HitTestCircle(s1, 200, 500, 20.0f, &vb));
    CHECK(vb == VARIANT_FALSE);

    /* rectangle hit test */
    {
        IInkRectangle *r2 = NULL;
        OK(IInkDisp_GetBoundingBox(ink, IBBM_Default, &r2));
        IInkRectangle_SetRectangle(r2, 0, 0, 200, 250);
        OK(IInkDisp_HitTestWithRectangle(ink, r2, 50.0f, &hits));
        OK(IInkStrokes_get_Count(hits, &count));
        CHECK(count == 2);       /* s1 is 75% inside, s3 entirely */
        IInkStrokes_Release(hits);
        OK(IInkDisp_HitTestWithRectangle(ink, r2, 90.0f, &hits));
        OK(IInkStrokes_get_Count(hits, &count));
        CHECK(count == 1);
        IInkStrokes_Release(hits);
        IInkRectangle_Release(r2);
    }
    {
        LONG poly[] = { 900, 900, 1300, 900, 1300, 1200, 900, 1200 };
        VARIANT lasso = longs(poly, 8), out;
        VariantInit(&out);
        OK(IInkDisp_HitTestWithLasso(ink, lasso, 100.0f, &out, &hits));
        OK(IInkStrokes_get_Count(hits, &count));
        CHECK(count == 1);
        IInkStrokes_Release(hits);
        VariantClear(&lasso);
    }

    /* transforms */
    OK(IInkStrokeDisp_Move(s2, 10.0f, -10.0f));
    OK(IInkStrokeDisp_GetPoints(s2, 0, 1, &var));
    {
        LONG *p;
        SafeArrayAccessData(V_ARRAY(&var), (void **)&p);
        CHECK(p[0] == 1010 && p[1] == 990);
        SafeArrayUnaccessData(V_ARRAY(&var));
    }
    VariantClear(&var);
    OK(IInkStrokeDisp_ScaleTransform(s2, 2.0f, 2.0f));
    OK(IInkStrokeDisp_GetPoints(s2, 0, 1, &var));
    {
        LONG *p;
        SafeArrayAccessData(V_ARRAY(&var), (void **)&p);
        CHECK(p[0] == 2020 && p[1] == 1980);
        SafeArrayUnaccessData(V_ARRAY(&var));
    }
    VariantClear(&var);
    OK(IInkStrokeDisp_Rotate(s3, 90.0f, 150.0f, 100.0f));
    OK(IInkStrokeDisp_GetPoints(s3, 0, 1, &var));
    {
        LONG *p;
        SafeArrayAccessData(V_ARRAY(&var), (void **)&p);
        CHECK(p[0] >= 195 && p[0] <= 205 && p[1] >= 95 && p[1] <= 105);
        SafeArrayUnaccessData(V_ARRAY(&var));
    }
    VariantClear(&var);
    OK(IInkStrokeDisp_Move(s3, -50.0f, 0.0f));

    /* drawing attributes + extended properties */
    OK(IInkStrokeDisp_get_DrawingAttributes(s1, &attrs));
    OK(IInkDrawingAttributes_put_Color(attrs, 0x00ff00));
    OK(IInkDrawingAttributes_put_Width(attrs, 100.0f));
    OK(IInkDrawingAttributes_get_Color(attrs, &v));
    CHECK(v == 0x00ff00);
    CHECK(FAILED(IInkDrawingAttributes_put_Transparency(attrs, 300)));
    IInkDrawingAttributes_Release(attrs);
    OK(IInkStrokeDisp_get_ExtendedProperties(s1, &props));
    V_VT(&data) = VT_I4;
    V_I4(&data) = 42;
    bstr = SysAllocString(L"{12345678-1234-1234-1234-123456789ABC}");
    OK(IInkExtendedProperties_Add(props, bstr, data, &prop));
    IInkExtendedProperty_Release(prop);
    OK(props->lpVtbl->DoesPropertyExist(props, bstr, &vb));
    CHECK(vb == VARIANT_TRUE);
    OK(IInkExtendedProperties_get_Count(props, &count));
    CHECK(count == 1);
    V_VT(&var) = VT_BSTR;
    V_BSTR(&var) = bstr;
    OK(props->lpVtbl->Item(props, var, &prop));
    VariantInit(&data);
    OK(IInkExtendedProperty_get_Data(prop, &data));
    CHECK(V_VT(&data) == VT_I4 && V_I4(&data) == 42);
    IInkExtendedProperty_Release(prop);
    OK(IInkExtendedProperties_Remove(props, var));
    OK(IInkExtendedProperties_get_Count(props, &count));
    CHECK(count == 0);
    SysFreeString(bstr);
    IInkExtendedProperties_Release(props);

    /* save / load / clone */
    {
        VARIANT saved, b64;
        OK(IInkDisp_Save(ink, IPF_InkSerializedFormat, 0, &saved));
        CHECK(V_VT(&saved) == (VT_ARRAY | VT_UI1));
        OK(IClassFactory_CreateInstance(cf, NULL, &IID_IInkDisp, (void **)&ink2));
        OK(IInkDisp_Load(ink2, saved));
        OK(IInkDisp_get_Strokes(ink2, &strokes));
        OK(IInkStrokes_get_Count(strokes, &count));
        CHECK(count == 3);
        IInkStrokes_Release(strokes);
        OK(IInkDisp_get_Dirty(ink2, &vb));
        CHECK(vb == VARIANT_FALSE);
        OK(IInkDisp_Save(ink, IPF_Base64InkSerializedFormat, 0, &b64));
        IInkDisp_Release(ink2);
        OK(IClassFactory_CreateInstance(cf, NULL, &IID_IInkDisp, (void **)&ink2));
        OK(IInkDisp_Load(ink2, b64));
        OK(IInkDisp_get_Strokes(ink2, &strokes));
        OK(IInkStrokes_get_Count(strokes, &count));
        CHECK(count == 3);
        IInkStrokes_Release(strokes);
        IInkDisp_Release(ink2);
        VariantClear(&saved);
        VariantClear(&b64);
        {
            SAFEARRAY *sa = SafeArrayCreateVector(VT_UI1, 0, 5);
            VARIANT junk;
            VariantInit(&junk);
            V_VT(&junk) = VT_ARRAY | VT_UI1;
            V_ARRAY(&junk) = sa;
            OK(IClassFactory_CreateInstance(cf, NULL, &IID_IInkDisp, (void **)&ink2));
            CHECK(FAILED(IInkDisp_Load(ink2, junk)));
            IInkDisp_Release(ink2);
            SafeArrayDestroy(sa);
        }
        CHECK(IInkDisp_Save(ink, IPF_GIF, 0, &saved) == E_NOTIMPL);
    }
    OK(IInkDisp_Clone(ink, &ink2));
    OK(IInkDisp_get_Strokes(ink2, &strokes));
    OK(IInkStrokes_get_Count(strokes, &count));
    CHECK(count == 3);
    IInkStrokes_Release(strokes);
    IInkDisp_Release(ink2);

    /* extract with rectangle: s1 crosses x=200 */
    {
        IInkRectangle *r2 = NULL;
        OK(IInkDisp_GetBoundingBox(ink, IBBM_Default, &r2));
        IInkRectangle_SetRectangle(r2, 0, 0, 200, 190);
        OK(IInkDisp_ExtractWithRectangle(ink, r2, IEF_CopyFromOriginal, &ink2));
        OK(IInkDisp_get_Strokes(ink2, &strokes));
        OK(IInkStrokes_get_Count(strokes, &count));
        CHECK(count >= 1);
        IInkStrokes_Release(strokes);
        IInkDisp_Release(ink2);
        IInkRectangle_Release(r2);
    }

    /* clipboard copy / paste (ExtractOnly keeps the real clipboard alone) */
    {
        IDataObject *dobj = NULL;
        IInkStrokes *sel = NULL;
        VARIANT ids;
        VariantInit(&ids);
        OK(IInkDisp_CreateStrokes(ink, ids, &sel));
        OK(IInkStrokes_get_Count(sel, &count));
        CHECK(count == 3);
        OK(IInkDisp_ClipboardCopy(ink, sel, ICF_Default, ICB_ExtractOnly, &dobj));
        CHECK(dobj != NULL);
        OK(ink->lpVtbl->CanPaste(ink, dobj, &vb));
        CHECK(vb == VARIANT_TRUE);
        OK(IInkDisp_get_Strokes(ink, &strokes));
        OK(IInkStrokes_get_Count(strokes, &count));
        CHECK(count == 0);        /* extract-only removes them */
        IInkStrokes_Release(strokes);
        OK(IInkDisp_ClipboardPaste(ink, 500, 500, dobj, &hits));
        OK(IInkStrokes_get_Count(hits, &count));
        CHECK(count == 3);
        IInkStrokes_Release(hits);
        OK(IInkDisp_GetBoundingBox(ink, IBBM_PointsOnly, &rect));
        CHECK(rect_get(rect, 1) == 500 && rect_get(rect, 0) == 500);
        IInkRectangle_Release(rect);
        IDataObject_Release(dobj);
        IInkStrokes_Release(sel);
    }

    /* delete */
    OK(IInkDisp_get_Strokes(ink, &strokes));
    OK(IInkStrokes_Item(strokes, 0, &tmp));
    OK(IInkDisp_DeleteStroke(ink, tmp));
    OK(IInkStrokeDisp_get_Deleted(tmp, &vb));
    CHECK(vb == VARIANT_TRUE);
    CHECK(FAILED(IInkDisp_DeleteStroke(ink, tmp)));
    IInkStrokeDisp_Release(tmp);
    IInkStrokes_Release(strokes);
    OK(IInkDisp_DeleteStrokes(ink, NULL));
    OK(IInkDisp_get_Strokes(ink, &strokes));
    OK(IInkStrokes_get_Count(strokes, &count));
    CHECK(count == 0);
    IInkStrokes_Release(strokes);

    IInkStrokeDisp_Release(s1);
    IInkStrokeDisp_Release(s2);
    IInkStrokeDisp_Release(s3);
    IInkDisp_Release(ink);
    IClassFactory_Release(cf);
    printf("%d checks, %d failures\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
