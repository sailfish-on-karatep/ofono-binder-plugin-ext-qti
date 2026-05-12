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
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 */

#include <glib-object.h>

#include "qti_ims_call.h"
#include "qti_radio_ext.h"
#include "qti_radio_ext_types.h"

#include <binder_ext_call_impl.h>
#include <gbinder.h>

#include <ofono/log.h>

#include <gutil_idlepool.h>
#include <gutil_macros.h>
#include <gutil_misc.h>
#include <gutil_log.h>

#undef DBG
#define DBG(fmt, ...) \
    gutil_log(GLOG_MODULE_CURRENT, GLOG_LEVEL_ALWAYS, "ims:"fmt, ##__VA_ARGS__)

typedef GObjectClass QtiImsCallClass;
typedef struct qti_ims_call {
    GObject parent;
    GUtilIdlePool* pool;
    QtiRadioExt* radio_ext;
    GPtrArray* calls;
    GHashTable* id_map;
} QtiImsCall;

static
void
qti_ims_call_iface_init(
    BinderExtCallInterface* iface);

GType qti_ims_call_get_type() G_GNUC_INTERNAL;
G_DEFINE_TYPE_WITH_CODE(QtiImsCall, qti_ims_call, G_TYPE_OBJECT,
G_IMPLEMENT_INTERFACE(BINDER_EXT_TYPE_CALL, qti_ims_call_iface_init))

#define THIS_TYPE qti_ims_call_get_type()
#define THIS(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, THIS_TYPE, QtiImsCall)
#define PARENT_CLASS qti_ims_call_parent_class

#define ID_KEY(id) GUINT_TO_POINTER(id)
#define ID_VALUE(id) GUINT_TO_POINTER(id)

typedef struct qti_ims_call_result_request {
    int ref_count;
    guint id;
    guint id_mapped;
    guint param;
    BinderExtCall* ext;
    BinderExtCallResultFunc complete;
    GDestroyNotify destroy;
    void* user_data;
} QtiImsCallResultRequest;

enum qti_ims_call_signal {
    SIGNAL_CALL_STATE_CHANGED,
    SIGNAL_CALL_END,
    SIGNAL_CALL_RING,
    SIGNAL_CALL_SUPP_SVC_NOTIFY,
    SIGNAL_CALL_RINGBACK_TONE,
    SIGNAL_COUNT
};

#define SIGNAL_CALL_STATE_CHANGED_NAME    "qti-ims-call-state-changed"
#define SIGNAL_CALL_END_NAME              "qti-ims-call-end"
#define SIGNAL_CALL_RING_NAME             "qti-ims-call-ring"
#define SIGNAL_CALL_SUPP_SVC_NOTIFY_NAME  "qti-ims-call-supp-svc-notify"
#define SIGNAL_CALL_RINGBACK_TONE_NAME    "qti-ims-call-ringback-tone"

static guint qti_ims_call_signals[SIGNAL_COUNT] = { 0 };

static
void
qti_call_info_free(
    gpointer data)
{
    BinderExtCallInfo* info = data;

    g_free((char*)info->number);
    g_free((char*)info->name);
    g_free(info);
}

static
QtiImsCallResultRequest*
qti_ims_call_result_request_new(
    BinderExtCall* ext,
    BinderExtCallResultFunc complete,
    GDestroyNotify destroy,
    void* user_data)
{
    QtiImsCallResultRequest* req =
        g_new0(QtiImsCallResultRequest, 1);

    req->ref_count = 1;
    req->ext = binder_ext_call_ref(ext);
    req->complete = complete;
    req->destroy = destroy;
    req->user_data = user_data;
    return req;
}

static
void
qti_ims_call_result_request_free(
    QtiImsCallResultRequest* req)
{
    BinderExtCall* ext = req->ext;

    if (req->destroy) {
        req->destroy(req->user_data);
    }
    if (req->id_mapped) {
        g_hash_table_remove(THIS(ext)->id_map, ID_KEY(req->id_mapped));
    }
    binder_ext_call_unref(ext);
    g_free(req);
}

static
gboolean
qti_ims_call_result_request_unref(
    QtiImsCallResultRequest* req)
{
    if (!--(req->ref_count)) {
        qti_ims_call_result_request_free(req);
        return TRUE;
    } else {
        return FALSE;
    }
}

static
void
qti_ims_call_result_request_destroy(
    gpointer req)
{
    qti_ims_call_result_request_unref(req);
}

/*==========================================================================*
 * BinderExtCallInterface
 *==========================================================================*/

// find call by id
static
BinderExtCallInfo*
qti_ims_call_info_find(
    QtiImsCall* self,
    guint call_id)
{
    for (int i = 0; i < self->calls->len; i++) {
        BinderExtCallInfo* info =
            (BinderExtCallInfo*) g_ptr_array_index(self->calls, i);
        if (info->call_id == call_id) {
            return info;
        }
    }
    return NULL;
}

static
void
qti_ims_call_handle_call_info(
    QtiRadioExt* radio,
    GPtrArray* updated_calls,
    void* user_data)
{
    QtiImsCall* self = THIS(user_data);

    // loop over the updated calls
    for (int i = 0; i < updated_calls->len; i++) {
        BinderExtCallInfo* info = g_ptr_array_index(updated_calls, i);
        BinderExtCallInfo* call = qti_ims_call_info_find(self, info->call_id);

        if (info->state == BINDER_EXT_CALL_STATE_END) {
            g_signal_emit(THIS(user_data),
                qti_ims_call_signals[SIGNAL_CALL_END], 0, info->call_id, "");

            if (call)
                g_ptr_array_remove(self->calls, call);
            continue;
        }  else if (call) {
            call->state = info->state;
        } else {
            // add a new call
            BinderExtCallInfo* copy = g_memdup2(info, sizeof(BinderExtCallInfo));
            copy->number = g_strdup(info->number);
            copy->name = g_strdup(info->name);
            g_ptr_array_add(self->calls, copy);
        }
    }

    g_signal_emit(THIS(user_data),
                    qti_ims_call_signals[SIGNAL_CALL_STATE_CHANGED], 0);
}

static
void
qti_ims_call_handle_ring(
    QtiRadioExt* radio,
    void* user_data)
{
    g_signal_emit(THIS(user_data),
        qti_ims_call_signals[SIGNAL_CALL_RING], 0);
}

static
void
qti_ims_call_handle_ringback_tone(
        QtiRadioExt* radio,
        gboolean status,
        void* user_data)
{
    g_signal_emit(THIS(user_data),
        qti_ims_call_signals[SIGNAL_CALL_RINGBACK_TONE], 0, status);
}

static
const BinderExtCallInfo* const*
qti_ims_call_get_calls(
    BinderExtCall* ext)
{
    static const BinderExtCallInfo* none = NULL;
    QtiImsCall* self = THIS(ext);

    return self->calls->len ? (const BinderExtCallInfo**)self->calls->pdata : &none;
}

static
void
qti_ims_call_result_response(
    QtiRadioExt* radio,
    int result,
    GBinderReader* reader,
    void* user_data)
{
    QtiImsCallResultRequest* req = user_data;
    BinderExtCallResultFunc complete = req->complete;

    if (complete) {
        complete(req->ext, result ? BINDER_EXT_CALL_RESULT_ERROR :
            BINDER_EXT_CALL_RESULT_OK, req->user_data);
    }

    DBG("qti_ims_call_result_response %d", result);
}

static
guint
qti_ims_call_dial(
    BinderExtCall* ext,
    const char* number,
    BINDER_EXT_TOA toa,
    BINDER_EXT_CALL_CLIR clir,
    BINDER_EXT_CALL_DIAL_FLAGS flags,
    BinderExtCallResultFunc complete,
    GDestroyNotify destroy,
    void* user_data)
{
    QtiImsCall* self = THIS(ext);

    QtiImsCallResultRequest* req = qti_ims_call_result_request_new(ext,
        complete, destroy, user_data);
    guint id = qti_radio_ext_dial(self->radio_ext, number, toa, clir, flags,
        qti_ims_call_result_response, qti_ims_call_result_request_destroy, req);

    if (id) {
        req->id = id;
        g_hash_table_insert(self->id_map, ID_KEY(id), ID_VALUE(id));
    } else {
        qti_ims_call_result_request_free(req);
    }

    DBG("Dialing return %d", id);

    return id;
}

static
guint
qti_ims_call_answer(
    BinderExtCall* ext,
    BINDER_EXT_CALL_ANSWER_FLAGS flags,
    BinderExtCallResultFunc complete,
    GDestroyNotify destroy,
    void* user_data)
{
    QtiImsCall* self = THIS(ext);
    QTI_RADIO_RTT_MODE mode = flags & BINDER_EXT_CALL_ANSWER_FLAG_RTT ?
        QTI_RADIO_RTT_MODE_FULL : QTI_RADIO_RTT_MODE_DISABLED;
    QTI_RADIO_IP_PRESENTATION presentation = QTI_RADIO_IP_PRESENTATION_NUM_DEFAULT;
    QTI_RADIO_CALL_TYPE call_type = QTI_RADIO_CALL_TYPE_VOICE;

    QtiImsCallResultRequest* req = qti_ims_call_result_request_new(ext,
        complete, destroy, user_data);

    guint id = qti_radio_ext_answer(self->radio_ext, call_type, presentation, mode,
        qti_ims_call_result_response, qti_ims_call_result_request_destroy, req);

    DBG("Answering return %d", id);

    if (id) {
        req->id = id;
        g_hash_table_insert(self->id_map, ID_KEY(id), ID_VALUE(id));
    } else {
        qti_ims_call_result_request_free(req);
    }

    return id;
}

static
guint
qti_ims_call_swap(
    BinderExtCall* ext,
    BINDER_EXT_CALL_SWAP_FLAGS swap_flags,
    BINDER_EXT_CALL_ANSWER_FLAGS answer_flags,
    BinderExtCallResultFunc complete,
    GDestroyNotify destroy,
    void* user_data)
{
    DBG("swap is not implemented yet");
    return 0;
}

static
guint
qti_ims_call_hangup(
    BinderExtCall* ext,
    guint call_id,
    BINDER_EXT_CALL_HANGUP_REASON reason,
    BINDER_EXT_CALL_HANGUP_FLAGS flags,
    BinderExtCallResultFunc complete,
    GDestroyNotify destroy,
    void* user_data)
{
    // eh, we can use built in hangup
    // some devices may not support this, so we need to implement
    // it eventually
    return 0;

    QtiImsCall* self = THIS(ext);

    QtiImsCallResultRequest* req = qti_ims_call_result_request_new(ext,
        complete, destroy, user_data);

    guint id = qti_radio_ext_hangup(self->radio_ext, call_id,
        qti_ims_call_result_response, qti_ims_call_result_request_destroy, req);

    DBG("Hanging up return %d", id);

    if (id) {
        req->id = id;
        g_hash_table_insert(self->id_map, ID_KEY(id), ID_VALUE(id));
    } else {
        qti_ims_call_result_request_free(req);
    }

    return id;
}

static
guint
qti_ims_call_conference(
    BinderExtCall* ext,
    BINDER_EXT_CALL_CONFERENCE_FLAGS flags,
    BinderExtCallResultFunc complete,
    GDestroyNotify destroy,
    void* user_data)
{
    DBG("conference is not implemented yet");
    return 0;
}

static
guint
qti_ims_call_send_dtmf(
    BinderExtCall* ext,
    const char* tones,
    BinderExtCallResultFunc complete,
    GDestroyNotify destroy,
    void* user_data)
{
    DBG("send_dtmf is not implemented yet");
    return 0;
}

static
void
qti_ims_call_cancel(
    BinderExtCall* ext,
    guint id)
{
    QtiImsCall* self = THIS(ext);
    const guint mapped = GPOINTER_TO_UINT(g_hash_table_lookup(self->id_map,
        ID_KEY(id)));

    qti_radio_ext_cancel(self->radio_ext, mapped ? mapped : id);
}

static
gulong
qti_ims_call_add_calls_changed_handler(
    BinderExtCall* ext,
    BinderExtCallFunc cb,
    void* user_data)
{
    return G_LIKELY(cb) ? g_signal_connect(THIS(ext),
        SIGNAL_CALL_STATE_CHANGED_NAME, G_CALLBACK(cb), user_data) : 0;
}

static
gulong
qti_ims_call_add_disconnect_handler(
    BinderExtCall* ext,
    BinderExtCallDisconnectFunc cb,
    void* user_data)
{
    return G_LIKELY(cb) ? g_signal_connect(THIS(ext),
        SIGNAL_CALL_END_NAME, G_CALLBACK(cb), user_data) : 0;
}

static
gulong
qti_ims_call_add_ring_handler(
    BinderExtCall* ext,
    BinderExtCallFunc cb,
    void* user_data)
{
    return G_LIKELY(cb) ? g_signal_connect(THIS(ext),
        SIGNAL_CALL_RING_NAME, G_CALLBACK(cb), user_data) : 0;
}

static
gulong
qti_ims_call_add_ringback_tone_handler(
    BinderExtCall* ext,
    BinderExtCallRingbackToneFunc cb,
    void* user_data)
{
    return G_LIKELY(cb) ? g_signal_connect(THIS(ext),
        SIGNAL_CALL_RINGBACK_TONE_NAME, G_CALLBACK(cb), user_data) : 0;
}

static
gulong
qti_ims_call_add_ssn_handler(
    BinderExtCall* ext,
    BinderExtCallSuppSvcNotifyFunc cb,
    void* user_data)
{
    return G_LIKELY(cb) ? g_signal_connect(THIS(ext),
        SIGNAL_CALL_SUPP_SVC_NOTIFY_NAME, G_CALLBACK(cb), user_data) : 0;
}

void
qti_ims_call_iface_init(
    BinderExtCallInterface* iface)
{
    iface->flags |= BINDER_EXT_CALL_INTERFACE_FLAG_IMS_SUPPORT |
        BINDER_EXT_CALL_INTERFACE_FLAG_IMS_REQUIRED;
    iface->version = BINDER_EXT_CALL_INTERFACE_VERSION;
    iface->get_calls = qti_ims_call_get_calls;
    iface->dial = qti_ims_call_dial;
    iface->answer = qti_ims_call_answer;
    iface->swap = qti_ims_call_swap;
    iface->conference = qti_ims_call_conference;
    iface->send_dtmf = qti_ims_call_send_dtmf;
    iface->hangup = qti_ims_call_hangup;
    iface->cancel = qti_ims_call_cancel;
    iface->add_calls_changed_handler =
        qti_ims_call_add_calls_changed_handler;
    iface->add_disconnect_handler = qti_ims_call_add_disconnect_handler;
    iface->add_ring_handler = qti_ims_call_add_ring_handler;
    iface->add_ssn_handler = qti_ims_call_add_ssn_handler;
    iface->add_ringback_tone_handler = qti_ims_call_add_ringback_tone_handler;
}

/*==========================================================================*
 * API
 *==========================================================================*/

BinderExtCall*
qti_ims_call_new(
    QtiRadioExt* radio_ext)
{
    if (G_LIKELY(radio_ext)) {
        QtiImsCall* self = g_object_new(THIS_TYPE, NULL);

        self->radio_ext = qti_radio_ext_ref(radio_ext);
        self->calls = g_ptr_array_new_with_free_func(qti_call_info_free);

        qti_radio_ext_add_call_state_handler(radio_ext,
            qti_ims_call_handle_call_info, self);
        qti_radio_ext_add_ring_handler(radio_ext,
            qti_ims_call_handle_ring, self);
        qti_radio_ext_add_ringback_tone_handler(radio_ext,
            qti_ims_call_handle_ringback_tone, self);

        return BINDER_EXT_CALL(self);
    }
    return NULL;
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

static
void
qti_ims_call_finalize(
    GObject* object)
{
    QtiImsCall* self = THIS(object);

    qti_radio_ext_unref(self->radio_ext);
    gutil_idle_pool_destroy(self->pool);
    g_ptr_array_free(self->calls, TRUE);
    g_hash_table_unref(self->id_map);
    G_OBJECT_CLASS(PARENT_CLASS)->finalize(object);
}

static
void
qti_ims_call_init(
    QtiImsCall* self)
{
    self->pool = gutil_idle_pool_new();
    self->id_map = g_hash_table_new(g_direct_hash, g_direct_equal);
}

static
void
qti_ims_call_class_init(
    QtiImsCallClass* klass)
{
    GType type = G_OBJECT_CLASS_TYPE(klass);

    G_OBJECT_CLASS(klass)->finalize = qti_ims_call_finalize;
    qti_ims_call_signals[SIGNAL_CALL_STATE_CHANGED] =
        g_signal_new(SIGNAL_CALL_STATE_CHANGED_NAME, type,
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
    qti_ims_call_signals[SIGNAL_CALL_END] =
        g_signal_new(SIGNAL_CALL_END_NAME, type,
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE,
            2, G_TYPE_INT, G_TYPE_INT);
    qti_ims_call_signals[SIGNAL_CALL_RING] =
        g_signal_new(SIGNAL_CALL_RING_NAME, type, G_SIGNAL_RUN_FIRST, 0,
            NULL, NULL, NULL, G_TYPE_NONE, 0);
    qti_ims_call_signals[SIGNAL_CALL_SUPP_SVC_NOTIFY] =
        g_signal_new(SIGNAL_CALL_SUPP_SVC_NOTIFY_NAME, type,
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE,
            1, G_TYPE_POINTER);
    qti_ims_call_signals[SIGNAL_CALL_RINGBACK_TONE] =
            g_signal_new(SIGNAL_CALL_RINGBACK_TONE_NAME, type, G_SIGNAL_RUN_FIRST, 0,
                         NULL, NULL, NULL, G_TYPE_NONE, 1, G_TYPE_BOOLEAN);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
