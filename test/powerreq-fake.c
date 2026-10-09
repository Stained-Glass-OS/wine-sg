/* A stand-in org.freedesktop.ScreenSaver service for test/powerreq-gate.sh:
 * answers Inhibit, UnInhibit and SimulateUserActivity, drops a program's
 * inhibitions when its bus connection goes, and writes its state
 * ("held N", "activity N", "last APP") to the file named by its argument.
 * SPDX-License-Identifier: LGPL-2.1-or-later */
#include <dbus/dbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct { dbus_uint32_t cookie; char owner[128]; } held[64];
static int n_held, activity;
static dbus_uint32_t next = 1;
static const char *state;
static char last[128] = "-";

static void write_state(void)
{
    char tmp[4096];
    FILE *f;
    snprintf(tmp, sizeof(tmp), "%s.tmp", state);
    if (!(f = fopen(tmp, "w"))) return;
    fprintf(f, "held %d\nactivity %d\nlast %s\n", n_held, activity, last);
    fclose(f);
    rename(tmp, state);
}

static DBusHandlerResult filter(DBusConnection *conn, DBusMessage *msg, void *data)
{
    DBusMessage *reply = NULL;
    const char *sender = dbus_message_get_sender(msg);
    int i;

    if (dbus_message_is_signal(msg, DBUS_INTERFACE_DBUS, "NameOwnerChanged"))
    {
        const char *name, *old, *new;
        if (dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &name, DBUS_TYPE_STRING, &old,
                                  DBUS_TYPE_STRING, &new, DBUS_TYPE_INVALID) && !new[0])
        {
            for (i = 0; i < n_held;)
                if (!strcmp(held[i].owner, name)) held[i] = held[--n_held];
                else i++;
            write_state();
        }
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (dbus_message_is_method_call(msg, "org.freedesktop.ScreenSaver", "Inhibit"))
    {
        const char *app, *reason;
        if (dbus_message_get_args(msg, NULL, DBUS_TYPE_STRING, &app, DBUS_TYPE_STRING, &reason, DBUS_TYPE_INVALID) &&
            n_held < 64)
        {
            held[n_held].cookie = next++;
            snprintf(held[n_held].owner, sizeof(held[0].owner), "%s", sender);
            snprintf(last, sizeof(last), "%s", app);
            reply = dbus_message_new_method_return(msg);
            dbus_message_append_args(reply, DBUS_TYPE_UINT32, &held[n_held++].cookie, DBUS_TYPE_INVALID);
            write_state();
        }
    }
    else if (dbus_message_is_method_call(msg, "org.freedesktop.ScreenSaver", "UnInhibit"))
    {
        dbus_uint32_t cookie;
        if (dbus_message_get_args(msg, NULL, DBUS_TYPE_UINT32, &cookie, DBUS_TYPE_INVALID))
            for (i = 0; i < n_held; i++)
                if (held[i].cookie == cookie && !strcmp(held[i].owner, sender))
                {
                    held[i] = held[--n_held];
                    reply = dbus_message_new_method_return(msg);
                    break;
                }
        write_state();
    }
    else if (dbus_message_is_method_call(msg, "org.freedesktop.ScreenSaver", "SimulateUserActivity"))
    {
        activity++;
        reply = dbus_message_new_method_return(msg);
        write_state();
    }
    else return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    if (!reply) reply = dbus_message_new_error(msg, DBUS_ERROR_INVALID_ARGS, "refused");
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
    return DBUS_HANDLER_RESULT_HANDLED;
}

int main(int argc, char **argv)
{
    DBusConnection *conn = dbus_bus_get(DBUS_BUS_SESSION, NULL);

    if (argc < 2 || !conn) return 1;
    state = argv[1];
    if (dbus_bus_request_name(conn, "org.freedesktop.ScreenSaver", DBUS_NAME_FLAG_DO_NOT_QUEUE, NULL) !=
        DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) return 1;
    dbus_bus_add_match(conn, "type='signal',sender='org.freedesktop.DBus',member='NameOwnerChanged'", NULL);
    dbus_connection_add_filter(conn, filter, NULL, NULL);
    write_state();
    while (dbus_connection_read_write_dispatch(conn, -1));
    return 0;
}
