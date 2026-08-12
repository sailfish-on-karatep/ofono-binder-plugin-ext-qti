/*
 * Copyright (C) 2022 Jolla Ltd.
 * Copyright (C) 2022 Slava Monich <slava.monich@jolla.com>
 * Copyright (C) 2024 Marius Gripsgard <marius@ubports.com>
 *
 * You may use this file under the terms of the BSD license as follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in
 *      the documentation and/or other materials provided with the
 *      distribution.
 *   3. Neither the names of the copyright holders nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation
 * are those of the authors and should not be interpreted as representing
 * any official policies, either expressed or implied.
 */

#include "qti_ims.h"
#include "qti_slot.h"
#include "qti_radio_ext.h"

#include <binder_ext_ims_impl.h>

#include <ofono/log.h>

#include <gutil_macros.h>
#include <gutil_log.h>
#include <gbinder.h>

#undef DBG
#define DBG(fmt, ...) \
    gutil_log(GLOG_MODULE_CURRENT, GLOG_LEVEL_ALWAYS, "ims:"fmt, ##__VA_ARGS__)


typedef GObjectClass QtiImsClass;
typedef struct qti_ims {
    GObject parent;
    char* slot;
    QtiRadioExt* radio_ext;
    BINDER_EXT_IMS_STATE ims_state;
} QtiIms;

static
void
qti_ims_iface_init(
    BinderExtImsInterface* iface);

GType qti_ims_get_type() G_GNUC_INTERNAL;
G_DEFINE_TYPE_WITH_CODE(QtiIms, qti_ims, G_TYPE_OBJECT,
G_IMPLEMENT_INTERFACE(BINDER_EXT_TYPE_IMS, qti_ims_iface_init))

#define THIS_TYPE qti_ims_get_type()
#define THIS(obj) G_TYPE_CHECK_INSTANCE_CAST(obj, THIS_TYPE, QtiIms)
#define PARENT_CLASS qti_ims_parent_class

enum qti_ims_signal {
    SIGNAL_STATE_CHANGED,
    SIGNAL_GET_STATE,
    SIGNAL_COUNT
};

#define SIGNAL_STATE_CHANGED_NAME    "qti-ims-state-changed"
#define SIGNAL_GET_STATE_NAME        "qti-ims-get-state"

static guint qti_ims_signals[SIGNAL_COUNT] = { 0 };

typedef struct qti_ims_result_request {
    BinderExtIms* ext;
    BinderExtImsResultFunc complete;
    GDestroyNotify destroy;
    void* user_data;
} QtiImsResultRequest;

static
QtiImsResultRequest*
qti_ims_result_request_new(
    BinderExtIms* ext,
    BinderExtImsResultFunc complete,
    GDestroyNotify destroy,
    void* user_data)
{
    QtiImsResultRequest* req = g_new(QtiImsResultRequest, 1);

    req->ext = binder_ext_ims_ref(ext);
    req->complete = complete;
    req->destroy = destroy;
    req->user_data = user_data;
    return req;
}

static
void
qti_ims_result_request_free(
    QtiImsResultRequest* req)
{
    binder_ext_ims_unref(req->ext);
    g_free(req);
}

static
void
qti_ims_result_request_complete(
    QtiRadioExt* radio_ext,
    int result,
    GBinderReader* reader,
    void* user_data)
{
    QtiImsResultRequest* req = user_data;

    if (req->complete) {
        req->complete(req->ext, result ? BINDER_EXT_IMS_RESULT_ERROR :
            BINDER_EXT_IMS_RESULT_OK, req->user_data);
    }
}

static
void
qti_ims_result_request_destroy(
    gpointer user_data)
{
    QtiImsResultRequest* req = user_data;

    if (req->destroy) {
        req->destroy(req->user_data);
    }
    qti_ims_result_request_free(req);
}

static
void
qti_ims_reg_status_changed(
    QtiRadioExt* radio,
    QTI_RADIO_REG_STATE state,
    void* user_data)
{
    QtiIms* self = THIS(user_data);
    BINDER_EXT_IMS_STATE ims_state;

    switch (state) {
    case QTI_RADIO_REG_STATE_REGISTERING:
        ims_state = BINDER_EXT_IMS_STATE_REGISTERING;
        break;
    case QTI_RADIO_REG_STATE_REGISTERED:
        ims_state = BINDER_EXT_IMS_STATE_REGISTERED;
        break;
    case QTI_RADIO_REG_STATE_NOT_REGISTERED:
        ims_state = BINDER_EXT_IMS_STATE_NOT_REGISTERED;
        break;
    default:
        ims_state = BINDER_EXT_IMS_STATE_UNKNOWN;
    }

    if (ims_state != self->ims_state) {
        self->ims_state = ims_state;
        g_signal_emit(self, qti_ims_signals[SIGNAL_STATE_CHANGED], 0);
    }
}


static
void
qti_ims_reg_status_response(
    QtiRadioExt* radio_ext,
    int result,
    GBinderReader* reader,
    void* user_data)
{
    DBG("qti_ims_reg_status_response");
    QtiImsResultRequest* req = user_data;

    QTI_RADIO_REG_STATE state = QTI_RADIO_REG_STATE_INVALID;
    GBinderReader reader_copy;

    /*
     * The payload only means anything if the request succeeded. When the HAL
     * refuses getImsRegistrationState it still returns a RegistrationInfo, but
     * a zeroed one -- and QTI_RADIO_REG_STATE_REGISTERED is 0, so taking
     * info->state on faith turns every failure into a claim that IMS is
     * registered. That is exactly what happens on a modem that rejects the
     * request, and it is invisible from the outside: ofono publishes
     * IpMultimediaSystem Registered=true while no IMS registration exists and
     * every call falls back to CS.
     */
    if (result) {
        DBG("Get reg state failed, error %d", result);
        return;
    }

    gbinder_reader_copy(&reader_copy, reader);
    const QtiRadioRegInfo* info = qti_radio_ext_read_ims_reg_status_info(radio_ext, &reader_copy);

    if (!info) {
        DBG("Failed to parse QtiRadioRegInfo");
        return;
    }

    state = info->state;

    DBG("Get reg state %d now", state);

    qti_ims_reg_status_changed(radio_ext, state, req->user_data);
}

static
void
qti_ims_set_service_status_response(
    QtiRadioExt* radio_ext,
    int result,
    GBinderReader* reader,
    void* user_data)
{
    QtiIms* self = THIS(user_data);

    if (result) {
        DBG("%s setServiceStatus failed, error %d", self->slot, result);
    } else {
        DBG("%s IMS voice service enabled", self->slot);
    }
}

static
void
qti_ims_set_config_response(
    QtiRadioExt* radio_ext,
    int result,
    GBinderReader* reader,
    void* user_data)
{
    QtiIms* self = THIS(user_data);

    if (result) {
        DBG("%s setConfig failed, error %d", self->slot, result);
    } else {
        DBG("%s VoLTE user opt-in set", self->slot);
    }
}

/*==========================================================================*
 * BinderExtImsInterface
 *==========================================================================*/

static
BINDER_EXT_IMS_STATE
qti_ims_get_state(
    BinderExtIms* ext)
{
    QtiIms* self = THIS(ext);

    DBG("%s ims_state=%d", self->slot, self->ims_state);
    return self->ims_state;
}

static
void
qti_ims_get_registrations(
    BinderExtIms* ext, void* user_data)
{
    QtiIms* self = THIS(ext);

    // We should always check for updated state
    // get updated state
    QtiImsResultRequest* req = qti_ims_result_request_new(ext,
        NULL, NULL, self);
    qti_radio_ext_get_ims_reg_state(self->radio_ext,
        qti_ims_reg_status_response,
        qti_ims_result_request_destroy, req);

    DBG("Get reg state %d", self->ims_state);
}

static
guint
qti_ims_set_registration(
    BinderExtIms* ext,
    BINDER_EXT_IMS_REGISTRATION registration,
    BinderExtImsResultFunc complete,
    GDestroyNotify destroy,
    void* user_data)
{
    QtiIms* self = THIS(ext);
    const gboolean enabled = (registration != BINDER_EXT_IMS_REGISTRATION_OFF);

    // update the state
    g_signal_emit(self, qti_ims_signals[SIGNAL_GET_STATE], 0);

    /*
     * Enable the IMS voice service before asking for registration.
     *
     * requestRegistrationChange alone is not enough on every QTI RIL: some map
     * it to QMI IMSS "set IMS test mode", which a production modem refuses, so
     * nothing ever reaches the modem's IMS stack and it never attempts to
     * register. setServiceStatus is the call Android's ims.apk uses, and it
     * reaches QMI IMSS "set IMS service enable config". Sent fire-and-forget:
     * a HAL that does not implement it answers with an error we only log, and
     * the requestRegistrationChange below still drives ofono's result as
     * before.
     */
    /*
     * Provision the subscriber first.
     *
     * VOLTE_USER_OPT_IN_STATUS is what Android's telephony framework writes
     * when the user turns the VoLTE switch on, and it reaches QMI IMSS "set
     * client provisioning config". Where the handset does not recognise the
     * carrier -- BSNL, whose 4G postdates most shipped carrier configs -- the
     * framework hides that switch, never writes the value, and the modem
     * declines to register however well everything else is configured. That
     * is what the widely circulated *#*#86583#*#* dialer code forces open on
     * Xiaomi builds. ofono has no such switch and no carrier database, so it
     * is simply written whenever registration is asked for.
     *
     * Fire-and-forget, like setServiceStatus below: a HAL without setConfig
     * answers with an error we only log.
     */
    qti_radio_ext_set_config(self->radio_ext,
        QTI_RADIO_CONFIG_ITEM_VLT_SETTING_ENABLED, enabled,
        qti_ims_set_config_response, NULL, self);

    /*
     * VOLTE_USER_OPT_IN_STATUS is sent as well but is the weaker of the two:
     * qcril routes it to its *presence* config handler, and on this modem
     * that write comes back CONFIG_WRITE_FAILED -- the presence service is
     * one of the QMI services this firmware does not publish. Harmless to
     * attempt, and it is the item that matters on modems that do have it.
     */
    qti_radio_ext_set_config(self->radio_ext,
        QTI_RADIO_CONFIG_ITEM_VOLTE_USER_OPT_IN_STATUS, enabled,
        qti_ims_set_config_response, NULL, self);

    qti_radio_ext_set_service_status(self->radio_ext,
        QTI_RADIO_SERVICE_TYPE_VOIP,
        enabled ? QTI_RADIO_STATUS_ENABLED : QTI_RADIO_STATUS_DISABLED,
        qti_ims_set_service_status_response, NULL, self);

    QtiImsResultRequest* req = qti_ims_result_request_new(ext,
        complete, destroy, user_data);
    guint id = qti_radio_ext_set_reg_state(self->radio_ext,
        registration,
        complete ? qti_ims_result_request_complete : NULL,
        qti_ims_result_request_destroy, req);

    DBG("%s %s", self->slot, enabled ? "on" : "off");
    if (id) {
        return id;
    } else {
        qti_ims_result_request_free(req);
    }

    return 0;
}

static
void
qti_ims_cancel(
    BinderExtIms* ext,
    guint id)
{
    QtiIms* self = THIS(ext);

    /*
     * Cancel a pending operation identified by the id returned by the
     * above qti_ims_set_registration() call.
     */
    DBG("%s %u", self->slot, id);
}

static
gulong
qti_ims_add_state_handler(
    BinderExtIms* ext,
    BinderExtImsFunc handler,
    void* user_data)
{
    QtiIms* self = THIS(ext);

    DBG("%s", self->slot);
    return G_LIKELY(handler) ? g_signal_connect(self,
        SIGNAL_STATE_CHANGED_NAME, G_CALLBACK(handler), user_data) : 0;
}

static
gulong
qti_ims_add_get_state_handler(
    QtiIms* self,
    QtiImsGetRegStatusFunc handler)
{
    DBG("%s", self->slot);
    return G_LIKELY(handler) ? g_signal_connect(self,
        SIGNAL_GET_STATE_NAME, G_CALLBACK(handler), self) : 0;
}

static
void
qti_ims_iface_init(
    BinderExtImsInterface* iface)
{
    iface->version = BINDER_EXT_IMS_INTERFACE_VERSION;
    iface->flags = BINDER_EXT_IMS_INTERFACE_FLAG_VOICE_SUPPORT | BINDER_EXT_IMS_INTERFACE_FLAG_SMS_SUPPORT;
    iface->get_state = qti_ims_get_state;
    iface->set_registration = qti_ims_set_registration;
    iface->cancel = qti_ims_cancel;
    iface->add_state_handler = qti_ims_add_state_handler;
}

/*==========================================================================*
 * API
 *==========================================================================*/

BinderExtIms*
qti_ims_new(
    const char* slot,
    QtiRadioExt* radio_ext)
{
    QtiIms* self = g_object_new(THIS_TYPE, NULL);

    /*
     * This could be the place to register a listener that gets invoked
     * on registration state change and emits SIGNAL_STATE_CHANGED.
     */
    self->slot = g_strdup(slot);
    self->radio_ext = qti_radio_ext_ref(radio_ext);
    self->ims_state = BINDER_EXT_IMS_STATE_UNKNOWN;

    if (self->radio_ext) {
        qti_radio_ext_add_ims_reg_status_handler(self->radio_ext,
            qti_ims_reg_status_changed, self);
    }

    qti_ims_add_get_state_handler(self, qti_ims_get_registrations);

    return BINDER_EXT_IMS(self);
}

/*==========================================================================*
 * Internals
 *==========================================================================*/

static
void
qti_ims_finalize(
    GObject* object)
{
    QtiIms* self = THIS(object);

    g_free(self->slot);
    qti_radio_ext_unref(self->radio_ext);
    G_OBJECT_CLASS(PARENT_CLASS)->finalize(object);
}

static
void
qti_ims_init(
    QtiIms* self)
{
}

static
void
qti_ims_class_init(
    QtiImsClass* klass)
{
    G_OBJECT_CLASS(klass)->finalize = qti_ims_finalize;
    qti_ims_signals[SIGNAL_STATE_CHANGED] =
        g_signal_new(SIGNAL_STATE_CHANGED_NAME, G_OBJECT_CLASS_TYPE(klass),
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
    qti_ims_signals[SIGNAL_GET_STATE] =
        g_signal_new(SIGNAL_GET_STATE_NAME, G_OBJECT_CLASS_TYPE(klass),
            G_SIGNAL_RUN_FIRST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
