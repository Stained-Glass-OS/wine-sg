/* fastmapi-probe: mapi32 notification sinks, utility initialization, property object copies, named properties,
 * streams over properties, and compressed RTF */
#define COBJMACROS
#include <windows.h>
#include <objbase.h>
#include <mapix.h>
#include <mapiutil.h>
#include <mapidefs.h>
#include <mapicode.h>
#include <mapitags.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MAPI_MOVE
#error need the MAPI flags
#endif


static const GUID my_IID_IMAPIPropData = { 0x0002031a, 0, 0, { 0xc0, 0, 0, 0, 0, 0, 0, 0x46 } };
static const GUID my_IID_IMAPIProp = { 0x00020303, 0, 0, { 0xc0, 0, 0, 0, 0, 0, 0, 0x46 } };
static const GUID my_IID_IMAPIAdviseSink = { 0x00020302, 0, 0, { 0xc0, 0, 0, 0, 0, 0, 0, 0x46 } };
#define IID_IMAPIPropData my_IID_IMAPIPropData
#define IID_IMAPIProp my_IID_IMAPIProp
#define IID_IMAPIAdviseSink my_IID_IMAPIAdviseSink
#define IMAPIAdviseSink_OnNotify(p,a,b) (p)->lpVtbl->OnNotify(p,a,b)
#define IMAPIAdviseSink_Release(p) (p)->lpVtbl->Release(p)
#define IPropData_Release(p) (p)->lpVtbl->Release(p)
#define IPropData_SetProps(p,a,b,c) (p)->lpVtbl->SetProps(p,a,b,c)
#define IPropData_GetProps(p,a,b,c,d) (p)->lpVtbl->GetProps(p,a,b,c,d)
#define IPropData_GetPropList(p,a,b) (p)->lpVtbl->GetPropList(p,a,b)
#define IPropData_OpenProperty(p,a,b,c,d,e) (p)->lpVtbl->OpenProperty(p,a,b,c,d,e)
#define IPropData_CopyTo(p,a,b,c,d,e,f,g,h,i) (p)->lpVtbl->CopyTo(p,a,b,c,d,e,f,g,h,i)
#define IPropData_CopyProps(p,a,b,c,d,e,f,g) (p)->lpVtbl->CopyProps(p,a,b,c,d,e,f,g)
#define IPropData_GetNamesFromIDs(p,a,b,c,d,e) (p)->lpVtbl->GetNamesFromIDs(p,a,b,c,d,e)
#define IPropData_GetIDsFromNames(p,a,b,c,d) (p)->lpVtbl->GetIDsFromNames(p,a,b,c,d)
#define IPropData_HrSetObjAccess(p,a) (p)->lpVtbl->HrSetObjAccess(p,a)
#define IPropData_HrSetPropAccess(p,a,b) (p)->lpVtbl->HrSetPropAccess(p,a,b)
#define IPropData_HrAddObjProps(p,a,b) (p)->lpVtbl->HrAddObjProps(p,a,b)

typedef struct { ULONG cValues; ULONG aulPropTag[4]; } TAGS4;
static int checks, fails;
#define CHECK(cond, ...) do { checks++; if (!(cond)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static SCODE (WINAPI *pScInitMapiUtil)(ULONG);
static void (WINAPI *pDeinitMapiUtil)(void);
static HRESULT (WINAPI *pHrDispatchNotifications)(ULONG);
static HRESULT (WINAPI *pHrThisThreadAdviseSink)(LPMAPIADVISESINK, LPMAPIADVISESINK *);
static SCODE (WINAPI *pCreateIProp)(LPCIID, ALLOCATEBUFFER *, ALLOCATEMORE *, FREEBUFFER *, LPVOID, LPPROPDATA *);
static SCODE (WINAPI *pMAPIAllocateBuffer)(ULONG, LPVOID *);
static SCODE (WINAPI *pMAPIAllocateMore)(ULONG, LPVOID, LPVOID *);
static ULONG (WINAPI *pMAPIFreeBuffer)(LPVOID);
static HRESULT (WINAPI *pWrapCompressedRTFStream)(LPSTREAM, ULONG, LPSTREAM *);

/* ------------------------------------------------------------------ utility init */
static void test_init(void)
{
    HRESULT hr;
    MSG msg;

    hr = pHrDispatchNotifications(0);
    CHECK(hr == MAPI_E_NOT_INITIALIZED, "dispatch before init %08lx", hr);
    hr = pScInitMapiUtil(1);
    CHECK(hr == MAPI_E_INVALID_PARAMETER, "init reserved %08lx", hr);
    hr = pHrDispatchNotifications(0);
    CHECK(hr == MAPI_E_NOT_INITIALIZED, "a failed init does not count %08lx", hr);
    hr = pScInitMapiUtil(0);
    CHECK(hr == S_OK, "init %08lx", hr);
    hr = pHrDispatchNotifications(0);
    CHECK(hr == S_OK, "dispatch %08lx", hr);
    hr = pHrDispatchNotifications(1);
    CHECK(hr == MAPI_E_INVALID_PARAMETER, "dispatch flags %08lx", hr);
    pScInitMapiUtil(0);
    pDeinitMapiUtil();
    hr = pHrDispatchNotifications(0);
    CHECK(hr == S_OK, "still initialized after one deinit %08lx", hr);

    PostQuitMessage(0);
    hr = pHrDispatchNotifications(0);
    CHECK(hr == MAPI_E_USER_CANCEL, "quit pending %08lx", hr);
    PeekMessageW(&msg, NULL, WM_QUIT, WM_QUIT, PM_REMOVE);
    hr = pHrDispatchNotifications(0);
    CHECK(hr == S_OK, "quit consumed %08lx", hr);

    pDeinitMapiUtil();
    hr = pHrDispatchNotifications(0);
    CHECK(hr == MAPI_E_NOT_INITIALIZED, "deinit %08lx", hr);
    pDeinitMapiUtil();
    pDeinitMapiUtil();
    pScInitMapiUtil(0);
    hr = pHrDispatchNotifications(0);
    CHECK(hr == S_OK, "extra deinit calls do not go below zero %08lx", hr);
}

