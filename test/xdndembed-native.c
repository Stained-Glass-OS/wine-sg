/* The virtual desktop as an XDND target the way a window manager's frames are
 * (wine-sg 7788): a small native program for test/xdndembed-gate.sh, both an
 * XDND target and an XDND source.
 *
 *   xdndembed-native LOG NAME X Y W H [PATH]
 *
 * A normal top-level window NAME (the taskbar frames it) speaking XDND 5.
 * As a target it accepts a copy and converts text/uri-list: "DROP data".
 * With PATH it is a source too: a press in it starts a drag of file://PATH,
 * the pointer grabbed; the target is found as Qt finds it (the first
 * XdndAware window from the root down): "TARGET id name" at each change; a
 * drag over its own window is its own (as Qt's is, no messages): "SRCRESULT
 * own"; otherwise XdndEnter/Position/Drop and "SRCRESULT finished=ACTION"
 * after XdndFinished, or "SRCRESULT none". */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>

static Display *dpy;
static Window win;
static FILE *logf;
static const char *path;

static Atom A( const char *name ) { return XInternAtom( dpy, name, False ); }

static void say( const char *fmt, ... )
{
    va_list args;
    va_start( args, fmt );
    vfprintf( logf, fmt, args );
    va_end( args );
    fputc( '\n', logf );
    fflush( logf );
}

static int ignore( Display *d, XErrorEvent *e ) { return 0; }

static long aware_version( Window w )
{
    Atom type;
    int format;
    unsigned long count, after;
    unsigned char *data = NULL;
    long ret = 0;

    if (XGetWindowProperty( dpy, w, A("XdndAware"), 0, 1, False, AnyPropertyType, &type, &format,
                            &count, &after, &data ) == Success && data)
    {
        if (count) ret = *(long *)data;
        XFree( data );
    }
    return ret;
}

/* Qt's findXdndAwareTarget: the first XdndAware window from the root down */
static Window find_target( int x, int y )
{
    Window root = DefaultRootWindow( dpy ), src = root, target = 0, child;
    int lx = x, ly = y, nx, ny;

    XTranslateCoordinates( dpy, root, root, x, y, &nx, &ny, &target );
    while (target)
    {
        if (aware_version( target )) return target;
        if (!XTranslateCoordinates( dpy, src, target, lx, ly, &nx, &ny, &child )) return 0;
        lx = nx; ly = ny; src = target; target = child;
    }
    return 0;
}

static void client( Window to, Window about, Atom type, long l0, long l1, long l2, long l3, long l4 )
{
    XEvent e;
    memset( &e, 0, sizeof(e) );
    e.xclient.type = ClientMessage;
    e.xclient.window = about;
    e.xclient.message_type = type;
    e.xclient.format = 32;
    e.xclient.data.l[0] = l0;
    e.xclient.data.l[1] = l1;
    e.xclient.data.l[2] = l2;
    e.xclient.data.l[3] = l3;
    e.xclient.data.l[4] = l4;
    XSendEvent( dpy, to, False, NoEventMask, &e );
    XFlush( dpy );
}

static void answer_request( XSelectionRequestEvent *req )
{
    XEvent n;
    char uri[4200];
    Atom prop = req->property ? req->property : req->target;

    memset( &n, 0, sizeof(n) );
    n.xselection.type = SelectionNotify;
    n.xselection.requestor = req->requestor;
    n.xselection.selection = req->selection;
    n.xselection.target = req->target;
    n.xselection.time = req->time;
    if (req->target == A("text/uri-list"))
    {
        snprintf( uri, sizeof(uri), "file://%s\r\n", path );
        XChangeProperty( dpy, req->requestor, prop, req->target, 8, PropModeReplace,
                         (unsigned char *)uri, strlen( uri ) );
        n.xselection.property = prop;
    }
    else if (req->target == A("TARGETS"))
    {
        Atom t[2] = { A("text/uri-list"), A("TARGETS") };
        XChangeProperty( dpy, req->requestor, prop, XA_ATOM, 32, PropModeReplace, (unsigned char *)t, 2 );
        n.xselection.property = prop;
    }
    XSendEvent( dpy, req->requestor, False, NoEventMask, &n );
    XFlush( dpy );
}

static char *name_of( Window w )
{
    static char buf[256];
    char *name = NULL;
    strcpy( buf, "-" );
    if (XFetchName( dpy, w, &name ) && name)
    {
        snprintf( buf, sizeof(buf), "%s", name );
        XFree( name );
    }
    return buf;
}

