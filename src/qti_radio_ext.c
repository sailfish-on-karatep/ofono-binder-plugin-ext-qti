/*
 *  oFono - Open Source Telephony - binder based adaptation QTI plugin
 *
 *  Copyright (C) 2022 Jolla Ltd.
 *  Copyright (C) 2024 TheKit <thekit@disroot.org>
 *  Copyright (C) 2024 Marius Gripsgard <marius@ubports.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <glib-object.h>

#include "qti_radio_ext.h"
#include "qti_radio_ext_types.h"

#include <radio_types.h>
#include <binder_ext_ims_impl.h>
#include <binder_ext_call_impl.h>
#include <binder_ext_sms_impl.h>

#include <ofono/log.h>
#include <ofono/misc.h>

#include <gbinder.h>

#include <gutil_idlepool.h>
#include <gutil_log.h>
#include <gutil_macros.h>

#undef DBG
#define DBG(fmt, ...) \
    gutil_log(GLOG_MODULE_CURRENT, GLOG_LEVEL_ALWAYS, "ims:"fmt, ##__VA_ARGS__)


#define QTI_RADIO_CALL_TIMEOUT (3*1000) /* ms */

typedef GObjectClass QtiRadioExtClass;
typedef struct qti_radio_ext {
    GObject parent;
    char* slot;
    GBinderClient* client;
    GBinderRemoteObject* remote;
    GBinderLocalObject* response;
    GBinderLocalObject* indication;
    GUtilIdlePool* pool;
    GHashTable* requests;
} QtiRadioExt;

GType qti_radio_ext_get_type() G_GNUC_INTERNAL;
G_DEFINE_TYPE(QtiRadioExt, qti_radio_ext, G_TYPE_OBJECT)

#define THIS_TYPE qti_radio_ext_get_type()
#define THIS(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, THIS_TYPE, QtiRadioExt)
#define PARENT_CLASS qti_radio_ext_parent_class
#define KEY(serial) GUINT_TO_POINTER(serial)

typedef struct qti_radio_ext_request QtiRadioExtRequest;

typedef void (*QtiRadioExtArgWriteFunc)(
    GBinderWriter* args,
    va_list va);

typedef void (*QtiRadioExtRequestHandlerFunc)(
    QtiRadioExtRequest* req,
    const GBinderReader* args);

typedef void (*QtiRadioExtResultFunc)(
    QtiRadioExt* radio,
    int result,
    GBinderReader* reader,
    void* user_data);

struct qti_radio_ext_request {
    guint id;  /* request id */
    gulong tx; /* binder transaction id */
    QtiRadioExt* radio;
    gint32 response_code;
    QtiRadioExtRequestHandlerFunc handle_response;
    void (*free)(QtiRadioExtRequest* req);
    GDestroyNotify destroy;
    void* user_data;
};

typedef struct qti_radio_ext_result_request {
    QtiRadioExtRequest base;
    QtiRadioExtResultFunc complete;
} QtiRadioExtResultRequest;

enum qti_radio_ext_signal {
    SIGNAL_IMS_REG_STATUS_CHANGED,
    SIGNAL_EXT_CALL_STATE_CHANGED,
    SIGNAL_EXT_ON_RING,
    SIGNAL_EXT_ON_INCOMING_SMS,
    SIGNAL_EXT_ON_SMS_REPORT,
    SIGNAL_EXT_ON_RINGBACK_TONE,
    SIGNAL_COUNT
};

#define SIGNAL_IMS_REG_STATUS_CHANGED_NAME          "qti-radio-ext-ims-reg-status-changed"
#define SIGNAL_EXT_CALL_STATE_CHANGED_NAME          "qti-radio-ext-call-state-changed"
#define SIGNAL_EXT_ON_RING_NAME                     "qti-radio-ext-on-ring"
#define SIGNAL_EXT_ON_INCOMING_SMS_NAME             "qti-radio-ext-on-incoming-sms"
#define SIGNAL_EXT_ON_SMS_REPORT_NAME               "qti-radio-ext-on-sms-report"
#define SIGNAL_EXT_ON_RINGBACK_TONE_NAME            "qti-radio-ext-on-ringback-tone"

static guint qti_radio_ext_signals[SIGNAL_COUNT] = { 0 };

static GLogModule qti_radio_ext_binder_log_module = {
    .max_level = GLOG_LEVEL_VERBOSE,
    .level = GLOG_LEVEL_VERBOSE,
    .flags = GLOG_FLAG_HIDE_NAME
};

static GLogModule qti_radio_ext_binder_dump_module = {
    .parent = &qti_radio_ext_binder_log_module,
    .max_level = GLOG_LEVEL_VERBOSE,
    .level = GLOG_LEVEL_INHERIT,
    .flags = GLOG_FLAG_HIDE_NAME
};

static
void
qti_radio_ext_log_notify(
    struct ofono_debug_desc* desc)
{
    qti_radio_ext_binder_log_module.level = (desc->flags &
        OFONO_DEBUG_FLAG_PRINT) ? GLOG_LEVEL_VERBOSE : GLOG_LEVEL_INHERIT;
}

static
void
qti_radio_ext_dump_notify(
    struct ofono_debug_desc* desc)
{
    qti_radio_ext_binder_dump_module.level = (desc->flags &
        OFONO_DEBUG_FLAG_PRINT) ? GLOG_LEVEL_VERBOSE : GLOG_LEVEL_INHERIT;
}

static struct ofono_debug_desc logger_trace OFONO_DEBUG_ATTR = {
    .name = "qti_binder_trace",
    .flags = OFONO_DEBUG_FLAG_DEFAULT | OFONO_DEBUG_FLAG_HIDE_NAME,
    .notify = qti_radio_ext_log_notify
};

static struct ofono_debug_desc logger_dump OFONO_DEBUG_ATTR = {
    .name = "qti_binder_dump",
    .flags = OFONO_DEBUG_FLAG_DEFAULT | OFONO_DEBUG_FLAG_HIDE_NAME,
    .notify = qti_radio_ext_dump_notify
};

static
guint
qti_radio_ext_new_req_id()
{
    // Start from 1
    static guint last_id = 1;
    return last_id++;
}

static const char*
qti_radio_ext_req_name(
    guint32 req)
{
    switch (req) {
#define QTI_RADIO_REQ_(req, resp, name, NAME) \
        case QTI_RADIO_REQ_##NAME: return #name;
    QTI_RADIO_EXT_IMS_CALL_1_0(QTI_RADIO_REQ_)
    QTI_RADIO_EXT_IMS_CALL_1_1(QTI_RADIO_REQ_)
    QTI_RADIO_EXT_IMS_CALL_1_2(QTI_RADIO_REQ_)
#undef QTI_RADIO_REQ_
    }
    return NULL;
}

static const char*
qti_radio_ext_resp_name(
    guint32 resp)
{
    switch (resp) {
#define QTI_RADIO_RESP_(req, resp, name, NAME) \
        case QTI_RADIO_RESP_##NAME: return #name;
    QTI_RADIO_EXT_IMS_CALL_1_0(QTI_RADIO_RESP_)
    QTI_RADIO_EXT_IMS_CALL_1_1(QTI_RADIO_RESP_)
    QTI_RADIO_EXT_IMS_CALL_1_2(QTI_RADIO_RESP_)
#undef QTI_RADIO_RESP_
    }
    return NULL;
}

static const char*
qti_radio_ext_ind_name(
    guint32 ind)
{
    switch (ind) {
#define QTI_RADIO_IND_(code, name, NAME) \
        case QTI_RADIO_IND_##NAME: return #name;
    QTI_RADIO_IND_1_0(QTI_RADIO_IND_)
#undef QTI_RADIO_IND_
    }
    return NULL;
}

