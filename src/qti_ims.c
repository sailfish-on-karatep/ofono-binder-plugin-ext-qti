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

#include <stdlib.h>

#undef DBG
#define DBG(fmt, ...) \
    gutil_log(GLOG_MODULE_CURRENT, GLOG_LEVEL_ALWAYS, "ims:"fmt, ##__VA_ARGS__)


typedef GObjectClass QtiImsClass;
typedef struct qti_ims {
    GObject parent;
    char* slot;
    QtiRadioExt* radio_ext;
    BINDER_EXT_IMS_STATE ims_state;
    gboolean services_enabled;
    gboolean config_map;
} QtiIms;

static
void
qti_ims_enable_services(
    struct qti_ims* self,
    gboolean enabled);

static
void
qti_ims_set_config_response(
    QtiRadioExt* radio_ext,
    int result,
    GBinderReader* reader,
    void* user_data);

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

    /*
     * Enable the IMS services here rather than waiting to be asked.
     *
     * ofono drives set_registration off a desired-state change it never
     * decides to make, so on this stack that callback does not run and the
     * modem is never told the voice service is on. This indication is the
     * earliest point at which the IMS HAL is demonstrably live -- it is the
     * modem telling us its registration state -- so it is a sound place to
     * send the enable sequence, and doing it once per slot keeps it away from
     * the retry loop a modem in a registration cycle would otherwise drive.
     */
    if (!self->services_enabled) {
        qti_ims_enable_services(self, TRUE);
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

/*
 * Recover which qcril radio-config item an ims ConfigItem maps to.
 *
 * qcril names the item it dispatched to only when the request is a *set*
 * ("Config item to set: QCRIL_QMI_RADIO_CONFIG_..."); on a get it logs nothing
 * identifying at all. So a getConfig sweep cannot recover the mapping, and the
 * table inside libril-qc-qmi-1.so cannot be resolved statically because
 * Android packs its relocations.
 *
 * Writing all 72 items to find out would be reckless. Instead each item is
 * read and then written back with the value it already had, which makes qcril
 * log the name while changing nothing.
 *
 * That is only truly a no-op for booleans. setConfig carries one boolean, so
 * writing back an item holding, say, a 2000 ms SIP timer would store 1 and
 * destroy it. Items whose current value is not plainly boolean are therefore
 * read and reported but never written -- their names stay unknown, which is an
 * acceptable price. The item being hunted, QIPCALL_VOLTE_ENABLED, is a
 * boolean, so it is inside the set this can reach.
 */
static
void
qti_ims_get_config_response(
    QtiRadioExt* radio_ext,
    int result,
    GBinderReader* reader,
    void* user_data)
{
    QtiIms* self = THIS(user_data);
    GBinderReader reader_copy;
    const QtiRadioConfigInfo* info;

    if (result) {
        DBG("%s getConfig result %d", self->slot, result);
        return;
    }

    gbinder_reader_copy(&reader_copy, reader);
    info = gbinder_reader_read_hidl_struct(&reader_copy, QtiRadioConfigInfo);
    if (!info) {
        DBG("%s getConfig: unparsable ConfigInfo", self->slot);
        return;
    }

    if (info->error_cause) {
        DBG("%s item %u: error %u", self->slot, info->item, info->error_cause);
        return;
    }

    if (info->has_bool_value) {
        DBG("%s item %u: bool %u -- writing back", self->slot, info->item,
            info->bool_value);
    } else if (info->int_value <= 1) {
        DBG("%s item %u: int %u -- writing back", self->slot, info->item,
            info->int_value);
    } else {
        DBG("%s item %u: int %u -- not boolean, left alone", self->slot,
            info->item, info->int_value);
        return;
    }

    if (self->config_map) {
        qti_radio_ext_set_config(self->radio_ext,
            (QTI_RADIO_CONFIG_ITEM) info->item,
            info->has_bool_value ? info->bool_value : (info->int_value != 0),
            qti_ims_set_config_response, NULL, self);
    }
}

/*
 * Diagnostic: ask for every config item in turn.
 *
 * getConfig writes nothing, but qcril logs which radio-config item it mapped
 * the request to and which handler it dispatched to, so a sweep recovers the
 * whole ims-item -> radio-item -> handler table from the device itself. That
 * matters because the item that reaches QMI IMSS "set IMS service enable
 * config" -- the call a working handset makes on SIM insert, and the one this
 * stack has never made -- is not documented anywhere we can read, and the
 * mapping table inside libril-qc-qmi-1.so cannot be resolved statically
 * because Android packs its relocations.
 *
 * Off unless QTI_IMS_CONFIG_PROBE is set in ofono's environment, and runs at
 * most once per process.
 */
static
void
qti_ims_config_probe(
    QtiIms* self)
{
    static gboolean probed = FALSE;
    const char* env = getenv("QTI_IMS_CONFIG_PROBE");
    const char* map = getenv("QTI_IMS_CONFIG_MAP");
    int item;

    if (probed || !env || !env[0] || env[0] == '0') {
        return;
    }
    probed = TRUE;

    /*
     * QTI_IMS_CONFIG_MAP additionally writes each boolean item back with the
     * value it already holds, purely so qcril names it. See the comment on
     * qti_ims_get_config_response().
     */
    self->config_map = (map && map[0] && map[0] != '0');
    if (self->config_map) {
        DBG("%s config probe: write-back mapping enabled", self->slot);
    }

    /* 0 is CONFIG_ITEM_NONE and 73 is CONFIG_ITEM_INVALID; skip both */
    DBG("%s config probe: sweeping items 1..72", self->slot);
    for (item = 1; item <= 72; item++) {
        qti_radio_ext_get_config(self->radio_ext,
            (QTI_RADIO_CONFIG_ITEM) item,
            qti_ims_get_config_response, NULL, self);
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
        DBG("%s setConfig accepted", self->slot);
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

/*
 * Tell the modem's IMS stack that the voice service is on.
 *
 * This used to live inside qti_ims_set_registration(), which turned out to be
 * a callback ofono never invokes on this stack: with Registration=auto the
 * plugin connects to IImsRadio, takes the first onRegistrationChanged
 * indication (state 1, not registered), caches it and stops. Nothing further
 * happens, so none of the calls below were ever made -- confirmed with ofono
 * debug on, where neither the config probe nor "Setting config item" appears,
 * and qcril logs no "Set config" line at all.
 *
 * That matters because the modem was doing nothing wrong. Enabling the whole
 * DIAG log mask and scanning for SIP shows it never puts a REGISTER on the
 * wire -- it is idle, not refused -- and the switch that would make it try is
 * exactly what these calls throw.
 */
static
void
qti_ims_enable_services(
    QtiIms* self,
    gboolean enabled)
{
    /*
     * Enable the IMS voice service before asking for registration.
     *
     * requestRegistrationChange alone is not enough on every QTI RIL: some map
     * it to QMI IMSS "set IMS test mode", which a production modem refuses, so
     * nothing ever reaches the modem's IMS stack and it never attempts to
     * register. setServiceStatus is the call Android's ims.apk uses.
     *
     * On this HAL build it is a no-op, and worth keeping only because it costs
     * nothing. The binder transaction completes on both slots -- request 9,
     * response 6, no error -- and qcril logs absolutely nothing in return: no
     * set_ims_service_enable_config, no service-enable path at all. The HAL
     * accepts the call and drops it. Only the setConfig calls below actually
     * reach the modem, so they are the lever on this device.
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
    qti_ims_config_probe(self);

    qti_radio_ext_set_config(self->radio_ext,
        QTI_RADIO_CONFIG_ITEM_VLT_SETTING_ENABLED, enabled,
        qti_ims_set_config_response, NULL, self);

    /*
     * MOBILE_DATA_ENABLED lands in qcril's QIPCALL family, not the
     * IMS_SERVICE_ENABLE one this comment used to claim. Both names exist in
     * qcril, which is what made the guess plausible, but the running RIL is
     * unambiguous once the call actually happens:
     *
     *   Config item to set: QCRIL_QMI_RADIO_CONFIG_QIPCALL_MOBILE_DATA_ENABLED
     *   Set config QIPCALL mobile_data_enabled to: 1 -- response success
     *
     * Item 11 resolves the same way, to CLIENT_PROVISIONING_ENABLE_VOLTE
     * rather than anything in QIPCALL. Both writes reach the modem and both
     * succeed.
     *
     * VOLTE_USER_OPT_IN_STATUS is no longer sent. The same sweep shows this
     * qcril refuses it, and every other item in the presence family (14-24,
     * 30, 32), on read as well as write -- so its earlier CONFIG_WRITE_FAILED
     * was not the modem declining a value, it was a config path this build
     * does not implement at all.
     */
    qti_radio_ext_set_config(self->radio_ext,
        QTI_RADIO_CONFIG_ITEM_MOBILE_DATA_ENABLED, TRUE,
        qti_ims_set_config_response, NULL, self);

    qti_radio_ext_set_service_status(self->radio_ext,
        QTI_RADIO_SERVICE_TYPE_VOIP,
        enabled ? QTI_RADIO_STATUS_ENABLED : QTI_RADIO_STATUS_DISABLED,
        qti_ims_set_service_status_response, NULL, self);

    self->services_enabled = enabled;
    DBG("%s IMS services %s", self->slot, enabled ? "enabled" : "disabled");
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

    qti_ims_enable_services(self, enabled);

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
