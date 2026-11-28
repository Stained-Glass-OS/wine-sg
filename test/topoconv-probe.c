/* topoconv-probe: a media session plays uncompressed video the video renderer cannot take as it
 * is (patches/sg/9414).
 *
 * The topology loader puts the video processor between such a stream and the EVR, then offers the
 * renderer the processor's output types. Those came bare (subtype and frame size): the EVR's mixer
 * took every one for a compressed type and refused it, and the loader failed with
 * MF_E_TOPO_CODEC_NOT_FOUND: no media session could play WMV, MPEG-1/2, MPEG-4 part 2 or
 * uncompressed AVI video (Wine's source hands those out decoded, as I420 or RGB). As the source
 * reader's pipeline does, the output types now carry what the processor's input type says (frame
 * rate, stride, all samples independent...).
 *
 * The probe writes an RGB24 AVI (2 s, 10 fps), resolves source -> EVR with the topology loader
 * (expect: resolved, a video processor between, its output type uncompressed), then plays it in a
 * media session to MESessionEnded.
 *
 * Prints name=value lines; see test/topoconv-gate.sh.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <stdio.h>
#include <string.h>

#define W 64
#define H 48
#define FRAMES 20
#define RATE 10

static void say(const char *name, int ok, const char *fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    printf("%s=%d %s\n", name, ok ? 1 : 0, buf);
    fflush(stdout);
}

static void put32(BYTE **p, DWORD v) { memcpy(*p, &v, 4); *p += 4; }
static void put16(BYTE **p, WORD v) { memcpy(*p, &v, 2); *p += 2; }
static void putcc(BYTE **p, const char *cc) { memcpy(*p, cc, 4); *p += 4; }

static BOOL write_avi(const char *path)
{
    static BYTE file[FRAMES * (W * H * 3 + 8) + 4096];
    DWORD frame_size = W * H * 3;
    BYTE *p = file, *riff_size, *hdrl_size, *strl_size, *movi_size, *movi, *frames[FRAMES];
    DWORD written;
    HANDLE h;
    int f, i;

    putcc(&p, "RIFF"); riff_size = p; put32(&p, 0); putcc(&p, "AVI ");
    putcc(&p, "LIST"); hdrl_size = p; put32(&p, 0); putcc(&p, "hdrl");
    putcc(&p, "avih"); put32(&p, 56);
    put32(&p, 1000000 / RATE); put32(&p, frame_size * RATE); put32(&p, 0); put32(&p, 0x10);
    put32(&p, FRAMES); put32(&p, 0); put32(&p, 1); put32(&p, frame_size);
    put32(&p, W); put32(&p, H); put32(&p, 0); put32(&p, 0); put32(&p, 0); put32(&p, 0);
    putcc(&p, "LIST"); strl_size = p; put32(&p, 0); putcc(&p, "strl");
    putcc(&p, "strh"); put32(&p, 56);
    putcc(&p, "vids"); put32(&p, 0); put32(&p, 0); put16(&p, 0); put16(&p, 0); put32(&p, 0);
    put32(&p, 1); put32(&p, RATE); put32(&p, 0); put32(&p, FRAMES); put32(&p, frame_size);
    put32(&p, 0xffffffff); put32(&p, 0);
    put16(&p, 0); put16(&p, 0); put16(&p, W); put16(&p, H);
    putcc(&p, "strf"); put32(&p, 40);
    put32(&p, 40); put32(&p, W); put32(&p, H); put16(&p, 1); put16(&p, 24);
    put32(&p, BI_RGB); put32(&p, frame_size); put32(&p, 0); put32(&p, 0); put32(&p, 0); put32(&p, 0);
    *(DWORD *)strl_size = p - strl_size - 4;
    *(DWORD *)hdrl_size = p - hdrl_size - 4;
    putcc(&p, "LIST"); movi_size = p; put32(&p, 0); movi = p; putcc(&p, "movi");
    for (f = 0; f < FRAMES; f++)
    {
        frames[f] = p;
        putcc(&p, "00db"); put32(&p, frame_size);
        for (i = 0; i < W * H; i++) { *p++ = (BYTE)(f * 12); *p++ = (BYTE)(i % W * 4); *p++ = (BYTE)(i / W * 5); }
    }
    *(DWORD *)movi_size = p - movi_size - 4;
    putcc(&p, "idx1"); put32(&p, FRAMES * 16);
    for (f = 0; f < FRAMES; f++)
    {
        putcc(&p, "00db"); put32(&p, 0x10); put32(&p, frames[f] - movi); put32(&p, frame_size);
    }
    *(DWORD *)riff_size = p - riff_size - 4;

    h = CreateFileA(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return FALSE;
    WriteFile(h, file, p - file, &written, NULL);
    CloseHandle(h);
    return written == (DWORD)(p - file);
}

static HWND make_window(void)
{
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(NULL);
    wc.lpszClassName = "topoconv";
    RegisterClassA(&wc);
    return CreateWindowA("topoconv", "topoconv", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 320, 240, NULL, NULL, NULL, NULL);
}

static IMFTopology *partial_topology(IMFMediaSource *src, IMFPresentationDescriptor *pd, HWND hwnd, BOOL activated)
{
    IMFTopologyNode *srcn, *outn;
    IMFStreamDescriptor *sd;
    IMFActivate *sink;
    IMFTopology *topo;
    BOOL sel;

    MFCreateTopology(&topo);
    IMFPresentationDescriptor_GetStreamDescriptorByIndex(pd, 0, &sel, &sd);
    MFCreateVideoRendererActivate(hwnd, &sink);
    if (activated)
    {
        /* the topology loader takes stream sinks, not activation objects */
        IMFMediaSink *media_sink;
        IMFStreamSink *stream_sink = NULL;
        if (SUCCEEDED(IMFActivate_ActivateObject(sink, &IID_IMFMediaSink, (void **)&media_sink)))
        {
            IMFMediaSink_GetStreamSinkByIndex(media_sink, 0, &stream_sink);
            IMFMediaSink_Release(media_sink);
        }
        MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &srcn);
        IMFTopologyNode_SetUnknown(srcn, &MF_TOPONODE_SOURCE, (IUnknown *)src);
        IMFTopologyNode_SetUnknown(srcn, &MF_TOPONODE_PRESENTATION_DESCRIPTOR, (IUnknown *)pd);
        IMFTopologyNode_SetUnknown(srcn, &MF_TOPONODE_STREAM_DESCRIPTOR, (IUnknown *)sd);
        MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &outn);
        IMFTopologyNode_SetObject(outn, (IUnknown *)stream_sink);
        IMFTopology_AddNode(topo, srcn);
        IMFTopology_AddNode(topo, outn);
        IMFTopologyNode_ConnectOutput(srcn, 0, outn, 0);
        IMFTopologyNode_Release(srcn);
        IMFTopologyNode_Release(outn);
        if (stream_sink) IMFStreamSink_Release(stream_sink);
        IMFActivate_Release(sink);
        IMFStreamDescriptor_Release(sd);
        return topo;
    }
    MFCreateTopologyNode(MF_TOPOLOGY_SOURCESTREAM_NODE, &srcn);
    IMFTopologyNode_SetUnknown(srcn, &MF_TOPONODE_SOURCE, (IUnknown *)src);
    IMFTopologyNode_SetUnknown(srcn, &MF_TOPONODE_PRESENTATION_DESCRIPTOR, (IUnknown *)pd);
    IMFTopologyNode_SetUnknown(srcn, &MF_TOPONODE_STREAM_DESCRIPTOR, (IUnknown *)sd);
    MFCreateTopologyNode(MF_TOPOLOGY_OUTPUT_NODE, &outn);
    IMFTopologyNode_SetObject(outn, (IUnknown *)sink);
    IMFTopology_AddNode(topo, srcn);
    IMFTopology_AddNode(topo, outn);
    IMFTopologyNode_ConnectOutput(srcn, 0, outn, 0);
    IMFTopologyNode_Release(srcn);
    IMFTopologyNode_Release(outn);
    IMFActivate_Release(sink);
    IMFStreamDescriptor_Release(sd);
    return topo;
}