/* ------------------------------------------------------------------ advise sinks */
struct sink
{
    IMAPIAdviseSink iface;
    LONG ref;
    DWORD notify_thread;
    int calls;
    ULONG last_count;
    ULONG event;
    char text[100];
    BYTE entry[20];
    ULONG entry_cb;
    const void *entry_ptr;
    ULONG values;
    ULONG tag;
    const void *input_ptr;
};

static struct sink *sink_from(IMAPIAdviseSink *i) { return (struct sink *)i; }

static HRESULT WINAPI s_QI(IMAPIAdviseSink *i, REFIID riid, void **o)
{
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IMAPIAdviseSink)) { *o = i; return S_OK; }
    *o = NULL;
    return E_NOINTERFACE;
}
static ULONG WINAPI s_AddRef(IMAPIAdviseSink *i) { return InterlockedIncrement(&sink_from(i)->ref); }
static ULONG WINAPI s_Release(IMAPIAdviseSink *i) { return InterlockedDecrement(&sink_from(i)->ref); }

static ULONG WINAPI s_OnNotify(IMAPIAdviseSink *i, ULONG count, LPNOTIFICATION n)
{
    struct sink *s = sink_from(i);

    s->calls++;
    s->notify_thread = GetCurrentThreadId();
    s->last_count = count;
    s->event = n[0].ulEventType;
    s->entry_cb = 0;
    s->values = 0;
    s->text[0] = 0;
    switch (n[0].ulEventType)
    {
    case fnevNewMail:
        s->entry_cb = n[0].info.newmail.cbEntryID;
        s->entry_ptr = n[0].info.newmail.lpEntryID;
        if (s->entry_cb <= sizeof(s->entry)) memcpy(s->entry, n[0].info.newmail.lpEntryID, s->entry_cb);
        lstrcpynA(s->text, (char *)n[0].info.newmail.lpszMessageClass, sizeof(s->text));
        break;
    case fnevCriticalError:
        s->entry_cb = n[0].info.err.cbEntryID;
        if (s->entry_cb <= sizeof(s->entry)) memcpy(s->entry, n[0].info.err.lpEntryID, s->entry_cb);
        if (n[0].info.err.lpMAPIError)
            lstrcpynA(s->text, (char *)n[0].info.err.lpMAPIError->lpszError, sizeof(s->text));
        s->values = n[0].info.err.scode;
        break;
    case fnevObjectCreated:
        s->entry_cb = n[0].info.obj.cbEntryID;
        if (s->entry_cb <= sizeof(s->entry)) memcpy(s->entry, n[0].info.obj.lpEntryID, s->entry_cb);
        s->values = n[0].info.obj.lpPropTagArray ? n[0].info.obj.lpPropTagArray->cValues : 99;
        s->tag = n[0].info.obj.lpPropTagArray ? n[0].info.obj.lpPropTagArray->aulPropTag[0] : 0;
        break;
    case fnevTableModified:
        s->values = n[0].info.tab.row.cValues;
        s->tag = n[0].info.tab.row.cValues ? n[0].info.tab.row.lpProps[0].ulPropTag : 0;
        if (n[0].info.tab.row.cValues > 1 && PROP_TYPE(n[0].info.tab.row.lpProps[1].ulPropTag) == PT_STRING8)
            lstrcpynA(s->text, n[0].info.tab.row.lpProps[1].Value.lpszA, sizeof(s->text));
        break;
    case fnevStatusObjectModified:
        s->entry_cb = n[0].info.statobj.cbEntryID;
        if (s->entry_cb <= sizeof(s->entry)) memcpy(s->entry, n[0].info.statobj.lpEntryID, s->entry_cb);
        s->values = n[0].info.statobj.cValues;
        if (s->values) lstrcpynA(s->text, n[0].info.statobj.lpPropVals[0].Value.lpszA, sizeof(s->text));
        break;
    case fnevExtended:
        s->entry_cb = n[0].info.ext.cb;
        if (n[0].info.ext.cb <= sizeof(s->entry)) memcpy(s->entry, n[0].info.ext.pbEventParameters, n[0].info.ext.cb);
        break;
    }
    return 0;
}

static const IMAPIAdviseSinkVtbl sink_vtbl = { s_QI, s_AddRef, s_Release, s_OnNotify };

struct thread_job { IMAPIAdviseSink *sink; NOTIFICATION notif; int sync; };

static DWORD WINAPI notify_thread(void *arg)
{
    struct thread_job *job = arg;
    IMAPIAdviseSink_OnNotify(job->sink, 1, &job->notif);
    return 0;
}

static void *poison_block;
static ULONG poison_size;

static void send_from_thread(IMAPIAdviseSink *sink, NOTIFICATION *n)
{
    struct thread_job job;
    HANDLE th;

    job.sink = sink;
    job.notif = *n;
    th = CreateThread(NULL, 0, notify_thread, &job, 0, NULL);
    WaitForSingleObject(th, 5000);
    CloseHandle(th);
    /* what the sender had is gone when it returns */
    if (poison_block) memset(poison_block, 0xaa, poison_size);
}

