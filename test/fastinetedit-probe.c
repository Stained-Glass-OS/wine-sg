#define COBJMACROS
#include <windows.h>
#include <ole2.h>
#include <mimeole.h>
#include <stdio.h>
#define P(...) printf(__VA_ARGS__)
#define H(x) ((unsigned)(ULONG_PTR)(x))
static IMimeMessage *msg;

static void order(const char *tag, HBODY parent)
{
    char out[128]; int n = 0; HBODY h = NULL; HRESULT hr;
    hr = IMimeMessage_GetBody(msg, IBL_FIRST, parent, &h);
    while (hr == S_OK && n < 100) {
        n += sprintf(out + n, "%s%u", n ? "," : "", H(h));
        hr = IMimeMessage_GetBody(msg, IBL_NEXT, h, &h);
    }
    out[n] = 0;
    P("%s %s\n", tag, out);
}

int main(void)
{
    HRESULT hr; HBODY root = NULL, h1 = NULL, h2 = NULL, h3 = NULL, h4 = NULL, none = NULL; ULONG cnt = 0;
    setvbuf(stdout, NULL, _IONBF, 0);
    CoInitialize(NULL);
    hr = MimeOleCreateMessage(NULL, &msg); P("CREATE %08lx\n", hr);
    hr = IMimeMessage_InitNew(msg);
    hr = IMimeMessage_GetBody(msg, IBL_ROOT, NULL, &root); P("ROOT %08lx %u\n", hr, H(root));
    hr = IMimeMessage_InsertBody(msg, IBL_LAST, root, &h1); P("INS_LAST %08lx %d\n", hr, h1 != NULL);
    hr = IMimeMessage_InsertBody(msg, IBL_FIRST, root, &h2); P("INS_FIRST %08lx\n", hr);
    hr = IMimeMessage_InsertBody(msg, IBL_NEXT, h2, &h3); P("INS_NEXT %08lx\n", hr);
    hr = IMimeMessage_InsertBody(msg, IBL_PREVIOUS, h2, &h4); P("INS_PREV %08lx\n", hr);
    order("ORDER1", root);
    P("HANDLES %u,%u,%u,%u\n", H(h4), H(h2), H(h3), H(h1));
    cnt = 0; hr = IMimeMessage_CountBodies(msg, root, TRUE, &cnt); P("COUNT1 %08lx %lu\n", hr, cnt);
    hr = IMimeMessage_InsertBody(msg, IBL_ROOT, NULL, &none); P("INS_ROOT_TWICE %08lx\n", hr);
    hr = IMimeMessage_InsertBody(msg, IBL_PARENT, h2, &none); P("INS_PARENT %08lx\n", hr);
    hr = IMimeMessage_InsertBody(msg, IBL_NEXT, root, &none); P("INS_NEXT_ROOT %08lx\n", hr);
    hr = IMimeMessage_DeleteBody(msg, h2, 0); P("DEL %08lx\n", hr);
    order("ORDER2", root);
    P("HANDLES2 %u,%u,%u\n", H(h4), H(h3), H(h1));
    cnt = 0; hr = IMimeMessage_CountBodies(msg, root, TRUE, &cnt); P("COUNT2 %08lx %lu\n", hr, cnt);
    hr = IMimeMessage_DeleteBody(msg, h2, 0); P("DEL_AGAIN %08lx\n", hr);
    hr = IMimeMessage_DeleteBody(msg, h3, 0x40); P("DEL_FLAGS %08lx\n", hr);
    hr = IMimeMessage_InsertBody(msg, IBL_FIRST, h3, &none); P("INS_UNDER %08lx\n", hr);
    hr = IMimeMessage_DeleteBody(msg, h3, 0); P("DEL_WITH_KIDS %08lx\n", hr);
    cnt = 0; hr = IMimeMessage_CountBodies(msg, root, TRUE, &cnt); P("COUNT3 %08lx %lu\n", hr, cnt);
    hr = IMimeMessage_DeleteBody(msg, root, 0); P("DEL_ROOT %08lx\n", hr);
    hr = IMimeMessage_GetBody(msg, IBL_ROOT, NULL, &none); P("NOROOT %08lx\n", hr);
    hr = IMimeMessage_InsertBody(msg, IBL_ROOT, NULL, &root); P("NEWROOT %08lx %d\n", hr, root != NULL);
    cnt = 0; hr = IMimeMessage_CountBodies(msg, root, TRUE, &cnt); P("COUNT4 %08lx %lu\n", hr, cnt);
    IMimeMessage_Release(msg);
    P("DONE\n");
    return 0;
}
