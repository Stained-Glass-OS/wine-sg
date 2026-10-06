/* usbprint-fakeusb.c -- a libusb-1.0 of the gate's own with one USB printer
 * on it, for test/usbprint-gate.sh. wineusb.sys's unix side links
 * libusb-1.0.so.0; the gate builds this as that name and puts it first on
 * LD_LIBRARY_PATH, so Wine's USB stack meets a printer without hardware, root
 * or a kernel module.
 *
 * The printer: USB 1209:0001 (pid.codes' test ID), one interface of class 7
 * (printer), subclass 1, protocol 2 (bidirectional), bulk OUT 0x01 and bulk IN
 * 0x82. It answers the printer class's GET_DEVICE_ID with
 * SG_FAKEUSB_ID (an IEEE 1284 ID) and GET_PORT_STATUS with 0x18; what is
 * written to it goes to the file SG_FAKEUSB_LOG; written ESC A n (a status
 * request, as label printers take it) makes it answer the next read with a
 * 32-byte status: byte 10 0x08, bytes 11-15 "30336" (the loaded roll), bytes
 * 27-28 the labels left (little-endian).
 *
 * A kernel driver has the interface, as usblp has a printer's on Linux: a
 * transfer on the interface fails (busy) until it is claimed, and the claim
 * fails unless the kernel driver may be detached
 * (libusb_set_auto_detach_kernel_driver) -- wineusb's 0874 claim.
 * SG_FAKEUSB_NODRIVER=1 leaves the interface free instead.
 *
 * Our own code against libusb's public header (LGPL-2.1 API); written for the
 * gate, it implements only what wineusb.sys calls. */

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <libusb-1.0/libusb.h>

struct libusb_device { int unused; };
struct libusb_device_handle { struct libusb_device *dev; };

static struct libusb_device fake_dev;
static struct libusb_device_handle fake_handle = { &fake_dev };

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
static int claimed, auto_detach, interrupted;

#define MAX_T 64
static struct libusb_transfer *done[MAX_T];   /* completed, callbacks due */
static int ndone;
static struct libusb_transfer *reads[MAX_T];  /* bulk IN waiting for data */
static int nreads;
static unsigned char reply[64];
static int reply_len;
static int labels_left = 297;

static const char *env(const char *name, const char *def)
{
    const char *v = getenv(name);
    return v && *v ? v : def;
}

static const char *device_id(void)
{
    return env("SG_FAKEUSB_ID", "MFG:Stained Glass;MDL:Test Label Printer;CMD:ESC;CLS:PRINTER;SN:SG0001;");
}

static void note(const char *fmt, const char *what)
{
    const char *path = getenv("SG_FAKEUSB_TRACE");
    FILE *f;
    if (!path || !(f = fopen(path, "a"))) return;
    fprintf(f, fmt, what);
    fputc('\n', f);
    fclose(f);
}

/* ---- descriptors ---- */

static const unsigned char dev_desc[18] =
{
    18, LIBUSB_DT_DEVICE, 0x00, 0x02, 0, 0, 0, 64,
    0x09, 0x12, 0x01, 0x00,   /* 1209:0001 */
    0x00, 0x01, 1, 2, 3, 1
};

static const unsigned char cfg_desc[32] =
{
    9, LIBUSB_DT_CONFIG, 32, 0, 1, 1, 0, 0xc0, 1,
    9, LIBUSB_DT_INTERFACE, 0, 0, 2, 7, 1, 2, 0,
    7, LIBUSB_DT_ENDPOINT, 0x01, LIBUSB_TRANSFER_TYPE_BULK, 64, 0, 0,
    7, LIBUSB_DT_ENDPOINT, 0x82, LIBUSB_TRANSFER_TYPE_BULK, 64, 0, 0,
};

static int string_desc(int index, unsigned char *buf, int size)
{
    const char *s;
    int i, n;

    if (index == 0)
    {
        unsigned char langs[4] = { 4, LIBUSB_DT_STRING, 0x09, 0x04 };
        n = size < 4 ? size : 4;
        memcpy(buf, langs, n);
        return n;
    }
    s = index == 1 ? "Stained Glass" : index == 2 ? "Test Label Printer" : index == 3 ? "SG0001" : NULL;
    if (!s) return LIBUSB_ERROR_PIPE;
    n = 2 + 2 * (int)strlen(s);
    if (n > size) n = size;
    if (n > 0) buf[0] = 2 + 2 * strlen(s);
    if (n > 1) buf[1] = LIBUSB_DT_STRING;
    for (i = 2; i < n; i++) buf[i] = (i & 1) ? 0 : s[(i - 2) / 2];
    return n;
}

