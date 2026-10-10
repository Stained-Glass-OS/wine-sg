/* d3dx9_36 (patches/sg/2649): the keyframed animation set (periods, interpolation, key access), the animation
 * controller (tracks, weights, priorities, speed, events, outputs) and the controller that
 * D3DXLoadMeshHierarchyFromX makes of the animation sets of a file. */
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <rmxftmpl.h>
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

static int near_(float a, float b) { return fabsf(a - b) < 0.001f; }

static ID3DXKeyframedAnimationSet *make_set(const char *name, D3DXPLAYBACK_TYPE type, float x0, float x1)
{
    ID3DXKeyframedAnimationSet *set = NULL;
    D3DXKEY_VECTOR3 t[2] = { { 0.0f, { x0, 0, 0 } }, { 4800.0f, { x1, 0, 0 } } };
    D3DXKEY_QUATERNION r[2] = { { 0.0f, { 0, 0, 0, 1 } }, { 4800.0f, { 0, 0.70710678f, 0, 0.70710678f } } };
    DWORD index;

    D3DXCreateKeyframedAnimationSet(name, 4800.0, type, 2, 0, NULL, &set);
    set->lpVtbl->RegisterAnimationSRTKeys(set, "bone", 0, 2, 2, NULL, r, t, &index);
    return set;
}

/* a hierarchy with nothing but frames */
static HRESULT WINAPI h_CreateFrame(ID3DXAllocateHierarchy *iface, const char *name, D3DXFRAME **out)
{
    D3DXFRAME *f = calloc(1, sizeof(*f));

    f->Name = name ? strdup(name) : NULL;
    *out = f;
    return D3D_OK;
}
static HRESULT WINAPI h_CreateMeshContainer(ID3DXAllocateHierarchy *iface, const char *name, const D3DXMESHDATA *data,
        const D3DXMATERIAL *mat, const D3DXEFFECTINSTANCE *fx, DWORD nmat, const DWORD *adj, ID3DXSkinInfo *skin,
        D3DXMESHCONTAINER **out)
{
    D3DXMESHCONTAINER *c = calloc(1, sizeof(*c));

    c->Name = name ? strdup(name) : NULL;
    c->MeshData = *data;
    if (data->pMesh) data->pMesh->lpVtbl->AddRef(data->pMesh);
    *out = c;
    return D3D_OK;
}
static HRESULT WINAPI h_DestroyFrame(ID3DXAllocateHierarchy *iface, D3DXFRAME *f) { free(f->Name); free(f); return D3D_OK; }
static HRESULT WINAPI h_DestroyMeshContainer(ID3DXAllocateHierarchy *iface, D3DXMESHCONTAINER *c)
{ if (c->MeshData.pMesh) c->MeshData.pMesh->lpVtbl->Release(c->MeshData.pMesh); free(c->Name); free(c); return D3D_OK; }
static ID3DXAllocateHierarchyVtbl h_vtbl = { h_CreateFrame, h_CreateMeshContainer, h_DestroyFrame, h_DestroyMeshContainer };