static void test_sinks(void)
{
    struct sink target = { { &sink_vtbl }, 1 };
    IMAPIAdviseSink *wrapped = (IMAPIAdviseSink *)1;
    NOTIFICATION n;
    BYTE id[4] = { 1, 2, 3, 4 };
    char klass[] = "IPM.Note";
    HRESULT hr;
    MSG msg;
    LONG ref;

    hr = pHrThisThreadAdviseSink(NULL, &wrapped);
    CHECK(hr == E_INVALIDARG, "null sink %08lx", hr);
    hr = pHrThisThreadAdviseSink(&target.iface, NULL);
    CHECK(hr == E_INVALIDARG, "null output %08lx", hr);
    hr = pHrThisThreadAdviseSink(&target.iface, &wrapped);
    CHECK(hr == S_OK && wrapped && wrapped != &target.iface, "wrapped %08lx %p", hr, wrapped);
    CHECK(target.ref == 2, "the wrapper holds the sink: %ld", target.ref);

    /* on the thread of the wrapper: straight through */
    memset(&n, 0, sizeof(n));
    n.ulEventType = fnevNewMail;
    n.info.newmail.cbEntryID = 4;
    n.info.newmail.lpEntryID = (LPENTRYID)id;
    n.info.newmail.lpszMessageClass = klass;
    IMAPIAdviseSink_OnNotify(wrapped, 1, &n);
    CHECK(target.calls == 1 && target.notify_thread == GetCurrentThreadId(), "same thread %d", target.calls);
    CHECK(target.entry_ptr == (void *)id, "no copy on the same thread");

    /* from another thread: queued for this thread */
    target.calls = 0;
    send_from_thread(wrapped, &n);
    CHECK(target.calls == 0, "not delivered on the other thread");
    hr = pHrDispatchNotifications(0);
    CHECK(hr == S_OK && target.calls == 1, "delivered by dispatch %08lx calls %d", hr, target.calls);
    CHECK(target.notify_thread == GetCurrentThreadId(), "on this thread");
    CHECK(target.event == fnevNewMail && target.entry_cb == 4 && !memcmp(target.entry, id, 4), "entry id copied");
    CHECK(!strcmp(target.text, "IPM.Note") && target.entry_ptr != (void *)id, "message class copied, not shared");
    hr = pHrDispatchNotifications(0);
    CHECK(target.calls == 1, "delivered once");

    /* by the message loop */
    target.calls = 0;
    send_from_thread(wrapped, &n);
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageW(&msg);
    CHECK(target.calls == 1, "delivered by the message loop %d", target.calls);

    /* the other kinds of notifications are copied whole: the sender's data is gone when it returns */
    {
        struct blk { BYTE id[4]; char text[20]; char comp[8]; MAPIERROR err; TAGS4 tags; SPropValue props[2]; char subject[20];
                     SPropValue status[1]; char name[20]; BYTE ext[4]; } *blk = malloc(sizeof(*blk));

        memset(blk, 0, sizeof(*blk));
        memcpy(blk->id, id, 4);
        strcpy(blk->text, "disk failed");
        strcpy(blk->comp, "store");
        blk->err.lpszError = (char *)blk->text;
        blk->err.lpszComponent = (char *)blk->comp;
        memset(&n, 0, sizeof(n));
        n.ulEventType = fnevCriticalError;
        n.info.err.cbEntryID = 4;
        n.info.err.lpEntryID = (LPENTRYID)blk->id;
        n.info.err.scode = MAPI_E_DISK_ERROR;
        n.info.err.lpMAPIError = &blk->err;
        poison_block = blk; poison_size = sizeof(*blk);
        send_from_thread(wrapped, &n);
        pHrDispatchNotifications(0);
        CHECK(target.event == fnevCriticalError && target.entry_cb == 4 && !memcmp(target.entry, id, 4) &&
              !strcmp(target.text, "disk failed") && target.values == (ULONG)MAPI_E_DISK_ERROR, "error notification '%s'", target.text);

        memcpy(blk->id, id, 4);
        blk->tags.cValues = 2; blk->tags.aulPropTag[0] = PR_SUBJECT_A; blk->tags.aulPropTag[1] = PR_BODY_A;
        memset(&n, 0, sizeof(n));
        n.ulEventType = fnevObjectCreated;
        n.info.obj.cbEntryID = 4;
        n.info.obj.lpEntryID = (LPENTRYID)blk->id;
        n.info.obj.lpPropTagArray = (SPropTagArray *)&blk->tags;
        send_from_thread(wrapped, &n);
        pHrDispatchNotifications(0);
        CHECK(target.event == fnevObjectCreated && target.entry_cb == 4 && !memcmp(target.entry, id, 4) &&
              target.values == 2 && target.tag == PR_SUBJECT_A, "object notification %lu %lx", target.values, target.tag);

        strcpy(blk->subject, "a subject");
        blk->props[0].ulPropTag = PR_IMPORTANCE; blk->props[0].Value.l = 2;
        blk->props[1].ulPropTag = PR_SUBJECT_A; blk->props[1].Value.lpszA = blk->subject;
        memset(&n, 0, sizeof(n));
        n.ulEventType = fnevTableModified;
        n.info.tab.ulTableEvent = TABLE_ROW_ADDED;
        n.info.tab.row.cValues = 2;
        n.info.tab.row.lpProps = blk->props;
        send_from_thread(wrapped, &n);
        pHrDispatchNotifications(0);
        CHECK(target.event == fnevTableModified && target.values == 2 && target.tag == PR_IMPORTANCE &&
              !strcmp(target.text, "a subject"), "table notification '%s'", target.text);

        strcpy(blk->name, "status name");
        blk->status[0].ulPropTag = PR_DISPLAY_NAME_A; blk->status[0].Value.lpszA = blk->name;
        memcpy(blk->id, id, 4);
        memset(&n, 0, sizeof(n));
        n.ulEventType = fnevStatusObjectModified;
        n.info.statobj.cbEntryID = 4;
        n.info.statobj.lpEntryID = (LPENTRYID)blk->id;
        n.info.statobj.cValues = 1;
        n.info.statobj.lpPropVals = blk->status;
        send_from_thread(wrapped, &n);
        pHrDispatchNotifications(0);
        CHECK(target.event == fnevStatusObjectModified && target.values == 1 && target.entry_cb == 4 &&
              !memcmp(target.entry, id, 4) && !strcmp(target.text, "status name"), "status notification");

        memcpy(blk->ext, id, 4);
        memset(&n, 0, sizeof(n));
        n.ulEventType = fnevExtended;
        n.info.ext.ulEvent = 7;
        n.info.ext.cb = 4;
        n.info.ext.pbEventParameters = blk->ext;
        send_from_thread(wrapped, &n);
        pHrDispatchNotifications(0);
        CHECK(target.event == fnevExtended && target.entry_cb == 4 && !memcmp(target.entry, id, 4), "extended notification");
        poison_block = NULL;
        free(blk);
    }

    ref = IMAPIAdviseSink_Release(wrapped);
    CHECK(ref == 0 && target.ref == 1, "release the wrapper: %ld, sink %ld", ref, target.ref);
}

/* ------------------------------------------------------------------ property objects */
static IPropData *make_data(void)
{
    IPropData *d = NULL;
    HRESULT hr = pCreateIProp(&IID_IMAPIPropData, pMAPIAllocateBuffer, pMAPIAllocateMore, pMAPIFreeBuffer, NULL, &d);

    CHECK(hr == S_OK && d, "CreateIProp %08lx", hr);
    return d;
}