/* a control request: the answer's length, or a libusb error */
static int control(uint8_t type, uint8_t req, uint16_t value, uint16_t index, unsigned char *data, int len)
{
    int n;

    if ((type & LIBUSB_REQUEST_TYPE_CLASS) == LIBUSB_REQUEST_TYPE_CLASS &&
        (type & 0x1f) == LIBUSB_RECIPIENT_INTERFACE)
    {
        switch (req)
        {
        case 0: /* GET_DEVICE_ID: a big-endian length, then the ID */
        {
            const char *id = device_id();
            int idlen = strlen(id) + 2;
            unsigned char tmp[1024];
            tmp[0] = idlen >> 8; tmp[1] = idlen & 0xff;
            memcpy(tmp + 2, id, idlen - 2);
            n = idlen < len ? idlen : len;
            memcpy(data, tmp, n);
            note("control %s", "GET_DEVICE_ID");
            return n;
        }
        case 1: /* GET_PORT_STATUS: selected, no error */
            if (len < 1) return 0;
            data[0] = 0x18;
            note("control %s", "GET_PORT_STATUS");
            return 1;
        case 2: /* SOFT_RESET */
            note("control %s", "SOFT_RESET");
            return 0;
        }
        return LIBUSB_ERROR_PIPE;
    }
    if (type == LIBUSB_ENDPOINT_IN && req == LIBUSB_REQUEST_GET_DESCRIPTOR)
    {
        switch (value >> 8)
        {
        case LIBUSB_DT_DEVICE:
            n = len < 18 ? len : 18; memcpy(data, dev_desc, n); return n;
        case LIBUSB_DT_CONFIG:
            n = len < 32 ? len : 32; memcpy(data, cfg_desc, n); return n;
        case LIBUSB_DT_STRING:
            return string_desc(value & 0xff, data, len);
        }
        return LIBUSB_ERROR_PIPE;
    }
    if (type == LIBUSB_ENDPOINT_IN && req == LIBUSB_REQUEST_GET_CONFIGURATION && len >= 1)
    {
        data[0] = 1;
        return 1;
    }
    if (type == LIBUSB_ENDPOINT_OUT) return 0; /* SET_CONFIGURATION and the like */
    return LIBUSB_ERROR_PIPE;
}

/* ---- the transfer machinery (lock held) ---- */

static void complete(struct libusb_transfer *t, enum libusb_transfer_status status, int actual)
{
    t->status = status;
    t->actual_length = actual;
    if (ndone < MAX_T) done[ndone++] = t;
    pthread_cond_broadcast(&cond);
}

static void serve_reads(void)
{
    while (nreads && reply_len)
    {
        struct libusb_transfer *t = reads[0];
        int n = reply_len < t->length ? reply_len : t->length;
        memcpy(t->buffer, reply, n);
        memmove(reply, reply + n, reply_len - n);
        reply_len -= n;
        memmove(reads, reads + 1, --nreads * sizeof(*reads));
        complete(t, LIBUSB_TRANSFER_COMPLETED, n);
    }
}

static void written(const unsigned char *data, int len)
{
    const char *path = getenv("SG_FAKEUSB_LOG");
    int i;

    if (path)
    {
        FILE *f = fopen(path, "ab");
        if (f) { fwrite(data, 1, len, f); fclose(f); }
    }
    for (i = 0; i + 1 < len; i++)
    {
        if (data[i] == 0x1b && data[i + 1] == 'A')
        {
            memset(reply, 0, 32);
            reply[10] = 0x08;
            memcpy(reply + 11, "30336", 5);
            reply[27] = labels_left & 0xff;  /* little-endian, as the 550 sends it */
            reply[28] = labels_left >> 8;
            reply_len = 32;
        }
    }
}

/* ---- the API ---- */

int LIBUSB_CALL libusb_init(libusb_context **ctx)
{
    if (ctx) *ctx = NULL;
    claimed = 0;
    auto_detach = 0;
    note("init %s", getenv("SG_FAKEUSB_NODRIVER") ? "nodriver" : "usblp");
    return 0;
}

void LIBUSB_CALL libusb_exit(libusb_context *ctx) { }

