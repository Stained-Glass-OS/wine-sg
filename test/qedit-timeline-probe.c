/* Native probe for the DirectShow Editing Services timeline object model
 * (qedit.dll IAMTimeline / IAMTimelineObj / IAMTimelineGroup). */
#define __USE_MINGW_ANSI_STDIO 1
#define COBJMACROS
#include <windows.h>
#include <dshow.h>
#include <qedit.h>
#include <stdio.h>
#include <math.h>

static int failures, checks;

#define CHECK(cond, ...) do { checks++; if (!(cond)) { failures++; printf("FAIL  line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define CHECK_HR(got, want) CHECK((got) == (want), "%s: hr %#lx, expected %#lx", #got, (unsigned long)(got), (unsigned long)(want))

static ULONG refs(void *p)
{
    IUnknown *u = p;
    IUnknown_AddRef(u);
    return IUnknown_Release(u);
}

static IAMTimelineObj *make_obj(IAMTimeline *tl, TIMELINE_MAJOR_TYPE type)
{
    IAMTimelineObj *obj = NULL;
    HRESULT hr = IAMTimeline_CreateEmptyNode(tl, &obj, type);
    CHECK_HR(hr, S_OK);
    return obj;
}

static void test_timeline(IAMTimeline *tl)
{
    static const GUID guid_a = {0x12345678, 0x1234, 0x5678, {1, 2, 3, 4, 5, 6, 7, 8}};
    static const GUID guid_b = {0x87654321, 0x4321, 0x8765, {8, 7, 6, 5, 4, 3, 2, 1}};
    GUID guid;
    BSTR bstr;
    LONG count, mode;
    BOOL enabled, dirty;
    double fps, dur;
    REFERENCE_TIME start, stop, rt;
    HRESULT hr;

    /* defaults */
    CHECK_HR(IAMTimeline_GetGroupCount(tl, &count), S_OK);
    CHECK(count == 0, "group count %ld", count);
    CHECK_HR(IAMTimeline_GetInsertMode(tl, &mode), S_OK);
    CHECK(mode == 1, "insert mode %ld", mode);
    CHECK_HR(IAMTimeline_TransitionsEnabled(tl, &enabled), S_OK);
    CHECK(enabled == TRUE, "transitions %d", enabled);
    CHECK_HR(IAMTimeline_EffectsEnabled(tl, &enabled), S_OK);
    CHECK(enabled == TRUE, "effects %d", enabled);
    CHECK_HR(IAMTimeline_GetDefaultFPS(tl, &fps), S_OK);
    CHECK(fps == 15.0, "default fps %f", fps);
    CHECK_HR(IAMTimeline_GetDuration(tl, &rt), S_OK);
    CHECK(rt == 0, "duration %lld", rt);
    CHECK_HR(IAMTimeline_GetDuration2(tl, &dur), S_OK);
    CHECK(dur == 0.0, "duration2 %f", dur);
    CHECK_HR(IAMTimeline_IsDirty(tl, &dirty), S_OK);
    CHECK(!dirty, "dirty");
    CHECK_HR(IAMTimeline_GetDefaultTransition(tl, &guid), S_OK);
    CHECK(IsEqualGUID(&guid, &GUID_NULL), "default transition %s", "not null");
    CHECK_HR(IAMTimeline_GetDefaultEffect(tl, &guid), S_OK);
    CHECK(IsEqualGUID(&guid, &GUID_NULL), "default effect not null");

    /* NULL pointers */
    CHECK_HR(IAMTimeline_GetGroupCount(tl, NULL), E_POINTER);
    CHECK_HR(IAMTimeline_GetInsertMode(tl, NULL), E_POINTER);
    CHECK_HR(IAMTimeline_TransitionsEnabled(tl, NULL), E_POINTER);
    CHECK_HR(IAMTimeline_EffectsEnabled(tl, NULL), E_POINTER);
    CHECK_HR(IAMTimeline_GetDefaultFPS(tl, NULL), E_POINTER);
    CHECK_HR(IAMTimeline_GetDuration(tl, NULL), E_POINTER);
    CHECK_HR(IAMTimeline_GetDuration2(tl, NULL), E_POINTER);
    CHECK_HR(IAMTimeline_IsDirty(tl, NULL), E_POINTER);
    CHECK_HR(IAMTimeline_GetDirtyRange(tl, NULL, &stop), E_POINTER);
    CHECK_HR(IAMTimeline_GetDirtyRange(tl, &start, NULL), E_POINTER);
    CHECK_HR(IAMTimeline_AddGroup(tl, NULL), E_POINTER);
    CHECK_HR(IAMTimeline_RemGroupFromList(tl, NULL), E_POINTER);
    CHECK_HR(IAMTimeline_GetGroup(tl, NULL, 0), E_POINTER);
    CHECK_HR(IAMTimeline_SetDefaultTransition(tl, NULL), E_POINTER);
    CHECK_HR(IAMTimeline_GetDefaultTransition(tl, NULL), E_POINTER);
    CHECK_HR(IAMTimeline_SetDefaultEffect(tl, NULL), E_POINTER);
    CHECK_HR(IAMTimeline_GetDefaultEffect(tl, NULL), E_POINTER);
    CHECK_HR(IAMTimeline_GetDefaultTransitionB(tl, NULL), E_POINTER);
    CHECK_HR(IAMTimeline_GetDefaultEffectB(tl, NULL), E_POINTER);
    CHECK_HR(IAMTimeline_SetDefaultTransitionB(tl, NULL), E_POINTER);
    CHECK_HR(IAMTimeline_SetDefaultEffectB(tl, NULL), E_POINTER);

    /* insert mode */
    CHECK_HR(IAMTimeline_SetInsertMode(tl, 0), E_INVALIDARG);
    CHECK_HR(IAMTimeline_SetInsertMode(tl, 3), E_INVALIDARG);
    CHECK_HR(IAMTimeline_SetInsertMode(tl, 2), S_OK);
    IAMTimeline_GetInsertMode(tl, &mode);
    CHECK(mode == 2, "insert mode after set %ld", mode);
    CHECK_HR(IAMTimeline_SetInsertMode(tl, 1), S_OK);
    IAMTimeline_GetInsertMode(tl, &mode);
    CHECK(mode == 1, "insert mode restored %ld", mode);

    /* enable flags */
    CHECK_HR(IAMTimeline_EnableTransitions(tl, FALSE), S_OK);
    IAMTimeline_TransitionsEnabled(tl, &enabled);
    CHECK(enabled == FALSE, "transitions disabled");
    CHECK_HR(IAMTimeline_EnableTransitions(tl, 5), S_OK);
    IAMTimeline_TransitionsEnabled(tl, &enabled);
    CHECK(enabled == TRUE, "transitions re-enabled gives %d", enabled);
    CHECK_HR(IAMTimeline_EnableEffects(tl, FALSE), S_OK);
    IAMTimeline_EffectsEnabled(tl, &enabled);
    CHECK(enabled == FALSE, "effects disabled");
    IAMTimeline_EnableEffects(tl, TRUE);

    /* interest range and fps */
    CHECK_HR(IAMTimeline_SetInterestRange(tl, 5, 3), E_INVALIDARG);
    CHECK_HR(IAMTimeline_SetInterestRange(tl, -1, 3), E_INVALIDARG);
    CHECK_HR(IAMTimeline_SetInterestRange(tl, 3, 5), S_OK);
    CHECK_HR(IAMTimeline_SetDefaultFPS(tl, 0.0), E_INVALIDARG);
    CHECK_HR(IAMTimeline_SetDefaultFPS(tl, -3.0), E_INVALIDARG);
    CHECK_HR(IAMTimeline_SetDefaultFPS(tl, 29.97), S_OK);
    IAMTimeline_GetDefaultFPS(tl, &fps);
    CHECK(fps == 29.97, "fps %f", fps);
    IAMTimeline_SetDefaultFPS(tl, 15.0);

    /* default GUIDs */
    CHECK_HR(IAMTimeline_SetDefaultTransition(tl, (GUID *)&guid_a), S_OK);
    CHECK_HR(IAMTimeline_GetDefaultTransition(tl, &guid), S_OK);
    CHECK(IsEqualGUID(&guid, &guid_a), "default transition roundtrip");
    CHECK_HR(IAMTimeline_SetDefaultEffect(tl, (GUID *)&guid_b), S_OK);
    CHECK_HR(IAMTimeline_GetDefaultEffect(tl, &guid), S_OK);
    CHECK(IsEqualGUID(&guid, &guid_b), "default effect roundtrip");
    CHECK(!IsEqualGUID(&guid_a, &guid_b), "sanity");
    bstr = NULL;
    CHECK_HR(IAMTimeline_GetDefaultTransitionB(tl, &bstr), S_OK);
    CHECK(bstr && !wcscmp(bstr, L"{12345678-1234-5678-0102-030405060708}"), "transition B: %ls", bstr ? bstr : L"(null)");
    SysFreeString(bstr);
    bstr = SysAllocString(L"{AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE}");
    CHECK_HR(IAMTimeline_SetDefaultEffectB(tl, bstr), S_OK);
    CHECK_HR(IAMTimeline_GetDefaultEffect(tl, &guid), S_OK);
    CHECK(guid.Data1 == 0xaaaaaaaa && guid.Data2 == 0xbbbb && guid.Data3 == 0xcccc && guid.Data4[0] == 0xdd
            && guid.Data4[7] == 0xee, "effect from B string");
    SysFreeString(bstr);
    bstr = SysAllocString(L"not a guid");
    CHECK_HR(IAMTimeline_SetDefaultTransitionB(tl, bstr), E_INVALIDARG);
    IAMTimeline_GetDefaultTransition(tl, &guid);
    CHECK(IsEqualGUID(&guid, &guid_a), "failed B set left transition alone");
    SysFreeString(bstr);
    bstr = NULL;
    CHECK_HR(IAMTimeline_GetDefaultEffectB(tl, &bstr), S_OK);
    CHECK(bstr && !wcscmp(bstr, L"{AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE}"), "effect B: %ls", bstr ? bstr : L"(null)");
    SysFreeString(bstr);

    /* nothing to validate or count yet */
    hr = IAMTimeline_ValidateSourceNames(tl, 0, NULL, 0);
    CHECK_HR(hr, S_OK);
    CHECK_HR(IAMTimeline_GetCountOfType(tl, 0, &count, NULL, TIMELINE_MAJOR_TYPE_SOURCE), E_INVALIDARG);
    CHECK_HR(IAMTimeline_GetCountOfType(tl, 0, NULL, NULL, TIMELINE_MAJOR_TYPE_SOURCE), E_POINTER);
}

