/* mf's sequencer source and standard quality manager notifications (patches
 * 2973-2975), run by test/mf-sequencer-gate.sh. Table-driven: every check
 * compares an HRESULT or a value. The expected values come from the public
 * documentation of IMFSequencerSource / IMFMediaSource / IMFQualityManager
 * (assumptions are listed in patches/notes/2973.txt).
 *
 *   mf-sequencer-probe.exe */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <stdio.h>
#include <string.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[256]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)
#define HR(what, got, want) CHECKF((got) == (want), "%s: hr %#lx (want %#lx)", what, (unsigned long)(got), (unsigned long)(want))

/* The mingw-w64 headers lack these two interfaces: vtables by hand. */
DEFINE_GUID(IID_IMFSequencerSource, 0x197cd219, 0x19cb, 0x4de1, 0xa6, 0x4c, 0xac, 0xf2, 0xed, 0xcb, 0xe5, 0x9e);
DEFINE_GUID(IID_IMFMediaSourceTopologyProvider, 0x0e1d6009, 0xc9f3, 0x442d, 0x8c, 0x51, 0xa4, 0x2d, 0x2d, 0x49, 0x45, 0x2f);
typedef struct IMFSequencerSource IMFSequencerSource;
typedef struct IMFMediaSourceTopologyProvider IMFMediaSourceTopologyProvider;
typedef struct
{
    HRESULT (WINAPI *QueryInterface)(IMFSequencerSource *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IMFSequencerSource *);
    ULONG (WINAPI *Release)(IMFSequencerSource *);
    HRESULT (WINAPI *AppendTopology)(IMFSequencerSource *, IMFTopology *, DWORD, MFSequencerElementId *);
    HRESULT (WINAPI *DeleteTopology)(IMFSequencerSource *, MFSequencerElementId);
    HRESULT (WINAPI *GetPresentationContext)(IMFSequencerSource *, IMFPresentationDescriptor *,
            MFSequencerElementId *, IMFTopology **);
    HRESULT (WINAPI *UpdateTopology)(IMFSequencerSource *, MFSequencerElementId, IMFTopology *);
    HRESULT (WINAPI *UpdateTopologyFlags)(IMFSequencerSource *, MFSequencerElementId, DWORD);
} IMFSequencerSourceVtbl;
struct IMFSequencerSource { const IMFSequencerSourceVtbl *lpVtbl; };
typedef struct
{
    HRESULT (WINAPI *QueryInterface)(IMFMediaSourceTopologyProvider *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IMFMediaSourceTopologyProvider *);
    ULONG (WINAPI *Release)(IMFMediaSourceTopologyProvider *);
    HRESULT (WINAPI *GetMediaSourceTopology)(IMFMediaSourceTopologyProvider *, IMFPresentationDescriptor *, IMFTopology **);
} IMFMediaSourceTopologyProviderVtbl;
struct IMFMediaSourceTopologyProvider { const IMFMediaSourceTopologyProviderVtbl *lpVtbl; };
#define IMFSequencerSource_QueryInterface(p, a, b) (p)->lpVtbl->QueryInterface(p, a, b)
#define IMFSequencerSource_Release(p) (p)->lpVtbl->Release(p)
#define IMFSequencerSource_AppendTopology(p, a, b, c) (p)->lpVtbl->AppendTopology(p, a, b, c)
#define IMFSequencerSource_DeleteTopology(p, a) (p)->lpVtbl->DeleteTopology(p, a)
#define IMFSequencerSource_GetPresentationContext(p, a, b, c) (p)->lpVtbl->GetPresentationContext(p, a, b, c)
#define IMFSequencerSource_UpdateTopology(p, a, b) (p)->lpVtbl->UpdateTopology(p, a, b)
#define IMFSequencerSource_UpdateTopologyFlags(p, a, b) (p)->lpVtbl->UpdateTopologyFlags(p, a, b)
#define IMFMediaSourceTopologyProvider_Release(p) (p)->lpVtbl->Release(p)
#define IMFMediaSourceTopologyProvider_GetMediaSourceTopology(p, a, b) (p)->lpVtbl->GetMediaSourceTopology(p, a, b)
HRESULT WINAPI MFCreateSequencerSource(IUnknown *reserved, IMFSequencerSource **seq);