static
void
qti_radio_ext_log_req(
    QtiRadioExt* self,
    guint32 code,
    guint32 serial)
{
    static const GLogModule* log = &qti_radio_ext_binder_log_module;
    const int level = GLOG_LEVEL_VERBOSE;
    const char* name;

    if (!gutil_log_enabled(log, level))
        return;

    name = qti_radio_ext_req_name(code);

    if (serial) {
        gutil_log(log, level, "%s< [%08x] %u %s",
            self->slot, serial, code, name ? name : "???");
    } else {
        gutil_log(log, level, "%s< %u %s",
            self->slot, code, name ? name : "???");
    }
}

void
qti_radio_ext_log_resp(
    QtiRadioExt* self,
    guint32 code,
    guint32 serial)
{
    static const GLogModule* log = &qti_radio_ext_binder_log_module;
    const int level = GLOG_LEVEL_VERBOSE;
    const char* name;

    if (!gutil_log_enabled(log, level))
        return;

    name = qti_radio_ext_resp_name(code);

    gutil_log(log, level, "%s> [%08x] %u %s",
        self->slot, serial, code, name ? name : "???");
}

static
void
qti_radio_ext_log_ind(
    QtiRadioExt* self,
    guint32 code)
{
    static const GLogModule* log = &qti_radio_ext_binder_log_module;
    const int level = GLOG_LEVEL_VERBOSE;
    const char* name;

    if (!gutil_log_enabled(log, level))
        return;

    name = qti_radio_ext_ind_name(code);

    gutil_log(log, level, "%s > %u %s", self->slot, code,
        name ? name : "???");
}

static
void
qti_radio_ext_dump_data(
    const GBinderReader* reader)
{
    static const GLogModule* log = &qti_radio_ext_binder_dump_module;
    const int level = GLOG_LEVEL_VERBOSE;
    gsize size;
    const guint8* data;

    if (!gutil_log_enabled(log, level))
        return;

    data = gbinder_reader_get_data(reader, &size);
    gutil_log_dump(log, level, "  ",  data, size);
}

static
void
qti_radio_ext_dump_request(
    GBinderLocalRequest* args)
{
    static const GLogModule* log = &qti_radio_ext_binder_dump_module;
    const int level = GLOG_LEVEL_VERBOSE;
    GBinderWriter writer;
    const guint8* data;
    gsize size;

    if (!gutil_log_enabled(log, level))
        return;

    /* Use writer API to fetch the raw data */
    gbinder_local_request_init_writer(args, &writer);
    data = gbinder_writer_get_data(&writer, &size);
    gutil_log_dump(log, level, "  ", data, size);
}

const QtiRadioRegInfo*
qti_radio_ext_read_ims_reg_status_info(
    QtiRadioExt* self,
    GBinderReader* reader)
{
    const QtiRadioRegInfo* info;

    info = gbinder_reader_read_hidl_struct(reader, QtiRadioRegInfo);
    if (info) {
        const char *uri = info->uri.data.str ? info->uri.data.str : "";
        const char *error_msg = info->error_message.data.str ? info->error_message.data.str : "";

        DBG("%s: QtiRadioRegInfo state:%d radiotech:%d"
            " error_code:%d\n"
            " uri:%s error_msg:%s",
            self->slot,
            info->state,
            info->radio_tech,
            info->error_code,
            uri, error_msg);

        return info;
    } else {
        DBG("%s: failed to parse QtiRadioRegInfo", self->slot);
        return NULL;
    }
}

static
void
qti_radio_ext_handle_ims_reg_status_report(
    QtiRadioExt* self,
    const GBinderReader* args)
{
    GBinderReader reader;

    gbinder_reader_copy(&reader, args);
    const QtiRadioRegInfo* info =
        qti_radio_ext_read_ims_reg_status_info(self, &reader);

    if (info) {
        g_signal_emit(self, qti_radio_ext_signals[SIGNAL_IMS_REG_STATUS_CHANGED],
                        0, info->state);
    } else {
        ofono_warn("Failed to parse reg status");
    }
}

static
BINDER_EXT_CALL_STATE
qti_ims_call_radio_state_to_state(
    QTI_RADIO_CALL_STATE state)
{
    switch (state) {
    case QTI_RADIO_CALL_STATE_INCOMING:
        return BINDER_EXT_CALL_STATE_INCOMING;
    case QTI_RADIO_CALL_STATE_ALERTING:
        return BINDER_EXT_CALL_STATE_ALERTING;
    case QTI_RADIO_CALL_STATE_HOLDING:
        return BINDER_EXT_CALL_STATE_HOLDING;
    case QTI_RADIO_CALL_STATE_WAITING:
        return BINDER_EXT_CALL_STATE_WAITING;
    case QTI_RADIO_CALL_STATE_ACTIVE:
        return BINDER_EXT_CALL_STATE_ACTIVE;
    case QTI_RADIO_CALL_STATE_END:
        return BINDER_EXT_CALL_STATE_END;
    case QTI_RADIO_CALL_STATE_DIALING:
        return BINDER_EXT_CALL_STATE_DIALING;
    default:
        return BINDER_EXT_CALL_STATE_INVALID;
    }
}

static
BinderExtCallInfo*
qti_ims_call_info_new(
    guint call_id,
    BINDER_EXT_CALL_STATE state,
    GBinderHidlString number,
    GBinderHidlString name
    )
{
    const gsize number_len = number.len;
    const gsize total = G_ALIGN8(sizeof(BinderExtCallInfo)) +
        G_ALIGN8(number_len + 1);
    BinderExtCallInfo* dest = g_malloc0(total);
    char* ptr = ((char*)dest) + G_ALIGN8(sizeof(BinderExtCallInfo));

    dest->call_id = call_id;

    // do we need name?
    dest->name = NULL;
    dest->state = qti_ims_call_radio_state_to_state(state);
    dest->type = BINDER_EXT_CALL_TYPE_VOICE;
    dest->flags = BINDER_EXT_CALL_FLAG_IMS | BINDER_EXT_CALL_FLAG_INCOMING;

    dest->number = ptr;
    if (number.data.str && number_len > 0)
        memcpy(ptr, number.data.str, number_len);
    ptr += G_ALIGN8(number_len + 1);

    return dest;
}

static
GPtrArray*
qti_radio_ext_read_call_state_info(
    const QtiRadioCallInfo* call_info_array,
    gsize count)
{
    // array of QtiRadioCallInfo
    GPtrArray* call_ext_info_array = g_ptr_array_new_with_free_func(g_free);

    for (gsize i = 0; i < count; i++) {
        const QtiRadioCallInfo* call_info = &call_info_array[i];
        BinderExtCallInfo* dest = qti_ims_call_info_new(call_info->index, call_info->state, call_info->number, call_info->name);

        g_ptr_array_add(call_ext_info_array, dest);
    }

    return call_ext_info_array;
}


static
void
qti_radio_ext_handle_call_state_indication(
    QtiRadioExt* self,
    const GBinderReader* args)
{
    /* callInfoIndication(vec<CallInfo> callList) */
    const QtiRadioCallInfo* call_infos;
    GBinderReader reader;
    gsize count;

    gbinder_reader_copy(&reader, args);
    call_infos = gbinder_reader_read_hidl_type_vec(&reader, QtiRadioCallInfo, &count);

    if (call_infos) {
        GPtrArray* call_info_ptr = qti_radio_ext_read_call_state_info(call_infos, count);

        g_signal_emit(self, qti_radio_ext_signals[SIGNAL_EXT_CALL_STATE_CHANGED],
                      0, call_info_ptr);
    } else {
        DBG("%s: failed to parse call state data", self->slot);
    }
}

