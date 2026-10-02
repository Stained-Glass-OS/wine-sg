/* netwm-fullscreen WINDOW 1|0 -- ask for full screen (or out of it) as a
 * program does: a _NET_WM_STATE client message to the root window (EWMH),
 * as GTK sends for Firefox's F11. For linuxembed-gate.sh.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>

int main( int argc, char **argv )
{
    Display *d;
    XEvent ev = { 0 };

    if (argc != 3 || !(d = XOpenDisplay( NULL ))) return 2;
    ev.xclient.type = ClientMessage;
    ev.xclient.window = strtoul( argv[1], NULL, 0 );
    ev.xclient.message_type = XInternAtom( d, "_NET_WM_STATE", False );
    ev.xclient.format = 32;
    ev.xclient.data.l[0] = atoi( argv[2] ) ? 1 : 0;   /* _NET_WM_STATE_ADD / _REMOVE */
    ev.xclient.data.l[1] = XInternAtom( d, "_NET_WM_STATE_FULLSCREEN", False );
    ev.xclient.data.l[3] = 1;   /* from a program */
    XSendEvent( d, DefaultRootWindow( d ), False, SubstructureRedirectMask | SubstructureNotifyMask, &ev );
    XCloseDisplay( d );
    return 0;
}