typedef struct IMFQualityManager IMFQualityManager;
typedef struct
{
    HRESULT (WINAPI *QueryInterface)(IMFQualityManager *, REFIID, void **);
    ULONG (WINAPI *AddRef)(IMFQualityManager *);
    ULONG (WINAPI *Release)(IMFQualityManager *);
    HRESULT (WINAPI *NotifyTopology)(IMFQualityManager *, IMFTopology *);
    HRESULT (WINAPI *NotifyPresentationClock)(IMFQualityManager *, IMFPresentationClock *);
    HRESULT (WINAPI *NotifyProcessInput)(IMFQualityManager *, IMFTopologyNode *, LONG, IMFSample *);
    HRESULT (WINAPI *NotifyProcessOutput)(IMFQualityManager *, IMFTopologyNode *, LONG, IMFSample *);
    HRESULT (WINAPI *NotifyQualityEvent)(IMFQualityManager *, IUnknown *, IMFMediaEvent *);
    HRESULT (WINAPI *Shutdown)(IMFQualityManager *);
} IMFQualityManagerVtbl;
struct IMFQualityManager { const IMFQualityManagerVtbl *lpVtbl; };
#define IMFQualityManager_Release(p) (p)->lpVtbl->Release(p)
#define IMFQualityManager_NotifyProcessInput(p, a, b, c) (p)->lpVtbl->NotifyProcessInput(p, a, b, c)
#define IMFQualityManager_NotifyProcessOutput(p, a, b, c) (p)->lpVtbl->NotifyProcessOutput(p, a, b, c)
#define IMFQualityManager_NotifyQualityEvent(p, a, b) (p)->lpVtbl->NotifyQualityEvent(p, a, b)
#define IMFQualityManager_Shutdown(p) (p)->lpVtbl->Shutdown(p)
HRESULT WINAPI MFCreateStandardQualityManager(IMFQualityManager **manager);

#define LAST 0x1

static MediaEventType next_event(IMFMediaSource *src, HRESULT *hr_out)
{
    IMFMediaEvent *event;
    MediaEventType type = 0;
    HRESULT hr = IMFMediaSource_GetEvent(src, MF_EVENT_FLAG_NO_WAIT, &event);
    if (hr_out) *hr_out = hr;
    if (SUCCEEDED(hr))
    {
        IMFMediaEvent_GetType(event, &type);
        IMFMediaEvent_Release(event);
    }
    return type;
}

static IMFPresentationDescriptor *make_pd(void)
{
    IMFMediaType *type;
    IMFStreamDescriptor *sd;
    IMFPresentationDescriptor *pd;
    HRESULT hr;

    MFCreateMediaType(&type);
    IMFMediaType_SetGUID(type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
    IMFMediaType_SetGUID(type, &MF_MT_SUBTYPE, &MFAudioFormat_PCM);
    hr = MFCreateStreamDescriptor(1, 1, &type, &sd);
    HR("MFCreateStreamDescriptor", hr, S_OK);
    hr = MFCreatePresentationDescriptor(1, &sd, &pd);
    HR("MFCreatePresentationDescriptor", hr, S_OK);
    IMFStreamDescriptor_Release(sd);
    IMFMediaType_Release(type);
    return pd;
}

static IMFTopology *make_topology(BOOL with_source)
{
    IMFTopologyNode *node;
    IMFPresentationDescriptor *pd;
    IMFTopology *topology;

    MFCreateTopology(&topology);
    if (with_source)
    {
        pd = make_pd();
        MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &node);
        IMFTopologyNode_SetUnknown(node, &MF_TOPONODE_PRESENTATION_DESCRIPTOR, (IUnknown *)pd);
        IMFTopology_AddNode(topology, node);
        IMFTopologyNode_Release(node);
        IMFPresentationDescriptor_Release(pd);
    }
    return topology;
}

