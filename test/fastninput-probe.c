#include <windows.h>
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL line %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

typedef struct { UINT32 id, flags; } CFG;
typedef struct { UINT32 id, flags; INT32 type; float x, y; UINT32 args[32]; } OUTP;
static int outputs; static UINT32 last_id, last_flags;
static void CALLBACK cb(void *data, const OUTP *o) { outputs++; last_id = o->id; last_flags = o->flags; }

static POINTER_INFO info(UINT32 flags, int x, int y, DWORD t)
{
    POINTER_INFO i = {0};
    i.pointerType = PT_TOUCH; i.pointerId = 1; i.pointerFlags = flags;
    i.ptPixelLocation.x = x; i.ptPixelLocation.y = y; i.dwTime = t;
    return i;
}

int main(void)
{
    HMODULE m = LoadLibraryA("ninput.dll");
    HRESULT (WINAPI *create)(void **) = (void *)GetProcAddress(m, "CreateInteractionContext");
    HRESULT (WINAPI *destroy)(void *) = (void *)GetProcAddress(m, "DestroyInteractionContext");
    HRESULT (WINAPI *setcb)(void *, void *, void *) = (void *)GetProcAddress(m, "RegisterOutputCallbackInteractionContext");
    HRESULT (WINAPI *setcfg)(void *, UINT32, const CFG *) = (void *)GetProcAddress(m, "SetInteractionConfigurationInteractionContext");
    HRESULT (WINAPI *addp)(void *, UINT32) = (void *)GetProcAddress(m, "AddPointerInteractionContext");
    HRESULT (WINAPI *frames)(void *, UINT32, UINT32, const POINTER_INFO *) = (void *)GetProcAddress(m, "ProcessPointerFramesInteractionContext");
    void *ctx = NULL;
    CFG cfg[2] = { { 2, 1 }, { 5, 1 } };
    POINTER_INFO i;
    HRESULT hr;

    CHECK(create && frames && addp, "exports");
    if (!frames) return 1;
    hr = create(&ctx); CHECK(hr == S_OK, "create %lx", hr);
    setcb(ctx, cb, NULL);
    hr = setcfg(ctx, 2, cfg); CHECK(hr == S_OK, "config %lx", hr);
    hr = addp(ctx, 1); CHECK(hr == S_OK, "add %lx", hr);

    i = info(POINTER_FLAG_DOWN | POINTER_FLAG_INCONTACT, 10, 10, 100); frames(ctx, 1, 1, &i);
    i = info(POINTER_FLAG_UP, 10, 10, 150); frames(ctx, 1, 1, &i);
    CHECK(outputs == 1 && last_id == 2 && last_flags == 3, "tap %d id %u flags %u", outputs, last_id, last_flags);

    outputs = 0;
    i = info(POINTER_FLAG_DOWN | POINTER_FLAG_INCONTACT, 10, 10, 1000); frames(ctx, 1, 1, &i);
    i = info(POINTER_FLAG_UPDATE | POINTER_FLAG_INCONTACT, 60, 10, 1020); frames(ctx, 1, 1, &i);
    i = info(POINTER_FLAG_UP, 60, 10, 1040); frames(ctx, 1, 1, &i);
    CHECK(outputs == 2 && last_id == 5 && last_flags == 2, "drag %d id %u flags %u", outputs, last_id, last_flags);
    destroy(ctx);
    printf(fails ? "RESULT: FAIL (%d)\n" : "RESULT: PASS\n", fails);
    return fails != 0;
}
