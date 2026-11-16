/* embedwheel-app: a Linux program's window that scrolls as Firefox does, by
 * XInput 2 (XI2.1 smooth scrolling: motion with a scroll valuator, and the
 * wheel's emulated button presses), logging what reaches it:
 *   main <window id>   once mapped
 *   wheel <n>          every XI2 button 4/5 press, and every scroll-valuator motion
 * embedwheel-app LOG */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>

int main(int argc, char **argv)
{
	Display *d = XOpenDisplay(NULL);
	FILE *log;
	Window w;
	int xi, ev, er, maj = 2, min = 2, n = 0;
	unsigned char mask[XIMaskLen(XI_LASTEVENT)] = { 0 };
	XIEventMask em = { XIAllMasterDevices, sizeof(mask), mask };
	int scroll[64] = { 0 }, ndev, i, j;
	XIDeviceInfo *info;

	if (!d || argc < 2 || !(log = fopen(argv[1], "a")) ||
	    !XQueryExtension(d, "XInputExtension", &xi, &ev, &er) || XIQueryVersion(d, &maj, &min)) {
		return 1;
	}
	setvbuf(log, NULL, _IOLBF, 0);
	info = XIQueryDevice(d, XIAllMasterDevices, &ndev);
	for (i = 0; i < ndev; i++) {
		for (j = 0; j < info[i].num_classes; j++) {
			if (info[i].classes[j]->type == XIScrollClass) {
				XIScrollClassInfo *s = (XIScrollClassInfo *) info[i].classes[j];
				if (s->number < 64) scroll[s->number] = 1;
			}
		}
	}
	w = XCreateSimpleWindow(d, DefaultRootWindow(d), 100, 100, 400, 300, 0, 0, 0x2060d0);
	XStoreName(d, w, "Wheel Test");
	{
		XClassHint ch = { "wheeltest", "WheelTest" };
		XSetClassHint(d, w, &ch);
	}
	XISetMask(mask, XI_ButtonPress);
	XISetMask(mask, XI_Motion);
	XISelectEvents(d, w, &em, 1);
	XSelectInput(d, w, ExposureMask | StructureNotifyMask);
	XMapWindow(d, w);
	XSync(d, False);
	fprintf(log, "main %lu\n", w);
	for (;;) {
		XEvent e;
		XNextEvent(d, &e);
		if (e.type == GenericEvent && e.xcookie.extension == xi && XGetEventData(d, &e.xcookie)) {
			XIDeviceEvent *de = e.xcookie.data;
			int hit = 0;
			if (e.xcookie.evtype == XI_ButtonPress && (de->detail == 4 || de->detail == 5)) hit = 1;
			if (e.xcookie.evtype == XI_Motion)
				for (i = 0; i < de->valuators.mask_len * 8 && i < 64; i++)
					if (XIMaskIsSet(de->valuators.mask, i) && scroll[i]) hit = 1;
			if (hit) fprintf(log, "wheel %d\n", ++n);
			XFreeEventData(d, &e.xcookie);
		}
	}
}
