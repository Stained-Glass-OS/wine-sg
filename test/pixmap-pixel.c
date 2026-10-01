/* pixmap-pixel PIXMAP X Y: one pixel of an X pixmap, "r,g,b" (deskprops-gate.sh) */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
static int failed;
static int on_error(Display *d, XErrorEvent *e) { (void)d; (void)e; failed = 1; return 0; }
int main(int argc, char **argv)
{
    Display *dpy = XOpenDisplay(NULL);
    XImage *img;
    unsigned long p;
    if (argc != 4 || !dpy) return 2;
    XSetErrorHandler(on_error);
    img = XGetImage(dpy, strtoul(argv[1], NULL, 0), atoi(argv[2]), atoi(argv[3]), 1, 1, AllPlanes, ZPixmap);
    XSync(dpy, False);
    if (!img || failed) { printf("gone\n"); return 1; }
    p = XGetPixel(img, 0, 0);
    printf("%lu,%lu,%lu\n", (p >> 16) & 0xff, (p >> 8) & 0xff, p & 0xff);
    return 0;
}