static void set_str(IPropData *d, ULONG tag, const char *s)
{
    SPropValue v;
    HRESULT hr;

    v.ulPropTag = tag; v.dwAlignPad = 0; v.Value.lpszA = (char *)s;
    hr = IPropData_SetProps(d, 1, &v, NULL);
    CHECK(hr == S_OK, "set %08lx", hr);
}

static char *get_str(IPropData *d, ULONG tag, ULONG *type)
{
    SPropTagArray t = { 1, { tag } };
    SPropValue *v = NULL;
    ULONG n = 0;
    static char buf[200];

    buf[0] = 0;
    if (type) *type = 0;
    if (SUCCEEDED(IPropData_GetProps(d, &t, 0, &n, &v)) && v)
    {
        if (type) *type = v[0].ulPropTag;
        if (PROP_TYPE(v[0].ulPropTag) == PT_STRING8) lstrcpynA(buf, v[0].Value.lpszA, sizeof(buf));
        else if (PROP_TYPE(v[0].ulPropTag) == PT_LONG) sprintf(buf, "%ld", v[0].Value.l);
        pMAPIFreeBuffer(v);
    }
    return buf;
}

static void test_copy(void)
{
    IPropData *src = make_data(), *dst = make_data(), *dst2;
    SPropTagArray none = { 0 }, excl = { 1, { PR_BODY_A } }, incl = { 1, { PR_SUBJECT_A } };
    SPropProblemArray *probs;
    SPropValue v;
    ULONG type;
    HRESULT hr;

    set_str(src, PR_SUBJECT_A, "the subject");
    set_str(src, PR_BODY_A, "the body");
    v.ulPropTag = PR_IMPORTANCE; v.dwAlignPad = 0; v.Value.l = 2;
    IPropData_SetProps(src, 1, &v, NULL);
    set_str(dst, PR_SUBJECT_A, "old subject");

    hr = IPropData_CopyTo(src, 0, NULL, NULL, 0, NULL, &IID_IMAPIPropData, NULL, 0, NULL);
    CHECK(hr == MAPI_E_INVALID_PARAMETER, "no destination %08lx", hr);
    hr = IPropData_CopyTo(src, 0, NULL, NULL, 0, NULL, NULL, dst, 0, NULL);
    CHECK(hr == MAPI_E_INVALID_PARAMETER, "no interface %08lx", hr);
    hr = IPropData_CopyTo(src, 0, NULL, &none, 0, NULL, &IID_IMAPIPropData, dst, 0, NULL);
    CHECK(hr == MAPI_E_INVALID_PARAMETER, "empty exclude list %08lx", hr);
    hr = IPropData_CopyTo(src, 0, NULL, NULL, 0, NULL, &IID_IMAPIPropData, src, 0, NULL);
    CHECK(hr == MAPI_E_NO_ACCESS, "onto itself %08lx", hr);
    hr = IPropData_CopyTo(src, 0, NULL, NULL, 0, NULL, &IID_IStorage, dst, 0, NULL);
    CHECK(hr == MAPI_E_INTERFACE_NOT_SUPPORTED, "unsupported interface %08lx", hr);

    /* no replace keeps what is there */
    probs = (SPropProblemArray *)1;
    hr = IPropData_CopyTo(src, 0, NULL, &excl, 0, NULL, &IID_IMAPIPropData, dst, MAPI_NOREPLACE, &probs);
    CHECK(hr == S_OK && probs == NULL, "copy without replacing %08lx", hr);
    CHECK(!strcmp(get_str(dst, PR_SUBJECT_A, NULL), "old subject"), "subject kept");
    CHECK(!strcmp(get_str(dst, PR_IMPORTANCE, NULL), "2"), "importance copied");
    CHECK(get_str(dst, PR_BODY_A, &type)[0] == 0 && PROP_TYPE(type) == PT_ERROR, "the excluded property did not go");

    /* replace */
    hr = IPropData_CopyTo(src, 0, NULL, &excl, 0, NULL, &IID_IMAPIPropData, dst, 0, NULL);
    CHECK(hr == S_OK, "copy %08lx", hr);
    CHECK(!strcmp(get_str(dst, PR_SUBJECT_A, NULL), "the subject"), "subject replaced");

    /* copy of the properties asked for, and a move */
    dst2 = make_data();
    hr = IPropData_CopyProps(src, &incl, 0, NULL, &IID_IMAPIPropData, dst2, MAPI_MOVE, NULL);
    CHECK(hr == S_OK, "copy props %08lx", hr);
    CHECK(!strcmp(get_str(dst2, PR_SUBJECT_A, NULL), "the subject"), "copied");
    CHECK(get_str(dst2, PR_BODY_A, NULL)[0] == 0, "only the one asked for");
    CHECK(get_str(src, PR_SUBJECT_A, &type)[0] == 0 && PROP_TYPE(type) == PT_ERROR, "moved away from the source");
    CHECK(!strcmp(get_str(src, PR_BODY_A, NULL), "the body"), "the others stay");
    hr = IPropData_CopyProps(src, NULL, 0, NULL, &IID_IMAPIPropData, dst2, 0, NULL);
    CHECK(hr == MAPI_E_INVALID_PARAMETER, "no property list %08lx", hr);
    hr = IPropData_CopyProps(src, &none, 0, NULL, &IID_IMAPIPropData, dst2, 0, NULL);
    CHECK(hr == MAPI_E_INVALID_PARAMETER, "empty property list %08lx", hr);

    /* a read only source is not emptied by a move */
    {
        SPropTagArray t = { 1, { PR_BODY_A } };
        ULONG access = IPROP_READONLY | IPROP_CLEAN;
        IPropData_HrSetPropAccess(src, &t, &access);
        hr = IPropData_CopyProps(src, &t, 0, NULL, &IID_IMAPIPropData, dst2, MAPI_MOVE, NULL);
        CHECK(hr == S_OK && !strcmp(get_str(dst2, PR_BODY_A, NULL), "the body") && !strcmp(get_str(src, PR_BODY_A, NULL), "the body"),
              "a read only property is copied, not moved");
    }
    IPropData_Release(src);
    IPropData_Release(dst);
    IPropData_Release(dst2);
}

