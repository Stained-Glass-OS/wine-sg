/* Wine as an XDND source (wine-sg 7787), for test/xdndsrc-gate.sh: the
 * pointer's cursor as shown now (XFixes), as a hash of its picture. */
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>
#include <stdio.h>

int main( void )
{
    Display *dpy = XOpenDisplay( NULL );
    XFixesCursorImage *img;
    unsigned long hash = 5381, i;

    if (!dpy || !(img = XFixesGetCursorImage( dpy ))) return 1;
    for (i = 0; i < (unsigned long)img->width * img->height; i++) hash = hash * 33 + (img->pixels[i] & 0xffffffff);
    printf( "%ux%u-%lx\n", img->width, img->height, hash & 0xffffffffffff );
    return 0;
}