static
void
qti_radio_ext_handle_ringback_tone_indication(
    QtiRadioExt* self,
    const GBinderReader* args)
{
    gboolean status;
    GBinderReader reader;

    gbinder_reader_copy(&reader, args);
    if (gbinder_reader_read_bool(&reader, &status)) {
        g_signal_emit(self, qti_radio_ext_signals[SIGNAL_EXT_ON_RINGBACK_TONE], 0, status);
    }

    DBG("%s: failed to parse ringback tone data", self->slot);
}

/*
typedef struct qti_radio_incoming_ims_sms {
    GBinderHidlString format RADIO_ALIGNED(8);
    GBinderHidlVec pdu RADIO_ALIGNED(8);
    guint32 verstat RADIO_ALIGNED(4);
} RADIO_ALIGNED(8) QtiRadioIncomingImsSms;
*/

static
void
qti_radio_ext_handle_incoming_sms_indication(
    QtiRadioExt* self,
    const GBinderReader* args)
{
    GBinderReader reader;
    const QtiRadioIncomingImsSms* sms;

    gbinder_reader_copy(&reader, args);
    sms = gbinder_reader_read_hidl_struct(&reader, QtiRadioIncomingImsSms);

    if (sms) {
        const char *format = sms->format.data.str ? sms->format.data.str : "";
        const guint32 verstat = sms->verstat;
        const guint pdu_len = sms->pdu.count;
        const void* pdu = sms->pdu.data.ptr;

        // copy pdu to a new buffer
        const void* pdu_copy = g_memdup2(pdu, pdu_len);

        DBG("%s: Incoming SMS indication format:%s verstat:%d pdu_len:%d",
            self->slot, format, verstat, pdu_len);
        gutil_log_dump(&qti_radio_ext_binder_dump_module, GLOG_LEVEL_VERBOSE, "  ", pdu_copy, pdu_len);

        g_signal_emit(self, qti_radio_ext_signals[SIGNAL_EXT_ON_INCOMING_SMS], 0, pdu_copy, pdu_len);
        g_free((void*)pdu_copy);
    } else {
        DBG("%s: failed to parse incoming SMS data", self->slot);
    }
}

/*

typedef struct qti_radio_ims_sms_send_status_report {
    guint32 message_ref RADIO_ALIGNED(4);
    GBinderHidlString format RADIO_ALIGNED(8);
    GBinderHidlVec pdu RADIO_ALIGNED(8);
} RADIO_ALIGNED(8) QtiRadioImsSmsSendStatusReport;

*/

// implement sms report
static
void
qti_radio_ext_handle_sms_report_indication(
    QtiRadioExt* self,
    const GBinderReader* args)
{
    GBinderReader reader;
    const QtiRadioImsSmsSendStatusReport* report;

    gbinder_reader_copy(&reader, args);
    report = gbinder_reader_read_hidl_struct(&reader, QtiRadioImsSmsSendStatusReport);

    if (report) {
        const guint32 message_ref = report->message_ref;
        const char *format = report->format.data.str ? report->format.data.str : "";
        const guint32 pdu_len = report->pdu.count;
        const void* pdu = report->pdu.data.ptr;

        // copy pdu to a new buffer
        const void* pdu_copy = g_memdup2(pdu, pdu_len);

        DBG("%s: SMS status report indication message_ref:%d format:%s pdu_len:%d",
            self->slot, message_ref, format, pdu_len);
        gutil_log_dump(&qti_radio_ext_binder_dump_module, GLOG_LEVEL_VERBOSE, "  ", pdu_copy, pdu_len);

        g_signal_emit(self, qti_radio_ext_signals[SIGNAL_EXT_ON_SMS_REPORT], 0, pdu_copy, pdu_len, message_ref);
        g_free((void*)pdu_copy);
    } else {
        DBG("%s: failed to parse SMS status report data", self->slot);
    }
}


static
GBinderLocalReply*
qti_radio_ext_indication(
    GBinderLocalObject* obj,
    GBinderRemoteRequest* req,
    guint code,
    guint flags,
    int* status,
    void* user_data)
{
    QtiRadioExt* self = THIS(user_data);
    const char* iface = gbinder_remote_request_interface(req);
    GBinderReader args;

    gbinder_remote_request_init_reader(req, &args);
    qti_radio_ext_log_ind(self, code);
    qti_radio_ext_dump_data(&args);

    if (g_str_equal(iface, QTI_RADIO_INDICATION_1_0)) {
        switch(code) {
        case QTI_RADIO_IND_REG_STATE_INDICATION:
            qti_radio_ext_handle_ims_reg_status_report(self, &args);
            return NULL;
        case QTI_RADIO_IND_CALL_STATE_INDICATION:
            qti_radio_ext_handle_call_state_indication(self, &args);
            return NULL;
        case QTI_RADIO_IND_RING_INDICATION:
            g_signal_emit(self, qti_radio_ext_signals[SIGNAL_EXT_ON_RING], 0);
            return NULL;
        case QTI_RADIO_IND_RINGBACK_TONE_INDICATION:
            qti_radio_ext_handle_ringback_tone_indication(self, &args);
            return NULL;
        }
    } else if (g_str_equal(iface, QTI_RADIO_INDICATION_1_1)) {
        switch(code) {
        case QTI_RADIO_IND_CALL_STATE_INDICATION_1_1:
            qti_radio_ext_handle_call_state_indication(self, &args);
            return NULL;
        }
    } else if (g_str_equal(iface, QTI_RADIO_INDICATION_1_2)) {
        switch(code) {
        case QTI_RADIO_IND_CALL_STATE_INDICATION_1_2:
            qti_radio_ext_handle_call_state_indication(self, &args);
            return NULL;
        case QTI_RADIO_IND_SMS_STATUS_REPORT_INDICATION:
            qti_radio_ext_handle_sms_report_indication(self, &args);
            return NULL;
        case QTI_RADIO_IND_INCOMING_SMS_INDICATION:
            qti_radio_ext_handle_incoming_sms_indication(self, &args);
            return NULL;
        }
    }

    return NULL;
}

gulong
qti_radio_ext_add_ims_reg_status_handler(
    QtiRadioExt* self,
    QtiRadioExtImsRegStatusFunc handler,
    void* user_data)
{
    return (G_LIKELY(self) && G_LIKELY(handler)) ? g_signal_connect(self,
        SIGNAL_IMS_REG_STATUS_CHANGED_NAME, G_CALLBACK(handler), user_data) : 0;
}

gulong
qti_radio_ext_add_call_state_handler(
    QtiRadioExt* self,
    QtiRadioExtCallStateFunc handler,
    void* user_data)
{
    return (G_LIKELY(self) && G_LIKELY(handler)) ? g_signal_connect(self,
        SIGNAL_EXT_CALL_STATE_CHANGED_NAME, G_CALLBACK(handler), user_data) : 0;
}

gulong
qti_radio_ext_add_ring_handler(
    QtiRadioExt* self,
    QtiRadioExtRingFunc handler,
    void* user_data)
{
    return (G_LIKELY(self) && G_LIKELY(handler)) ? g_signal_connect(self,
        SIGNAL_EXT_ON_RING_NAME, G_CALLBACK(handler), user_data) : 0;
}

gulong
qti_radio_ext_add_ringback_tone_handler(
    QtiRadioExt* self,
    QtiRadioExtRingbackToneFunc handler,
    void* user_data)
{
    return (G_LIKELY(self) && G_LIKELY(handler)) ? g_signal_connect(self,
        SIGNAL_EXT_ON_RINGBACK_TONE_NAME, G_CALLBACK(handler), user_data) : 0;
}

