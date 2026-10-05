/* xpropgone-destroy WINDOW: destroys another client's X window (X allows it) */
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv)
{
    Display *d = XOpenDisplay(NULL);
    if (!d || argc < 2) return 2;
    XDestroyWindow(d, (Window)strtoul(argv[1], NULL, 16));
    XSync(d, False);
    return 0;
}