static void test_qi(void)
{
    static const struct { const GUID *iid; const char *name; HRESULT hr; } tests[] =
    {
        { &IID_IMFSequencerSource, "IMFSequencerSource", S_OK },
        { &IID_IMFMediaSourceTopologyProvider, "IMFMediaSourceTopologyProvider", S_OK },
        { &IID_IMFMediaSource, "IMFMediaSource", S_OK },
        { &IID_IMFMediaSourceEx, "IMFMediaSourceEx", S_OK },
        { &IID_IMFMediaEventGenerator, "IMFMediaEventGenerator", S_OK },
        { &IID_IMFTopology, "IMFTopology", E_NOINTERFACE },
        { &IID_IMFMediaStream, "IMFMediaStream", E_NOINTERFACE },
    };
    IMFSequencerSource *seq;
    unsigned int i;
    IUnknown *unk;
    HRESULT hr;

    hr = MFCreateSequencerSource(NULL, &seq);
    HR("MFCreateSequencerSource", hr, S_OK);
    hr = MFCreateSequencerSource(NULL, NULL);
    HR("MFCreateSequencerSource(NULL out)", hr, E_POINTER);
    for (i = 0; i < ARRAYSIZE(tests); ++i)
    {
        unk = (IUnknown *)1;
        hr = IMFSequencerSource_QueryInterface(seq, tests[i].iid, (void **)&unk);
        CHECKF(hr == tests[i].hr, "QI %s: hr %#lx", tests[i].name, (unsigned long)hr);
        CHECKF(SUCCEEDED(hr) == (unk != NULL && unk != (IUnknown *)1), "QI %s: out pointer", tests[i].name);
        if (SUCCEEDED(hr)) IUnknown_Release(unk);
    }
    IMFSequencerSource_Release(seq);
}

static void test_idle(void)
{
    IMFSequencerSource *seq;
    IMFMediaSourceEx *ex;
    IMFMediaSource *src;
    IMFPresentationDescriptor *pd;
    IMFAttributes *attrs, *attrs2;
    PROPVARIANT pos;
    DWORD flags;
    HRESULT hr;

    MFCreateSequencerSource(NULL, &seq);
    IMFSequencerSource_QueryInterface(seq, &IID_IMFMediaSourceEx, (void **)&ex);
    src = (IMFMediaSource *)ex;

    flags = 0;
    hr = IMFMediaSource_GetCharacteristics(src, &flags);
    HR("idle GetCharacteristics", hr, S_OK);
    CHECKF(flags == (MFMEDIASOURCE_CAN_PAUSE | MFMEDIASOURCE_CAN_SEEK), "idle characteristics %#lx", (unsigned long)flags);
    hr = IMFMediaSource_GetCharacteristics(src, NULL);
    HR("GetCharacteristics(NULL)", hr, E_POINTER);

    pd = (void *)1;
    hr = IMFMediaSource_CreatePresentationDescriptor(src, &pd);
    HR("empty CreatePresentationDescriptor", hr, MF_E_NOT_INITIALIZED);
    CHECKF(pd == NULL, "empty CreatePresentationDescriptor clears the output");
    hr = IMFMediaSource_CreatePresentationDescriptor(src, NULL);
    HR("CreatePresentationDescriptor(NULL)", hr, E_POINTER);

    hr = IMFMediaSource_Stop(src);
    HR("idle Stop", hr, MF_E_INVALID_STATE_TRANSITION);
    hr = IMFMediaSource_Pause(src);
    HR("idle Pause", hr, MF_E_INVALID_STATE_TRANSITION);
    CHECKF(next_event(src, &hr) == 0 && hr == MF_E_NO_EVENTS_AVAILABLE, "idle: no events queued (hr %#lx)", (unsigned long)hr);

    attrs = (void *)1;
    hr = IMFMediaSourceEx_GetStreamAttributes(ex, 0, &attrs);
    HR("GetStreamAttributes(0)", hr, MF_E_INVALIDSTREAMNUMBER);
    CHECKF(attrs == NULL, "GetStreamAttributes clears the output");
    hr = IMFMediaSourceEx_GetStreamAttributes(ex, 0, NULL);
    HR("GetStreamAttributes(NULL)", hr, E_POINTER);
    hr = IMFMediaSourceEx_SetD3DManager(ex, NULL);
    HR("SetD3DManager(NULL)", hr, E_NOTIMPL);
    hr = IMFMediaSourceEx_GetSourceAttributes(ex, NULL);
    HR("GetSourceAttributes(NULL)", hr, E_POINTER);
    hr = IMFMediaSourceEx_GetSourceAttributes(ex, &attrs);
    HR("GetSourceAttributes", hr, S_OK);
    hr = IMFMediaSourceEx_GetSourceAttributes(ex, &attrs2);
    CHECKF(hr == S_OK && attrs == attrs2 && attrs, "GetSourceAttributes returns the same store");
    if (attrs) IMFAttributes_Release(attrs);
    if (attrs2) IMFAttributes_Release(attrs2);

    {
        IMFTopology *empty = make_topology(FALSE);
        MFSequencerElementId eid;
        hr = IMFSequencerSource_AppendTopology(seq, empty, 0, &eid);
        HR("Append(topology without source node)", hr, S_OK);
        pd = (void *)1;
        hr = IMFMediaSource_CreatePresentationDescriptor(src, &pd);
        HR("CreatePresentationDescriptor without a source node", hr, MF_E_INVALIDREQUEST);
        CHECKF(pd == NULL, "failed CreatePresentationDescriptor clears the output");
        IMFSequencerSource_DeleteTopology(seq, eid);
        IMFTopology_Release(empty);
    }
    PropVariantInit(&pos);
    hr = IMFMediaSource_Start(src, NULL, &GUID_NULL, &pos);
    HR("Start(NULL pd)", hr, E_INVALIDARG);
    pd = make_pd();
    hr = IMFMediaSource_Start(src, pd, &GUID_NULL, &pos);
    HR("Start(foreign pd)", hr, E_INVALIDARG);
    IMFPresentationDescriptor_Release(pd);

    IMFMediaSourceEx_Release(ex);
    IMFSequencerSource_Release(seq);
}