gulong
qti_radio_ext_add_incoming_sms_handler(
    QtiRadioExt* self,
    QtiRadioExtIncomingSmsFunc handler,
    void* user_data)
{
    return (G_LIKELY(self) && G_LIKELY(handler)) ? g_signal_connect(self,
        SIGNAL_EXT_ON_INCOMING_SMS_NAME, G_CALLBACK(handler), user_data) : 0;
}

gulong
qti_radio_ext_add_sms_report_handler(
    QtiRadioExt* self,
    QtiRadioExtSmsReportFunc handler,
    void* user_data)
{
    return (G_LIKELY(self) && G_LIKELY(handler)) ? g_signal_connect(self,
        SIGNAL_EXT_ON_SMS_REPORT_NAME, G_CALLBACK(handler), user_data) : 0;
}

static
GBinderLocalReply*
qti_radio_ext_response(
    GBinderLocalObject* obj,
    GBinderRemoteRequest* req,
    guint code,
    guint flags,
    int* status,
    void* user_data)
{
    QtiRadioExt* self = THIS(user_data);
    const char* iface = gbinder_remote_request_interface(req);
    GBinderReader reader;
    guint32 serial = 0;

    gbinder_remote_request_init_reader(req, &reader);

    gbinder_reader_read_uint32(&reader, &serial);
    qti_radio_ext_log_resp(self, code, serial);
    qti_radio_ext_dump_data(&reader);

    if (serial) {
        QtiRadioExtRequest* req = g_hash_table_lookup(self->requests,
            KEY(serial));

        if (req && req->response_code == code) {
            g_object_ref(self);
            if (req->handle_response) {
                req->handle_response(req, &reader);
            }
            g_hash_table_remove(self->requests, KEY(serial));
            g_object_unref(self);
        } else {
            DBG("Unexpected response %s %u", iface, code);
            *status = GBINDER_STATUS_FAILED;
        }
    }

    return NULL;
}

static
void
qti_radio_ext_result_response(
    QtiRadioExtRequest* req,
    const GBinderReader* args)
{
    gint32 result;
    GBinderReader reader;
    QtiRadioExt* self = req->radio;
    QtiRadioExtResultRequest* result_req = G_CAST(req,
        QtiRadioExtResultRequest, base);

    gbinder_reader_copy(&reader, args);
    if (!gbinder_reader_read_int32(&reader, &result)) {
        ofono_warn("Failed to parse response");
        result = -1;
    }
    if (result_req->complete) {
        result_req->complete(self, result, &reader, req->user_data);
    }
}

static
void
qti_radio_ext_request_default_free(
    QtiRadioExtRequest* req)
{
    if (req->destroy) {
        req->destroy(req->user_data);
    }
    g_free(req);
}

static
void
qti_radio_ext_request_destroy(
    gpointer user_data)
{
    QtiRadioExtRequest* req = user_data;

    gbinder_client_cancel(req->radio->client, req->tx);
    req->free(req);
}

static
gpointer
qti_radio_ext_request_alloc(
    QtiRadioExt* self,
    gint32 resp,
    QtiRadioExtRequestHandlerFunc handler,
    GDestroyNotify destroy,
    void* user_data,
    gsize size)
{
    QtiRadioExtRequest* req = g_malloc0(size);

    req->radio = self;
    req->response_code = resp;
    req->handle_response = handler;
    req->id = qti_radio_ext_new_req_id();
    req->free = qti_radio_ext_request_default_free;
    req->destroy = destroy;
    req->user_data = user_data;
    g_hash_table_insert(self->requests, KEY(req->id), req);
    return req;
}

static
QtiRadioExtResultRequest*
qti_radio_ext_result_request_new(
    QtiRadioExt* self,
    gint32 resp,
    QtiRadioExtResultFunc complete,
    GDestroyNotify destroy,
    void* user_data)
{
    QtiRadioExtResultRequest* req =
        (QtiRadioExtResultRequest*)qti_radio_ext_request_alloc(self, resp,
            qti_radio_ext_result_response, destroy, user_data,
            sizeof(QtiRadioExtResultRequest));

    req->complete = complete;
    return req;
}

static
void
qti_radio_ext_request_sent(
    GBinderClient* client,
    GBinderRemoteReply* reply,
    int status,
    void* user_data)
{
    ((QtiRadioExtRequest*)user_data)->tx = 0;
}

static
gulong
qti_radio_ext_call(
    QtiRadioExt* self,
    gint32 code,
    gint32 serial,
    GBinderLocalRequest* req,
    GBinderClientReplyFunc reply,
    GDestroyNotify destroy,
    void* user_data)
{
    qti_radio_ext_log_req(self, code, serial);
    qti_radio_ext_dump_request(req);

    return gbinder_client_transact(self->client, code,
        GBINDER_TX_FLAG_ONEWAY, req, reply, destroy, user_data);
}

void
qti_radio_ext_cancel(
    QtiRadioExt* self,
    guint id)
{
    if (G_LIKELY(self) && G_LIKELY(id)) {
        g_hash_table_remove(self->requests, KEY(id));
    }
}


static
gulong
qti_radio_ext_submit_request(
    QtiRadioExtRequest* request,
    gint32 code,
    gint32 serial,
    GBinderLocalRequest* args)
{
    return (request->tx = qti_radio_ext_call(request->radio,
        code, serial, args, qti_radio_ext_request_sent, NULL, request));
}

static
guint
qti_radio_ext_result_request_submit(
    QtiRadioExt* self,
    gint32 req_code,
    gint32 resp_code,
    QtiRadioExtArgWriteFunc write_args,
    QtiRadioExtResultFunc complete,
    GDestroyNotify destroy,
    void* user_data,
    ...)
{
    if (G_LIKELY(self)) {
        GBinderLocalRequest* args;
        GBinderWriter writer;
        QtiRadioExtResultRequest* req =
            qti_radio_ext_result_request_new(self, resp_code,
                complete, destroy, user_data);
        const guint req_id = req->base.id;

        args = gbinder_client_new_request2(self->client, req_code);
        gbinder_local_request_init_writer(args, &writer);
        gbinder_writer_append_int32(&writer, req_id);
        if (write_args) {
            va_list va;

            va_start(va, user_data);
            write_args(&writer, va);
            va_end(va);
        }

        /* Submit the request */
        qti_radio_ext_submit_request(&req->base, req_code, req_id, args);
        gbinder_local_request_unref(args);
        if (req->base.tx) {
            /* Success */
            return req_id;
        }
        g_hash_table_remove(self->requests, KEY(req_id));
    }
    return 0;
}


static const GBinderClientIfaceInfo radio_iface_info[] = {
    {QTI_RADIO_1_2, QTI_RADIO_REQ_LAST_1_2 },
    {QTI_RADIO_1_1, QTI_RADIO_REQ_LAST_1_1 },
    {QTI_RADIO_1_0, QTI_RADIO_REQ_LAST_1_0 }
};

typedef struct qti_radio_interface_desc {
    QTI_RADIO_INTERFACE version;
    const char* radio;
    const char* response;
    const char* indication;
} QtiRadioInterfaceDesc;

#define QTI_RADIO_INTERFACE_DESC(v) \
        QTI_RADIO_INTERFACE_##v, \
        QTI_RADIO_##v, \
        QTI_RADIO_RESPONSE_##v, \
        QTI_RADIO_INDICATION_##v

static const QtiRadioInterfaceDesc qti_radio_interfaces[] = {
   { QTI_RADIO_INTERFACE_DESC(1_2) },
   { QTI_RADIO_INTERFACE_DESC(1_1) },
   { QTI_RADIO_INTERFACE_DESC(1_0) }
};

#define DEFAULT_INTERFACE QTI_RADIO_INTERFACE_1_2


