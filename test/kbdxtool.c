/* kbdxtool (host, test/kbdxwayland-gate.sh, test/kbdgroups-gate.sh; patches/sg/9153, 9154):
 *   kbdxtool keymap LAYOUTS VARIANTS OPTIONS  the keymap text a Wayland compositor gives its clients
 *                                             (xkbcommon, as sg-compositor/wlroots do), on stdout;
 *                                             xkbcomp loads it into an X server the way Xwayland does
 *   kbdxtool keys [+N|-N|sMS]...              XTEST: press (+) or release (-) key code N, or wait MS
 *   kbdxtool group                            the X keyboard's group now, and its group names */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XTest.h>
#include <xkbcommon/xkbcommon.h>

int main(int argc, char **argv)
{
    Display *d;
    int i;

    if (argc >= 2 && !strcmp(argv[1], "keymap"))
    {
        struct xkb_rule_names names = { "evdev", "pc105", argc > 2 ? argv[2] : "us",
                                        argc > 3 ? argv[3] : "", argc > 4 ? argv[4] : "" };
        struct xkb_context *ctx = xkb_context_new(XKB_CONTEXT_NO_ENVIRONMENT_NAMES);
        struct xkb_keymap *km = ctx ? xkb_keymap_new_from_names(ctx, &names, 0) : NULL;
        char *s;
        if (!km || !(s = xkb_keymap_get_as_string(km, XKB_KEYMAP_FORMAT_TEXT_V1))) return 1;
        fputs(s, stdout);
        free(s);
        return 0;
    }
    if (!(d = XOpenDisplay(NULL))) return 2;
    if (argc >= 2 && !strcmp(argv[1], "keys"))
    {
        for (i = 2; i < argc; i++)
        {
            if (argv[i][0] == 's')
            {
                struct timespec ts = { 0, 0 };
                long ms = atol(argv[i] + 1);
                XSync(d, False);
                ts.tv_sec = ms / 1000; ts.tv_nsec = (ms % 1000) * 1000000L;
                nanosleep(&ts, NULL);
                continue;
            }
            XTestFakeKeyEvent(d, atoi(argv[i] + 1), argv[i][0] == '+', CurrentTime);
            XSync(d, False);
        }
        XCloseDisplay(d);
        return 0;
    }
    if (argc >= 2 && !strcmp(argv[1], "group"))
    {
        XkbStateRec st;
        XkbDescPtr xkb = XkbAllocKeyboard();
        if (XkbGetState(d, XkbUseCoreKbd, &st) == Success) printf("group %d\n", st.group);
        xkb->device_spec = XkbUseCoreKbd;
        if (XkbGetNames(d, XkbGroupNamesMask, xkb) == Success && xkb->names)
            for (i = 0; i < 4 && xkb->names->groups[i]; i++) printf("name %d %s\n", i, XGetAtomName(d, xkb->names->groups[i]));
        return 0;
    }
    return 3;
}