static void test_queue(void)
{
    IMFTopology *topo[4], *out;
    IMFSequencerSource *seq;
    IMFMediaSourceTopologyProvider *provider;
    IMFMediaSource *src;
    IMFPresentationDescriptor *pd, *foreign;
    MFSequencerElementId id[4], got;
    PROPVARIANT pos;
    HRESULT hr, ehr;
    GUID bad = { 0x12345678, 1, 2, { 3, 4, 5, 6, 7, 8, 9, 10 } };
    unsigned int i;

    for (i = 0; i < 4; ++i) topo[i] = make_topology(TRUE);
    MFCreateSequencerSource(NULL, &seq);
    IMFSequencerSource_QueryInterface(seq, &IID_IMFMediaSource, (void **)&src);
    IMFSequencerSource_QueryInterface(seq, &IID_IMFMediaSourceTopologyProvider, (void **)&provider);

    hr = IMFSequencerSource_AppendTopology(seq, NULL, 0, &id[0]);
    HR("Append(NULL topology)", hr, E_INVALIDARG);
    hr = IMFSequencerSource_AppendTopology(seq, topo[0], 0, NULL);
    HR("Append(NULL id)", hr, E_POINTER);
    hr = IMFSequencerSource_AppendTopology(seq, topo[0], 2, &id[0]);
    HR("Append(invalid flags)", hr, E_INVALIDARG);
    hr = IMFSequencerSource_AppendTopology(seq, topo[0], 0, &id[0]);
    HR("Append #0", hr, S_OK);
    hr = IMFSequencerSource_AppendTopology(seq, topo[1], 0, &id[1]);
    HR("Append #1", hr, S_OK);
    CHECKF(id[0] != id[1], "element ids are unique (%#lx, %#lx)", (unsigned long)id[0], (unsigned long)id[1]);

    /* presentation descriptors map back to the first topology */
    hr = IMFMediaSource_CreatePresentationDescriptor(src, &pd);
    HR("CreatePresentationDescriptor", hr, S_OK);
    got = 0xdead; out = (void *)1;
    hr = IMFSequencerSource_GetPresentationContext(seq, pd, &got, &out);
    HR("GetPresentationContext", hr, S_OK);
    CHECKF(got == id[0], "context id %#lx == first id %#lx", (unsigned long)got, (unsigned long)id[0]);
    CHECKF(out == topo[0], "context topology is the first one appended");
    if (out && out != (void *)1) IMFTopology_Release(out);
    hr = IMFSequencerSource_GetPresentationContext(seq, pd, &got, NULL);
    HR("GetPresentationContext(NULL topology)", hr, S_OK);
    hr = IMFSequencerSource_GetPresentationContext(seq, pd, NULL, &out);
    HR("GetPresentationContext(NULL id)", hr, E_POINTER);
    hr = IMFSequencerSource_GetPresentationContext(seq, NULL, &got, &out);
    HR("GetPresentationContext(NULL pd)", hr, E_INVALIDARG);
    foreign = make_pd();
    out = (void *)1;
    hr = IMFSequencerSource_GetPresentationContext(seq, foreign, &got, &out);
    HR("GetPresentationContext(foreign pd)", hr, MF_E_NOT_FOUND);
    CHECKF(out == NULL, "foreign pd clears the topology output");
    out = (void *)1;
    hr = IMFMediaSourceTopologyProvider_GetMediaSourceTopology(provider, foreign, &out);
    HR("GetMediaSourceTopology(foreign pd)", hr, MF_E_NOT_FOUND);
    CHECKF(out == NULL, "GetMediaSourceTopology clears the output on failure");
    IMFPresentationDescriptor_Release(foreign);
    hr = IMFMediaSourceTopologyProvider_GetMediaSourceTopology(provider, pd, &out);
    CHECKF(hr == S_OK && out == topo[0], "GetMediaSourceTopology(pd) returns the first topology (hr %#lx)", (unsigned long)hr);
    if (out) IMFTopology_Release(out);
    hr = IMFMediaSourceTopologyProvider_GetMediaSourceTopology(provider, NULL, &out);
    HR("GetMediaSourceTopology(NULL pd)", hr, E_INVALIDARG);
    hr = IMFMediaSourceTopologyProvider_GetMediaSourceTopology(provider, pd, NULL);
    HR("GetMediaSourceTopology(NULL out)", hr, E_POINTER);

    /* update */
    hr = IMFSequencerSource_UpdateTopology(seq, id[0], topo[2]);
    HR("UpdateTopology", hr, S_OK);
    hr = IMFSequencerSource_GetPresentationContext(seq, pd, &got, &out);
    CHECKF(hr == S_OK && out == topo[2], "context topology replaced by UpdateTopology (hr %#lx)", (unsigned long)hr);
    if (out) IMFTopology_Release(out);
    hr = IMFSequencerSource_UpdateTopology(seq, 0xfffff, topo[2]);
    HR("UpdateTopology(unknown id)", hr, MF_E_NOT_FOUND);
    hr = IMFSequencerSource_UpdateTopology(seq, id[0], NULL);
    HR("UpdateTopology(NULL)", hr, E_INVALIDARG);

    /* flags */
    hr = IMFSequencerSource_UpdateTopologyFlags(seq, 0xfffff, 0);
    HR("UpdateTopologyFlags(unknown id)", hr, MF_E_NOT_FOUND);
    hr = IMFSequencerSource_UpdateTopologyFlags(seq, id[1], 2);
    HR("UpdateTopologyFlags(invalid)", hr, E_INVALIDARG);
    hr = IMFSequencerSource_UpdateTopologyFlags(seq, id[1], LAST);
    HR("UpdateTopologyFlags(Last)", hr, S_OK);
    hr = IMFSequencerSource_AppendTopology(seq, topo[3], 0, &id[3]);
    HR("Append after Last", hr, MF_E_INVALIDREQUEST);
    hr = IMFSequencerSource_UpdateTopologyFlags(seq, id[1], 0);
    HR("UpdateTopologyFlags(0)", hr, S_OK);
    hr = IMFSequencerSource_AppendTopology(seq, topo[3], LAST, &id[3]);
    HR("Append(Last) after flag cleared", hr, S_OK);
    hr = IMFSequencerSource_AppendTopology(seq, topo[3], 0, &got);
    HR("Append after Last (appended flag)", hr, MF_E_INVALIDREQUEST);

    /* start / pause / stop */
    PropVariantInit(&pos);
    hr = IMFMediaSource_Start(src, pd, &bad, &pos);
    HR("Start(unsupported time format)", hr, MF_E_UNSUPPORTED_TIME_FORMAT);
    hr = IMFMediaSource_Start(src, pd, &GUID_NULL, &pos);
    HR("Start", hr, S_OK);
    CHECKF(next_event(src, &ehr) == MESourceStarted && ehr == S_OK, "Start queues MESourceStarted");
    hr = IMFMediaSource_Pause(src);
    HR("Pause", hr, S_OK);
    CHECKF(next_event(src, &ehr) == MESourcePaused, "Pause queues MESourcePaused");
    hr = IMFMediaSource_Start(src, pd, NULL, &pos);
    HR("Start from paused (NULL time format)", hr, S_OK);
    CHECKF(next_event(src, &ehr) == MESourceStarted, "second Start queues MESourceStarted");
    hr = IMFMediaSource_Stop(src);
    HR("Stop", hr, S_OK);
    CHECKF(next_event(src, &ehr) == MESourceStopped, "Stop queues MESourceStopped");
    hr = IMFMediaSource_Stop(src);
    HR("second Stop", hr, MF_E_INVALID_STATE_TRANSITION);
    hr = IMFMediaSource_Pause(src);
    HR("Pause when stopped", hr, MF_E_INVALID_STATE_TRANSITION);
    hr = IMFMediaSource_QueueEvent(src, MEUnknown, &GUID_NULL, S_OK, NULL);
    HR("QueueEvent", hr, S_OK);
    CHECKF(next_event(src, &ehr) == MEUnknown, "QueueEvent event is returned by GetEvent");

    /* delete */
    hr = IMFSequencerSource_DeleteTopology(seq, id[0]);
    HR("DeleteTopology", hr, S_OK);
    hr = IMFSequencerSource_DeleteTopology(seq, id[0]);
    HR("DeleteTopology again", hr, MF_E_NOT_FOUND);
    hr = IMFSequencerSource_GetPresentationContext(seq, pd, &got, &out);
    HR("context of deleted topology", hr, MF_E_NOT_FOUND);
    IMFPresentationDescriptor_Release(pd);
    hr = IMFMediaSource_CreatePresentationDescriptor(src, &pd);
    HR("CreatePresentationDescriptor after delete", hr, S_OK);
    hr = IMFSequencerSource_GetPresentationContext(seq, pd, &got, &out);
    CHECKF(hr == S_OK && got == id[1] && out == topo[1], "next descriptor maps to the second topology (hr %#lx, id %#lx)",
            (unsigned long)hr, (unsigned long)got);
    if (out) IMFTopology_Release(out);

    /* shutdown */
    hr = IMFMediaSource_Shutdown(src);
    HR("Shutdown", hr, S_OK);
    hr = IMFMediaSource_Shutdown(src);
    HR("second Shutdown", hr, MF_E_SHUTDOWN);
    {
        IMFMediaEvent *event;
        IMFPresentationDescriptor *pd2;
        DWORD flags;
        IMFAttributes *attrs;
        hr = IMFMediaSource_GetEvent(src, MF_EVENT_FLAG_NO_WAIT, &event);
        HR("GetEvent after Shutdown", hr, MF_E_SHUTDOWN);
        hr = IMFMediaSource_CreatePresentationDescriptor(src, &pd2);
        HR("CreatePresentationDescriptor after Shutdown", hr, MF_E_SHUTDOWN);
        hr = IMFMediaSource_GetCharacteristics(src, &flags);
        HR("GetCharacteristics after Shutdown", hr, MF_E_SHUTDOWN);
        hr = IMFMediaSource_Start(src, pd, &GUID_NULL, &pos);
        HR("Start after Shutdown", hr, MF_E_SHUTDOWN);
        hr = IMFMediaSource_Stop(src);
        HR("Stop after Shutdown", hr, MF_E_SHUTDOWN);
        hr = IMFMediaSource_Pause(src);
        HR("Pause after Shutdown", hr, MF_E_SHUTDOWN);
        hr = IMFMediaSource_QueueEvent(src, MEUnknown, &GUID_NULL, S_OK, NULL);
        HR("QueueEvent after Shutdown", hr, MF_E_SHUTDOWN);
        hr = IMFMediaSourceEx_GetSourceAttributes((IMFMediaSourceEx *)src, &attrs);
        HR("GetSourceAttributes after Shutdown", hr, MF_E_SHUTDOWN);
        hr = IMFSequencerSource_AppendTopology(seq, topo[3], 0, &got);
        HR("Append after Shutdown", hr, MF_E_SHUTDOWN);
        hr = IMFSequencerSource_DeleteTopology(seq, id[1]);
        HR("Delete after Shutdown", hr, MF_E_SHUTDOWN);
        hr = IMFSequencerSource_UpdateTopology(seq, id[1], topo[3]);
        HR("UpdateTopology after Shutdown", hr, MF_E_SHUTDOWN);
        hr = IMFSequencerSource_UpdateTopologyFlags(seq, id[1], 0);
        HR("UpdateTopologyFlags after Shutdown", hr, MF_E_SHUTDOWN);
        hr = IMFSequencerSource_GetPresentationContext(seq, pd, &got, &out);
        HR("GetPresentationContext after Shutdown", hr, MF_E_SHUTDOWN);
    }
    IMFPresentationDescriptor_Release(pd);
    IMFMediaSourceTopologyProvider_Release(provider);
    IMFMediaSource_Release(src);
    {
        ULONG ref = IMFSequencerSource_Release(seq);
        CHECKF(ref == 0, "sequencer source released (%lu)", ref);
    }
    for (i = 0; i < 4; ++i)
    {
        ULONG ref = IMFTopology_Release(topo[i]);
        CHECKF(ref == 0, "topology %u released by the sequencer (%lu)", i, ref);
    }
}

