/* linuxdialog-gate.sh's Linux program: a window "Gate Main" and its dialog "Gate Dialog"
 * (WM_TRANSIENT_FOR the main one), as a GTK or Qt program's Open dialog. Runs until killed.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
int main( void )
{
    Display *d = XOpenDisplay( NULL );
    Window main, dialog;
    XClassHint hint = { (char *)"gatedialog", (char *)"GateDialogApp" };
    XEvent ev;
    if (!d) return 2;
    main = XCreateSimpleWindow( d, DefaultRootWindow( d ), 100, 100, 500, 350, 0, 0, 0x3060a0 );
    XStoreName( d, main, "Gate Main" );
    XSetClassHint( d, main, &hint );
    XMapWindow( d, main );
    XSync( d, False );
    sleep( 4 );
    dialog = XCreateSimpleWindow( d, DefaultRootWindow( d ), 200, 160, 260, 160, 0, 0, 0xa03060 );
    XStoreName( d, dialog, "Gate Dialog" );
    XSetClassHint( d, dialog, &hint );
    XSetTransientForHint( d, dialog, main );
    XMapWindow( d, dialog );
    XSync( d, False );
    for (;;) XNextEvent( d, &ev );
    return 0;
}