static void test_groups(IAMTimeline *tl)
{
    IAMTimelineObj *g[3], *comp, *obj, *obj2;
    IAMTimelineGroup *group;
    IAMTimeline *tl2;
    LONG count, prio, n;
    BOOL dirty;
    REFERENCE_TIME start, stop;
    ULONG ref;
    HRESULT hr;
    int i;

    comp = make_obj(tl, TIMELINE_MAJOR_TYPE_COMPOSITE);
    for (i = 0; i < 3; i++)
        g[i] = make_obj(tl, TIMELINE_MAJOR_TYPE_GROUP);

    CHECK_HR(IAMTimeline_AddGroup(tl, comp), E_INVALIDARG);
    CHECK_HR(IAMTimeline_GetGroup(tl, &obj, 0), E_INVALIDARG);
    CHECK(obj == NULL, "failed GetGroup output %p", obj);

    ref = refs(g[0]);
    CHECK(ref == 1, "group refcount %lu", ref);
    for (i = 0; i < 3; i++)
        CHECK_HR(IAMTimeline_AddGroup(tl, g[i]), S_OK);
    ref = refs(g[0]);
    CHECK(ref == 2, "group refcount after add %lu", ref);
    CHECK_HR(IAMTimeline_AddGroup(tl, g[0]), E_INVALIDARG);

    IAMTimeline_GetGroupCount(tl, &count);
    CHECK(count == 3, "count %ld", count);

    for (i = 0; i < 3; i++)
    {
        obj = NULL;
        CHECK_HR(IAMTimeline_GetGroup(tl, &obj, i), S_OK);
        CHECK(obj == g[i], "GetGroup(%d) returned %p, expected %p", i, obj, g[i]);
        if (obj) IAMTimelineObj_Release(obj);

        IAMTimelineObj_QueryInterface(g[i], &IID_IAMTimelineGroup, (void **)&group);
        prio = -1;
        CHECK_HR(IAMTimelineGroup_GetPriority(group, &prio), S_OK);
        CHECK(prio == i, "priority %ld, expected %d", prio, i);
        tl2 = NULL;
        CHECK_HR(IAMTimelineGroup_GetTimeline(group, &tl2), S_OK);
        CHECK(tl2 == tl, "group timeline %p", tl2);
        if (tl2) IAMTimeline_Release(tl2);
        IAMTimelineGroup_Release(group);
    }
    obj = (IAMTimelineObj *)0xdead;
    CHECK_HR(IAMTimeline_GetGroup(tl, &obj, -1), E_INVALIDARG);
    CHECK(obj == NULL, "negative index output");
    CHECK_HR(IAMTimeline_GetGroup(tl, &obj, 3), E_INVALIDARG);

    CHECK_HR(IAMTimeline_GetCountOfType(tl, 0, &count, &n, TIMELINE_MAJOR_TYPE_TRACK), S_OK);
    CHECK(count == 0 && n == 0, "track counts %ld/%ld", count, n);
    CHECK_HR(IAMTimeline_GetCountOfType(tl, 3, &count, &n, TIMELINE_MAJOR_TYPE_TRACK), E_INVALIDARG);

    /* dirty tracking through the group objects */
    IAMTimeline_IsDirty(tl, &dirty);
    CHECK(dirty, "adding a group makes the timeline dirty");
    for (i = 0; i < 3; i++)
        IAMTimelineObj_ClearDirty(g[i]);
    IAMTimeline_IsDirty(tl, &dirty);
    CHECK(!dirty, "cleared");
    CHECK_HR(IAMTimelineObj_SetStartStop(g[1], 10000000, 30000000), S_OK);
    IAMTimeline_IsDirty(tl, &dirty);
    CHECK(dirty, "SetStartStop dirties the timeline");
    CHECK_HR(IAMTimeline_GetDirtyRange(tl, &start, &stop), S_OK);
    CHECK(start == 0 && stop == 30000000, "dirty range %lld..%lld", start, stop);
    CHECK_HR(IAMTimelineObj_SetStartStop(g[2], 40000000, 50000000), S_OK);
    IAMTimeline_GetDirtyRange(tl, &start, &stop);
    CHECK(start == 0 && stop == 50000000, "merged dirty range %lld..%lld", start, stop);
    {
        REFERENCE_TIME dur;
        double dur2;
        IAMTimeline_GetDuration(tl, &dur);
        CHECK(dur == 50000000, "duration %lld", dur);
        IAMTimeline_GetDuration2(tl, &dur2);
        CHECK(fabs(dur2 - 5.0) < 1e-9, "duration2 %f", dur2);
    }
    for (i = 0; i < 3; i++)
        IAMTimelineObj_ClearDirty(g[i]);

    /* removal */
    CHECK_HR(IAMTimeline_RemGroupFromList(tl, comp), E_INVALIDARG);
    CHECK_HR(IAMTimeline_RemGroupFromList(tl, g[0]), S_OK);
    ref = refs(g[0]);
    CHECK(ref == 1, "group refcount after removal %lu", ref);
    CHECK_HR(IAMTimeline_RemGroupFromList(tl, g[0]), E_INVALIDARG);
    IAMTimeline_GetGroupCount(tl, &count);
    CHECK(count == 2, "count after removal %ld", count);
    IAMTimelineObj_QueryInterface(g[0], &IID_IAMTimelineGroup, (void **)&group);
    CHECK_HR(IAMTimelineGroup_GetTimeline(group, &tl2), E_NOINTERFACE);
    CHECK(tl2 == NULL, "detached group timeline");
    CHECK_HR(IAMTimelineGroup_GetPriority(group, &prio), E_NOINTERFACE);
    IAMTimelineGroup_Release(group);
    IAMTimelineObj_QueryInterface(g[2], &IID_IAMTimelineGroup, (void **)&group);
    CHECK_HR(IAMTimelineGroup_GetPriority(group, &prio), S_OK);
    CHECK(prio == 1, "priority after removal %ld", prio);
    IAMTimelineGroup_Release(group);

    /* Remove / RemoveAll on the object itself */
    CHECK_HR(IAMTimelineObj_Remove(g[1]), S_OK);
    IAMTimeline_GetGroupCount(tl, &count);
    CHECK(count == 1, "count after Remove %ld", count);
    CHECK_HR(IAMTimelineObj_Remove(g[1]), S_OK);
    CHECK_HR(IAMTimelineObj_RemoveAll(g[2]), S_OK);
    IAMTimeline_GetGroupCount(tl, &count);
    CHECK(count == 0, "count after RemoveAll %ld", count);

    /* a group outlives its timeline */
    CHECK_HR(IAMTimeline_AddGroup(tl, g[0]), S_OK);
    CHECK_HR(IAMTimeline_AddGroup(tl, g[1]), S_OK);
    CHECK_HR(IAMTimeline_ClearAllGroups(tl), S_OK);
    IAMTimeline_GetGroupCount(tl, &count);
    CHECK(count == 0, "count after ClearAllGroups %ld", count);
    ref = refs(g[1]);
    CHECK(ref == 1, "refcount after ClearAllGroups %lu", ref);

    hr = CoCreateInstance(&CLSID_AMTimeline, NULL, CLSCTX_INPROC_SERVER, &IID_IAMTimeline, (void **)&tl2);
    CHECK_HR(hr, S_OK);
    obj2 = make_obj(tl2, TIMELINE_MAJOR_TYPE_GROUP);
    CHECK_HR(IAMTimeline_AddGroup(tl2, obj2), S_OK);
    CHECK_HR(IAMTimeline_AddGroup(tl, obj2), E_INVALIDARG);
    IAMTimeline_Release(tl2);
    IAMTimelineObj_QueryInterface(obj2, &IID_IAMTimelineGroup, (void **)&group);
    {
        IAMTimeline *none = (IAMTimeline *)0xdead;
        CHECK_HR(IAMTimelineGroup_GetTimeline(group, &none), E_NOINTERFACE);
        CHECK(none == NULL, "timeline after release");
    }
    IAMTimelineGroup_Release(group);
    ref = IAMTimelineObj_Release(obj2);
    CHECK(ref == 0, "obj2 leaked, ref %lu", ref);

    (void)obj;
    IAMTimelineObj_Release(comp);
    for (i = 0; i < 3; i++)
    {
        ref = IAMTimelineObj_Release(g[i]);
        CHECK(ref == 0, "group %d leaked, ref %lu", i, ref);
    }
}