static
QtiRadioExt*
qti_radio_ext_create(
    GBinderServiceManager* sm,
    GBinderRemoteObject* remote,
    const char* slot,
    const QtiRadioInterfaceDesc* desc)
{
    QtiRadioExt* self = g_object_new(THIS_TYPE, NULL);
    const gint code = QTI_RADIO_REQ_SET_CALLBACK;
    GBinderLocalRequest* req;
    GBinderWriter writer;
    int status;

    self->slot = g_strdup(slot);

    self->client = gbinder_client_new2(remote,
        radio_iface_info, G_N_ELEMENTS(radio_iface_info));
    self->response = gbinder_servicemanager_new_local_object(sm,
        desc->response, qti_radio_ext_response, self);
    self->indication = gbinder_servicemanager_new_local_object(sm,
        desc->indication, qti_radio_ext_indication, self);
    req = gbinder_client_new_request2(self->client, code);
    gbinder_local_request_init_writer(req, &writer);
    gbinder_writer_append_local_object(&writer, self->response);
    gbinder_writer_append_local_object(&writer, self->indication);
    qti_radio_ext_log_req(self, code, 0 /*serial*/);
    qti_radio_ext_dump_request(req);
    gbinder_remote_reply_unref(gbinder_client_transact_sync_reply(self->client,
        code, req, &status));
    gbinder_local_request_unref(req);

    return self;
}

/*==========================================================================*
 * API
 *==========================================================================*/


QtiRadioExt*
qti_radio_ext_new_with_version(
    const char* dev,
    const char* slot,
    QTI_RADIO_INTERFACE max_version)
{
    QtiRadioExt* self = NULL;

    GBinderServiceManager* sm = gbinder_servicemanager_new(dev);
    if (sm) {
        guint i;
        for (i = 0; i < G_N_ELEMENTS(qti_radio_interfaces) && !self; i++) {
            const QtiRadioInterfaceDesc* desc = qti_radio_interfaces + i;

            if (desc->version <= max_version) {
                char* fqname = g_strconcat(desc->radio, "/", slot, NULL);
                // try to connect to the service 5 times
                // service might not be ready yet
                GBinderRemoteObject* obj = NULL;
                for (int i = 0; i < 5; i++) {
                    obj = gbinder_servicemanager_get_service_sync(sm, fqname, NULL);
                    if (obj) {
                        break;
                    }
                    // wait 500ms before trying again
                    g_usleep(500000);
                }

                if (obj) {
                    DBG("Connected to %s", fqname);
                    self = qti_radio_ext_create(sm, obj, slot, desc);
                } else {
                    DBG("can't connect to %s", fqname);
                }
                g_free(fqname);
            }
        }
        gbinder_servicemanager_unref(sm);
    }

    return self;
}

QtiRadioExt*
qti_radio_ext_new(
    const char* dev,
    const char* slot)
{
    return qti_radio_ext_new_with_version(dev, slot, DEFAULT_INTERFACE);
}


QtiRadioExt*
qti_radio_ext_ref(
    QtiRadioExt* self)
{
    if (G_LIKELY(self)) {
        g_object_ref(self);
    }
    return self;
}

void
qti_radio_ext_unref(
    QtiRadioExt* self)
{
    if (G_LIKELY(self)) {
        g_object_unref(self);
    }
}

static
void
qti_radio_ext_set_reg_state_args(
    GBinderWriter* args,
    va_list va)
{
    gbinder_writer_append_int32(args, va_arg(va, gint32));
}

static
QTI_RADIO_REG_STATE
qti_radio_ext_reg_state(
    BINDER_EXT_IMS_REGISTRATION state)
{
    switch (state) {
    case BINDER_EXT_IMS_REGISTRATION_ON:
        return QTI_RADIO_REG_STATE_REGISTERED;
    case BINDER_EXT_IMS_REGISTRATION_OFF:
        return QTI_RADIO_REG_STATE_NOT_REGISTERED;
    default:
        return QTI_RADIO_REG_STATE_INVALID;
    }
}


guint
qti_radio_ext_set_reg_state(
    QtiRadioExt* self,
    BINDER_EXT_IMS_REGISTRATION state,
    QtiRadioExtResultFunc complete,
    GDestroyNotify destroy,
    void* user_data)
{
    QTI_RADIO_REG_STATE reg_state = qti_radio_ext_reg_state(state);

    DBG("Setting registration state %d", reg_state);

    return qti_radio_ext_result_request_submit(self,
        QTI_RADIO_REQ_REQ_REG_CHANGE,
        QTI_RADIO_RESP_REQ_REG_CHANGE,
        qti_radio_ext_set_reg_state_args,
        complete, destroy, user_data,
        reg_state);
}

/* From ofono-binder-plugin's binder-util.c */
static
void
binder_copy_hidl_string(
    GBinderWriter* writer,
    GBinderHidlString* dest,
    const char* src)
{
    gssize len = src ? strlen(src) : 0;
    dest->owns_buffer = TRUE;
    if (len > 0) {
        /* GBinderWriter takes ownership of the string contents */
        dest->len = (guint32) len;
        dest->data.str = gbinder_writer_memdup(writer, src, len + 1);
    } else {
        /* Replace NULL strings with empty strings */
        dest->data.str = "";
        dest->len = 0;
    }
}

static
void
qti_radio_ext_set_service_status_args(
    GBinderWriter* args,
    va_list va)
{
    const gint32 type = va_arg(va, gint32);
    const gint32 status = va_arg(va, gint32);

    /*
     * Each accTechStatus entry embeds a RegistrationInfo, whose two hidl_string
     * members need their own child buffers even when empty -- hence the nested
     * writer type.
     */
    static const GBinderWriterField qti_radio_status_for_access_tech_f[] = {
        GBINDER_WRITER_FIELD_HIDL_STRING
            (QtiRadioStatusForAccessTech, registration.error_message),
        GBINDER_WRITER_FIELD_HIDL_STRING
            (QtiRadioStatusForAccessTech, registration.uri),
        GBINDER_WRITER_FIELD_END()
    };
    static const GBinderWriterType qti_radio_status_for_access_tech_t = {
        GBINDER_WRITER_STRUCT_NAME_AND_SIZE(QtiRadioStatusForAccessTech),
        qti_radio_status_for_access_tech_f
    };

    static const GBinderWriterField qti_radio_service_status_info_f[] = {
        GBINDER_WRITER_FIELD_HIDL_VEC_BYTE
            (QtiRadioServiceStatusInfo, userdata),
        GBINDER_WRITER_FIELD_HIDL_VEC
            (QtiRadioServiceStatusInfo, acc_tech_status,
             &qti_radio_status_for_access_tech_t),
        GBINDER_WRITER_FIELD_END()
    };
    static const GBinderWriterType qti_radio_service_status_info_t = {
        GBINDER_WRITER_STRUCT_NAME_AND_SIZE(QtiRadioServiceStatusInfo),
        qti_radio_service_status_info_f
    };

    QtiRadioServiceStatusInfo* info =
        gbinder_writer_new0(args, QtiRadioServiceStatusInfo);
    QtiRadioStatusForAccessTech* acc_tech =
        gbinder_writer_new0(args, QtiRadioStatusForAccessTech);
    GBinderHidlVec* empty_userdata = gbinder_writer_new0(args, GBinderHidlVec);

    info->has_is_valid = TRUE;
    info->is_valid = TRUE;
    info->type = type;
    info->call_type = QTI_RADIO_CALL_TYPE_VOICE;
    info->status = status;
    info->restrict_cause = 0;
    info->rtt_mode = 0;

    info->userdata.count = 0;
    info->userdata.data.ptr = empty_userdata;
    info->userdata.owns_buffer = TRUE;

    /*
     * accTechStatus must not be empty. qcril's handler checks it and answers
     * "request misses some necessary information" -> GENERIC_FAILURE if the
     * list is missing, which is indistinguishable from the modem refusing.
     * One entry naming LTE is what enables VoLTE; the embedded RegistrationInfo
     * is an output field, so it is left zeroed with hasRegistration false.
     */
    acc_tech->network_mode = QTI_RADIO_TECH_TYPE_LTE;
    acc_tech->status = status;
    acc_tech->restrict_cause = 0;
    acc_tech->has_registration = FALSE;
    binder_copy_hidl_string(args, &acc_tech->registration.error_message, NULL);
    binder_copy_hidl_string(args, &acc_tech->registration.uri, NULL);

    info->acc_tech_status.count = 1;
    info->acc_tech_status.data.ptr = acc_tech;
    info->acc_tech_status.owns_buffer = TRUE;

    gbinder_writer_append_struct(args, info,
        &qti_radio_service_status_info_t, NULL);
}