static void test_stream(void)
{
    IPropData *d = make_data();
    IStream *stream = NULL;
    char buf[100];
    ULONG got = 0, written;
    LARGE_INTEGER zero = {{0}};
    HRESULT hr;
    ULONG type;
    SPropValue v;
    BYTE bytes[3] = { 7, 8, 9 };

    set_str(d, PR_BODY_A, "some text");
    hr = IPropData_OpenProperty(d, PR_BODY_A, &IID_IStream, 0, 0, (IUnknown **)&stream);
    CHECK(hr == S_OK && stream, "open %08lx", hr);
    if (stream)
    {
        memset(buf, 0, sizeof(buf));
        hr = IStream_Read(stream, buf, sizeof(buf), &got);
        CHECK(got == 9 && !memcmp(buf, "some text", 9), "read %lu", got);
        hr = IStream_Write(stream, "x", 1, &written);
        CHECK(hr == STG_E_ACCESSDENIED, "written without MAPI_MODIFY %08lx", hr);
        IStream_Release(stream);
    }
    hr = IPropData_OpenProperty(d, PR_BODY_A, &IID_IStream, 0, MAPI_MODIFY, (IUnknown **)&stream);
    CHECK(hr == S_OK, "open for modifying %08lx", hr);
    if (stream)
    {
        IStream_Seek(stream, zero, STREAM_SEEK_END, NULL);
        hr = IStream_Write(stream, " more", 5, &written);
        CHECK(hr == S_OK && written == 5, "write %08lx", hr);
        CHECK(!strcmp(get_str(d, PR_BODY_A, NULL), "some text"), "not written before the stream is done");
        IStream_Release(stream);
        CHECK(!strcmp(get_str(d, PR_BODY_A, NULL), "some text more"), "written back by the release: %s", get_str(d, PR_BODY_A, NULL));
    }
    /* created */
    hr = IPropData_OpenProperty(d, PR_RTF_COMPRESSED, &IID_IStream, 0, 0, (IUnknown **)&stream);
    CHECK(hr == MAPI_E_NOT_FOUND, "missing property %08lx", hr);
    hr = IPropData_OpenProperty(d, PR_RTF_COMPRESSED, &IID_IStream, 0, MAPI_CREATE | MAPI_MODIFY, (IUnknown **)&stream);
    CHECK(hr == S_OK && stream, "create %08lx", hr);
    if (stream)
    {
        IStream_Write(stream, bytes, 3, &written);
        IStream_Commit(stream, 0);
        IStream_Release(stream);
        {
            SPropTagArray t = { 1, { PR_RTF_COMPRESSED } };
            SPropValue *pv = NULL;
            ULONG n = 0;
            hr = IPropData_GetProps(d, &t, 0, &n, &pv);
            CHECK(hr == S_OK && pv && PROP_TYPE(pv[0].ulPropTag) == PT_BINARY && pv[0].Value.bin.cb == 3 && !memcmp(pv[0].Value.bin.lpb, bytes, 3), "binary property %08lx tag %lx cb %lu", hr, pv ? pv[0].ulPropTag : 0, pv ? pv[0].Value.bin.cb : 0);
            if (pv) pMAPIFreeBuffer(pv);
        }
    }
    hr = IPropData_OpenProperty(d, PR_BODY_A, &IID_IStorage, 0, 0, (IUnknown **)&stream);
    CHECK(hr == MAPI_E_INTERFACE_NOT_SUPPORTED, "storage %08lx", hr);
    v.ulPropTag = PR_IMPORTANCE; v.dwAlignPad = 0; v.Value.l = 1;
    IPropData_SetProps(d, 1, &v, NULL);
    hr = IPropData_OpenProperty(d, PR_IMPORTANCE, &IID_IStream, 0, 0, (IUnknown **)&stream);
    CHECK(hr == MAPI_E_NO_SUPPORT, "a long as a stream %08lx", hr);
    hr = IPropData_OpenProperty(d, PROP_TAG(PT_UNSPECIFIED, 0x1234), &IID_IStream, 0, 0, (IUnknown **)&stream);
    CHECK(hr == MAPI_E_INVALID_PARAMETER, "unspecified type %08lx", hr);
    hr = IPropData_OpenProperty(d, PR_BODY_A, NULL, 0, 0, (IUnknown **)&stream);
    CHECK(hr == MAPI_E_INVALID_PARAMETER, "no interface %08lx", hr);
    {
        ULONG access = IPROP_READONLY | IPROP_CLEAN;
        SPropTagArray t = { 1, { PR_BODY_A } };
        IPropData_HrSetPropAccess(d, &t, &access);
        hr = IPropData_OpenProperty(d, PR_BODY_A, &IID_IStream, 0, MAPI_MODIFY, (IUnknown **)&stream);
        CHECK(hr == MAPI_E_NO_ACCESS, "modify a read only property %08lx", hr);
        (void)type;
    }
    IPropData_Release(d);
}