static void test_obj(IAMTimeline *tl)
{
    IAMTimelineObj *comp, *grp, *grpb;
    IAMTimelineGroup *group, *group2;
    REFERENCE_TIME start, stop;
    REFTIME rstart, rstop;
    LONG id, id2, size, depth;
    BYTE data[8], buf[16];
    BOOL flag;
    BSTR name;
    GUID guid;
    IUnknown *sub, *sub2;
    IAMTimeline *tl2;
    IPropertySetter *setter;
    HRESULT hr;

    comp = make_obj(tl, TIMELINE_MAJOR_TYPE_COMPOSITE);
    grp = make_obj(tl, TIMELINE_MAJOR_TYPE_GROUP);
    grpb = make_obj(tl, TIMELINE_MAJOR_TYPE_GROUP);

    CHECK_HR(IAMTimelineObj_QueryInterface(comp, &IID_IAMTimelineGroup, (void **)&group), E_NOINTERFACE);
    CHECK_HR(IAMTimelineObj_QueryInterface(grp, &IID_IAMTimelineGroup, (void **)&group), S_OK);
    IAMTimelineObj_QueryInterface(grpb, &IID_IAMTimelineGroup, (void **)&group2);

    /* start / stop */
    start = stop = 77;
    CHECK_HR(IAMTimelineObj_GetStartStop(comp, &start, &stop), S_OK);
    CHECK(start == 0 && stop == 0, "default start/stop %lld/%lld", start, stop);
    CHECK_HR(IAMTimelineObj_GetStartStop(comp, NULL, &stop), E_POINTER);
    CHECK_HR(IAMTimelineObj_GetStartStop(comp, &start, NULL), E_POINTER);
    CHECK_HR(IAMTimelineObj_GetStartStop2(comp, NULL, &rstop), E_POINTER);
    CHECK_HR(IAMTimelineObj_SetStartStop(comp, -1, 5), E_INVALIDARG);
    CHECK_HR(IAMTimelineObj_SetStartStop(comp, 20, 10), E_INVALIDARG);
    CHECK_HR(IAMTimelineObj_SetStartStop(comp, 10000000, 30000000), S_OK);
    IAMTimelineObj_GetStartStop(comp, &start, &stop);
    CHECK(start == 10000000 && stop == 30000000, "start/stop %lld/%lld", start, stop);
    CHECK_HR(IAMTimelineObj_GetStartStop2(comp, &rstart, &rstop), S_OK);
    CHECK(fabs(rstart - 1.0) < 1e-9 && fabs(rstop - 3.0) < 1e-9, "start/stop2 %f/%f", rstart, rstop);
    CHECK_HR(IAMTimelineObj_SetStartStop2(comp, 2.5, 4.0), S_OK);
    IAMTimelineObj_GetStartStop(comp, &start, &stop);
    CHECK(start == 25000000 && stop == 40000000, "start/stop after SetStartStop2 %lld/%lld", start, stop);
    CHECK_HR(IAMTimelineObj_SetStartStop2(comp, 4.0, 2.0), E_INVALIDARG);

    /* dirty range follows the changes */
    CHECK_HR(IAMTimelineObj_GetDirtyRange(comp, &start, &stop), S_OK);
    CHECK(start == 0 && stop == 40000000, "dirty range %lld/%lld", start, stop);
    CHECK_HR(IAMTimelineObj_GetDirtyRange2(comp, &rstart, &rstop), S_OK);
    CHECK(fabs(rstart) < 1e-9 && fabs(rstop - 4.0) < 1e-9, "dirty range2 %f/%f", rstart, rstop);
    CHECK_HR(IAMTimelineObj_ClearDirty(comp), S_OK);
    IAMTimelineObj_GetDirtyRange(comp, &start, &stop);
    CHECK(start == 0 && stop == 0, "cleared dirty range %lld/%lld", start, stop);
    CHECK_HR(IAMTimelineObj_SetDirtyRange(comp, 7, 3), E_INVALIDARG);
    CHECK_HR(IAMTimelineObj_SetDirtyRange(comp, 100, 200), S_OK);
    IAMTimelineObj_GetDirtyRange(comp, &start, &stop);
    CHECK(start == 100 && stop == 200, "set dirty range %lld/%lld", start, stop);
    CHECK_HR(IAMTimelineObj_SetDirtyRange2(comp, 1.0, 2.0), S_OK);
    IAMTimelineObj_GetDirtyRange(comp, &start, &stop);
    CHECK(start == 10000000 && stop == 20000000, "set dirty range2 %lld/%lld", start, stop);
    CHECK_HR(IAMTimelineObj_GetDirtyRange(comp, NULL, &stop), E_POINTER);
    IAMTimelineObj_ClearDirty(comp);

    /* FixTimes: outside a timeline nothing changes; a group snaps to its frame rate */
    start = 700000; stop = 1400000;
    CHECK_HR(IAMTimelineObj_FixTimes(comp, &start, &stop), S_OK);
    CHECK(start == 700000 && stop == 1400000, "unattached FixTimes changed %lld/%lld", start, stop);
    CHECK_HR(IAMTimeline_AddGroup(tl, grp), S_OK);
    start = 700000; stop = 1400000;
    CHECK_HR(IAMTimelineObj_FixTimes(grp, &start, &stop), S_OK);
    CHECK(start == 666667 && stop == 1333333, "group FixTimes %lld/%lld", start, stop);
    CHECK_HR(IAMTimelineGroup_SetOutputFPS(group, 10.0), S_OK);
    start = 700000; stop = 1400000;
    IAMTimelineObj_FixTimes(grp, &start, &stop);
    CHECK(start == 1000000 && stop == 1000000, "FixTimes at 10 fps %lld/%lld", start, stop);
    rstart = 0.07; rstop = 0.16;
    CHECK_HR(IAMTimelineObj_FixTimes2(grp, &rstart, &rstop), S_OK);
    CHECK(fabs(rstart - 0.1) < 1e-9 && fabs(rstop - 0.2) < 1e-9, "FixTimes2 %f/%f", rstart, rstop);
    IAMTimelineGroup_SetOutputFPS(group, 15.0);

    /* ids */
    CHECK_HR(IAMTimelineObj_GetUserID(comp, &id), S_OK);
    CHECK(id == 0, "default user id %ld", id);
    CHECK_HR(IAMTimelineObj_SetUserID(comp, 1234), S_OK);
    IAMTimelineObj_GetUserID(comp, &id);
    CHECK(id == 1234, "user id %ld", id);
    CHECK_HR(IAMTimelineObj_GetUserID(comp, NULL), E_POINTER);
    CHECK_HR(IAMTimelineObj_GetGenID(comp, &id), S_OK);
    CHECK_HR(IAMTimelineObj_GetGenID(grp, &id2), S_OK);
    CHECK(id != id2 && id && id2, "gen ids %ld %ld", id, id2);
    CHECK_HR(IAMTimelineObj_GetGenID(comp, NULL), E_POINTER);

    /* name */
    name = (BSTR)0xdead;
    CHECK_HR(IAMTimelineObj_GetUserName(comp, &name), S_OK);
    CHECK(name == NULL, "default user name");
    name = SysAllocString(L"first clip");
    CHECK_HR(IAMTimelineObj_SetUserName(comp, name), S_OK);
    SysFreeString(name);
    name = NULL;
    CHECK_HR(IAMTimelineObj_GetUserName(comp, &name), S_OK);
    CHECK(name && !wcscmp(name, L"first clip"), "user name %ls", name ? name : L"(null)");
    SysFreeString(name);
    CHECK_HR(IAMTimelineObj_GetUserName(comp, NULL), E_POINTER);
    CHECK_HR(IAMTimelineObj_SetUserName(comp, NULL), S_OK);
    IAMTimelineObj_GetUserName(comp, &name);
    CHECK(name == NULL, "cleared user name");

    /* user data */
    size = 99;
    CHECK_HR(IAMTimelineObj_GetUserData(comp, NULL, &size), S_OK);
    CHECK(size == 0, "default data size %ld", size);
    CHECK_HR(IAMTimelineObj_GetUserData(comp, buf, NULL), E_POINTER);
    CHECK_HR(IAMTimelineObj_SetUserData(comp, NULL, 4), E_POINTER);
    CHECK_HR(IAMTimelineObj_SetUserData(comp, data, -1), E_INVALIDARG);
    memcpy(data, "abcdefgh", 8);
    CHECK_HR(IAMTimelineObj_SetUserData(comp, data, 8), S_OK);
    memset(data, 0, 8);
    size = 0;
    CHECK_HR(IAMTimelineObj_GetUserData(comp, NULL, &size), S_OK);
    CHECK(size == 8, "data size %ld", size);
    memset(buf, 0, sizeof(buf));
    size = sizeof(buf);
    CHECK_HR(IAMTimelineObj_GetUserData(comp, buf, &size), S_OK);
    CHECK(size == 8 && !memcmp(buf, "abcdefgh", 8), "data roundtrip size %ld", size);
    CHECK_HR(IAMTimelineObj_SetUserData(comp, NULL, 0), S_OK);
    IAMTimelineObj_GetUserData(comp, NULL, &size);
    CHECK(size == 0, "cleared data size %ld", size);

    /* flags */
    CHECK_HR(IAMTimelineObj_GetMuted(comp, &flag), S_OK);
    CHECK(flag == FALSE, "default muted");
    CHECK_HR(IAMTimelineObj_GetLocked(comp, &flag), S_OK);
    CHECK(flag == FALSE, "default locked");
    CHECK_HR(IAMTimelineObj_SetMuted(comp, 7), S_OK);
    IAMTimelineObj_GetMuted(comp, &flag);
    CHECK(flag == TRUE, "muted %d", flag);
    CHECK_HR(IAMTimelineObj_SetLocked(comp, 3), S_OK);
    IAMTimelineObj_GetLocked(comp, &flag);
    CHECK(flag == TRUE, "locked %d", flag);
    CHECK_HR(IAMTimelineObj_GetMuted(comp, NULL), E_POINTER);
    CHECK_HR(IAMTimelineObj_GetLocked(comp, NULL), E_POINTER);
    IAMTimelineObj_ClearDirty(comp);
    IAMTimelineObj_SetMuted(comp, FALSE);
    IAMTimelineObj_GetDirtyRange(comp, &start, &stop);
    CHECK(start == 25000000 && stop == 40000000, "unmute dirty range %lld/%lld", start, stop);

    /* property setter */
    setter = (IPropertySetter *)0xdead;
    CHECK_HR(IAMTimelineObj_GetPropertySetter(comp, &setter), S_OK);
    CHECK(setter == NULL, "default setter");
    CHECK_HR(IAMTimelineObj_GetPropertySetter(comp, NULL), E_POINTER);
    CHECK_HR(IAMTimelineObj_SetPropertySetter(comp, NULL), S_OK);

    /* sub-object */
    CHECK_HR(IAMTimelineObj_GetSubObjectLoaded(comp, &flag), S_OK);
    CHECK(flag == FALSE, "sub-object loaded by default");
    sub = (IUnknown *)0xdead;
    CHECK_HR(IAMTimelineObj_GetSubObject(comp, &sub), S_OK);
    CHECK(sub == NULL, "default sub-object");
    CHECK_HR(IAMTimelineObj_GetSubObject(comp, NULL), E_POINTER);
    CHECK_HR(IAMTimelineObj_GetSubObjectLoaded(comp, NULL), E_POINTER);
    CHECK_HR(IAMTimelineObj_GetSubObjectGUID(comp, &guid), S_OK);
    CHECK(IsEqualGUID(&guid, &GUID_NULL), "default sub-object guid");
    CHECK_HR(IAMTimelineObj_GetSubObjectGUID(comp, NULL), E_POINTER);

    {
        ULONG before = refs(tl);
        CHECK_HR(IAMTimelineObj_SetSubObject(comp, (IUnknown *)tl), S_OK);
        CHECK(refs(tl) == before + 1, "SetSubObject takes a reference");
        CHECK_HR(IAMTimelineObj_GetSubObjectLoaded(comp, &flag), S_OK);
        CHECK(flag == TRUE, "loaded after SetSubObject");
        sub = NULL;
        CHECK_HR(IAMTimelineObj_GetSubObject(comp, &sub), S_OK);
        CHECK(sub != NULL, "GetSubObject");
        if (sub)
        {
            hr = IUnknown_QueryInterface(sub, &IID_IAMTimeline, (void **)&tl2);
            CHECK_HR(hr, S_OK);
            CHECK(tl2 == tl, "sub-object identity");
            if (tl2) IAMTimeline_Release(tl2);
            IUnknown_Release(sub);
        }
        CHECK_HR(IAMTimelineObj_SetSubObject(comp, NULL), S_OK);
        CHECK(refs(tl) == before, "SetSubObject(NULL) drops the reference");
    }

    /* naming a CLSID instantiates on demand */
    CHECK_HR(IAMTimelineObj_SetSubObjectGUID(comp, CLSID_AMTimeline), S_OK);
    IAMTimelineObj_GetSubObjectLoaded(comp, &flag);
    CHECK(flag == FALSE, "not loaded until used");
    IAMTimelineObj_GetSubObjectGUID(comp, &guid);
    CHECK(IsEqualGUID(&guid, &CLSID_AMTimeline), "guid roundtrip");
    sub = NULL;
    CHECK_HR(IAMTimelineObj_GetSubObject(comp, &sub), S_OK);
    CHECK(sub != NULL, "sub-object created from CLSID");
    if (sub)
    {
        CHECK(IUnknown_QueryInterface(sub, &IID_IAMTimeline, (void **)&tl2) == S_OK, "created object is a timeline");
        if (tl2) IAMTimeline_Release(tl2);
        sub2 = NULL;
        IAMTimelineObj_GetSubObject(comp, &sub2);
        CHECK(sub2 == sub, "same sub-object returned again");
        if (sub2) IUnknown_Release(sub2);
        IUnknown_Release(sub);
    }
    IAMTimelineObj_GetSubObjectLoaded(comp, &flag);
    CHECK(flag == TRUE, "loaded after use");
    name = NULL;
    CHECK_HR(IAMTimelineObj_GetSubObjectGUIDB(comp, &name), S_OK);
    CHECK(name && !wcsicmp(name, L"{78530B75-61F9-11D2-8CAD-00A024580902}"), "guid B %ls", name ? name : L"(null)");
    SysFreeString(name);
    CHECK_HR(IAMTimelineObj_GetSubObjectGUIDB(comp, NULL), E_POINTER);
    name = SysAllocString(L"{12345678-1234-5678-0102-030405060708}");
    CHECK_HR(IAMTimelineObj_SetSubObjectGUIDB(comp, name), S_OK);
    SysFreeString(name);
    IAMTimelineObj_GetSubObjectLoaded(comp, &flag);
    CHECK(flag == FALSE, "new CLSID drops the old object");
    IAMTimelineObj_GetSubObjectGUID(comp, &guid);
    CHECK(guid.Data1 == 0x12345678 && guid.Data4[7] == 8, "guid from B string");
    name = SysAllocString(L"junk");
    CHECK_HR(IAMTimelineObj_SetSubObjectGUIDB(comp, name), E_INVALIDARG);
    SysFreeString(name);
    CHECK_HR(IAMTimelineObj_SetSubObjectGUIDB(comp, NULL), E_POINTER);
    /* unknown CLSID: instantiation failure is reported */
    sub = (IUnknown *)0xdead;
    hr = IAMTimelineObj_GetSubObject(comp, &sub);
    CHECK(FAILED(hr) && sub == NULL, "unregistered CLSID gives hr %#lx", (unsigned long)hr);

    /* group / depth / timeline back references */
    {
        IAMTimelineGroup *belongs = NULL;
        CHECK_HR(IAMTimelineObj_GetGroupIBelongTo(grp, &belongs), S_OK);
        CHECK(belongs == group, "group belongs to itself: %p vs %p", belongs, group);
        if (belongs) IAMTimelineGroup_Release(belongs);
        belongs = (IAMTimelineGroup *)0xdead;
        CHECK_HR(IAMTimelineObj_GetGroupIBelongTo(comp, &belongs), E_NOINTERFACE);
        CHECK(belongs == NULL, "output on failure");
        CHECK_HR(IAMTimelineObj_GetGroupIBelongTo(comp, NULL), E_POINTER);
    }
    CHECK_HR(IAMTimelineObj_GetEmbedDepth(grp, &depth), S_OK);
    CHECK(depth == 0, "embed depth %ld", depth);
    CHECK_HR(IAMTimelineObj_GetEmbedDepth(grp, NULL), E_POINTER);

    /* group properties */
    {
        double fps;
        int buffering;
        BOOL preview, set, rdirty;
        AM_MEDIA_TYPE mt = {0}, out = {0};
        WAVEFORMATEX wfx = {WAVE_FORMAT_PCM, 2, 44100, 176400, 4, 16, 0};
        LONG *fmt = NULL;

        CHECK_HR(IAMTimelineGroup_GetOutputFPS(group, &fps), S_OK);
        CHECK(fps == 15.0, "default output fps %f", fps);
        CHECK_HR(IAMTimelineGroup_SetOutputFPS(group, 0.0), E_INVALIDARG);
        CHECK_HR(IAMTimelineGroup_SetOutputFPS(group, 25.0), S_OK);
        IAMTimelineGroup_GetOutputFPS(group, &fps);
        CHECK(fps == 25.0, "output fps %f", fps);
        CHECK_HR(IAMTimelineGroup_GetOutputFPS(group, NULL), E_POINTER);

        name = (BSTR)0xdead;
        CHECK_HR(IAMTimelineGroup_GetGroupName(group, &name), S_OK);
        CHECK(name == NULL, "default group name");
        name = SysAllocString(L"video 1");
        CHECK_HR(IAMTimelineGroup_SetGroupName(group, name), S_OK);
        SysFreeString(name);
        name = NULL;
        IAMTimelineGroup_GetGroupName(group, &name);
        CHECK(name && !wcscmp(name, L"video 1"), "group name");
        SysFreeString(name);
        CHECK_HR(IAMTimelineGroup_GetGroupName(group, NULL), E_POINTER);

        CHECK_HR(IAMTimelineGroup_GetPreviewMode(group, &preview), S_OK);
        CHECK(!preview, "default preview");
        CHECK_HR(IAMTimelineGroup_SetPreviewMode(group, 9), S_OK);
        IAMTimelineGroup_GetPreviewMode(group, &preview);
        CHECK(preview == TRUE, "preview %d", preview);
        CHECK_HR(IAMTimelineGroup_GetPreviewMode(group, NULL), E_POINTER);

        CHECK_HR(IAMTimelineGroup_GetOutputBuffering(group, &buffering), S_OK);
        CHECK(buffering == 30, "default buffering %d", buffering);
        CHECK_HR(IAMTimelineGroup_SetOutputBuffering(group, 0), E_INVALIDARG);
        CHECK_HR(IAMTimelineGroup_SetOutputBuffering(group, 5), S_OK);
        IAMTimelineGroup_GetOutputBuffering(group, &buffering);
        CHECK(buffering == 5, "buffering %d", buffering);
        CHECK_HR(IAMTimelineGroup_GetOutputBuffering(group, NULL), E_POINTER);

        CHECK_HR(IAMTimelineGroup_SetMediaType(group, NULL), E_POINTER);
        CHECK_HR(IAMTimelineGroup_GetMediaType(group, NULL), E_POINTER);
        mt.majortype = MEDIATYPE_Audio;
        mt.subtype = MEDIASUBTYPE_PCM;
        mt.formattype = FORMAT_WaveFormatEx;
        mt.cbFormat = sizeof(wfx);
        mt.pbFormat = (BYTE *)&wfx;
        CHECK_HR(IAMTimelineGroup_SetMediaType(group, &mt), S_OK);
        CHECK_HR(IAMTimelineGroup_GetMediaType(group, &out), S_OK);
        CHECK(IsEqualGUID(&out.majortype, &MEDIATYPE_Audio) && IsEqualGUID(&out.subtype, &MEDIASUBTYPE_PCM),
                "media type guids");
        CHECK(out.cbFormat == sizeof(wfx) && out.pbFormat && out.pbFormat != (BYTE *)&wfx
                && !memcmp(out.pbFormat, &wfx, sizeof(wfx)), "media type format copy");
        CoTaskMemFree(out.pbFormat);
        CHECK(IAMTimelineGroup_SetMediaTypeForVB(group, 2) == E_INVALIDARG, "VB type 2");
        CHECK_HR(IAMTimelineGroup_SetMediaTypeForVB(group, 0), S_OK);
        memset(&out, 0, sizeof(out));
        IAMTimelineGroup_GetMediaType(group, &out);
        CHECK(IsEqualGUID(&out.majortype, &MEDIATYPE_Video), "VB video type");
        CoTaskMemFree(out.pbFormat);
        CHECK_HR(IAMTimelineGroup_SetMediaTypeForVB(group, 1), S_OK);
        memset(&out, 0, sizeof(out));
        IAMTimelineGroup_GetMediaType(group, &out);
        CHECK(IsEqualGUID(&out.majortype, &MEDIATYPE_Audio), "VB audio type");
        CoTaskMemFree(out.pbFormat);

        CHECK_HR(IAMTimelineGroup_IsSmartRecompressFormatSet(group, &set), S_OK);
        CHECK(!set, "recompress set by default");
        CHECK_HR(IAMTimelineGroup_IsRecompressFormatDirty(group, &rdirty), S_OK);
        CHECK(!rdirty, "recompress dirty by default");
        CHECK_HR(IAMTimelineGroup_GetSmartRecompressFormat(group, &fmt), S_FALSE);
        CHECK(fmt == NULL, "no recompress format");
        CHECK_HR(IAMTimelineGroup_SetSmartRecompressFormat(group, NULL), E_POINTER);
        CHECK_HR(IAMTimelineGroup_SetSmartRecompressFormat(group, (LONG *)&mt), S_OK);
        IAMTimelineGroup_IsSmartRecompressFormatSet(group, &set);
        CHECK(set, "recompress set");
        IAMTimelineGroup_IsRecompressFormatDirty(group, &rdirty);
        CHECK(rdirty, "recompress dirty");
        CHECK_HR(IAMTimelineGroup_GetSmartRecompressFormat(group, &fmt), S_OK);
        if (fmt)
        {
            AM_MEDIA_TYPE *got = (AM_MEDIA_TYPE *)fmt;
            CHECK(IsEqualGUID(&got->majortype, &MEDIATYPE_Audio) && got->cbFormat == sizeof(wfx)
                    && !memcmp(got->pbFormat, &wfx, sizeof(wfx)), "recompress format copy");
            CoTaskMemFree(got->pbFormat);
            CoTaskMemFree(got);
        }
        else
            CHECK(0, "no recompress format returned");
        CHECK_HR(IAMTimelineGroup_ClearRecompressFormatDirty(group), S_OK);
        IAMTimelineGroup_IsRecompressFormatDirty(group, &rdirty);
        CHECK(!rdirty, "recompress dirty cleared");
        CHECK_HR(IAMTimelineGroup_IsSmartRecompressFormatSet(group, NULL), E_POINTER);
        CHECK_HR(IAMTimelineGroup_IsRecompressFormatDirty(group, NULL), E_POINTER);
        CHECK_HR(IAMTimelineGroup_SetRecompFormatFromSource(group, NULL), E_POINTER);

        /* SetTimeline only accepts the timeline the group is in */
        CHECK_HR(IAMTimelineGroup_SetTimeline(group, tl), S_OK);
        CHECK_HR(IAMTimelineGroup_SetTimeline(group2, tl), E_INVALIDARG);
        CHECK_HR(IAMTimelineGroup_GetTimeline(group, NULL), E_POINTER);
        CHECK_HR(IAMTimelineGroup_GetPriority(group, NULL), E_POINTER);
    }

    IAMTimelineGroup_Release(group);
    IAMTimelineGroup_Release(group2);
    IAMTimeline_RemGroupFromList(tl, grp);
    IAMTimelineObj_Release(comp);
    IAMTimelineObj_Release(grp);
    IAMTimelineObj_Release(grpb);
}

int main(void)
{
    IAMTimeline *tl;
    HRESULT hr;

    CoInitialize(NULL);
    hr = CoCreateInstance(&CLSID_AMTimeline, NULL, CLSCTX_INPROC_SERVER, &IID_IAMTimeline, (void **)&tl);
    if (FAILED(hr))
    {
        printf("FAIL  cannot create timeline: %#lx\n", (unsigned long)hr);
        printf("RESULT: FAIL\n");
        return 1;
    }

    test_timeline(tl);
    test_groups(tl);
    test_obj(tl);

    {
        ULONG ref = IAMTimeline_Release(tl);
        CHECK(ref == 0, "timeline leaked, ref %lu", ref);
    }
    CoUninitialize();

    printf("%d checks, %d failures\n", checks, failures);
    printf(failures ? "RESULT: FAIL\n" : "RESULT: PASS\n");
    return failures != 0;
}