static void test_quality_manager(void)
{
    IMFQualityManager *qm;
    IMFMediaEvent *event;
    HRESULT hr;

    hr = MFCreateStandardQualityManager(&qm);
    HR("MFCreateStandardQualityManager", hr, S_OK);
    MFCreateMediaEvent(MEQualityNotify, &GUID_NULL, S_OK, NULL, &event);

    hr = IMFQualityManager_NotifyQualityEvent(qm, NULL, event);
    HR("NotifyQualityEvent", hr, S_OK);
    hr = IMFQualityManager_NotifyQualityEvent(qm, NULL, NULL);
    HR("NotifyQualityEvent(NULL event)", hr, E_POINTER);
    hr = IMFQualityManager_NotifyProcessInput(qm, NULL, 0, NULL);
    HR("NotifyProcessInput", hr, S_OK);
    hr = IMFQualityManager_NotifyProcessOutput(qm, NULL, 0, NULL);
    HR("NotifyProcessOutput", hr, S_OK);

    hr = IMFQualityManager_Shutdown(qm);
    HR("quality manager Shutdown", hr, S_OK);
    hr = IMFQualityManager_NotifyQualityEvent(qm, NULL, event);
    HR("NotifyQualityEvent after Shutdown", hr, MF_E_SHUTDOWN);
    hr = IMFQualityManager_NotifyProcessInput(qm, NULL, 0, NULL);
    HR("NotifyProcessInput after Shutdown", hr, MF_E_SHUTDOWN);
    hr = IMFQualityManager_NotifyProcessOutput(qm, NULL, 0, NULL);
    HR("NotifyProcessOutput after Shutdown", hr, MF_E_SHUTDOWN);

    IMFMediaEvent_Release(event);
    IMFQualityManager_Release(qm);
}

int main(void)
{
    HRESULT hr = MFStartup(MF_VERSION, MFSTARTUP_FULL);
    HR("MFStartup", hr, S_OK);
    test_qi();
    test_idle();
    test_queue();
    test_quality_manager();
    MFShutdown();
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
