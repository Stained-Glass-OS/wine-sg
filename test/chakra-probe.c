/* chakra-gate.sh's probe (0508): Windows' JavaScript hosting API (chakra.dll),
 * used the way React Native for Windows' Chakra runtime uses it. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef void *JsValueRef, *JsRuntimeHandle, *JsContextRef, *JsPropertyIdRef;
typedef unsigned int JsErrorCode;
typedef JsValueRef (CALLBACK *JsNativeFunction)(JsValueRef, BOOLEAN, JsValueRef *, unsigned short, void *);

static HMODULE chakra;
#define F(name) static JsErrorCode (WINAPI *p##name)
F(JsCreateRuntime)(DWORD, void *, JsRuntimeHandle *);
F(JsCreateContext)(JsRuntimeHandle, JsContextRef *);
F(JsSetCurrentContext)(JsContextRef);
F(JsRunScript)(const WCHAR *, DWORD_PTR, const WCHAR *, JsValueRef *);
F(JsGetGlobalObject)(JsValueRef *);
F(JsGetPropertyIdFromName)(const WCHAR *, JsPropertyIdRef *);
F(JsGetProperty)(JsValueRef, JsPropertyIdRef, JsValueRef *);
F(JsSetProperty)(JsValueRef, JsPropertyIdRef, JsValueRef, BOOLEAN);
F(JsNumberToDouble)(JsValueRef, double *);
F(JsDoubleToNumber)(double, JsValueRef *);
F(JsPointerToString)(const WCHAR *, size_t, JsValueRef *);
F(JsStringToPointer)(JsValueRef, const WCHAR **, size_t *);
F(JsCreateNamedFunction)(JsValueRef, JsNativeFunction, void *, JsValueRef *);
F(JsCallFunction)(JsValueRef, JsValueRef *, unsigned short, JsValueRef *);
F(JsConstructObject)(JsValueRef, JsValueRef *, unsigned short, JsValueRef *);
F(JsGetAndClearException)(JsValueRef *);
F(JsSetException)(JsValueRef);
F(JsCreateError)(JsValueRef, JsValueRef *);
F(JsCreateExternalObject)(void *, void (CALLBACK *)(void *), JsValueRef *);
F(JsGetExternalData)(JsValueRef, void **);
F(JsGetValueType)(JsValueRef, int *);
F(JsCreateArray)(unsigned int, JsValueRef *);
F(JsSetIndexedProperty)(JsValueRef, JsValueRef, JsValueRef);
F(JsGetIndexedProperty)(JsValueRef, JsValueRef, JsValueRef *);
F(JsIntToNumber)(int, JsValueRef *);
F(JsNumberToInt)(JsValueRef, int *);
F(JsGetArrayBufferStorage)(JsValueRef, BYTE **, unsigned int *);
F(JsSerializeScript)(const WCHAR *, BYTE *, ULONG *);
F(JsRunSerializedScript)(const WCHAR *, BYTE *, DWORD_PTR, const WCHAR *, JsValueRef *);
F(JsSetPromiseContinuationCallback)(void (CALLBACK *)(JsValueRef, void *), void *);
F(JsGetUndefinedValue)(JsValueRef *);
F(JsCreateSymbol)(JsValueRef, JsValueRef *);
F(JsGetPropertyIdFromSymbol)(JsValueRef, JsPropertyIdRef *);
F(JsGetPropertyIdType)(JsPropertyIdRef, int *);
F(JsGetPropertyNameFromId)(JsPropertyIdRef, const WCHAR **);
F(JsInstanceOf)(JsValueRef, JsValueRef, BOOLEAN *);
F(JsStrictEquals)(JsValueRef, JsValueRef, BOOLEAN *);
F(JsGetOwnPropertyNames)(JsValueRef, JsValueRef *);
F(JsDefineProperty)(JsValueRef, JsPropertyIdRef, JsValueRef, BOOLEAN *);
F(JsHasProperty)(JsValueRef, JsPropertyIdRef, BOOLEAN *);
F(JsCreateObject)(JsValueRef *);
F(JsBoolToBoolean)(BOOLEAN, JsValueRef *);
F(JsAddRef)(void *, unsigned int *);
F(JsRelease)(void *, unsigned int *);
F(JsGetPrototype)(JsValueRef, JsValueRef *);
F(JsDisposeRuntime)(JsRuntimeHandle);
#define LOAD(name) p##name = (void *)GetProcAddress(chakra, #name); if (!p##name) { printf("missing %s\n", #name); return 1; }

static JsValueRef global;

static JsValueRef prop(JsValueRef obj, const WCHAR *name)
{
    JsPropertyIdRef id;
    JsValueRef v = NULL;
    pJsGetPropertyIdFromName(name, &id);
    pJsGetProperty(obj, id, &v);
    return v;
}

static void set(JsValueRef obj, const WCHAR *name, JsValueRef v)
{
    JsPropertyIdRef id;
    pJsGetPropertyIdFromName(name, &id);
    pJsSetProperty(obj, id, v, TRUE);
}

static double num(JsValueRef v)
{
    double d = -1;
    pJsNumberToDouble(v, &d);
    return d;
}

static const char *str(JsValueRef v)
{
    static char buf[4][512];
    static int n;
    char *b = buf[n++ & 3];
    const WCHAR *s = NULL;
    size_t len = 0;
    b[0] = 0;
    if (!pJsStringToPointer(v, &s, &len))
    {
        int n = WideCharToMultiByte(CP_UTF8, 0, s, (int)len, b, 511, NULL, NULL);
        b[n] = 0;
    }
    return b;
}

static JsValueRef run(const WCHAR *script, JsErrorCode *err)
{
    JsValueRef r = NULL;
    *err = pJsRunScript(script, 0, L"probe.js", &r);
    return r;
}

/* native: add(a, b) */
static JsValueRef CALLBACK native_add(JsValueRef callee, BOOLEAN construct, JsValueRef *args, unsigned short count, void *state)
{
    JsValueRef r;
    pJsDoubleToNumber(num(args[1]) + num(args[2]) + (double)(INT_PTR)state, &r);
    return r;
}

