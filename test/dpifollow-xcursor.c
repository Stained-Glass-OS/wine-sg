/* dpifollow-gate.sh: the size of the pointer the X server shows now
 * (XFixesGetCursorImage), "WxH". Native: cc -o x dpifollow-xcursor.c -lX11 -lXfixes
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

int main( void )
{
    Display *dpy = XOpenDisplay( NULL );
    XFixesCursorImage *image;
    int ev, err;

    if (!dpy || !XFixesQueryExtension( dpy, &ev, &err )) { printf( "none\n" ); return 1; }
    if (!(image = XFixesGetCursorImage( dpy ))) { printf( "none\n" ); return 1; }
    printf( "%ux%u\n", image->width, image->height );
    XFree( image );
    XCloseDisplay( dpy );
    return 0;
}
