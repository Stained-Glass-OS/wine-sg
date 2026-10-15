/* Probe driver for patches/sg/5072: callback objects (ExCreateCallback, ExRegisterCallback,
 * ExUnregisterCallback, ExNotifyCallback).
 * - a name is required; opening a name that does not exist fails unless Create; the system's
 *   \Callback\PowerState and \Callback\SetSystemTime open; opening a name again gives the same object;
 * - an object made without AllowMultipleCallbacks takes one routine (a second registration is NULL);
 * - ExNotifyCallback calls every registered routine with its context and both arguments,
 *   also one that unregisters itself; an unregistered routine is not called again. */
#include "sg-driver.h"

static int calls[3];
static void *seen_ctx, *seen_a1, *seen_a2;
static void *self_reg;

static void NTAPI cb(void *ctx, void *a1, void *a2)
{
    calls[(ULONG_PTR)ctx]++;
    seen_ctx = ctx; seen_a1 = a1; seen_a2 = a2;
}

static void NTAPI cb_self(void *ctx, void *a1, void *a2)
{
    calls[2]++;
    ExUnregisterCallback(self_reg);
}

static NTSTATUS open_cb(const WCHAR *name, BOOLEAN create, BOOLEAN multiple, PCALLBACK_OBJECT *obj)
{
    UNICODE_STRING str;
    OBJECT_ATTRIBUTES attr;

    *obj = NULL;
    RtlInitUnicodeString(&str, name);
    InitializeObjectAttributes(&attr, name ? &str : NULL, OBJ_CASE_INSENSITIVE | OBJ_PERMANENT, NULL, NULL);
    return ExCreateCallback(obj, &attr, create, multiple);
}

static void run_probe(DRIVER_OBJECT *driver)
{
    static const struct { const WCHAR *name; BOOLEAN create; NTSTATUS st; } opens[] =
    {
        {L"\\Callback\\SgProbeNone", FALSE, STATUS_OBJECT_NAME_NOT_FOUND},
        {L"\\Callback\\PowerState", FALSE, STATUS_SUCCESS},
        {L"\\Callback\\SetSystemTime", FALSE, STATUS_SUCCESS},
        {NULL, TRUE, STATUS_UNSUCCESSFUL},
    };
    PCALLBACK_OBJECT one, again, multi;
    void *r1, *r2, *r3;
    unsigned int i;
    NTSTATUS st;

    for (i = 0; i < ARRAYSIZE(opens); i++)
    {
        PCALLBACK_OBJECT obj;
        st = opens[i].name ? open_cb(opens[i].name, opens[i].create, TRUE, &obj) : STATUS_PENDING;
        if (!opens[i].name)
        {
            OBJECT_ATTRIBUTES attr;
            InitializeObjectAttributes(&attr, NULL, 0, NULL, NULL);
            st = ExCreateCallback(&obj, &attr, TRUE, TRUE);
        }
        check(st == opens[i].st, "open %u (%ls): %#lx", i, opens[i].name ? opens[i].name : L"no name", st);
        if (!st) ObDereferenceObject(obj);
    }

    st = open_cb(L"\\Callback\\SgProbeOne", TRUE, FALSE, &one);
    check(st == STATUS_SUCCESS && one, "a new object, one routine: %#lx", st);
    if (st) return;
    st = open_cb(L"\\CALLBACK\\sgprobeone", FALSE, TRUE, &again);
    check(st == STATUS_SUCCESS && again == one, "opened again by name, any case: %#lx %p %p", st, again, one);
    if (!st) ObDereferenceObject(again);

    r1 = ExRegisterCallback(one, cb, (void *)0);
    r2 = ExRegisterCallback(one, cb, (void *)1);
    check(r1 && !r2, "one routine only: %p %p", r1, r2);
    ExNotifyCallback(one, (void *)0x11, (void *)0x22);
    check(calls[0] == 1 && seen_a1 == (void *)0x11 && seen_a2 == (void *)0x22 && seen_ctx == (void *)0,
          "notified once with both arguments: %d", calls[0]);
    if (r1) ExUnregisterCallback(r1);
    ExNotifyCallback(one, NULL, NULL);
    check(calls[0] == 1, "not called once unregistered: %d", calls[0]);
    r2 = ExRegisterCallback(one, cb, (void *)1);
    check(r2 != NULL, "the place is free again");
    if (r2) ExUnregisterCallback(r2);
    ObDereferenceObject(one);

    st = open_cb(L"\\Callback\\SgProbeMulti", TRUE, TRUE, &multi);
    check(st == STATUS_SUCCESS, "a new object, many routines: %#lx", st);
    if (st) return;
    calls[0] = calls[1] = calls[2] = 0;
    r1 = ExRegisterCallback(multi, cb, (void *)0);
    r2 = ExRegisterCallback(multi, cb, (void *)1);
    self_reg = r3 = ExRegisterCallback(multi, cb_self, NULL);
    check(r1 && r2 && r3, "three routines: %p %p %p", r1, r2, r3);
    ExNotifyCallback(multi, NULL, NULL);
    check(calls[0] == 1 && calls[1] == 1 && calls[2] == 1, "each called: %d %d %d", calls[0], calls[1], calls[2]);
    ExNotifyCallback(multi, NULL, NULL);
    check(calls[0] == 2 && calls[1] == 2 && calls[2] == 1, "the one that unregistered itself is gone: %d %d %d",
          calls[0], calls[1], calls[2]);
    if (r1) ExUnregisterCallback(r1);
    if (r2) ExUnregisterCallback(r2);
    ObDereferenceObject(multi);
}