const char * LIBUSB_CALL libusb_strerror(int code)
{
    switch (code)
    {
    case LIBUSB_SUCCESS: return "Success";
    case LIBUSB_ERROR_IO: return "Input/Output Error";
    case LIBUSB_ERROR_BUSY: return "Resource busy";
    case LIBUSB_ERROR_PIPE: return "Pipe error";
    case LIBUSB_ERROR_NOT_FOUND: return "Entity not found";
    default: return "Other error";
    }
}

int LIBUSB_CALL libusb_hotplug_register_callback(libusb_context *ctx, int events, int flags,
        int vendor_id, int product_id, int dev_class, libusb_hotplug_callback_fn cb_fn,
        void *user_data, libusb_hotplug_callback_handle *handle)
{
    if (handle) *handle = 1;
    if ((flags & LIBUSB_HOTPLUG_ENUMERATE) && (events & LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED))
        cb_fn(ctx, &fake_dev, LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED, user_data);
    return 0;
}

void LIBUSB_CALL libusb_hotplug_deregister_callback(libusb_context *ctx, libusb_hotplug_callback_handle handle) { }

int LIBUSB_CALL libusb_get_device_descriptor(libusb_device *dev, struct libusb_device_descriptor *desc)
{
    memcpy(desc, dev_desc, sizeof(dev_desc));
    desc->bcdUSB = 0x0200;
    desc->idVendor = 0x1209;
    desc->idProduct = 0x0001;
    desc->bcdDevice = 0x0100;
    return 0;
}

int LIBUSB_CALL libusb_get_active_config_descriptor(libusb_device *dev, struct libusb_config_descriptor **config)
{
    struct libusb_config_descriptor *c = calloc(1, sizeof(*c));
    struct libusb_interface *iface = calloc(1, sizeof(*iface));
    struct libusb_interface_descriptor *alt = calloc(1, sizeof(*alt));
    struct libusb_endpoint_descriptor *ep = calloc(2, sizeof(*ep));

    memcpy(c, cfg_desc, 9);
    c->wTotalLength = 32;
    c->interface = iface;
    iface->altsetting = alt;
    iface->num_altsetting = 1;
    memcpy(alt, cfg_desc + 9, 9);
    alt->endpoint = ep;
    memcpy(&ep[0], cfg_desc + 18, 7);
    memcpy(&ep[1], cfg_desc + 25, 7);
    ep[0].wMaxPacketSize = ep[1].wMaxPacketSize = 64;
    *config = c;
    return 0;
}

void LIBUSB_CALL libusb_free_config_descriptor(struct libusb_config_descriptor *c)
{
    if (!c) return;
    free((void *)c->interface->altsetting->endpoint);
    free((void *)c->interface->altsetting);
    free((void *)c->interface);
    free(c);
}

uint8_t LIBUSB_CALL libusb_get_bus_number(libusb_device *dev) { return 1; }
uint8_t LIBUSB_CALL libusb_get_port_number(libusb_device *dev) { return 1; }
uint8_t LIBUSB_CALL libusb_get_device_address(libusb_device *dev) { return 5; }
int LIBUSB_CALL libusb_get_device_speed(libusb_device *dev) { return LIBUSB_SPEED_HIGH; }

int LIBUSB_CALL libusb_open(libusb_device *dev, libusb_device_handle **handle)
{
    *handle = &fake_handle;
    return 0;
}

void LIBUSB_CALL libusb_close(libusb_device_handle *handle) { }

libusb_device * LIBUSB_CALL libusb_get_device(libusb_device_handle *handle) { return handle->dev; }

int LIBUSB_CALL libusb_get_configuration(libusb_device_handle *handle, int *config)
{
    *config = 1;
    return 0;
}

int LIBUSB_CALL libusb_set_auto_detach_kernel_driver(libusb_device_handle *handle, int enable)
{
    auto_detach = enable;
    return 0;
}

int LIBUSB_CALL libusb_claim_interface(libusb_device_handle *handle, int iface)
{
    pthread_mutex_lock(&lock);
    if (!getenv("SG_FAKEUSB_NODRIVER") && !auto_detach)
    {
        pthread_mutex_unlock(&lock);
        note("claim %s", "refused: usblp has it");
        return LIBUSB_ERROR_BUSY;
    }
    claimed = 1;
    pthread_mutex_unlock(&lock);
    note("claim %s", "ok");
    return 0;
}

