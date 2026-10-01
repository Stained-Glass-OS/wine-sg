/* backdrop-check WINDOW: the backdrop a window names (_SG_BACKDROP, patches/
 * sg/0615) -- "pixmap=ID WxH pixel=RRGGBB" for its middle, or "pixmap=none". */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    Display *d = XOpenDisplay(NULL);
    Window w, root;
    Atom type;
    int format, x, y;
    unsigned long count, after, pixel;
    unsigned int pw, ph, border, depth;
    unsigned char *prop = NULL;
    Pixmap p;
    XImage *img;

    if (!d || argc < 2) return 2;
    w = strtoul(argv[1], NULL, 0);
    if (XGetWindowProperty(d, w, XInternAtom(d, "_SG_BACKDROP", False), 0, 1, False, XA_PIXMAP, &type, &format,
                           &count, &after, &prop) || !prop || count != 1) {
        printf("pixmap=none\n");
        return 1;
    }
    p = *(Pixmap *)prop;
    if (!XGetGeometry(d, p, &root, &x, &y, &pw, &ph, &border, &depth)) return 1;
    img = XGetImage(d, p, pw / 2, ph / 2, 1, 1, AllPlanes, ZPixmap);
    pixel = img ? XGetPixel(img, 0, 0) & 0xffffff : 0;
    printf("pixmap=%#lx %ux%u pixel=%06lx\n", p, pw, ph, pixel);
    return 0;
}