/* native: fail(msg) -- throws an Error into the script */
static JsValueRef CALLBACK native_fail(JsValueRef callee, BOOLEAN construct, JsValueRef *args, unsigned short count, void *state)
{
    JsValueRef e;
    pJsCreateError(args[1], &e);
    pJsSetException(e);
    return NULL;
}

/* native constructor: new Point(x) sets this.x */
static JsValueRef CALLBACK native_point(JsValueRef callee, BOOLEAN construct, JsValueRef *args, unsigned short count, void *state)
{
    if (construct) set(args[0], L"x", args[1]);
    return construct ? args[0] : NULL;
}

static int finalized;
static void CALLBACK finalize(void *data) { finalized++; }

static int tasks_seen;
static JsValueRef pending_task;
static void CALLBACK continuation(JsValueRef task, void *state)
{
    tasks_seen++;
    pending_task = task;
    pJsAddRef(task, NULL);
}

int main(void)
{
    JsRuntimeHandle rt;
    JsContextRef ctx;
    JsErrorCode err;
    JsValueRef v, fn, name, e, arr, idx, obj;
    int type;

    if (!(chakra = LoadLibraryA("chakra.dll"))) { printf("no chakra.dll\n"); return 1; }
    LOAD(JsCreateRuntime) LOAD(JsCreateContext) LOAD(JsSetCurrentContext) LOAD(JsRunScript) LOAD(JsGetGlobalObject)
    LOAD(JsGetPropertyIdFromName) LOAD(JsGetProperty) LOAD(JsSetProperty) LOAD(JsNumberToDouble) LOAD(JsDoubleToNumber)
    LOAD(JsPointerToString) LOAD(JsStringToPointer) LOAD(JsCreateNamedFunction) LOAD(JsCallFunction) LOAD(JsConstructObject)
    LOAD(JsGetAndClearException) LOAD(JsSetException) LOAD(JsCreateError) LOAD(JsCreateExternalObject) LOAD(JsGetExternalData)
    LOAD(JsGetValueType) LOAD(JsCreateArray) LOAD(JsSetIndexedProperty) LOAD(JsGetIndexedProperty) LOAD(JsIntToNumber)
    LOAD(JsNumberToInt) LOAD(JsGetArrayBufferStorage) LOAD(JsSerializeScript) LOAD(JsRunSerializedScript)
    LOAD(JsSetPromiseContinuationCallback) LOAD(JsGetUndefinedValue) LOAD(JsCreateSymbol) LOAD(JsGetPropertyIdFromSymbol)
    LOAD(JsGetPropertyIdType) LOAD(JsGetPropertyNameFromId) LOAD(JsInstanceOf) LOAD(JsStrictEquals) LOAD(JsGetOwnPropertyNames)
    LOAD(JsDefineProperty) LOAD(JsHasProperty) LOAD(JsCreateObject) LOAD(JsBoolToBoolean) LOAD(JsAddRef) LOAD(JsRelease)
    LOAD(JsGetPrototype) LOAD(JsDisposeRuntime)

    printf("create %u", pJsCreateRuntime(0, NULL, &rt));
    printf(" %u", pJsCreateContext(rt, &ctx));
    printf(" %u\n", pJsSetCurrentContext(ctx));
    pJsGetGlobalObject(&global);
    pJsSetPromiseContinuationCallback(continuation, NULL);

    /* modern JavaScript: classes, arrow functions, destructuring, template strings, optional chaining */
    v = run(L"class A { #n = 20; get n() { return this.#n; } } const {n} = new A(); const o = {p: {q: 2}}; "
            L"[1, 2, 3].map(x => x * n).reduce((a, b) => a + b, 0) + (o?.p?.q ?? 0) + `${n}`.length", &err);
    printf("modern %u %g\n", err, num(v));

    /* strings, both ways, beyond ASCII */
    pJsPointerToString(L"héllo 世界", 8, &v);
    set(global, L"greeting", v);
    v = run(L"greeting.length + ':' + greeting.toUpperCase()", &err);
    printf("string %u %s\n", err, str(v));

    /* a native function, called from script with state */
    pJsPointerToString(L"add", 3, &name);
    pJsCreateNamedFunction(name, native_add, (void *)(INT_PTR)100, &fn);
    set(global, L"add", fn);
    v = run(L"add(2, 3) + ':' + add.name + ':' + (typeof add)", &err);
    printf("native %u %s\n", err, str(v));

    /* a script's exception reaches the host */
    run(L"throw new TypeError('bad thing')", &err);
    e = NULL;
    printf("scriptthrow %u %u", err, pJsGetAndClearException(&e));
    printf(" %s\n", str(prop(e, L"message")));

    /* a compile error */
    run(L"var = ;", &err);
    pJsGetAndClearException(&e);
    printf("compile %u\n", err);

    /* the host throws into the script, which catches it */
    pJsPointerToString(L"fail", 4, &name);
    pJsCreateNamedFunction(name, native_fail, NULL, &fn);
    set(global, L"fail", fn);
    v = run(L"let r; try { fail('from host'); r = 'no'; } catch (x) { r = 'caught ' + x.message + ' ' + (x instanceof Error); } r", &err);
    printf("hostthrow %u %s\n", err, str(v));

    /* a native constructor */
    pJsPointerToString(L"Point", 5, &name);
    pJsCreateNamedFunction(name, native_point, NULL, &fn);
    set(global, L"Point", fn);
    v = run(L"const pt = new Point(7); pt.x + ':' + (pt instanceof Point)", &err);
    printf("construct %u %s\n", err, str(v));
    {
        JsValueRef args[2], made;
        BOOLEAN isinst = 0;
        pJsGetUndefinedValue(&args[0]);
        pJsIntToNumber(9, &args[1]);
        pJsConstructObject(fn, args, 2, &made);
        pJsInstanceOf(made, fn, &isinst);
        printf("constructhost %g %d\n", num(prop(made, L"x")), isinst);
    }

    /* the host calls a script function with 'this' and arguments */
    run(L"function mul(a, b) { return this.k * a * b; }", &err);
    {
        JsValueRef args[3], self, r = NULL, k;
        pJsCreateObject(&self);
        pJsIntToNumber(5, &k);
        set(self, L"k", k);
        args[0] = self;
        pJsIntToNumber(3, &args[1]);
        pJsIntToNumber(4, &args[2]);
        err = pJsCallFunction(prop(global, L"mul"), args, 3, &r);
        printf("call %u %g\n", err, num(r));
    }

    /* an external object keeps the host's data */
    pJsCreateExternalObject((void *)0x1234, finalize, &obj);
    {
        void *data = NULL;
        pJsGetExternalData(obj, &data);
        pJsGetValueType(obj, &type);
        printf("external %p %d\n", data, type);
    }

    /* arrays and indexed properties */
    pJsCreateArray(0, &arr);
    pJsIntToNumber(2, &idx);
    pJsPointerToString(L"two", 3, &v);
    pJsSetIndexedProperty(arr, idx, v);
    pJsGetIndexedProperty(arr, idx, &v);
    pJsGetValueType(arr, &type);
    printf("array %s %g %d\n", str(v), num(prop(arr, L"length")), type);

    /* an ArrayBuffer's bytes */
    v = run(L"const ab = new ArrayBuffer(4); new Uint8Array(ab).set([1, 2, 3, 4]); ab", &err);
    {
        BYTE *bytes = NULL;
        unsigned int len = 0;
        pJsGetArrayBufferStorage(v, &bytes, &len);
        printf("arraybuffer %u %u %d%d%d%d\n", err, len, bytes ? bytes[0] : 0, bytes ? bytes[1] : 0, bytes ? bytes[2] : 0, bytes ? bytes[3] : 0);
    }

    /* symbols as property ids */
    {
        JsValueRef sym, val, got;
        JsPropertyIdRef sid, nid;
        const WCHAR *pname = NULL;
        int ptype = -1;
        pJsPointerToString(L"tag", 3, &name);
        pJsCreateSymbol(name, &sym);
        pJsGetPropertyIdFromSymbol(sym, &sid);
        pJsGetPropertyIdType(sid, &ptype);
        pJsIntToNumber(42, &val);
        pJsSetProperty(global, sid, val, TRUE);
        pJsGetProperty(global, sid, &got);
        pJsGetPropertyIdFromName(L"greeting", &nid);
        pJsGetPropertyNameFromId(nid, &pname);
        printf("symbol %d %g %ls\n", ptype, num(got), pname);
    }

    /* defineProperty with a getter; own property names; strict equality; prototype */
    {
        JsValueRef desc, target, names, a, b, proto;
        JsPropertyIdRef id;
        BOOLEAN ok = 0, eq = 0, has = 0;
        desc = run(L"({ get: function () { return 'got' }, enumerable: true, configurable: true })", &err);
        pJsCreateObject(&target);
        pJsGetPropertyIdFromName(L"thing", &id);
        pJsDefineProperty(target, id, desc, &ok);
        pJsHasProperty(target, id, &has);
        pJsGetOwnPropertyNames(target, &names);
        a = prop(target, L"thing");
        pJsPointerToString(L"got", 3, &b);
        pJsStrictEquals(a, b, &eq);
        pJsGetPrototype(target, &proto);
        printf("define %d %d %s %g %d %d\n", ok, has, str(a), num(prop(names, L"length")), eq,
               proto == prop(prop(global, L"Object"), L"prototype"));
    }

    /* serialized bytecode runs, and the same object comes back as the same handle */
    {
        static const WCHAR *src = L"(function () { return 6 * 7; })()";
        ULONG size = 0;
        BYTE *buf;
        JsValueRef r = NULL, bad = NULL;
        BYTE junk[64] = {0};
        pJsSerializeScript(src, NULL, &size);
        buf = malloc(size);
        pJsSerializeScript(src, buf, &size);
        err = pJsRunSerializedScript(src, buf, 0, L"s.js", &r);
        printf("serialized %u %lu %g", err, size > 16, num(r));
        err = pJsRunSerializedScript(src, junk, 0, L"s.js", &bad);
        printf(" %u %g\n", err, num(bad));
        printf("samehandle %d\n", prop(global, L"Object") == prop(global, L"Object"));
    }

    /* promise jobs: the host is given a task, and running it runs them */
    run(L"var settled = 'no'; Promise.resolve(5).then(x => { settled = 'yes ' + x; }); "
        L"(async () => { await null; settled += ' async'; })();", &err);
    printf("promise %u %d %s", err, tasks_seen, str(prop(global, L"settled")));
    if (pending_task)
    {
        JsValueRef args[1], r;
        pJsGetUndefinedValue(&args[0]);
        pJsCallFunction(pending_task, args, 1, &r);
    }
    printf(" %s\n", str(prop(global, L"settled")));

    /* refcounts */
    {
        unsigned int n1 = 0, n2 = 0;
        pJsAddRef(obj, &n1);
        pJsRelease(obj, &n2);
        printf("refs %u %u\n", n1, n2);
    }

    pJsSetCurrentContext(NULL);
    err = pJsDisposeRuntime(rt);
    printf("dispose %u %d\n", err, finalized);
    return 0;
}
