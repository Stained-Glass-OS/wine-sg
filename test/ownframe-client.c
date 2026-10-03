/* ownframe-client TITLE X Y W H -- a window that draws its own title bar, as
 * SG Office (Qt, frameless) and Firefox (its tabs in the title bar) do: it
 * asks a window manager for no decorations (_MOTIF_WM_HINTS) and, pressed in
 * its top 30 px, gives the pointer up and asks the window manager to move it
 * (_NET_WM_MOVERESIZE, as Qt's startSystemMove does); pressed in its bottom
 * right corner, to size it. Filled orange, its "title bar" dark blue.
 * For ownframe-gate.sh. SPDX-License-Identifier: LGPL-2.1-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

int main( int argc, char **argv )
{
    Display *d;
    Window w;
    XEvent ev;
    GC gc;
    unsigned long hints[5] = { 1 << 1, 0, 0, 0, 0 };   /* MWM_HINTS_DECORATIONS: none */
    int width, height;

    if (argc < 6 || !(d = XOpenDisplay( NULL ))) return 2;
    width = atoi( argv[4] ); height = atoi( argv[5] );
    w = XCreateSimpleWindow( d, DefaultRootWindow( d ), atoi( argv[2] ), atoi( argv[3] ), width, height, 0, 0, 0xe08020 );
    XStoreName( d, w, argv[1] );
    XChangeProperty( d, w, XInternAtom( d, "_MOTIF_WM_HINTS", False ), XInternAtom( d, "_MOTIF_WM_HINTS", False ), 32,
                     PropModeReplace, (unsigned char *)hints, 5 );
    XSelectInput( d, w, ExposureMask | ButtonPressMask | StructureNotifyMask );
    XMapWindow( d, w );
    gc = XCreateGC( d, w, 0, NULL );
    for (;;)
    {
        XNextEvent( d, &ev );
        if (ev.type == ConfigureNotify) { width = ev.xconfigure.width; height = ev.xconfigure.height; }
        if (ev.type == Expose)
        {
            XSetForeground( d, gc, 0x203080 );
            XFillRectangle( d, w, gc, 0, 0, width, 30 );
        }
        if (ev.type == ButtonPress && ev.xbutton.button == 1 &&
            (ev.xbutton.y < 30 || (ev.xbutton.x > width - 20 && ev.xbutton.y > height - 20)))
        {
            XEvent m = { 0 };
            XUngrabPointer( d, ev.xbutton.time );
            m.xclient.type = ClientMessage;
            m.xclient.window = w;
            m.xclient.message_type = XInternAtom( d, "_NET_WM_MOVERESIZE", False );
            m.xclient.format = 32;
            m.xclient.data.l[0] = ev.xbutton.x_root;
            m.xclient.data.l[1] = ev.xbutton.y_root;
            m.xclient.data.l[2] = ev.xbutton.y < 30 ? 8 : 4;   /* move, or size from the bottom right */
            m.xclient.data.l[3] = 1;
            m.xclient.data.l[4] = 1;
            XSendEvent( d, DefaultRootWindow( d ), False, SubstructureRedirectMask | SubstructureNotifyMask, &m );
            XFlush( d );
        }
    }
    return 0;
}