static void test_names(void)
{
    IPropData *d = make_data();
    GUID set1 = { 0x11112222, 0x3333, 0x4444, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    GUID set2 = { 0x55556666, 0x3333, 0x4444, { 1, 2, 3, 4, 5, 6, 7, 8 } };
    MAPINAMEID n1 = { &set1, MNID_ID }, n2 = { &set1, MNID_STRING }, n3 = { &set2, MNID_ID }, n4 = { &set1, MNID_ID };
    LPMAPINAMEID names[4];
    SPropTagArray *tags = NULL;
    LPMAPINAMEID *out = NULL;
    ULONG count = 0;
    HRESULT hr;

    n1.Kind.lID = 0x6001;
    n2.Kind.lpwstrName = (LPWSTR)L"MyName";
    n3.Kind.lID = 0x6001;
    n4.Kind.lID = 0x6001;
    names[0] = &n1; names[1] = &n2; names[2] = &n3; names[3] = &n4;

    hr = IPropData_GetIDsFromNames(d, 4, names, 0, &tags);
    CHECK(hr == MAPI_W_ERRORS_RETURNED && tags && tags->cValues == 4, "names that are not there %08lx", hr);
    if (tags)
    {
        CHECK(PROP_TYPE(tags->aulPropTag[0]) == PT_ERROR && PROP_ID(tags->aulPropTag[0]) == 0, "error entry");
        pMAPIFreeBuffer(tags);
    }
    hr = IPropData_GetIDsFromNames(d, 4, names, MAPI_CREATE, &tags);
    CHECK(hr == S_OK && tags && tags->cValues == 4, "create %08lx", hr);
    if (tags)
    {
        ULONG i;
        for (i = 0; i < 4; i++)
            CHECK(PROP_TYPE(tags->aulPropTag[i]) == PT_UNSPECIFIED && PROP_ID(tags->aulPropTag[i]) >= 0x8000 &&
                  PROP_ID(tags->aulPropTag[i]) <= 0xfffe, "id %d %lx", (int)i, tags->aulPropTag[i]);
        CHECK(PROP_ID(tags->aulPropTag[0]) != PROP_ID(tags->aulPropTag[1]) && PROP_ID(tags->aulPropTag[0]) != PROP_ID(tags->aulPropTag[2]),
              "different names, different ids");
        CHECK(PROP_ID(tags->aulPropTag[0]) == PROP_ID(tags->aulPropTag[3]), "the same name once");
        count = PROP_ID(tags->aulPropTag[1]);
        pMAPIFreeBuffer(tags);
    }
    hr = IPropData_GetIDsFromNames(d, 1, &names[1], 0, &tags);
    CHECK(hr == S_OK && tags && PROP_ID(tags->aulPropTag[0]) == count, "found again %08lx", hr);
    if (tags) pMAPIFreeBuffer(tags);
    hr = IPropData_GetIDsFromNames(d, 1, NULL, MAPI_CREATE, &tags);
    CHECK(hr == MAPI_E_INVALID_PARAMETER, "create without names %08lx", hr);
    hr = IPropData_GetIDsFromNames(d, 0, NULL, 0, &tags);
    CHECK(hr == S_OK && tags && tags->cValues == 3, "all names %08lx %lu", hr, tags ? tags->cValues : 0);
    if (tags) pMAPIFreeBuffer(tags);

    /* the names of identifiers */
    hr = IPropData_GetIDsFromNames(d, 4, names, 0, &tags);
    {
        SPropTagArray *ask = NULL;
        count = 0;
        hr = IPropData_GetNamesFromIDs(d, &tags, NULL, 0, &count, &out);
        CHECK(hr == S_OK && count == 4 && out, "names of ids %08lx %lu", hr, count);
        if (out)
        {
            CHECK(out[0] && out[0]->ulKind == MNID_ID && out[0]->Kind.lID == 0x6001 && IsEqualGUID(out[0]->lpguid, &set1), "first name");
            CHECK(out[1] && out[1]->ulKind == MNID_STRING && !lstrcmpW(out[1]->Kind.lpwstrName, L"MyName"), "string name");
            CHECK(out[2] && IsEqualGUID(out[2]->lpguid, &set2), "second set");
            pMAPIFreeBuffer(out);
        }
        out = NULL;
        count = 0;
        hr = IPropData_GetNamesFromIDs(d, &tags, NULL, MAPI_NO_STRINGS, &count, &out);
        CHECK(hr == MAPI_W_ERRORS_RETURNED && count == 4 && out && out[1] == NULL && PROP_TYPE(tags->aulPropTag[1]) == PT_ERROR,
              "without the strings %08lx", hr);
        if (out) pMAPIFreeBuffer(out);
        if (tags) pMAPIFreeBuffer(tags);

        /* every name, or of a set */
        hr = IPropData_GetNamesFromIDs(d, &ask, NULL, 0, &count, &out);
        CHECK(hr == S_OK && count == 3 && out && ask && ask->cValues == 3, "all names %08lx %lu", hr, count);
        if (out) pMAPIFreeBuffer(out);
        if (ask) pMAPIFreeBuffer(ask);
        ask = NULL; out = NULL;
        hr = IPropData_GetNamesFromIDs(d, &ask, &set1, 0, &count, &out);
        CHECK(hr == S_OK && count == 2, "names of the first set %08lx %lu", hr, count);
        if (out) pMAPIFreeBuffer(out);
        if (ask) pMAPIFreeBuffer(ask);
        ask = NULL; out = NULL;
        hr = IPropData_GetNamesFromIDs(d, &ask, &set1, MAPI_NO_IDS | MAPI_NO_STRINGS, &count, &out);
        CHECK(hr == S_OK && count == 0 && out == NULL, "no names %08lx %lu", hr, count);
        if (ask) pMAPIFreeBuffer(ask);
        {
            SPropTagArray zero = { 0 };
            SPropTagArray *z = &zero;
            hr = IPropData_GetNamesFromIDs(d, &z, NULL, 0, &count, &out);
            CHECK(hr == MAPI_E_INVALID_PARAMETER, "no identifiers %08lx", hr);
        }
    }
    IPropData_Release(d);
}

static void test_objprops(void)
{
    IPropData *d = make_data();
    SPropTagArray bad = { 1, { PR_IMPORTANCE } };
    TAGS4 obj4 = { 2, { PR_ATTACH_DATA_OBJ, PROP_TAG(PT_OBJECT, 0x3000) } };
    SPropTagArray *obj_ = (SPropTagArray *)&obj4;
    SPropProblemArray *probs = (SPropProblemArray *)1;
    SPropTagArray *list = NULL;
    HRESULT hr;

    hr = IPropData_HrAddObjProps(d, &bad, &probs);
    CHECK(hr == MAPI_E_INVALID_TYPE, "not an object property %08lx", hr);
    hr = IPropData_HrAddObjProps(d, obj_, &probs);
    CHECK(hr == S_OK && probs == NULL, "add %08lx", hr);
    IPropData_GetPropList(d, 0, &list);
    CHECK(list && list->cValues == 2 && list->aulPropTag[0] == PR_ATTACH_DATA_OBJ, "in the list");
    if (list) pMAPIFreeBuffer(list);
    hr = IPropData_HrAddObjProps(d, obj_, NULL);
    CHECK(hr == S_OK, "again %08lx", hr);
    IPropData_GetPropList(d, 0, &list);
    CHECK(list && list->cValues == 2, "once");
    if (list) pMAPIFreeBuffer(list);
    IPropData_HrSetObjAccess(d, IPROP_READONLY);
    hr = IPropData_HrAddObjProps(d, obj_, NULL);
    CHECK(hr == MAPI_E_NO_ACCESS, "read only object %08lx", hr);
    hr = IPropData_HrAddObjProps(d, NULL, NULL);
    CHECK(hr == MAPI_E_INVALID_PARAMETER, "no tags %08lx", hr);
    IPropData_Release(d);
}

/* ------------------------------------------------------------------ compressed RTF */
static IStream *mem_stream(const void *data, ULONG size)
{
    IStream *s = NULL;
    LARGE_INTEGER zero = {{0}};
    ULONG w;

    CreateStreamOnHGlobal(NULL, TRUE, &s);
    if (size) IStream_Write(s, data, size, &w);
    IStream_Seek(s, zero, STREAM_SEEK_SET, NULL);
    return s;
}

static ULONG slurp(IStream *s, BYTE *buf, ULONG max)
{
    ULONG total = 0, got;

    for (;;)
    {
        got = 0;
        if (FAILED(IStream_Read(s, buf + total, max - total, &got)) || !got) break;
        total += got;
    }
    return total;
}

static void test_rtf(void)
{
    static const BYTE sample[] = {
        0x2d, 0x00, 0x00, 0x00, 0x2b, 0x00, 0x00, 0x00, 0x4c, 0x5a, 0x46, 0x75, 0xf1, 0xc5, 0xc7, 0xa7,
        0x03, 0x00, 0x0a, 0x00, 0x72, 0x63, 0x70, 0x67, 0x31, 0x32, 0x35, 0x42, 0x32, 0x0a, 0xf3, 0x20,
        0x68, 0x65, 0x6c, 0x09, 0x00, 0x20, 0x62, 0x77, 0x05, 0xb0, 0x6c, 0x64, 0x7d, 0x0a, 0x80, 0x0f,
        0xa0 };
    static const char text[] = "{\\rtf1\\ansi\\ansicpg1252\\pard hello world}\r\n";
    IStream *comp, *plain = NULL, *out;
    BYTE buf[20000], *big;
    ULONG n, i;
    HRESULT hr;

    /* the example of the format description */
    comp = mem_stream(sample, sizeof(sample));
    hr = pWrapCompressedRTFStream(comp, 0, &plain);
    CHECK(hr == S_OK && plain, "wrap %08lx", hr);
    if (plain)
    {
        n = slurp(plain, buf, sizeof(buf));
        CHECK(n == strlen(text) && !memcmp(buf, text, n), "text of the example: %lu", n);
        IStream_Release(plain);
    }
    IStream_Release(comp);

    /* the other calls fail as documented */
    comp = mem_stream(sample, sizeof(sample));
    pWrapCompressedRTFStream(comp, 0, &plain);
    {
        LARGE_INTEGER zero = {{0}};
        STATSTG st;
        CHECK(IStream_Seek(plain, zero, STREAM_SEEK_SET, NULL) == E_NOTIMPL, "no seek");
        CHECK(IStream_Stat(plain, &st, STATFLAG_NONAME) == E_NOTIMPL, "no stat");
        CHECK(IStream_Clone(plain, &out) == E_NOTIMPL, "no clone");
        CHECK(IStream_Write(plain, "x", 1, &n) == STG_E_ACCESSDENIED, "no write");
    }
    IStream_Release(plain);
    IStream_Release(comp);

    hr = pWrapCompressedRTFStream(NULL, 0, &plain);
    CHECK(hr == MAPI_E_INVALID_PARAMETER, "no stream %08lx", hr);
    hr = pWrapCompressedRTFStream(mem_stream(sample, sizeof(sample)), 0x100, &plain);
    CHECK(hr == MAPI_E_INVALID_PARAMETER, "flags %08lx", hr);
    {
        BYTE bad[sizeof(sample)];
        memcpy(bad, sample, sizeof(sample));
        bad[8] = 'X';
        comp = mem_stream(bad, sizeof(bad));
        hr = pWrapCompressedRTFStream(comp, 0, &plain);
        CHECK(hr == MAPI_E_CORRUPT_DATA, "bad type %08lx", hr);
        IStream_Release(comp);
        comp = mem_stream(sample, 10);
        hr = pWrapCompressedRTFStream(comp, 0, &plain);
        CHECK(hr == MAPI_E_CORRUPT_DATA, "short %08lx", hr);
        IStream_Release(comp);
        memcpy(bad, sample, sizeof(sample));
        bad[4] = 0x63;       /* a raw size that the data does not give */
        comp = mem_stream(bad, sizeof(bad));
        hr = pWrapCompressedRTFStream(comp, 0, &plain);
        CHECK(hr == MAPI_E_CORRUPT_DATA, "wrong raw size %08lx", hr);
        IStream_Release(comp);
    }

    /* writing: the text goes back compressed, and reads back */
    big = malloc(100000);
    for (i = 0; i < 100000; i++) big[i] = "{\\rtf1 abc defghij \\par\r\n klm}"[(i * 7 + i / 13) % 29];
    comp = mem_stream(NULL, 0);
    hr = pWrapCompressedRTFStream(comp, MAPI_MODIFY, &plain);
    CHECK(hr == S_OK, "wrap for writing %08lx", hr);
    IStream_Write(plain, big, 100000, &n);
    CHECK(n == 100000, "written %lu", n);
    CHECK(IStream_Read(plain, buf, 1, &n) == STG_E_ACCESSDENIED, "no read while writing");
    IStream_Release(plain);
    {
        LARGE_INTEGER zero = {{0}};
        STATSTG st;
        BYTE *cbuf = malloc(200000);
        ULONG cn;

        IStream_Seek(comp, zero, STREAM_SEEK_SET, NULL);
        IStream_Stat(comp, &st, STATFLAG_NONAME);
        cn = slurp(comp, cbuf, 200000);
        CHECK(cn == st.cbSize.LowPart && cn > 16 && cn < 100000, "compressed size %lu", cn);
        CHECK(!memcmp(cbuf + 8, "LZFu", 4), "type");
        CHECK(*(DWORD *)cbuf == cn - 4 && *(DWORD *)(cbuf + 4) == 100000, "sizes %lu %lu", *(DWORD *)cbuf, *(DWORD *)(cbuf + 4));
        free(cbuf);
        IStream_Seek(comp, zero, STREAM_SEEK_SET, NULL);
    }
    pWrapCompressedRTFStream(comp, 0, &plain);
    {
        BYTE *back = malloc(100001);
        n = slurp(plain, back, 100001);
        CHECK(n == 100000 && !memcmp(back, big, n), "round trip of a long text: %lu", n);
        free(back);
    }
    IStream_Release(plain);
    IStream_Release(comp);

    /* the format description's text: our compression reads back, and the data has the right check value */
    comp = mem_stream(NULL, 0);
    pWrapCompressedRTFStream(comp, MAPI_MODIFY, &plain);
    IStream_Write(plain, text, strlen(text), &n);
    IStream_Commit(plain, 0);
    IStream_Release(plain);
    {
        LARGE_INTEGER zero = {{0}};
        BYTE cbuf[200];
        ULONG cn, k, crc = 0, bit;

        IStream_Seek(comp, zero, STREAM_SEEK_SET, NULL);
        cn = slurp(comp, cbuf, sizeof(cbuf));
        for (k = 16; k < cn; k++)
        {
            crc ^= cbuf[k];
            for (bit = 0; bit < 8; bit++) crc = (crc & 1) ? (crc >> 1) ^ 0xedb88320 : crc >> 1;
        }
        CHECK(*(DWORD *)(cbuf + 12) == crc, "the check value of the compressed data %08lx %08lx", *(DWORD *)(cbuf + 12), crc);
        CHECK(*(DWORD *)cbuf == cn - 4 && *(DWORD *)(cbuf + 4) == strlen(text), "header sizes");
        /* the data ends with a reference to the write position of the dictionary: 207 + 43 = 250 */
        CHECK(cn >= 2 && cbuf[cn - 2] == 0x0f && cbuf[cn - 1] == 0xa0, "end of the data %02x %02x", cbuf[cn - 2], cbuf[cn - 1]);
        IStream_Seek(comp, zero, STREAM_SEEK_SET, NULL);
        pWrapCompressedRTFStream(comp, 0, &plain);
        n = slurp(plain, buf, sizeof(buf));
        CHECK(n == strlen(text) && !memcmp(buf, text, n), "text of the format description reads back");
        IStream_Release(plain);
    }
    IStream_Release(comp);

    /* not compressed */
    comp = mem_stream(NULL, 0);
    pWrapCompressedRTFStream(comp, MAPI_MODIFY | STORE_UNCOMPRESSED_RTF, &plain);
    IStream_Write(plain, text, strlen(text), &n);
    IStream_Release(plain);
    {
        LARGE_INTEGER zero = {{0}};
        BYTE cbuf[200];
        ULONG cn;

        IStream_Seek(comp, zero, STREAM_SEEK_SET, NULL);
        cn = slurp(comp, cbuf, sizeof(cbuf));
        CHECK(cn == 16 + strlen(text) && !memcmp(cbuf + 8, "MELA", 4) && *(DWORD *)(cbuf + 12) == 0 &&
              *(DWORD *)cbuf == strlen(text) + 12 && !memcmp(cbuf + 16, text, strlen(text)), "stored as it is");
        IStream_Seek(comp, zero, STREAM_SEEK_SET, NULL);
        pWrapCompressedRTFStream(comp, 0, &plain);
        n = slurp(plain, buf, sizeof(buf));
        CHECK(n == strlen(text) && !memcmp(buf, text, n), "reads back");
        IStream_Release(plain);
    }
    IStream_Release(comp);

    /* an empty text */
    comp = mem_stream(NULL, 0);
    pWrapCompressedRTFStream(comp, MAPI_MODIFY, &plain);
    IStream_Write(plain, "", 0, &n);
    IStream_Release(plain);
    {
        LARGE_INTEGER zero = {{0}};
        IStream_Seek(comp, zero, STREAM_SEEK_SET, NULL);
        hr = pWrapCompressedRTFStream(comp, 0, &plain);
        CHECK(hr == S_OK, "empty read back %08lx", hr);
        if (plain)
        {
            CHECK(slurp(plain, buf, sizeof(buf)) == 0, "empty");
            IStream_Release(plain);
        }
    }
    IStream_Release(comp);
    free(big);
}

int main(void)
{
    HMODULE m;

    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    m = LoadLibraryA("mapi32.dll");
    pScInitMapiUtil = (void *)GetProcAddress(m, "ScInitMapiUtil@4");
    pDeinitMapiUtil = (void *)GetProcAddress(m, "DeinitMapiUtil@0");
    pHrDispatchNotifications = (void *)GetProcAddress(m, "HrDispatchNotifications@4");
    pHrThisThreadAdviseSink = (void *)GetProcAddress(m, "HrThisThreadAdviseSink@8");
    pCreateIProp = (void *)GetProcAddress(m, "CreateIProp@24");
    pMAPIAllocateBuffer = (void *)GetProcAddress(m, "MAPIAllocateBuffer");
    pMAPIAllocateMore = (void *)GetProcAddress(m, "MAPIAllocateMore");
    pMAPIFreeBuffer = (void *)GetProcAddress(m, "MAPIFreeBuffer");
    pWrapCompressedRTFStream = (void *)GetProcAddress(m, "WrapCompressedRTFStream@12");
    if (!pScInitMapiUtil || !pCreateIProp || !pWrapCompressedRTFStream || !pHrThisThreadAdviseSink)
    {
        printf("SKIP: mapi32 exports\n");
        return 0;
    }
    printf("-- test_init\n");
    test_init();
    printf("-- test_sinks\n");
    test_sinks();
    printf("-- test_copy\n");
    test_copy();
    printf("-- test_stream\n");
    test_stream();
    printf("-- test_names\n");
    test_names();
    printf("-- test_objprops\n");
    test_objprops();
    printf("-- test_rtf\n");
    test_rtf();
    printf("checks %d fails %d\n", checks, fails);
    printf("RESULT: %s\n", fails ? "FAIL" : "PASS");
    return fails != 0;
}
