/* uianimation RepeatBetweenKeyframes and Conclude (patch 3300), run by test/fastuiarepeat-gate.sh */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <uianimation.h>
#include <stdio.h>
#include <math.h>

static int failures;
static void check(int ok, const char *what)
{
    printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
    fflush(stdout);
    if (!ok) failures++;
}
#define CHECKF(ok, ...) do { char _b[300]; snprintf(_b, sizeof(_b), __VA_ARGS__); check(ok, _b); } while (0)
#define CLOSE(a, b) (fabs((a) - (b)) < 1e-6)

#ifndef UI_E_START_KEYFRAME_AFTER_END
#define UI_E_START_KEYFRAME_AFTER_END ((HRESULT)0x802A0103)
#define UI_E_END_KEYFRAME_NOT_DETERMINED ((HRESULT)0x802A0104)
#define UI_E_LOOPS_OVERLAP ((HRESULT)0x802A0105)
#define UI_E_STORYBOARD_ACTIVE ((HRESULT)0x802A0101)
#endif

static IUIAnimationManager *mgr;
static IUIAnimationTransitionLibrary *lib;

static double value_at(IUIAnimationVariable *var, double t)
{
    double v = -1;
    IUIAnimationManager_Update(mgr, t, NULL);
    IUIAnimationVariable_GetValue(var, &v);
    return v;
}

static UI_ANIMATION_STORYBOARD_STATUS status(IUIAnimationStoryboard *sb)
{
    UI_ANIMATION_STORYBOARD_STATUS st = -1;
    IUIAnimationStoryboard_GetStatus(sb, &st);
    return st;
}