/*
 * Enable or disable an IMS service in the modem.
 *
 * This -- not requestRegistrationChange -- is what actually turns VoLTE on.
 * On this vendor's RIL requestRegistrationChange is wired to QMI IMSS "set IMS
 * test mode", which a production modem refuses with GENERIC_FAILURE, so the
 * modem is never asked to register at all. Android's ims.apk drives IMS
 * through setServiceStatus, which reaches QMI IMSS "set IMS service enable
 * config".
 */
guint
qti_radio_ext_set_service_status(
    QtiRadioExt* self,
    QTI_RADIO_SERVICE_TYPE type,
    QTI_RADIO_STATUS status,
    QtiRadioExtResultFunc complete,
    GDestroyNotify destroy,
    void* user_data)
{
    DBG("Setting service %d status %d", type, status);

    return qti_radio_ext_result_request_submit(self,
        QTI_RADIO_REQ_SET_SERVICE_STATUS,
        QTI_RADIO_RESP_SET_SERVICE_STATUS,
        qti_radio_ext_set_service_status_args,
        complete, destroy, user_data,
        (gint32) type, (gint32) status);
}

static
void
qti_radio_ext_set_config_args(
    GBinderWriter* args,
    va_list va)
{
    const gint32 item = va_arg(va, gint32);
    const gboolean value = va_arg(va, gboolean);

    static const GBinderWriterField qti_radio_config_info_f[] = {
        GBINDER_WRITER_FIELD_HIDL_STRING(QtiRadioConfigInfo, string_value),
        GBINDER_WRITER_FIELD_END()
    };
    static const GBinderWriterType qti_radio_config_info_t = {
        GBINDER_WRITER_STRUCT_NAME_AND_SIZE(QtiRadioConfigInfo),
        qti_radio_config_info_f
    };

    QtiRadioConfigInfo* info = gbinder_writer_new0(args, QtiRadioConfigInfo);

    info->item = item;

    /*
     * Fill both value slots.
     *
     * qcril decides which one to read per config item, not from hasBoolValue:
     * for the presence items it takes intValue and logs "type: 0", so sending
     * the value only as boolValue arrives as
     * "Set config PRESENCE volte_user_opted_in_status to: 0" -- the request
     * succeeds in reaching the modem and carries the wrong value. Setting both
     * is correct whichever slot the item happens to use.
     */
    info->has_bool_value = TRUE;
    info->bool_value = value ? TRUE : FALSE;
    info->int_value = value ? 1 : 0;
    info->error_cause = 0;

    /* stringValue is unused here but still needs its child buffer */
    binder_copy_hidl_string(args, &info->string_value, NULL);

    gbinder_writer_append_struct(args, info, &qti_radio_config_info_t, NULL);
}

/*
 * The provisioning half of turning VoLTE on. setServiceStatus tells the modem
 * which services to run; this tells it the subscriber is allowed to run them.
 * On stock Android the telephony framework writes VOLTE_USER_OPT_IN_STATUS
 * when the user flips the VoLTE switch, and on carriers whose config the
 * handset does not recognise -- BSNL being the case this was written for --
 * the switch is hidden, the value is never written, and the modem never
 * registers no matter what else is configured.
 */
guint
qti_radio_ext_set_config(
    QtiRadioExt* self,
    QTI_RADIO_CONFIG_ITEM item,
    gboolean value,
    QtiRadioExtResultFunc complete,
    GDestroyNotify destroy,
    void* user_data)
{
    DBG("Setting config item %d to %d", item, value);

    return qti_radio_ext_result_request_submit(self,
        QTI_RADIO_REQ_SET_CONFIG,
        QTI_RADIO_RESP_SET_CONFIG,
        qti_radio_ext_set_config_args,
        complete, destroy, user_data,
        (gint32) item, (gboolean) value);
}

static
void
qti_radio_ext_get_config_args(
    GBinderWriter* args,
    va_list va)
{
    const gint32 item = va_arg(va, gint32);

    static const GBinderWriterField qti_radio_config_info_f[] = {
        GBINDER_WRITER_FIELD_HIDL_STRING(QtiRadioConfigInfo, string_value),
        GBINDER_WRITER_FIELD_END()
    };
    static const GBinderWriterType qti_radio_config_info_t = {
        GBINDER_WRITER_STRUCT_NAME_AND_SIZE(QtiRadioConfigInfo),
        qti_radio_config_info_f
    };

    QtiRadioConfigInfo* info = gbinder_writer_new0(args, QtiRadioConfigInfo);

    info->item = item;
    binder_copy_hidl_string(args, &info->string_value, NULL);

    gbinder_writer_append_struct(args, info, &qti_radio_config_info_t, NULL);
}

/*
 * Read a config item back. Takes the same ConfigInfo as setConfig with only
 * item filled in, and writes nothing -- which is what makes it usable as a
 * probe: qcril logs the item it mapped the request to and the handler it
 * dispatched to, so asking for an item reveals which of its config families
 * that item belongs to without changing anything.
 */
guint
qti_radio_ext_get_config(
    QtiRadioExt* self,
    QTI_RADIO_CONFIG_ITEM item,
    QtiRadioExtResultFunc complete,
    GDestroyNotify destroy,
    void* user_data)
{
    DBG("Getting config item %d", item);

    return qti_radio_ext_result_request_submit(self,
        QTI_RADIO_REQ_GET_CONFIG,
        QTI_RADIO_RESP_GET_CONFIG,
        qti_radio_ext_get_config_args,
        complete, destroy, user_data,
        (gint32) item);
}

