/* A framed Linux program that moves itself with its own title bar, as Steam,
 * GTK and SG Office do, for test/moveresize-gate.sh (wine-sg 7790):
 *
 *   moveresize-client LOG NAME MODE [hide]
 *
 * A press in its window asks the window manager to move it
 * (_NET_WM_MOVERESIZE, direction 8, button 1, as from a pointer):
 *   gtk     after letting go of the pointer (XUngrabPointer), as GTK does
 *   keep    keeping the press's automatic grab (not letting go)
 *   late    only when the button is released (a quick tap seen late)
 *   die     and then exits at once, mid-move
 * With "hide" WM_DELETE_WINDOW (Alt+F4, the frame's close) only hides the
 * window and the program lives on with its X connection, as Steam does.
 * LOG gets READY id, PRESS, ASKED, RELEASE, HIDDEN. */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static Display *dpy;
static Window win;
static FILE *logf;

static void say( const char *s ) { fprintf( logf, "%s\n", s ); fflush( logf ); }

static void ask_move( int x, int y )
{
    XEvent e;
    memset( &e, 0, sizeof(e) );
    e.xclient.type = ClientMessage;
    e.xclient.window = win;
    e.xclient.message_type = XInternAtom( dpy, "_NET_WM_MOVERESIZE", False );
    e.xclient.format = 32;
    e.xclient.data.l[0] = x;
    e.xclient.data.l[1] = y;
    e.xclient.data.l[2] = 8;  /* _NET_WM_MOVERESIZE_MOVE */
    e.xclient.data.l[3] = 1;  /* button 1 */
    e.xclient.data.l[4] = 1;  /* from an application */
    XSendEvent( dpy, DefaultRootWindow( dpy ), False, SubstructureRedirectMask | SubstructureNotifyMask, &e );
    XFlush( dpy );
    say( "ASKED" );
}

int main( int argc, char **argv )
{
    Atom wm_delete;
    const char *mode;
    int hide;
    XEvent ev;

    if (argc < 4) return 2;
    if (!(logf = fopen( argv[1], "a" ))) return 2;
    if (!(dpy = XOpenDisplay( NULL ))) return 2;
    mode = argv[3];
    hide = argc > 4 && !strcmp( argv[4], "hide" );
    win = XCreateSimpleWindow( dpy, DefaultRootWindow( dpy ), 200, 150, 360, 240, 0, 0, 0x2a7a50 );
    XStoreName( dpy, win, argv[2] );
    wm_delete = XInternAtom( dpy, "WM_DELETE_WINDOW", False );
    XSetWMProtocols( dpy, win, &wm_delete, 1 );
    XSelectInput( dpy, win, ButtonPressMask | ButtonReleaseMask | StructureNotifyMask );
    XMapWindow( dpy, win );
    XSync( dpy, False );
    fprintf( logf, "READY %lu\n", win ); fflush( logf );
    for (;;)
    {
        XNextEvent( dpy, &ev );
        if (ev.type == ButtonPress && ev.xbutton.button == 1)
        {
            say( "PRESS" );
            if (!strcmp( mode, "late" )) continue;
            if (strcmp( mode, "keep" )) XUngrabPointer( dpy, ev.xbutton.time );
            ask_move( ev.xbutton.x_root, ev.xbutton.y_root );
            if (!strcmp( mode, "die" )) { usleep( 200000 ); return 0; }
        }
        else if (ev.type == ButtonRelease && ev.xbutton.button == 1)
        {
            say( "RELEASE" );
            if (!strcmp( mode, "late" )) ask_move( ev.xbutton.x_root, ev.xbutton.y_root );
        }
        else if (ev.type == ClientMessage && (Atom)ev.xclient.data.l[0] == wm_delete)
        {
            if (!hide) return 0;
            XUnmapWindow( dpy, win );
            XFlush( dpy );
            say( "HIDDEN" );
        }
    }
}