/* the drag, from a press in our window to the release */
static void drag( Time t )
{
    Window target = 0, last = 1;
    long version = 5;
    int accepted = 0;
    Atom action = 0;
    XEvent ev;

    XSetSelectionOwner( dpy, A("XdndSelection"), win, t );
    XGrabPointer( dpy, win, False, PointerMotionMask | ButtonReleaseMask, GrabModeAsync, GrabModeAsync,
                  None, None, t );
    for (;;)
    {
        XNextEvent( dpy, &ev );
        if (ev.type == SelectionRequest) answer_request( &ev.xselectionrequest );
        else if (ev.type == ClientMessage && ev.xclient.message_type == A("XdndStatus"))
        {
            if ((Window)ev.xclient.data.l[0] != target) continue;  /* Qt's check */
            accepted = ev.xclient.data.l[1] & 1;
            action = ev.xclient.data.l[4];
        }
        else if (ev.type == MotionNotify)
        {
            int x = ev.xmotion.x_root, y = ev.xmotion.y_root;
            Window w = find_target( x, y );
            if (w != last)
            {
                if (target && target != win) client( target, target, A("XdndLeave"), win, 0, 0, 0, 0 );
                say( "TARGET %lx %s%s", w, name_of( w ), w == win ? " own" : "" );
                last = target = w;
                accepted = 0;
                action = 0;
                if (target && target != win)
                {
                    version = aware_version( target );
                    if (version > 5) version = 5;
                    client( target, target, A("XdndEnter"), win, version << 24, A("text/uri-list"), 0, 0 );
                }
            }
            if (target && target != win)
                client( target, target, A("XdndPosition"), win, 0, (x << 16) | y, ev.xmotion.time,
                        A("XdndActionCopy") );
        }
        else if (ev.type == ButtonRelease)
        {
            XUngrabPointer( dpy, ev.xbutton.time );
            if (target == win) { say( "SRCRESULT own" ); return; }
            if (!target || !accepted)
            {
                if (target) client( target, target, A("XdndLeave"), win, 0, 0, 0, 0 );
                say( "SRCRESULT none" );
                return;
            }
            client( target, target, A("XdndDrop"), win, 0, ev.xbutton.time, 0, 0 );
            {
                struct timeval start, now;
                gettimeofday( &start, NULL );
                for (;;)
                {
                    while (XPending( dpy ))
                    {
                        XNextEvent( dpy, &ev );
                        if (ev.type == SelectionRequest) answer_request( &ev.xselectionrequest );
                        else if (ev.type == ClientMessage && ev.xclient.message_type == A("XdndFinished") &&
                                 (Window)ev.xclient.data.l[0] == target)
                        {
                            Atom done = ev.xclient.data.l[2];
                            say( "SRCRESULT finished=%s ok=%ld", done ? XGetAtomName( dpy, done ) : "none",
                                 ev.xclient.data.l[1] & 1 );
                            return;
                        }
                    }
                    gettimeofday( &now, NULL );
                    if (now.tv_sec - start.tv_sec > 8) { say( "SRCRESULT timeout" ); return; }
                    usleep( 10000 );
                }
            }
        }
    }
}

int main( int argc, char **argv )
{
    XSizeHints hints;
    XEvent ev;
    Window src = 0;

    if (argc < 7) return 2;
    if (!(logf = fopen( argv[1], "a" ))) return 2;
    if (!(dpy = XOpenDisplay( NULL ))) return 2;
    XSetErrorHandler( ignore );
    path = argc > 7 ? argv[7] : NULL;
    win = XCreateSimpleWindow( dpy, DefaultRootWindow( dpy ), atoi( argv[3] ), atoi( argv[4] ), atoi( argv[5] ),
                               atoi( argv[6] ), 0, 0, path ? 0x3366cc : 0xcc6633 );
    hints.flags = USPosition | USSize;
    hints.x = atoi( argv[3] ); hints.y = atoi( argv[4] );
    hints.width = atoi( argv[5] ); hints.height = atoi( argv[6] );
    XSetWMNormalHints( dpy, win, &hints );
    XStoreName( dpy, win, argv[2] );
    XChangeProperty( dpy, win, A("XdndAware"), XA_ATOM, 32, PropModeReplace, (unsigned char *)&(long){5}, 1 );
    XSelectInput( dpy, win, ButtonPressMask | StructureNotifyMask );
    XMapWindow( dpy, win );
    XSync( dpy, False );
    say( "READY %lx", win );

    for (;;)
    {
        XNextEvent( dpy, &ev );
        if (ev.type == ButtonPress && path) drag( ev.xbutton.time );
        else if (ev.type == SelectionRequest) answer_request( &ev.xselectionrequest );
        else if (ev.type == ClientMessage && ev.xclient.message_type == A("XdndEnter"))
            src = ev.xclient.data.l[0];
        else if (ev.type == ClientMessage && ev.xclient.message_type == A("XdndPosition"))
        {
            src = ev.xclient.data.l[0];
            client( src, src, A("XdndStatus"), win, 1, 0, 0, A("XdndActionCopy") );
        }
        else if (ev.type == ClientMessage && ev.xclient.message_type == A("XdndDrop"))
        {
            Atom type;
            int format, i;
            unsigned long count, after;
            unsigned char *data = NULL;
            XEvent n;

            src = ev.xclient.data.l[0];
            XConvertSelection( dpy, A("XdndSelection"), A("text/uri-list"), A("SGX_DROP"), win,
                               ev.xclient.data.l[2] );
            XFlush( dpy );
            for (i = 0; i < 500 && !XCheckTypedWindowEvent( dpy, win, SelectionNotify, &n ); i++) usleep( 10000 );
            if (i < 500 && n.xselection.property &&
                XGetWindowProperty( dpy, win, A("SGX_DROP"), 0, 65536, True, AnyPropertyType, &type, &format,
                                    &count, &after, &data ) == Success && data)
            {
                char *p = strpbrk( (char *)data, "\r\n" );
                if (p) *p = 0;
                say( "DROP %s", data );
                XFree( data );
            }
            else say( "DROP nodata" );
            client( src, src, A("XdndFinished"), win, 1, A("XdndActionCopy"), 0, 0 );
        }
        else if (ev.type == ClientMessage && ev.xclient.message_type == A("XdndLeave")) say( "LEAVE" );
    }
    return 0;
}