static
void
qti_radio_ext_dial_args(
    GBinderWriter* args,
    va_list va)
{
    QtiRadioDialRequest* dial_request_writer;

    const char* number = va_arg(va, const char*);
    //gint32 clir = va_arg(va, gint32);

    // for some reason, clir from binder is wrong, so we use default
    gint32 clir = RADIO_CLIR_DEFAULT;

    static const GBinderWriterField qti_radio_dial_request_f[] = {
        GBINDER_WRITER_FIELD_HIDL_STRING
            (QtiRadioDialRequest, address),
        GBINDER_WRITER_FIELD_HIDL_VEC_BYTE
            (QtiRadioDialRequest, call_details.extras),
        GBINDER_WRITER_FIELD_HIDL_VEC_BYTE
            (QtiRadioDialRequest, call_details.local_ability),
        GBINDER_WRITER_FIELD_HIDL_VEC_BYTE
            (QtiRadioDialRequest, call_details.peer_ability),
        GBINDER_WRITER_FIELD_HIDL_STRING
            (QtiRadioDialRequest, call_details.sip_alternate_uri),
        GBINDER_WRITER_FIELD_END()
    };
    static const GBinderWriterType qti_radio_dial_request_t = {
        GBINDER_WRITER_STRUCT_NAME_AND_SIZE(QtiRadioDialRequest),
        qti_radio_dial_request_f
    };

    dial_request_writer = gbinder_writer_new0(args, QtiRadioDialRequest);

    GBinderHidlVec* empty_vec1 = gbinder_writer_new0(args, GBinderHidlVec);
    GBinderHidlVec* empty_vec2 = gbinder_writer_new0(args, GBinderHidlVec);
    GBinderHidlVec* empty_vec3 = gbinder_writer_new0(args, GBinderHidlVec);

    dial_request_writer->clir_mode = clir;
    switch (clir)
    {
    case RADIO_CLIR_SUPPRESSION:
        dial_request_writer->presentation = QTI_RADIO_IP_PRESENTATION_NUM_RESTRICTED;
        break;
    default:
        dial_request_writer->presentation = QTI_RADIO_IP_PRESENTATION_NUM_ALLOWED;
        break;
    }

    dial_request_writer->call_details.call_type = QTI_RADIO_CALL_TYPE_VOICE;
    dial_request_writer->call_details.call_domain = QTI_RADIO_CALL_DOMAIN_UNKNOWN;
    dial_request_writer->call_details.extras_length = 0;

    dial_request_writer->call_details.extras.count = 0;
    dial_request_writer->call_details.extras.data.ptr = empty_vec1;
    dial_request_writer->call_details.extras.owns_buffer = TRUE;

    dial_request_writer->call_details.local_ability.count = 0;
    dial_request_writer->call_details.local_ability.data.ptr = empty_vec2;
    dial_request_writer->call_details.local_ability.owns_buffer = TRUE;

    dial_request_writer->call_details.peer_ability.count = 0;
    dial_request_writer->call_details.peer_ability.data.ptr = empty_vec3;
    dial_request_writer->call_details.peer_ability.owns_buffer = TRUE;

    dial_request_writer->call_details.call_substate = 0; // none
    dial_request_writer->call_details.media_id = -1; // unknown
    dial_request_writer->call_details.cause_code = 0; // none
    dial_request_writer->call_details.rtt_mode = 0;

    dial_request_writer->has_call_details = FALSE;
    dial_request_writer->has_is_conference_uri = FALSE;
    dial_request_writer->is_conference_uri = FALSE;
    dial_request_writer->has_is_call_pull = FALSE;
    dial_request_writer->is_call_pull = FALSE;
    dial_request_writer->has_is_encrypted = FALSE;
    dial_request_writer->is_encrypted = FALSE;

    binder_copy_hidl_string(args, &dial_request_writer->address, number);
    binder_copy_hidl_string(args, &dial_request_writer->call_details.sip_alternate_uri, NULL);

    gbinder_writer_append_struct(args, dial_request_writer,
            &qti_radio_dial_request_t, NULL);

    DBG("Dialing (ext) %s", number);
}

guint
qti_radio_ext_dial(
    QtiRadioExt* self,
    const char* number,
    BINDER_EXT_TOA toa,
    BINDER_EXT_CALL_CLIR clir,
    BINDER_EXT_CALL_DIAL_FLAGS flags,
    QtiRadioExtResultFunc complete,
    GDestroyNotify destroy,
    void* user_data)
{
    return qti_radio_ext_result_request_submit(self,
        QTI_RADIO_REQ_DIAL,
        QTI_RADIO_RESP_DIAL,
        qti_radio_ext_dial_args,
        complete, destroy, user_data,
        number, clir);
}

static
void
qti_radio_ext_answer_args(
    GBinderWriter* args,
    va_list va)
{
    gint32 call_type = va_arg(va, gint32);
    gint32 presentation = va_arg(va, gint32);
    gint32 mode = va_arg(va, gint32);

    gbinder_writer_append_int32(args, call_type);
    gbinder_writer_append_int32(args, presentation);
    gbinder_writer_append_int32(args, mode);
}

guint
qti_radio_ext_answer(
    QtiRadioExt* self,
    QTI_RADIO_CALL_TYPE call_type,
    QTI_RADIO_IP_PRESENTATION presentation,
    QTI_RADIO_RTT_MODE mode,
    QtiRadioExtResultFunc complete,
    GDestroyNotify destroy,
    void* user_data)
{
    return qti_radio_ext_result_request_submit(self,
        QTI_RADIO_REQ_ANSWER,
        QTI_RADIO_RESP_ANSWER,
        qti_radio_ext_answer_args,
        complete, destroy, user_data,
        call_type, presentation, mode);
}

static
void
qti_radio_ext_hangup_args(
    GBinderWriter* args,
    va_list va)
{
    gint32 call_id = va_arg(va, gint32);

    static const GBinderWriterField qti_radio_hangup_request_info_f[] = {
        GBINDER_WRITER_FIELD_HIDL_STRING
            (QtiRadioHangupRequestInfo, conn_uri),
        GBINDER_WRITER_FIELD_HIDL_VEC_BYTE
            (QtiRadioHangupRequestInfo, fail_cause_response.errorinfo), // we are not going to use this, so byte is fine
        GBINDER_WRITER_FIELD_HIDL_STRING
            (QtiRadioHangupRequestInfo, fail_cause_response.network_error_string),
        GBINDER_WRITER_FIELD_HIDL_STRING
            (QtiRadioHangupRequestInfo, fail_cause_response.error_details.error_string),
        GBINDER_WRITER_FIELD_END()
    };

    static const GBinderWriterType qti_radio_hangup_request_info_t = {
        GBINDER_WRITER_STRUCT_NAME_AND_SIZE(QtiRadioHangupRequestInfo),
        qti_radio_hangup_request_info_f
    };

    QtiRadioHangupRequestInfo* hangup_request_writer = gbinder_writer_new0(args, QtiRadioHangupRequestInfo);

    // empty vec
    GBinderHidlVec* empty_vec = gbinder_writer_new0(args, GBinderHidlVec);

    hangup_request_writer->conn_index = call_id;
    hangup_request_writer->has_multi_party = FALSE;
    hangup_request_writer->has_fail_cause_response = FALSE;
    hangup_request_writer->multi_party = FALSE;
    hangup_request_writer->conf_id = 0;

    hangup_request_writer->fail_cause_response.errorinfo.count = 0;
    hangup_request_writer->fail_cause_response.errorinfo.data.ptr = empty_vec;
    hangup_request_writer->fail_cause_response.errorinfo.owns_buffer = TRUE;

    hangup_request_writer->fail_cause_response.fail_cause = 0;
    hangup_request_writer->fail_cause_response.has_error_details = FALSE;

    hangup_request_writer->fail_cause_response.error_details.error_code = 0;

    binder_copy_hidl_string(args, &hangup_request_writer->conn_uri, NULL);
    binder_copy_hidl_string(args, &hangup_request_writer->fail_cause_response.network_error_string, NULL);
    binder_copy_hidl_string(args, &hangup_request_writer->fail_cause_response.error_details.error_string, NULL);

    gbinder_writer_append_struct(args, hangup_request_writer,
            &qti_radio_hangup_request_info_t, NULL);
}

guint
qti_radio_ext_hangup(
    QtiRadioExt* self,
    guint call_id,
    QtiRadioExtResultFunc complete,
    GDestroyNotify destroy,
    void* user_data)
{
    return qti_radio_ext_result_request_submit(self,
        QTI_RADIO_REQ_HANGUP_1_2,
        QTI_RADIO_RESP_HANGUP_1_2,
        qti_radio_ext_hangup_args,
        complete, destroy, user_data,
        call_id);
}

