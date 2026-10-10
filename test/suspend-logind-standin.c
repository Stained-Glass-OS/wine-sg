/* A stand-in for systemd-logind's Manager interface on a private bus, for
 * test/suspend-gate.sh: CanSuspend, CanHibernate, Suspend and Hibernate. The
 * answers come from a config file read on every call ("CanSuspend=yes",
 * "CanHibernate=na", "Suspend=denied", ...), each call is appended to a log
 * ("Suspend false"), and a successful Suspend/Hibernate is followed, after
 * the reply, by PrepareForSleep(true), a pause, and PrepareForSleep(false).
 *
 *   suspend-logind-standin CONFIG LOG */
#include <dbus/dbus.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *config, *logfile;

static void logline(const char *a, const char *b)
{
    FILE *f = fopen(logfile, "a");
    if (!f) return;
    fprintf(f, "%s%s%s\n", a, b ? " " : "", b ? b : "");
    fclose(f);
}

static void config_value(const char *key, char *out, size_t size, const char *def)
{
    char line[128];
    FILE *f = fopen(config, "r");
    size_t n = strlen(key);

    snprintf(out, size, "%s", def);
    if (!f) return;
    while (fgets(line, sizeof(line), f))
        if (!strncmp(line, key, n) && line[n] == '=')
        {
            line[strcspn(line, "\r\n")] = 0;
            snprintf(out, size, "%s", line + n + 1);
        }
    fclose(f);
}

static void signal_sleep(DBusConnection *c, dbus_bool_t value)
{
    DBusMessage *s = dbus_message_new_signal("/org/freedesktop/login1", "org.freedesktop.login1.Manager", "PrepareForSleep");
    dbus_message_append_args(s, DBUS_TYPE_BOOLEAN, &value, DBUS_TYPE_INVALID);
    dbus_connection_send(c, s, NULL);
    dbus_connection_flush(c);
    dbus_message_unref(s);
}

int main(int argc, char **argv)
{
    DBusConnection *c;
    DBusError err;

    if (argc < 3) return 2;
    config = argv[1]; logfile = argv[2];
    dbus_error_init(&err);
    if (!(c = dbus_bus_get_private(DBUS_BUS_SESSION, &err))) { fprintf(stderr, "no bus: %s\n", err.message); return 1; }
    dbus_bus_request_name(c, "org.freedesktop.login1", 0, &err);
    logline("READY", NULL);
    while (dbus_connection_read_write_dispatch(c, -1))
    {
        DBusMessage *m;

        while ((m = dbus_connection_pop_message(c)))
        {
            const char *member = dbus_message_get_member(m);
            DBusMessage *reply = NULL;

            if (dbus_message_get_type(m) != DBUS_MESSAGE_TYPE_METHOD_CALL || !member) { dbus_message_unref(m); continue; }
            if (!strcmp(member, "CanSuspend") || !strcmp(member, "CanHibernate"))
            {
                char v[32];
                const char *p = v;

                config_value(member, v, sizeof(v), "yes");
                logline(member, NULL);
                reply = dbus_message_new_method_return(m);
                dbus_message_append_args(reply, DBUS_TYPE_STRING, &p, DBUS_TYPE_INVALID);
                dbus_connection_send(c, reply, NULL);
            }
            else if (!strcmp(member, "Suspend") || !strcmp(member, "Hibernate"))
            {
                char v[32];
                dbus_bool_t interactive = TRUE;

                dbus_message_get_args(m, NULL, DBUS_TYPE_BOOLEAN, &interactive, DBUS_TYPE_INVALID);
                logline(member, interactive ? "true" : "false");
                config_value(member, v, sizeof(v), "ok");
                if (!strcmp(v, "denied"))
                    reply = dbus_message_new_error(m, DBUS_ERROR_ACCESS_DENIED, "not allowed");
                else if (!strcmp(v, "fail"))
                    reply = dbus_message_new_error(m, DBUS_ERROR_FAILED, "failed");
                else
                    reply = dbus_message_new_method_return(m);
                dbus_connection_send(c, reply, NULL);
                dbus_connection_flush(c);
                if (!strcmp(v, "ok"))
                {
                    signal_sleep(c, TRUE);
                    usleep(1200000);
                    signal_sleep(c, FALSE);
                }
            }
            if (reply) dbus_message_unref(reply);
            dbus_connection_flush(c);
            dbus_message_unref(m);
        }
    }
    return 0;
}
