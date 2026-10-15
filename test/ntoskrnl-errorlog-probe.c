/* Host for test/ntoskrnl-errorlog-driver.c: reads the driver's event back from the event log. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static void sg_read_event(void)
{
    static const char *logs[] = { "System", "Application" };
    BYTE buffer[8192];
    DWORD read, needed, i;
    int found = 0, l;

    for (l = 0; l < 2 && !found; l++)
    {
        HANDLE h = OpenEventLogA(NULL, logs[l]);
        if (!h) continue;
        while (!found && ReadEventLogA(h, EVENTLOG_SEQUENTIAL_READ | EVENTLOG_BACKWARDS_READ, 0, buffer, sizeof(buffer), &read, &needed))
        {
            for (i = 0; i < read && !found;)
            {
                EVENTLOGRECORD *r = (EVENTLOGRECORD *)(buffer + i);
                const char *src = (const char *)(r + 1);
                if (!strcmp(src, "SgProbe") && r->EventID == 0xC0001234)
                {
                    const char *str = (const char *)r + r->StringOffset;
                    const DWORD *data = (const DWORD *)((BYTE *)r + r->DataOffset);
                    printf("%s event in %s: type %u category %u strings %u '%s' '%s' data %lu %#lx %#lx\n",
                           r->EventType == EVENTLOG_ERROR_TYPE && r->EventCategory == 7 && r->NumStrings == 2 &&
                           !strcmp(str, "first") && !strcmp(str + 6, "second") && r->DataLength == 8 &&
                           data[0] == 0x11223344 && data[1] == 0x55667788 ? "ok  " : "FAIL",
                           logs[l], r->EventType, r->EventCategory, r->NumStrings, str, str + 6, r->DataLength,
                           data[0], data[1]);
                    found = 1;
                }
                i += r->Length;
            }
        }
        CloseEventLog(h);
    }
    if (!found) printf("FAIL no event from SgProbe in the event log\n");
}
#define SG_HOST_AFTER_IOCTL() sg_read_event()
#include "sg-driver-host.h"