static
void
qti_radio_ext_send_ims_sms_args(
    GBinderWriter* args,
    va_list va)
{
    const char* smsc = va_arg(va, const char*);
    const void* pdu = va_arg(va, const void*);
    gsize pdu_len = va_arg(va, gsize);
    guint msg_ref = va_arg(va, guint);
    BINDER_EXT_SMS_SEND_FLAGS flags = va_arg(va, BINDER_EXT_SMS_SEND_FLAGS);


    static const GBinderWriterField qti_radio_ims_sms_message_f[] = {
        GBINDER_WRITER_FIELD_HIDL_STRING
            (QtiRadioImsSmsMessage, format),
        GBINDER_WRITER_FIELD_HIDL_STRING
            (QtiRadioImsSmsMessage, smsc),
        GBINDER_WRITER_FIELD_HIDL_VEC_BYTE
            (QtiRadioImsSmsMessage, pdu),
        GBINDER_WRITER_FIELD_END()
    };

    static const GBinderWriterType qti_radio_ims_sms_message_t = {
        GBINDER_WRITER_STRUCT_NAME_AND_SIZE(QtiRadioImsSmsMessage),
        qti_radio_ims_sms_message_f
    };

    QtiRadioImsSmsMessage* sms = gbinder_writer_new0(args, QtiRadioImsSmsMessage);

    sms->message_ref = msg_ref;

    // I guess?
    // True if BINDER_EXT_SMS_SEND_RETRY is set
    sms->shall_retry = BINDER_EXT_SMS_SEND_RETRY == (flags & BINDER_EXT_SMS_SEND_RETRY);

    binder_copy_hidl_string(args, &sms->format, "3gpp");
    binder_copy_hidl_string(args, &sms->smsc, smsc);

    GBinderHidlVec* pdu_vec = gbinder_writer_new0(args, GBinderHidlVec);
    pdu_vec->count = pdu_len;
    pdu_vec->data.ptr = gbinder_writer_memdup(args, pdu, pdu_len);
    pdu_vec->owns_buffer = TRUE;

    sms->pdu = *pdu_vec;

    gbinder_writer_append_struct(args, sms, &qti_radio_ims_sms_message_t, NULL);
}

guint
qti_radio_ext_send_ims_sms(
    QtiRadioExt* self,
    const char* smsc,
    const void* pdu,
    gsize pdu_len,
    guint msg_ref,
    BINDER_EXT_SMS_SEND_FLAGS flags,
    QtiRadioExtResultFunc complete,
    GDestroyNotify destroy,
    void* user_data)
{
    return qti_radio_ext_result_request_submit(self,
        QTI_RADIO_REQ_SEND_IMS_SMS,
        QTI_RADIO_RESP_SEND_IMS_SMS,
        qti_radio_ext_send_ims_sms_args,
        complete, destroy, user_data,
        smsc, pdu, pdu_len, msg_ref, flags);
}

static
void
qti_radio_ext_acknowledge_sms_args(
    GBinderWriter* args,
    va_list va)
{
    guint32 message_ref = va_arg(va, guint32);
    guint32 sms_result = va_arg(va, guint32);

    gbinder_writer_append_int32(args, message_ref);
    gbinder_writer_append_int32(args, sms_result);
}

guint
qti_radio_ext_acknowledge_sms(
    QtiRadioExt* self,
    guint32 message_ref,
    gboolean sms_result,
    QtiRadioExtResultFunc complete,
    GDestroyNotify destroy,
    void* user_data)
{
    QTI_RADIO_IMS_SMS_DELIVER_STATUS_RESULT sms_result_code = sms_result ? QTI_RADIO_DELIVER_STATUS_OK : QTI_RADIO_DELIVER_STATUS_ERROR;

    return qti_radio_ext_result_request_submit(self,
        QTI_RADIO_REQ_ACK_SMS,
        QTI_RADIO_RESP_ACK_SMS,
        qti_radio_ext_acknowledge_sms_args,
        complete, destroy, user_data,
        message_ref, sms_result_code);
}

guint
qti_radio_ext_acknowledge_sms_report(
    QtiRadioExt* self,
    guint32 message_ref,
    gboolean sms_report,
    QtiRadioExtResultFunc complete,
    GDestroyNotify destroy,
    void* user_data)
{
    QTI_RADIO_IMS_SMS_STATUS_REPORT_RESULT sms_report_code = sms_report ? QTI_RADIO_STATUS_REPORT_OK : QTI_RADIO_STATUS_REPORT_ERROR;

    return qti_radio_ext_result_request_submit(self,
        QTI_RADIO_REQ_ACK_SMS_REPORT,
        QTI_RADIO_RESP_ACK_SMS_REPORT,
        qti_radio_ext_acknowledge_sms_args,
        complete, destroy, user_data,
        message_ref, sms_report_code);
}


static
void
qti_radio_ext_get_ims_reg_state_args(
    GBinderWriter* args,
    va_list va)
{
    // empty
}

guint
qti_radio_ext_get_ims_reg_state(
    QtiRadioExt* self,
    QtiRadioExtResultFunc complete,
    GDestroyNotify destroy,
    void* user_data)
{
    return qti_radio_ext_result_request_submit(self,
        QTI_RADIO_REQ_GET_IMS_REG_STATE,
        QTI_RADIO_RESP_GET_IMS_REG_STATE,
        qti_radio_ext_get_ims_reg_state_args,
        complete, destroy, user_data);
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

static
void
qti_radio_ext_finalize(
    GObject* object)
{
    QtiRadioExt* self = THIS(object);

    g_free(self->slot);
    g_hash_table_destroy(self->requests);
    gbinder_client_unref(self->client);
    gbinder_local_object_unref(self->response);
    gbinder_local_object_unref(self->indication);
    gutil_idle_pool_destroy(self->pool);
    G_OBJECT_CLASS(PARENT_CLASS)->finalize(object);
}

static
void
qti_radio_ext_init(
    QtiRadioExt* self)
{
    self->pool = gutil_idle_pool_new();
    self->requests = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL,
        qti_radio_ext_request_destroy);
}

static
void
qti_radio_ext_class_init(
    QtiRadioExtClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = qti_radio_ext_finalize;
    qti_radio_ext_signals[SIGNAL_IMS_REG_STATUS_CHANGED] =
        g_signal_new(SIGNAL_IMS_REG_STATUS_CHANGED_NAME, G_OBJECT_CLASS_TYPE(klass),
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE,
            1, G_TYPE_UINT);
    qti_radio_ext_signals[SIGNAL_EXT_CALL_STATE_CHANGED] =
        g_signal_new(SIGNAL_EXT_CALL_STATE_CHANGED_NAME, G_OBJECT_CLASS_TYPE(klass),
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE,
            1, G_TYPE_PTR_ARRAY);
    qti_radio_ext_signals[SIGNAL_EXT_ON_RING] =
        g_signal_new(SIGNAL_EXT_ON_RING_NAME, G_OBJECT_CLASS_TYPE(klass),
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE,
            0);
    qti_radio_ext_signals[SIGNAL_EXT_ON_RINGBACK_TONE] =
        g_signal_new(SIGNAL_EXT_ON_RINGBACK_TONE_NAME, G_OBJECT_CLASS_TYPE(klass),
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE,
            1, G_TYPE_BOOLEAN);
    qti_radio_ext_signals[SIGNAL_EXT_ON_INCOMING_SMS] =
        g_signal_new(SIGNAL_EXT_ON_INCOMING_SMS_NAME, G_OBJECT_CLASS_TYPE(klass),
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE,
            2, G_TYPE_POINTER, G_TYPE_UINT);
    qti_radio_ext_signals[SIGNAL_EXT_ON_SMS_REPORT] =
        g_signal_new(SIGNAL_EXT_ON_SMS_REPORT_NAME, G_OBJECT_CLASS_TYPE(klass),
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE,
            3, G_TYPE_POINTER, G_TYPE_UINT, G_TYPE_UINT);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
