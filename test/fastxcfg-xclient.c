/* Moves the virtual desktop's window on the root and sends a window a synthetic ConfigureNotify, as a
 * window manager would (ICCCM 4.1.5: position in root coordinates): fastxcfg-xclient XID HOSTX HOSTY X Y W H */
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    Display *d = XOpenDisplay(NULL);
    Window win, root, parent, *kids;
    unsigned int n;
    int hx, hy, x, y, w, h, tx, ty;
    Window child;
    XEvent ev;

    if (!d || argc < 8) return 2;
    win = strtoul(argv[1], NULL, 16);
    hx = atoi(argv[2]); hy = atoi(argv[3]); x = atoi(argv[4]); y = atoi(argv[5]); w = atoi(argv[6]); h = atoi(argv[7]);
    if (!XQueryTree(d, win, &root, &parent, &kids, &n)) return 3;
    if (kids) XFree(kids);
    if (parent == root) { printf("the window is a child of the root\n"); return 4; }
    XMoveWindow(d, parent, hx, hy);
    XSync(d, False);
    usleep(300000);
    XTranslateCoordinates(d, parent, root, 0, 0, &tx, &ty, &child);
    printf("desktop window %lx at %d,%d on the root\n", parent, tx, ty);

    ev.xconfigure.type = ConfigureNotify;
    ev.xconfigure.display = d;
    ev.xconfigure.event = win;
    ev.xconfigure.window = win;
    ev.xconfigure.x = x;
    ev.xconfigure.y = y;
    ev.xconfigure.width = w;
    ev.xconfigure.height = h;
    ev.xconfigure.border_width = 0;
    ev.xconfigure.above = None;
    ev.xconfigure.override_redirect = False;
    XSendEvent(d, win, False, StructureNotifyMask, &ev);
    XSync(d, False);
    printf("sent %d,%d %dx%d\n", x, y, w, h);
    return 0;
}