int main(void)
{
    ID3DXKeyframedAnimationSet *set, *set2;
    ID3DXAnimationController *ctrl;
    D3DXVECTOR3 scale, trans;
    D3DXQUATERNION rot;
    D3DXMATRIX matrix;
    D3DXTRACK_DESC desc;
    D3DXEVENT_DESC event_desc;
    D3DXEVENTHANDLE ev;
    HRESULT hr;
    DWORD index;
    const char *name;

    /* the set: period, positions, interpolation */
    set = make_set("walk", D3DXPLAY_LOOP, 0.0f, 10.0f);
    check(near_((float)set->lpVtbl->GetPeriod(set), 1.0f), "the period is the time of the last key (%g)", set->lpVtbl->GetPeriod(set));
    check(set->lpVtbl->GetNumAnimations(set) == 1, "one animation");
    hr = set->lpVtbl->GetAnimationNameByIndex(set, 0, &name);
    check(hr == D3D_OK && !strcmp(name, "bone"), "its name");
    check(set->lpVtbl->GetAnimationIndexByName(set, "bone", (UINT *)&index) == D3D_OK && index == 0, "its index");
    check(set->lpVtbl->GetAnimationIndexByName(set, "nobody", (UINT *)&index) == D3DERR_INVALIDCALL, "no such animation");
    check(near_((float)set->lpVtbl->GetPeriodicPosition(set, 1.25), 0.25f), "loop: 1.25 is at 0.25 (%g)", set->lpVtbl->GetPeriodicPosition(set, 1.25));
    hr = set->lpVtbl->GetSRT(set, 0.5, 0, &scale, &rot, &trans);
    check(hr == D3D_OK && near_(trans.x, 5.0f), "translation halfway = 5 (%g)", trans.x);
    check(near_(scale.x, 1.0f) && near_(scale.y, 1.0f), "no scale keys: scale 1");
    check(near_(rot.y, 0.38268343f) && near_(rot.w, 0.92387953f), "rotation halfway is the slerp (%g, %g)", rot.y, rot.w);
    set->lpVtbl->GetSRT(set, 1.0, 0, &scale, &rot, &trans);
    check(near_(trans.x, 10.0f), "at the end 10");
    set->lpVtbl->Release(set);

    set = make_set("once", D3DXPLAY_ONCE, 0.0f, 10.0f);
    check(near_((float)set->lpVtbl->GetPeriodicPosition(set, 1.25), 1.0f), "once: stays at the end (%g)", set->lpVtbl->GetPeriodicPosition(set, 1.25));
    set->lpVtbl->Release(set);
    set = make_set("pingpong", D3DXPLAY_PINGPONG, 0.0f, 10.0f);
    check(near_((float)set->lpVtbl->GetPeriodicPosition(set, 1.25), 0.75f), "ping-pong: comes back (%g)", set->lpVtbl->GetPeriodicPosition(set, 1.25));
    set->lpVtbl->Release(set);

    /* keys can be read and changed */
    set = make_set("keys", D3DXPLAY_LOOP, 0.0f, 10.0f);
    {
        D3DXKEY_VECTOR3 key;

        check(set->lpVtbl->GetNumTranslationKeys(set, 0) == 2 && set->lpVtbl->GetNumScaleKeys(set, 0) == 0, "key counts");
        set->lpVtbl->GetTranslationKey(set, 0, 1, &key);
        check(near_(key.Value.x, 10.0f) && near_(key.Time, 4800.0f), "key 1");
        key.Value.x = 20.0f;
        check(set->lpVtbl->SetTranslationKey(set, 0, 1, &key) == D3D_OK, "SetTranslationKey");
        set->lpVtbl->GetSRT(set, 1.0, 0, &scale, &rot, &trans);
        check(near_(trans.x, 20.0f), "and it is used (%g)", trans.x);
        check(set->lpVtbl->UnregisterTranslationKey(set, 0, 0) == D3D_OK && set->lpVtbl->GetNumTranslationKeys(set, 0) == 1, "UnregisterTranslationKey");
        check(set->lpVtbl->GetTranslationKey(set, 0, 5, &key) == D3DERR_INVALIDCALL, "key out of range");
    }
    set->lpVtbl->Release(set);

    /* the controller */
    set = make_set("a", D3DXPLAY_LOOP, 0.0f, 10.0f);
    set2 = make_set("b", D3DXPLAY_LOOP, 100.0f, 100.0f);
    D3DXCreateAnimationController(4, 4, 4, 8, &ctrl);
    D3DXMatrixIdentity(&matrix);
    check(ctrl->lpVtbl->RegisterAnimationOutput(ctrl, "bone", &matrix, NULL, NULL, NULL) == D3D_OK, "output");
    check(ctrl->lpVtbl->RegisterAnimationSet(ctrl, (ID3DXAnimationSet *)set) == D3D_OK && ctrl->lpVtbl->RegisterAnimationSet(ctrl, (ID3DXAnimationSet *)set2) == D3D_OK, "sets registered");
    check(ctrl->lpVtbl->GetNumAnimationSets(ctrl) == 2, "two sets");
    ctrl->lpVtbl->SetTrackAnimationSet(ctrl, 0, (ID3DXAnimationSet *)set);
    ctrl->lpVtbl->SetTrackEnable(ctrl, 0, TRUE);
    ctrl->lpVtbl->AdvanceTime(ctrl, 0.5, NULL);
    check(near_(matrix._41, 5.0f), "track at 0.5 s puts the bone at x=5 (%g)", matrix._41);
    check(near_((float)ctrl->lpVtbl->GetTime(ctrl), 0.5f), "controller time");
    ctrl->lpVtbl->GetTrackDesc(ctrl, 0, &desc);
    check(near_((float)desc.Position, 0.5f) && desc.Enable && desc.Speed == 1.0f && desc.Weight == 1.0f, "track position follows");

    ctrl->lpVtbl->SetTrackSpeed(ctrl, 0, 2.0f);
    ctrl->lpVtbl->SetTrackPosition(ctrl, 0, 0.0);
    ctrl->lpVtbl->AdvanceTime(ctrl, 0.25, NULL);
    check(near_(matrix._41, 5.0f), "twice the speed: 0.25 s is x=5 (%g)", matrix._41);
    ctrl->lpVtbl->SetTrackSpeed(ctrl, 0, 1.0f);

    /* two tracks, the same priority: weighted */
    ctrl->lpVtbl->SetTrackAnimationSet(ctrl, 1, (ID3DXAnimationSet *)set2);
    ctrl->lpVtbl->SetTrackEnable(ctrl, 1, TRUE);
    ctrl->lpVtbl->SetTrackPosition(ctrl, 0, 0.5);
    ctrl->lpVtbl->SetTrackPosition(ctrl, 1, 0.0);
    ctrl->lpVtbl->SetTrackWeight(ctrl, 0, 0.75f);
    ctrl->lpVtbl->SetTrackWeight(ctrl, 1, 0.25f);
    ctrl->lpVtbl->AdvanceTime(ctrl, 0.0, NULL);
    check(near_(matrix._41, 0.75f * 5.0f + 0.25f * 100.0f), "weights 3:1 blend x=5 and x=100 into %g", matrix._41);

    /* priorities and the blend between them */
    ctrl->lpVtbl->SetTrackWeight(ctrl, 0, 1.0f);
    ctrl->lpVtbl->SetTrackWeight(ctrl, 1, 1.0f);
    ctrl->lpVtbl->SetTrackPriority(ctrl, 1, D3DXPRIORITY_HIGH);
    ctrl->lpVtbl->SetPriorityBlend(ctrl, 0.5f);
    ctrl->lpVtbl->AdvanceTime(ctrl, 0.0, NULL);
    check(near_(matrix._41, 52.5f), "half the high priority track (%g)", matrix._41);
    ctrl->lpVtbl->SetPriorityBlend(ctrl, 1.0f);
    ctrl->lpVtbl->AdvanceTime(ctrl, 0.0, NULL);
    check(near_(matrix._41, 100.0f), "all of it (%g)", matrix._41);
    ctrl->lpVtbl->SetTrackEnable(ctrl, 1, FALSE);
    ctrl->lpVtbl->AdvanceTime(ctrl, 0.0, NULL);
    check(near_(matrix._41, 5.0f), "a disabled track does not count (%g)", matrix._41);
    ctrl->lpVtbl->SetTrackPriority(ctrl, 1, D3DXPRIORITY_LOW);
    ctrl->lpVtbl->SetTrackEnable(ctrl, 1, FALSE);

    /* events */
    ctrl->lpVtbl->SetTrackPosition(ctrl, 0, 0.0);
    ctrl->lpVtbl->SetTrackEnable(ctrl, 0, FALSE);
    ev = ctrl->lpVtbl->KeyTrackEnable(ctrl, 0, TRUE, ctrl->lpVtbl->GetTime(ctrl) + 1.0);
    check(ev && ctrl->lpVtbl->ValidateEvent(ctrl, ev) == D3D_OK, "an event is keyed");
    hr = ctrl->lpVtbl->GetEventDesc(ctrl, ev, &event_desc);
    check(hr == D3D_OK && event_desc.Type == D3DXEVENT_TRACKENABLE && event_desc.Track == 0 && event_desc.Enable, "its description");
    ctrl->lpVtbl->AdvanceTime(ctrl, 0.5, NULL);
    ctrl->lpVtbl->GetTrackDesc(ctrl, 0, &desc);
    check(!desc.Enable && near_((float)desc.Position, 0.0f), "not yet enabled");
    ctrl->lpVtbl->AdvanceTime(ctrl, 1.0, NULL);
    ctrl->lpVtbl->GetTrackDesc(ctrl, 0, &desc);
    check(desc.Enable, "enabled by the event");
    check(ctrl->lpVtbl->UnkeyEvent(ctrl, ev) == D3D_OK && ctrl->lpVtbl->ValidateEvent(ctrl, ev) == D3DERR_INVALIDCALL, "unkeyed");

    ev = ctrl->lpVtbl->KeyTrackSpeed(ctrl, 0, 3.0f, ctrl->lpVtbl->GetTime(ctrl), 2.0, D3DXTRANSITION_LINEAR);
    ctrl->lpVtbl->AdvanceTime(ctrl, 1.0, NULL);
    ctrl->lpVtbl->GetTrackDesc(ctrl, 0, &desc);
    check(near_(desc.Speed, 2.0f), "halfway through the speed transition: 2 (%g)", desc.Speed);
    ctrl->lpVtbl->AdvanceTime(ctrl, 1.5, NULL);
    ctrl->lpVtbl->GetTrackDesc(ctrl, 0, &desc);
    check(near_(desc.Speed, 3.0f), "and after it: 3 (%g)", desc.Speed);
    ctrl->lpVtbl->ResetTime(ctrl);
    check(near_((float)ctrl->lpVtbl->GetTime(ctrl), 0.0f), "ResetTime");

    /* a clone keeps what the controller had */
    {
        ID3DXAnimationController *clone = NULL;

        hr = ctrl->lpVtbl->CloneAnimationController(ctrl, 8, 8, 8, 8, &clone);
        check(hr == D3D_OK && clone && clone->lpVtbl->GetNumAnimationSets(clone) == 2, "clone (%#lx)", hr);
        if (clone)
        {
            ID3DXAnimationSet *s = NULL;

            clone->lpVtbl->GetTrackAnimationSet(clone, 0, &s);
            check(s == (ID3DXAnimationSet *)set, "with the same track set");
            if (s) s->lpVtbl->Release(s);
            clone->lpVtbl->Release(clone);
        }
    }
    ctrl->lpVtbl->Release(ctrl);
    set->lpVtbl->Release(set);
    set2->lpVtbl->Release(set2);

    /* the controller of a file */
    {
        static const char xfile[] =
            "xof 0303txt 0032"
            "Frame Bone {"
            "FrameTransformMatrix { 1.0,0.0,0.0,0.0, 0.0,1.0,0.0,0.0, 0.0,0.0,1.0,0.0, 0.0,0.0,0.0,1.0;; }"
            "}"
            "AnimationSet Walk {"
            "Animation A {"
            "{Bone}"
            "AnimationKey { 2; 2; 0; 3; 0.0, 0.0, 0.0;;, 4800; 3; 10.0, 0.0, 0.0;;; }"
            "}"
            "}";
        ID3DXAllocateHierarchy alloc = { &h_vtbl };
        D3DPRESENT_PARAMETERS pp = {0};
        IDirect3DDevice9 *device = NULL;
        IDirect3D9 *d3d;
        D3DXFRAME *frame = NULL;
        HWND window = CreateWindowA("static", "anim", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 200, 200, 0, 0, 0, 0);

        d3d = Direct3DCreate9(D3D_SDK_VERSION);
        pp.BackBufferWidth = 64; pp.BackBufferHeight = 64; pp.BackBufferFormat = D3DFMT_X8R8G8B8;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD; pp.Windowed = TRUE; pp.hDeviceWindow = window;
        hr = d3d ? IDirect3D9_CreateDevice(d3d, 0, D3DDEVTYPE_HAL, window, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device) : E_FAIL;
        check(hr == D3D_OK, "device (%#lx)", hr);
        if (device)
        {
            ctrl = NULL;
            hr = D3DXLoadMeshHierarchyFromXInMemory(xfile, sizeof(xfile) - 1, 0, device, &alloc, NULL, &frame, &ctrl);
            check(hr == D3D_OK && frame && ctrl, "file loaded with a controller (%#lx)", hr);
            if (ctrl)
            {
                check(ctrl->lpVtbl->GetNumAnimationSets(ctrl) == 1, "one animation set");
                ctrl->lpVtbl->AdvanceTime(ctrl, 0.5, NULL);
                check(near_(frame->TransformationMatrix._41, 5.0f), "the frame is moved by the animation (%g)", frame->TransformationMatrix._41);
                ctrl->lpVtbl->Release(ctrl);
            }
            if (frame) D3DXFrameDestroy(frame, &alloc);
        }
    }

    /* the object a reference points to is as deep as the object itself */
    {
        static const char xfile[] =
            "xof 0303txt 0032"
            "Frame F { Frame G { Frame H { FrameTransformMatrix { 1.0,0.0,0.0,0.0, 0.0,1.0,0.0,0.0, 0.0,0.0,1.0,0.0, 0.0,0.0,0.0,1.0;; } } } }"
            "Frame R { {F} }";
        ID3DXFile *file = NULL;
        ID3DXFileEnumObject *enum_object = NULL;
        D3DXF_FILELOADMEMORY source = { (void *)xfile, sizeof(xfile) - 1 };

        hr = D3DXFileCreate(&file);
        if (SUCCEEDED(hr)) hr = file->lpVtbl->RegisterTemplates(file, D3DRM_XTEMPLATES, D3DRM_XTEMPLATE_BYTES);
        if (SUCCEEDED(hr)) hr = file->lpVtbl->CreateEnumObject(file, &source, D3DXF_FILELOAD_FROMMEMORY, &enum_object);
        check(SUCCEEDED(hr), "x file parsed (%#lx)", hr);
        if (enum_object)
        {
            ID3DXFileData *r = NULL, *ref = NULL, *g = NULL, *h = NULL;
            SIZE_T count = 99;

            enum_object->lpVtbl->GetChild(enum_object, 1, &r);
            if (r) r->lpVtbl->GetChild(r, 0, &ref);
            check(ref && ref->lpVtbl->IsReference(ref), "a reference");
            if (ref) ref->lpVtbl->GetChild(ref, 0, &g);
            if (g) g->lpVtbl->GetChild(g, 0, &h);
            if (h) h->lpVtbl->GetChildren(h, &count);
            check(h && count == 1, "three levels down through the reference (%lu children)", (unsigned long)count);
            if (h) h->lpVtbl->Release(h);
            if (g) g->lpVtbl->Release(g);
            if (ref) ref->lpVtbl->Release(ref);
            if (r) r->lpVtbl->Release(r);
            enum_object->lpVtbl->Release(enum_object);
        }
        if (file) file->lpVtbl->Release(file);
    }

    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