int LIBUSB_CALL libusb_release_interface(libusb_device_handle *handle, int iface)
{
    pthread_mutex_lock(&lock);
    claimed = 0;
    pthread_mutex_unlock(&lock);
    note("release %s", "ok");
    return 0;
}

int LIBUSB_CALL libusb_clear_halt(libusb_device_handle *handle, unsigned char endpoint) { return 0; }

int LIBUSB_CALL libusb_control_transfer(libusb_device_handle *handle, uint8_t type, uint8_t req,
        uint16_t value, uint16_t index, unsigned char *data, uint16_t len, unsigned int timeout)
{
    return control(type, req, value, index, data, len);
}

struct libusb_transfer * LIBUSB_CALL libusb_alloc_transfer(int iso_packets)
{
    return calloc(1, sizeof(struct libusb_transfer) + iso_packets * sizeof(struct libusb_iso_packet_descriptor));
}

void LIBUSB_CALL libusb_free_transfer(struct libusb_transfer *t)
{
    if (t && (t->flags & LIBUSB_TRANSFER_FREE_BUFFER)) free(t->buffer);
    free(t);
}

int LIBUSB_CALL libusb_submit_transfer(struct libusb_transfer *t)
{
    int ret = 0;

    pthread_mutex_lock(&lock);
    if (t->type == LIBUSB_TRANSFER_TYPE_CONTROL)
    {
        struct libusb_control_setup *s = (struct libusb_control_setup *)t->buffer;
        int n = control(s->bmRequestType, s->bRequest, libusb_le16_to_cpu(s->wValue),
                        libusb_le16_to_cpu(s->wIndex), t->buffer + LIBUSB_CONTROL_SETUP_SIZE,
                        libusb_le16_to_cpu(s->wLength));
        if (n < 0) complete(t, LIBUSB_TRANSFER_STALL, 0);
        else complete(t, LIBUSB_TRANSFER_COMPLETED, n);
    }
    else if (t->type == LIBUSB_TRANSFER_TYPE_BULK)
    {
        if (!claimed && !getenv("SG_FAKEUSB_NODRIVER"))
        {
            note("bulk %s", "refused: the interface is usblp's");
            ret = LIBUSB_ERROR_BUSY;
        }
        else if (t->endpoint == 0x01)
        {
            written(t->buffer, t->length);
            note("bulk %s", "out");
            complete(t, LIBUSB_TRANSFER_COMPLETED, t->length);
            serve_reads();
        }
        else if (t->endpoint == 0x82)
        {
            if (nreads < MAX_T) reads[nreads++] = t;
            serve_reads();
        }
        else ret = LIBUSB_ERROR_NOT_FOUND;
    }
    else ret = LIBUSB_ERROR_NOT_SUPPORTED;
    pthread_mutex_unlock(&lock);
    return ret;
}

int LIBUSB_CALL libusb_cancel_transfer(struct libusb_transfer *t)
{
    int i;

    pthread_mutex_lock(&lock);
    for (i = 0; i < nreads; i++)
    {
        if (reads[i] != t) continue;
        memmove(reads + i, reads + i + 1, (nreads - i - 1) * sizeof(*reads));
        nreads--;
        complete(t, LIBUSB_TRANSFER_CANCELLED, 0);
        pthread_mutex_unlock(&lock);
        return 0;
    }
    pthread_mutex_unlock(&lock);
    return LIBUSB_ERROR_NOT_FOUND;
}

void LIBUSB_CALL libusb_interrupt_event_handler(libusb_context *ctx)
{
    pthread_mutex_lock(&lock);
    interrupted = 1;
    pthread_cond_broadcast(&cond);
    pthread_mutex_unlock(&lock);
}

int LIBUSB_CALL libusb_handle_events(libusb_context *ctx)
{
    struct libusb_transfer *batch[MAX_T];
    int i, n;

    struct timespec until;

    /* as libusb, back after a while with nothing done */
    clock_gettime(CLOCK_REALTIME, &until);
    until.tv_sec += 1;
    pthread_mutex_lock(&lock);
    while (!ndone && !interrupted)
        if (pthread_cond_timedwait(&cond, &lock, &until) == ETIMEDOUT) break;
    interrupted = 0;
    n = ndone;
    memcpy(batch, done, n * sizeof(*batch));
    ndone = 0;
    pthread_mutex_unlock(&lock);
    for (i = 0; i < n; i++) batch[i]->callback(batch[i]);
    return 0;
}
