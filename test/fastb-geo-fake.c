/* A fake GeoClue2 on a private bus: logs every call, serves one fixed position.
 * fastb-geo-fake ADDRESS LOGFILE [noalt] */
#include <dbus/dbus.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <float.h>

#define CLIENT_PATH "/org/freedesktop/GeoClue2/Client/1"
#define LOC_PATH    "/org/freedesktop/GeoClue2/Client/1/Location/1"

static FILE *logf;
static int noalt;
static int started;
static time_t signal_at;

static void logline(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(logf, fmt, ap);
    fputc('\n', logf);
    fflush(logf);
    va_end(ap);
}

static void reply_variant_double(DBusMessage *msg, DBusConnection *c, double v)
{
    DBusMessage *r = dbus_message_new_method_return(msg);
    DBusMessageIter it, var;
    dbus_message_iter_init_append(r, &it);
    dbus_message_iter_open_container(&it, DBUS_TYPE_VARIANT, "d", &var);
    dbus_message_iter_append_basic(&var, DBUS_TYPE_DOUBLE, &v);
    dbus_message_iter_close_container(&it, &var);
    dbus_connection_send(c, r, NULL);
    dbus_message_unref(r);
}

static DBusHandlerResult handle(DBusConnection *c, DBusMessage *msg, void *data)
{
    const char *iface = dbus_message_get_interface(msg), *member = dbus_message_get_member(msg);
    const char *path = dbus_message_get_path(msg);
    DBusMessage *r;

    if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL || !member) return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    if (!strcmp(member, "GetClient"))
    {
        const char *p = CLIENT_PATH;
        logline("GetClient");
        r = dbus_message_new_method_return(msg);
        dbus_message_append_args(r, DBUS_TYPE_OBJECT_PATH, &p, DBUS_TYPE_INVALID);
        dbus_connection_send(c, r, NULL);
        dbus_message_unref(r);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (!strcmp(member, "Start"))
    {
        logline("Start");
        started = 1;
        signal_at = time(NULL) + 1;
        r = dbus_message_new_method_return(msg);
        dbus_connection_send(c, r, NULL);
        dbus_message_unref(r);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (!strcmp(member, "Stop"))
    {
        logline("Stop");
        started = 0;
        r = dbus_message_new_method_return(msg);
        dbus_connection_send(c, r, NULL);
        dbus_message_unref(r);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (iface && !strcmp(iface, "org.freedesktop.DBus.Properties") && !strcmp(member, "Set"))
    {
        DBusMessageIter it, var;
        const char *ifn, *name;
        dbus_message_iter_init(msg, &it);
        dbus_message_iter_get_basic(&it, &ifn);
        dbus_message_iter_next(&it);
        dbus_message_iter_get_basic(&it, &name);
        dbus_message_iter_next(&it);
        dbus_message_iter_recurse(&it, &var);
        if (dbus_message_iter_get_arg_type(&var) == DBUS_TYPE_STRING)
        {
            const char *v;
            dbus_message_iter_get_basic(&var, &v);
            logline("Set %s=%s", name, v);
        }
        else
        {
            dbus_uint32_t v;
            dbus_message_iter_get_basic(&var, &v);
            logline("Set %s=%u", name, v);
        }
        r = dbus_message_new_method_return(msg);
        dbus_connection_send(c, r, NULL);
        dbus_message_unref(r);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    if (iface && !strcmp(iface, "org.freedesktop.DBus.Properties") && !strcmp(member, "Get"))
    {
        DBusMessageIter it;
        const char *ifn, *name;
        dbus_message_iter_init(msg, &it);
        dbus_message_iter_get_basic(&it, &ifn);
        dbus_message_iter_next(&it);
        dbus_message_iter_get_basic(&it, &name);

        if (!strcmp(name, "Location"))
        {
            const char *p = "/";
            DBusMessageIter out, var;
            r = dbus_message_new_method_return(msg);
            dbus_message_iter_init_append(r, &out);
            dbus_message_iter_open_container(&out, DBUS_TYPE_VARIANT, "o", &var);
            dbus_message_iter_append_basic(&var, DBUS_TYPE_OBJECT_PATH, &p);
            dbus_message_iter_close_container(&out, &var);
            dbus_connection_send(c, r, NULL);
            dbus_message_unref(r);
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        if (!strcmp(name, "Latitude")) reply_variant_double(msg, c, 47.6062);
        else if (!strcmp(name, "Longitude")) reply_variant_double(msg, c, -122.3321);
        else if (!strcmp(name, "Accuracy")) reply_variant_double(msg, c, 25.5);
        else if (!strcmp(name, "Altitude")) reply_variant_double(msg, c, noalt ? -DBL_MAX : 56.5);
        else if (!strcmp(name, "Speed")) reply_variant_double(msg, c, 3.25);
        else if (!strcmp(name, "Heading")) reply_variant_double(msg, c, -1.0);
        else if (!strcmp(name, "Timestamp"))
        {
            DBusMessageIter out, var, st;
            dbus_uint64_t sec = 1700000000, usec = 250000;
            r = dbus_message_new_method_return(msg);
            dbus_message_iter_init_append(r, &out);
            dbus_message_iter_open_container(&out, DBUS_TYPE_VARIANT, "(tt)", &var);
            dbus_message_iter_open_container(&var, DBUS_TYPE_STRUCT, NULL, &st);
            dbus_message_iter_append_basic(&st, DBUS_TYPE_UINT64, &sec);
            dbus_message_iter_append_basic(&st, DBUS_TYPE_UINT64, &usec);
            dbus_message_iter_close_container(&var, &st);
            dbus_message_iter_close_container(&out, &var);
            dbus_connection_send(c, r, NULL);
            dbus_message_unref(r);
        }
        else
        {
            r = dbus_message_new_error(msg, "org.freedesktop.DBus.Error.UnknownProperty", name);
            dbus_connection_send(c, r, NULL);
            dbus_message_unref(r);
        }
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    (void)path;
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

int main(int argc, char **argv)
{
    DBusError err;
    DBusConnection *c;

    if (argc < 3) return 2;
    noalt = argc > 3 && !strcmp(argv[3], "noalt");
    logf = fopen(argv[2], "a");
    dbus_error_init(&err);
    c = dbus_connection_open(argv[1], &err);
    if (!c || !dbus_bus_register(c, &err)) { fprintf(stderr, "connect: %s\n", err.message); return 1; }
    if (dbus_bus_request_name(c, "org.freedesktop.GeoClue2", 0, &err) != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
    { fprintf(stderr, "name: %s\n", err.message ? err.message : "?"); return 1; }
    dbus_connection_add_filter(c, handle, NULL, NULL);
    printf("ready\n");
    fflush(stdout);
    for (;;)
    {
        if (!dbus_connection_read_write_dispatch(c, 200)) break;
        if (started && signal_at && time(NULL) >= signal_at)
        {
            DBusMessage *s = dbus_message_new_signal(CLIENT_PATH, "org.freedesktop.GeoClue2.Client", "LocationUpdated");
            const char *old = "/", *new = LOC_PATH;
            dbus_message_append_args(s, DBUS_TYPE_OBJECT_PATH, &old, DBUS_TYPE_OBJECT_PATH, &new, DBUS_TYPE_INVALID);
            dbus_connection_send(c, s, NULL);
            dbus_message_unref(s);
            signal_at = 0;
            logline("signal");
        }
    }
    return 0;
}