int main(void)
{
    IMFSourceResolver *resolver;
    IMFPresentationDescriptor *pd;
    IMFTopology *topo, *full;
    IMFTopoLoader *loader;
    IMFMediaSession *session;
    IMFMediaSource *src;
    MF_OBJECT_TYPE type;
    IUnknown *obj;
    PROPVARIANT var;
    HWND hwnd;
    HRESULT hr;
    WORD count, i;
    int transforms = 0, independent = -1, ended = 0;
    DWORD t0;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    MFStartup(MF_VERSION, 0);
    say("write", write_avi("C:\\rgb24.avi"), "RGB24 AVI written");
    hwnd = make_window();

    MFCreateSourceResolver(&resolver);
    hr = IMFSourceResolver_CreateObjectFromURL(resolver, L"C:\\rgb24.avi", MF_RESOLUTION_MEDIASOURCE, NULL, &type, &obj);
    say("source", SUCCEEDED(hr), "media source %#lx", hr);
    if (FAILED(hr)) goto done;
    IUnknown_QueryInterface(obj, &IID_IMFMediaSource, (void **)&src);
    IUnknown_Release(obj);
    IMFMediaSource_CreatePresentationDescriptor(src, &pd);

    /* the topology loader */
    topo = partial_topology(src, pd, hwnd, TRUE);
    MFCreateTopoLoader(&loader);
    hr = IMFTopoLoader_Load(loader, topo, &full, NULL);
    say("load", hr == S_OK, "topology loader %#lx", hr);
    if (hr == S_OK)
    {
        IMFTopology_GetNodeCount(full, &count);
        for (i = 0; i < count; i++)
        {
            IMFTopologyNode *node;
            MF_TOPOLOGY_TYPE nt;
            IMFTopology_GetNode(full, i, &node);
            IMFTopologyNode_GetNodeType(node, &nt);
            if (nt == MF_TOPOLOGY_TRANSFORM_NODE)
            {
                IUnknown *t;
                IMFTransform *transform;
                IMFMediaType *out;
                transforms++;
                if (SUCCEEDED(IMFTopologyNode_GetObject(node, &t)))
                {
                    if (SUCCEEDED(IUnknown_QueryInterface(t, &IID_IMFTransform, (void **)&transform)))
                    {
                        if (SUCCEEDED(IMFTransform_GetOutputCurrentType(transform, 0, &out)))
                        {
                            BOOL compressed = TRUE;
                            IMFMediaType_IsCompressedFormat(out, &compressed);
                            independent = !compressed;
                            IMFMediaType_Release(out);
                        }
                        IMFTransform_Release(transform);
                    }
                    IUnknown_Release(t);
                }
            }
            IMFTopologyNode_Release(node);
        }
        say("converter", transforms == 1, "%d transform node(s) between the source and the EVR", transforms);
        say("uncompressed", independent == 1, "the converter's output type is uncompressed (%d)", independent);
        IMFTopology_Release(full);
    }
    IMFTopoLoader_Release(loader);
    IMFTopology_Release(topo);

    /* a media session plays it to the end */
    topo = partial_topology(src, pd, hwnd, FALSE);
    MFCreateMediaSession(NULL, &session);
    IMFMediaSession_SetTopology(session, 0, topo);
    PropVariantInit(&var);
    IMFMediaSession_Start(session, &GUID_NULL, &var);
    t0 = GetTickCount();
    hr = S_OK;
    while (GetTickCount() - t0 < 20000)
    {
        IMFMediaEvent *ev;
        MediaEventType met;
        HRESULT status;
        MSG msg;
        while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
        if (IMFMediaSession_GetEvent(session, MF_EVENT_FLAG_NO_WAIT, &ev) != S_OK) { Sleep(10); continue; }
        IMFMediaEvent_GetType(ev, &met);
        IMFMediaEvent_GetStatus(ev, &status);
        IMFMediaEvent_Release(ev);
        if (FAILED(status)) { hr = status; break; }
        if (met == MESessionEnded) { ended = 1; break; }
    }
    say("played", ended, "media session %s (%#lx) after %lu ms", ended ? "ended" : "did not end", hr, GetTickCount() - t0);
done:
    fflush(stdout);
    TerminateProcess(GetCurrentProcess(), 0);
    return 0;
}
