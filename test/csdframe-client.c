/* csdframe-client TITLE X Y W H L R T B -- a window as GTK draws one with
 * its own title bar (client-side decorations): its content W x H with a
 * shadow around it, L R T B pixels wide, that it names in _GTK_FRAME_EXTENTS
 * (a window manager keeps it outside the frame). The shadow is magenta, the
 * content orange with a dark blue title bar along its top; maximized (as
 * the window manager says in _NET_WM_STATE), it drops the shadow, as GTK
 * does, and fills the window. It asks for no decorations (_MOTIF_WM_HINTS);
 * pressed in its title bar it asks the window manager to move it, in the
 * content's bottom right corner to size it (_NET_WM_MOVERESIZE).
 * For csdframe-gate.sh. SPDX-License-Identifier: LGPL-2.1-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

static Display *d;
static Window w;
static GC gc;
static int width, height;
static long ext[4], shadow[4];     /* left right top bottom: now, and when not maximized */

static void set_extents( void )
{
    XChangeProperty( d, w, XInternAtom( d, "_GTK_FRAME_EXTENTS", False ), XA_CARDINAL, 32, PropModeReplace,
                     (unsigned char *)ext, 4 );
}

static void paint( void )
{
    XSetForeground( d, gc, 0xff00ff );
    XFillRectangle( d, w, gc, 0, 0, width, height );
    XSetForeground( d, gc, 0xe08020 );
    XFillRectangle( d, w, gc, ext[0], ext[2], width - ext[0] - ext[1], height - ext[2] - ext[3] );
    XSetForeground( d, gc, 0x203080 );
    XFillRectangle( d, w, gc, ext[0], ext[2], width - ext[0] - ext[1], 30 );
}

static int maximized( void )
{
    Atom type, *atoms = NULL, mv = XInternAtom( d, "_NET_WM_STATE_MAXIMIZED_VERT", False );
    int format, ret = 0;
    unsigned long count = 0, remaining, i;
    if (!XGetWindowProperty( d, w, XInternAtom( d, "_NET_WM_STATE", False ), 0, 32, False, XA_ATOM, &type, &format,
                             &count, &remaining, (unsigned char **)&atoms ) && atoms)
    {
        for (i = 0; i < count; i++) if (atoms[i] == mv) ret = 1;
        XFree( atoms );
    }
    return ret;
}

int main( int argc, char **argv )
{
    unsigned long hints[5] = { 1 << 1, 0, 0, 0, 0 };   /* MWM_HINTS_DECORATIONS: none */
    XEvent ev;
    int i;

    if (argc < 10 || !(d = XOpenDisplay( NULL ))) return 2;
    for (i = 0; i < 4; i++) shadow[i] = ext[i] = atol( argv[6 + i] );
    width = atoi( argv[4] ) + ext[0] + ext[1];
    height = atoi( argv[5] ) + ext[2] + ext[3];
    w = XCreateSimpleWindow( d, DefaultRootWindow( d ), atoi( argv[2] ), atoi( argv[3] ), width, height, 0, 0, 0xff00ff );
    XStoreName( d, w, argv[1] );
    XChangeProperty( d, w, XInternAtom( d, "_MOTIF_WM_HINTS", False ), XInternAtom( d, "_MOTIF_WM_HINTS", False ), 32,
                     PropModeReplace, (unsigned char *)hints, 5 );
    set_extents();
    XSelectInput( d, w, ExposureMask | ButtonPressMask | StructureNotifyMask | PropertyChangeMask );
    XMapWindow( d, w );
    gc = XCreateGC( d, w, 0, NULL );
    for (;;)
    {
        XNextEvent( d, &ev );
        if (ev.type == ConfigureNotify) { width = ev.xconfigure.width; height = ev.xconfigure.height; paint(); }
        if (ev.type == Expose) paint();
        if (ev.type == PropertyNotify && ev.xproperty.atom == XInternAtom( d, "_NET_WM_STATE", False ))
        {
            int max = maximized();
            if ((max && ext[0]) || (!max && !ext[0] && shadow[0]))
            {
                /* as GTK: no shadow when maximized, the shadow back after */
                for (i = 0; i < 4; i++) ext[i] = max ? 0 : shadow[i];
                set_extents();
                paint();
                XFlush( d );
            }
        }
        if (ev.type == ButtonPress && ev.xbutton.button == 1)
        {
            int title = ev.xbutton.y >= ext[2] && ev.xbutton.y < ext[2] + 30;
            int corner = ev.xbutton.x > width - ext[1] - 20 && ev.xbutton.x < width - ext[1] &&
                         ev.xbutton.y > height - ext[3] - 20 && ev.xbutton.y < height - ext[3];
            XEvent m = { 0 };
            if (!title && !corner) continue;
            XUngrabPointer( d, ev.xbutton.time );
            m.xclient.type = ClientMessage;
            m.xclient.window = w;
            m.xclient.message_type = XInternAtom( d, "_NET_WM_MOVERESIZE", False );
            m.xclient.format = 32;
            m.xclient.data.l[0] = ev.xbutton.x_root;
            m.xclient.data.l[1] = ev.xbutton.y_root;
            m.xclient.data.l[2] = title ? 8 : 4;   /* move, or size from the bottom right */
            m.xclient.data.l[3] = 1;
            m.xclient.data.l[4] = 1;
            XSendEvent( d, DefaultRootWindow( d ), False, SubstructureRedirectMask | SubstructureNotifyMask, &m );
            XFlush( d );
        }
    }
    return 0;
}