int main(void)
{
    IUIAnimationVariable *var;
    IUIAnimationStoryboard *sb;
    IUIAnimationTransition *tr, *tr2;
    UI_ANIMATION_KEYFRAME k1, k2, k3;
    HRESULT hr;
    double v;

    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    CoCreateInstance(&CLSID_UIAnimationManager, NULL, CLSCTX_INPROC_SERVER, &IID_IUIAnimationManager, (void **)&mgr);
    CoCreateInstance(&CLSID_UIAnimationTransitionLibrary, NULL, CLSCTX_INPROC_SERVER, &IID_IUIAnimationTransitionLibrary, (void **)&lib);
    if (!mgr || !lib) { check(0, "create manager and library"); return 1; }

    /* a one second ramp 0..1 played three times */
    IUIAnimationManager_CreateAnimationVariable(mgr, 0.0, &var);
    IUIAnimationManager_CreateStoryboard(mgr, &sb);
    IUIAnimationTransitionLibrary_CreateLinearTransition(lib, 1.0, 1.0, &tr);
    IUIAnimationStoryboard_AddTransition(sb, var, tr);
    IUIAnimationStoryboard_AddKeyframeAfterTransition(sb, tr, &k1);
    hr = IUIAnimationStoryboard_RepeatBetweenKeyframes(sb, UI_ANIMATION_KEYFRAME_STORYBOARD_START, k1, 2);
    CHECKF(hr == S_OK, "repeat the whole ramp twice more (%#lx)", hr);
    IUIAnimationStoryboard_Schedule(sb, 10.0, NULL);
    hr = IUIAnimationStoryboard_RepeatBetweenKeyframes(sb, UI_ANIMATION_KEYFRAME_STORYBOARD_START, k1, 1);
    CHECKF(hr == UI_E_STORYBOARD_ACTIVE, "repeat on a scheduled storyboard (%#lx)", hr);
    v = value_at(var, 10.5);  CHECKF(CLOSE(v, 0.5), "first pass at 0.5 s: %g", v);
    v = value_at(var, 11.25); CHECKF(CLOSE(v, 0.25), "second pass at 1.25 s: %g", v);
    v = value_at(var, 12.5);  CHECKF(CLOSE(v, 0.5), "third pass at 2.5 s: %g", v);
    CHECKF(status(sb) == UI_ANIMATION_STORYBOARD_PLAYING, "still playing at 2.5 s (%d)", status(sb));
    v = value_at(var, 12.9);  CHECKF(CLOSE(v, 0.9), "third pass at 2.9 s: %g", v);
    v = value_at(var, 13.0);  CHECKF(CLOSE(v, 1.0), "the end of the last pass: %g", v);
    CHECKF(status(sb) == UI_ANIMATION_STORYBOARD_READY, "finished after three passes (%d)", status(sb));
    IUIAnimationStoryboard_Release(sb);
    IUIAnimationTransition_Release(tr);
    IUIAnimationVariable_Release(var);

    /* a loop inside the storyboard: keyframes at 0.25 and 0.75 of a 1 s ramp, repeated once; the rest is shifted */
    IUIAnimationManager_CreateAnimationVariable(mgr, 0.0, &var);
    IUIAnimationManager_CreateStoryboard(mgr, &sb);
    IUIAnimationTransitionLibrary_CreateLinearTransition(lib, 1.0, 1.0, &tr);
    IUIAnimationTransitionLibrary_CreateLinearTransition(lib, 1.0, 5.0, &tr2);
    IUIAnimationStoryboard_AddTransition(sb, var, tr);
    IUIAnimationStoryboard_AddTransition(sb, var, tr2);
    IUIAnimationStoryboard_AddKeyframeAtOffset(sb, UI_ANIMATION_KEYFRAME_STORYBOARD_START, 0.25, &k1);
    IUIAnimationStoryboard_AddKeyframeAtOffset(sb, UI_ANIMATION_KEYFRAME_STORYBOARD_START, 0.75, &k2);
    hr = IUIAnimationStoryboard_RepeatBetweenKeyframes(sb, k1, k2, 1);
    CHECKF(hr == S_OK, "repeat between two offsets (%#lx)", hr);
    hr = IUIAnimationStoryboard_RepeatBetweenKeyframes(sb, k1, k2, 1);
    CHECKF(hr == UI_E_LOOPS_OVERLAP, "the same loop twice overlaps (%#lx)", hr);
    hr = IUIAnimationStoryboard_RepeatBetweenKeyframes(sb, UI_ANIMATION_KEYFRAME_STORYBOARD_START, k2, 1);
    CHECKF(hr == UI_E_LOOPS_OVERLAP, "a loop around another overlaps (%#lx)", hr);
    IUIAnimationStoryboard_AddKeyframeAtOffset(sb, UI_ANIMATION_KEYFRAME_STORYBOARD_START, 1.0, &k3);
    hr = IUIAnimationStoryboard_RepeatBetweenKeyframes(sb, k2, k3, 0);
    CHECKF(hr == S_OK, "a loop that starts where another ends, zero repeats (%#lx)", hr);
    IUIAnimationStoryboard_Schedule(sb, 20.0, NULL);
    v = value_at(var, 20.5);  CHECKF(CLOSE(v, 0.5), "before the loop's end: %g", v);
    v = value_at(var, 20.8);  CHECKF(CLOSE(v, 0.3), "the loop's second pass at 0.8 s: %g", v);
    v = value_at(var, 21.25); CHECKF(CLOSE(v, 0.75 + (1.25 - 1.25) * 0), "the second ramp is shifted by the pass: 1.25 s is at 0.75 of the first ramp: %g", v);
    v = value_at(var, 22.0);  CHECKF(CLOSE(v, 0.0 + 1.0 + 4.0 * 0.5), "the second transition starts at 1.5 s: at 2.0 s half way from 1 to 5: %g", v);
    v = value_at(var, 22.6);
    CHECKF(status(sb) == UI_ANIMATION_STORYBOARD_READY && CLOSE(v, 5.0), "the storyboard ends after 2.5 s (%g, %d)", v, status(sb));
    IUIAnimationStoryboard_Release(sb);
    IUIAnimationTransition_Release(tr);
    IUIAnimationTransition_Release(tr2);
    IUIAnimationVariable_Release(var);

    /* errors */
    IUIAnimationManager_CreateAnimationVariable(mgr, 0.0, &var);
    IUIAnimationManager_CreateStoryboard(mgr, &sb);
    IUIAnimationTransitionLibrary_CreateLinearTransition(lib, 1.0, 1.0, &tr);
    IUIAnimationTransitionLibrary_CreateLinearTransitionFromSpeed(lib, 2.0, 4.0, &tr2);
    IUIAnimationStoryboard_AddTransition(sb, var, tr);
    IUIAnimationStoryboard_AddKeyframeAtOffset(sb, UI_ANIMATION_KEYFRAME_STORYBOARD_START, 1.0, &k1);
    IUIAnimationStoryboard_AddKeyframeAtOffset(sb, UI_ANIMATION_KEYFRAME_STORYBOARD_START, 0.5, &k2);
    hr = IUIAnimationStoryboard_RepeatBetweenKeyframes(sb, k1, k2, 1);
    CHECKF(hr == UI_E_START_KEYFRAME_AFTER_END, "start after end (%#lx)", hr);
    hr = IUIAnimationStoryboard_RepeatBetweenKeyframes(sb, k2, k1, -2);
    CHECKF(hr == E_INVALIDARG, "a count below -1 (%#lx)", hr);
    hr = IUIAnimationStoryboard_RepeatBetweenKeyframes(sb, (UI_ANIMATION_KEYFRAME)(ULONG_PTR)77, k1, 1);
    CHECKF(hr == E_INVALIDARG, "an unknown keyframe (%#lx)", hr);
    IUIAnimationStoryboard_AddTransition(sb, var, tr2);
    IUIAnimationStoryboard_AddKeyframeAfterTransition(sb, tr2, &k3);
    hr = IUIAnimationStoryboard_RepeatBetweenKeyframes(sb, UI_ANIMATION_KEYFRAME_STORYBOARD_START, k3, 1);
    CHECKF(hr == UI_E_END_KEYFRAME_NOT_DETERMINED, "an end that depends on a constant-speed transition (%#lx)", hr);
    hr = IUIAnimationStoryboard_RepeatBetweenKeyframes(sb, k2, k1, 1);
    CHECKF(hr == S_OK, "a fixed loop is accepted after the errors (%#lx)", hr);
    IUIAnimationStoryboard_Release(sb);
    IUIAnimationTransition_Release(tr);
    IUIAnimationTransition_Release(tr2);
    IUIAnimationVariable_Release(var);

    /* an indefinite loop runs until the storyboard is concluded, then finishes its pass */
    IUIAnimationManager_CreateAnimationVariable(mgr, 0.0, &var);
    IUIAnimationManager_CreateStoryboard(mgr, &sb);
    IUIAnimationTransitionLibrary_CreateLinearTransition(lib, 1.0, 1.0, &tr);
    IUIAnimationTransitionLibrary_CreateLinearTransition(lib, 1.0, 9.0, &tr2);
    IUIAnimationStoryboard_AddTransition(sb, var, tr);
    IUIAnimationStoryboard_AddTransition(sb, var, tr2);
    IUIAnimationStoryboard_AddKeyframeAfterTransition(sb, tr, &k1);
    hr = IUIAnimationStoryboard_RepeatBetweenKeyframes(sb, UI_ANIMATION_KEYFRAME_STORYBOARD_START, k1, UI_ANIMATION_REPEAT_INDEFINITELY);
    CHECKF(hr == S_OK, "an indefinite loop (%#lx)", hr);
    IUIAnimationStoryboard_Schedule(sb, 30.0, NULL);
    v = value_at(var, 130.25); CHECKF(CLOSE(v, 0.25), "a hundred passes later, still looping: %g", v);
    CHECKF(status(sb) == UI_ANIMATION_STORYBOARD_PLAYING, "still playing (%d)", status(sb));
    hr = IUIAnimationVariable_GetFinalValue(var, &v);
    CHECKF(hr == S_OK && CLOSE(v, 9.0), "the final value of a variable in an indefinite loop is where the rest ends: %g", v);
    hr = IUIAnimationStoryboard_Conclude(sb);
    CHECKF(hr == S_OK, "Conclude (%#lx)", hr);
    v = value_at(var, 130.9);  CHECKF(CLOSE(v, 0.9), "the pass in progress goes on after Conclude: %g", v);
    CHECKF(status(sb) == UI_ANIMATION_STORYBOARD_PLAYING, "playing until the pass ends (%d)", status(sb));
    v = value_at(var, 131.5);  CHECKF(CLOSE(v, 1.0 + 8.0 * 0.5), "then the rest plays: half way of the second ramp: %g", v);
    v = value_at(var, 132.1);
    CHECKF(status(sb) == UI_ANIMATION_STORYBOARD_READY && CLOSE(v, 9.0), "and the storyboard ends (%g, %d)", v, status(sb));
    IUIAnimationStoryboard_Release(sb);
    IUIAnimationTransition_Release(tr);
    IUIAnimationTransition_Release(tr2);
    IUIAnimationVariable_Release(var);

    /* finishing a storyboard with an indefinite loop does not loop for ever */
    IUIAnimationManager_CreateAnimationVariable(mgr, 0.0, &var);
    IUIAnimationManager_CreateStoryboard(mgr, &sb);
    IUIAnimationTransitionLibrary_CreateLinearTransition(lib, 1.0, 1.0, &tr);
    IUIAnimationStoryboard_AddTransition(sb, var, tr);
    IUIAnimationStoryboard_AddKeyframeAfterTransition(sb, tr, &k1);
    IUIAnimationStoryboard_RepeatBetweenKeyframes(sb, UI_ANIMATION_KEYFRAME_STORYBOARD_START, k1, UI_ANIMATION_REPEAT_INDEFINITELY);
    IUIAnimationStoryboard_Schedule(sb, 200.0, NULL);
    value_at(var, 200.5);
    hr = IUIAnimationManager_FinishAllStoryboards(mgr, 1.0);
    v = value_at(var, 200.6);
    CHECKF(hr == S_OK && status(sb) == UI_ANIMATION_STORYBOARD_READY && CLOSE(v, 1.0), "FinishAllStoryboards ends an indefinite loop (%#lx, %g, %d)", hr, v, status(sb));
    IUIAnimationStoryboard_Release(sb);
    IUIAnimationTransition_Release(tr);
    IUIAnimationVariable_Release(var);

    /* a storyboard with an indefinite loop that is truncated ends its loop */
    {
        IUIAnimationVariable *va, *vb;
        IUIAnimationStoryboard *sb1, *sb2;
        IUIAnimationTransition *ta, *tb, *tc;

        IUIAnimationManager_CreateAnimationVariable(mgr, 0.0, &va);
        IUIAnimationManager_CreateAnimationVariable(mgr, 0.0, &vb);
        IUIAnimationManager_CreateStoryboard(mgr, &sb1);
        IUIAnimationManager_CreateStoryboard(mgr, &sb2);
        IUIAnimationTransitionLibrary_CreateLinearTransition(lib, 1.0, 1.0, &ta);
        IUIAnimationTransitionLibrary_CreateLinearTransition(lib, 3.0, 3.0, &tb);
        IUIAnimationTransitionLibrary_CreateLinearTransition(lib, 1.0, 5.0, &tc);
        IUIAnimationStoryboard_AddTransition(sb1, va, ta);
        IUIAnimationStoryboard_AddTransition(sb1, vb, tb);
        IUIAnimationStoryboard_AddKeyframeAfterTransition(sb1, ta, &k1);
        IUIAnimationStoryboard_RepeatBetweenKeyframes(sb1, UI_ANIMATION_KEYFRAME_STORYBOARD_START, k1, UI_ANIMATION_REPEAT_INDEFINITELY);
        IUIAnimationStoryboard_AddTransition(sb2, va, tc);
        IUIAnimationStoryboard_Schedule(sb1, 300.0, NULL);
        value_at(va, 300.5);
        IUIAnimationStoryboard_Schedule(sb2, 300.5, NULL);
        CHECKF(status(sb1) == UI_ANIMATION_STORYBOARD_TRUNCATED, "the first storyboard is truncated (%d)", status(sb1));
        v = value_at(vb, 302.5);
        CHECKF(CLOSE(v, 2.5) && status(sb1) == UI_ANIMATION_STORYBOARD_TRUNCATED, "its loop ended: the other variable plays on at 2.5 s (%g, %d)", v, status(sb1));
        v = value_at(vb, 303.1);
        CHECKF(CLOSE(v, 3.0) && status(sb1) == UI_ANIMATION_STORYBOARD_READY, "and it ends at 3 s (%g, %d)", v, status(sb1));
        IUIAnimationStoryboard_Release(sb1);
        IUIAnimationStoryboard_Release(sb2);
        IUIAnimationTransition_Release(ta);
        IUIAnimationTransition_Release(tb);
        IUIAnimationTransition_Release(tc);
        IUIAnimationVariable_Release(va);
        IUIAnimationVariable_Release(vb);
    }

    /* and so does IUIAnimationStoryboard::Finish */
    IUIAnimationManager_CreateAnimationVariable(mgr, 0.0, &var);
    IUIAnimationManager_CreateStoryboard(mgr, &sb);
    IUIAnimationTransitionLibrary_CreateLinearTransition(lib, 1.0, 1.0, &tr);
    IUIAnimationStoryboard_AddTransition(sb, var, tr);
    IUIAnimationStoryboard_AddKeyframeAfterTransition(sb, tr, &k1);
    IUIAnimationStoryboard_RepeatBetweenKeyframes(sb, UI_ANIMATION_KEYFRAME_STORYBOARD_START, k1, UI_ANIMATION_REPEAT_INDEFINITELY);
    IUIAnimationStoryboard_Schedule(sb, 400.0, NULL);
    value_at(var, 400.5);
    hr = IUIAnimationStoryboard_Finish(sb, 1.0);
    v = value_at(var, 400.6);
    CHECKF(hr == S_OK && status(sb) == UI_ANIMATION_STORYBOARD_READY && CLOSE(v, 1.0), "Finish ends an indefinite loop (%#lx, %g, %d)", hr, v, status(sb));
    IUIAnimationStoryboard_Release(sb);
    IUIAnimationTransition_Release(tr);
    IUIAnimationVariable_Release(var);

    IUIAnimationManager_Release(mgr);
    IUIAnimationTransitionLibrary_Release(lib);
    printf("RESULT: %s\n", failures ? "FAIL" : "PASS");
    return failures != 0;
}
