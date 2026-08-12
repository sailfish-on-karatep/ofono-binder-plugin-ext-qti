/*
 *  oFono - Open Source Telephony - binder based adaptation QTI plugin
 *
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

#ifndef QTI_RADIO_EXT_TYPES_H
#define QTI_RADIO_EXT_TYPES_H

typedef enum qti_radio_interface {
    QTI_RADIO_INTERFACE_NONE = -1,
    QTI_RADIO_INTERFACE_1_0,
    QTI_RADIO_INTERFACE_1_1,
    QTI_RADIO_INTERFACE_1_2,
    QTI_RADIO_INTERFACE_COUNT
} QTI_RADIO_INTERFACE;

#define QTI_RADIO_IFACE                 "IImsRadio"
#define QTI_RADIO_RESPONSE_IFACE        "IImsRadioResponse"
#define QTI_RADIO_INDICATION_IFACE      "IImsRadioIndication"
#define QTI_RADIO_IFACE_PREFIX          "vendor.qti.hardware.radio.ims@"

#define QTI_RADIO_IFACE_1_0(x)          QTI_RADIO_IFACE_PREFIX "1.0::" x
#define QTI_RADIO_IFACE_1_1(x)          QTI_RADIO_IFACE_PREFIX "1.1::" x
#define QTI_RADIO_IFACE_1_2(x)          QTI_RADIO_IFACE_PREFIX "1.2::" x

#define QTI_RADIO_1_0                   QTI_RADIO_IFACE_1_0(QTI_RADIO_IFACE)
#define QTI_RADIO_1_1                   QTI_RADIO_IFACE_1_1(QTI_RADIO_IFACE)
#define QTI_RADIO_1_2                   QTI_RADIO_IFACE_1_2(QTI_RADIO_IFACE)

#define QTI_RADIO_RESPONSE_1_0          QTI_RADIO_IFACE_1_0(QTI_RADIO_RESPONSE_IFACE)
#define QTI_RADIO_RESPONSE_1_1          QTI_RADIO_IFACE_1_1(QTI_RADIO_RESPONSE_IFACE)
#define QTI_RADIO_RESPONSE_1_2          QTI_RADIO_IFACE_1_2(QTI_RADIO_RESPONSE_IFACE)

#define QTI_RADIO_INDICATION_1_0        QTI_RADIO_IFACE_1_0(QTI_RADIO_INDICATION_IFACE)
#define QTI_RADIO_INDICATION_1_1        QTI_RADIO_IFACE_1_1(QTI_RADIO_INDICATION_IFACE)
#define QTI_RADIO_INDICATION_1_2        QTI_RADIO_IFACE_1_2(QTI_RADIO_INDICATION_IFACE)

#define QTI_RADIO_REQ_LAST_1_0          40
#define QTI_RADIO_REQ_LAST_1_1          41
#define QTI_RADIO_REQ_LAST_1_2          47

/*
enum RegState : int32_t {
    REGISTERED,
    NOT_REGISTERED,
    REGISTERING,
    INVALID,
};
*/

typedef enum qti_radio_reg_state {
    QTI_RADIO_REG_STATE_REGISTERED = 0,
    QTI_RADIO_REG_STATE_NOT_REGISTERED = 1,
    QTI_RADIO_REG_STATE_REGISTERING = 2,
    QTI_RADIO_REG_STATE_INVALID = 3,
} QTI_RADIO_REG_STATE;

/*
enum ServiceType : int32_t {
    SMS,
    VOIP,
    VT,
    INVALID,
};
*/

typedef enum qti_radio_service_type {
    QTI_RADIO_SERVICE_TYPE_SMS = 0,
    QTI_RADIO_SERVICE_TYPE_VOIP = 1,
    QTI_RADIO_SERVICE_TYPE_VT = 2,
    QTI_RADIO_SERVICE_TYPE_INVALID = 3,
} QTI_RADIO_SERVICE_TYPE;

/*
enum RadioTechType : int32_t {
    RADIO_TECH_ANY,
    RADIO_TECH_UNKNOWN,
    ...
};

Only the values this plugin needs are spelled out; the full enum runs to
RADIO_TECH_INVALID = 21.
*/

typedef enum qti_radio_tech_type {
    QTI_RADIO_TECH_TYPE_ANY = 0,
    QTI_RADIO_TECH_TYPE_UNKNOWN = 1,
    QTI_RADIO_TECH_TYPE_LTE = 15,
    QTI_RADIO_TECH_TYPE_IWLAN = 20,
    QTI_RADIO_TECH_TYPE_INVALID = 21,
} QTI_RADIO_TECH_TYPE;

/*
enum StatusType : int32_t {
    STATUS_DISABLED,
    STATUS_PARTIALLY_ENABLED,
    STATUS_ENABLED,
    STATUS_NOT_SUPPORTED,
    STATUS_INVALID,
};
*/

typedef enum qti_radio_status {
    QTI_RADIO_STATUS_DISABLED = 0,
    QTI_RADIO_STATUS_PARTIALLY_ENABLED = 1,
    QTI_RADIO_STATUS_ENABLED = 2,
    QTI_RADIO_STATUS_NOT_SUPPORTED = 3,
    QTI_RADIO_STATUS_INVALID = 4,
} QTI_RADIO_STATUS;

/*
enum ServiceClassStatus : int32_t {
    DISABLED,
    ENABLED,
    INVALID,
};
*/

typedef enum qti_radio_service_status {
    QTI_RADIO_SERVICE_STATUS_DISABLED = 0,
    QTI_RADIO_SERVICE_STATUS_ENABLED = 1,
    QTI_RADIO_SERVICE_STATUS_INVALID = 2,
} QTI_RADIO_SERVICE_STATUS;

/*
enum CallState : int32_t {
    CALL_ACTIVE,
    CALL_HOLDING,
    CALL_DIALING,
    CALL_ALERTING,
    CALL_INCOMING,
    CALL_WAITING,
    CALL_END,
    CALL_STATE_INVALID,
};
*/

typedef enum qti_radio_call_state {
    QTI_RADIO_CALL_STATE_ACTIVE = 0,
    QTI_RADIO_CALL_STATE_HOLDING = 1,
    QTI_RADIO_CALL_STATE_DIALING = 2,
    QTI_RADIO_CALL_STATE_ALERTING = 3,
    QTI_RADIO_CALL_STATE_INCOMING = 4,
    QTI_RADIO_CALL_STATE_WAITING = 5,
    QTI_RADIO_CALL_STATE_END = 6,
    QTI_RADIO_CALL_STATE_INVALID = 7,
} QTI_RADIO_CALL_STATE;

/*
enum IpPresentation : int32_t {
    IP_PRESENTATION_NUM_ALLOWED,
    IP_PRESENTATION_NUM_RESTRICTED,
    IP_PRESENTATION_NUM_DEFAULT,
    IP_PRESENTATION_INVALID,
};

*/

typedef enum qti_radio_ip_presentation {
    QTI_RADIO_IP_PRESENTATION_NUM_ALLOWED = 0,
    QTI_RADIO_IP_PRESENTATION_NUM_RESTRICTED = 1,
    QTI_RADIO_IP_PRESENTATION_NUM_DEFAULT = 2,
    QTI_RADIO_IP_PRESENTATION_INVALID = 3,
} QTI_RADIO_IP_PRESENTATION;

/*
enum CallType : int32_t {
    CALL_TYPE_VOICE,
    CALL_TYPE_VT_TX,
    CALL_TYPE_VT_RX,
    CALL_TYPE_VT,
    CALL_TYPE_VT_NODIR,
    CALL_TYPE_CS_VS_TX,
    CALL_TYPE_CS_VS_RX,
    CALL_TYPE_PS_VS_TX,
    CALL_TYPE_PS_VS_RX,
    CALL_TYPE_UNKNOWN,
    CALL_TYPE_SMS,
    CALL_TYPE_UT,
    CALL_TYPE_INVALID,
};

*/

typedef enum qti_radio_call_type {
    QTI_RADIO_CALL_TYPE_VOICE = 0,
    QTI_RADIO_CALL_TYPE_VT_TX = 1,
    QTI_RADIO_CALL_TYPE_VT_RX = 2,
    QTI_RADIO_CALL_TYPE_VT = 3,
    QTI_RADIO_CALL_TYPE_VT_NODIR = 4,
    QTI_RADIO_CALL_TYPE_CS_VS_TX = 5,
    QTI_RADIO_CALL_TYPE_CS_VS_RX = 6,
    QTI_RADIO_CALL_TYPE_PS_VS_TX = 7,
    QTI_RADIO_CALL_TYPE_PS_VS_RX = 8,
    QTI_RADIO_CALL_TYPE_UNKNOWN = 9,
    QTI_RADIO_CALL_TYPE_SMS = 10,
    QTI_RADIO_CALL_TYPE_UT = 11,
    QTI_RADIO_CALL_TYPE_INVALID = 12,
} QTI_RADIO_CALL_TYPE;

/*

enum CallDomain : int32_t {
    CALL_DOMAIN_UNKNOWN,
    CALL_DOMAIN_CS,
    CALL_DOMAIN_PS,
    CALL_DOMAIN_AUTOMATIC,
    CALL_DOMAIN_NOT_SET,
    CALL_DOMAIN_INVALID,
};
*/

typedef enum qti_radio_call_domain {
    QTI_RADIO_CALL_DOMAIN_UNKNOWN = 0,
    QTI_RADIO_CALL_DOMAIN_CS = 1,
    QTI_RADIO_CALL_DOMAIN_PS = 2,
    QTI_RADIO_CALL_DOMAIN_AUTOMATIC = 3,
    QTI_RADIO_CALL_DOMAIN_NOT_SET = 4,
    QTI_RADIO_CALL_DOMAIN_INVALID = 5,
} QTI_RADIO_CALL_DOMAIN;

/*
enum RttMode : int32_t {
    RTT_MODE_DISABLED,
    RTT_MODE_FULL,
    RTT_MODE_INVALID,
};
*/

typedef enum qti_radio_rtt_mode {
    QTI_RADIO_RTT_MODE_DISABLED = 0,
    QTI_RADIO_RTT_MODE_FULL = 1,
    QTI_RADIO_RTT_MODE_INVALID = 2,
} QTI_RADIO_RTT_MODE;


/*
enum ImsSmsSendStatusResult : int32_t {
    // Message was sent successfully.
    SEND_STATUS_OK,
    // IMS provider failed to send the message and platform
        should not retry falling back to sending the message
        using the radio.
    SEND_STATUS_ERROR,
    // IMS provider failed to send the message and platform
        should retry again after setting TP-RD
    SEND_STATUS_ERROR_RETRY,
    // IMS provider failed to send the message and platform
        should retry falling back to sending the message
        using the radio.
    SEND_STATUS_ERROR_FALLBACK,
};
*/

typedef enum qti_radio_ims_sms_send_status_result {
    QTI_RADIO_SEND_STATUS_OK = 0,
    QTI_RADIO_SEND_STATUS_ERROR = 1,
    QTI_RADIO_SEND_STATUS_ERROR_RETRY = 2,
    QTI_RADIO_SEND_STATUS_ERROR_FALLBACK = 3,
} QTI_RADIO_IMS_SMS_SEND_STATUS_RESULT;

/*
enum ImsSmsSendFailureReason : int32_t {
    RESULT_ERROR_NONE,
    // Generic failure cause
    RESULT_ERROR_GENERIC_FAILURE,
    // Failed because radio was explicitly turned off
    RESULT_ERROR_RADIO_OFF,
    // Failed because no pdu provided
    RESULT_ERROR_NULL_PDU,
    // Failed because service is currently unavailable
    RESULT_ERROR_NO_SERVICE,
    // Failed because we reached the sending queue limit.
    RESULT_ERROR_LIMIT_EXCEEDED,
    // Failed because user denied the sending of this short code.
    RESULT_ERROR_SHORT_CODE_NOT_ALLOWED,
    // Failed because the user has denied this app
        ever send premium short codes.
    RESULT_ERROR_SHORT_CODE_NEVER_ALLOWED,
};
*/

typedef enum qti_radio_ims_sms_send_failure_reason {
    QTI_RADIO_RESULT_ERROR_NONE = 0,
    QTI_RADIO_RESULT_ERROR_GENERIC_FAILURE = 1,
    QTI_RADIO_RESULT_ERROR_RADIO_OFF = 2,
    QTI_RADIO_RESULT_ERROR_NULL_PDU = 3,
    QTI_RADIO_RESULT_ERROR_NO_SERVICE = 4,
    QTI_RADIO_RESULT_ERROR_LIMIT_EXCEEDED = 5,
    QTI_RADIO_RESULT_ERROR_SHORT_CODE_NOT_ALLOWED = 6,
    QTI_RADIO_RESULT_ERROR_SHORT_CODE_NEVER_ALLOWED = 7,
} QTI_RADIO_IMS_SMS_SEND_FAILURE_REASON;


/*
enum ImsSmsDeliverStatusResult : int32_t {
    Message was delivered successfully.
    DELIVER_STATUS_OK,
    Message was not delivered.
    DELIVER_STATUS_ERROR,
};
*/

typedef enum qti_radio_ims_sms_deliver_status_result {
    QTI_RADIO_DELIVER_STATUS_OK = 0,
    QTI_RADIO_DELIVER_STATUS_ERROR = 1,
} QTI_RADIO_IMS_SMS_DELIVER_STATUS_RESULT;

/*
enum ImsSmsStatusReportResult : int32_t {
    // Status Report was set successfully.
    STATUS_REPORT_STATUS_OK,
    // Error while setting status report
    STATUS_REPORT_STATUS_ERROR,
};
*/

typedef enum qti_radio_ims_sms_status_report_result {
    QTI_RADIO_STATUS_REPORT_OK = 0,
    QTI_RADIO_STATUS_REPORT_ERROR = 1,
} QTI_RADIO_IMS_SMS_STATUS_REPORT_RESULT;

/*
enum VerificationStatus : int32_t {
    //Telephone number is not validated.
    STATUS_VALIDATION_NONE,
    //Telephone number validation passed.
    STATUS_VALIDATION_PASS,
    // Telephone number validation failed.
    STATUS_VALIDATION_FAIL,
};
*/

typedef enum qti_radio_verification_status {
    QTI_RADIO_VALIDATION_NONE = 0,
    QTI_RADIO_VALIDATION_PASS = 1,
    QTI_RADIO_VALIDATION_FAIL = 2,
} QTI_RADIO_VERIFICATION_STATUS;

/*
struct ImsSmsMessage {
    uint32_t messageRef;
    string format;
    string smsc;
    bool shallRetry;
    vec<uint8_t>  pdu;
};
*/

typedef struct qti_radio_ims_sms_message {
    guint32 message_ref RADIO_ALIGNED(4);
    GBinderHidlString format RADIO_ALIGNED(8);
    GBinderHidlString smsc RADIO_ALIGNED(8);
    guint8 shall_retry RADIO_ALIGNED(1);
    GBinderHidlVec pdu RADIO_ALIGNED(8);
} RADIO_ALIGNED(8) QtiRadioImsSmsMessage;

/*
struct ImsSmsSendStatusReport {
    uint32_t messageRef;
    string format;
    vec<uint8_t>  pdu;
};
*/

typedef struct qti_radio_ims_sms_send_status_report {
    guint32 message_ref RADIO_ALIGNED(4);
    GBinderHidlString format RADIO_ALIGNED(8);
    GBinderHidlVec pdu RADIO_ALIGNED(8);
} RADIO_ALIGNED(8) QtiRadioImsSmsSendStatusReport;

/*
struct IncomingImsSms {
    string format;
    vec<uint8_t>  pdu;
    VerificationStatus verstat;
};
*/

typedef struct qti_radio_incoming_ims_sms {
    GBinderHidlString format RADIO_ALIGNED(8);
    GBinderHidlVec pdu RADIO_ALIGNED(8);
    guint32 verstat RADIO_ALIGNED(4);
} RADIO_ALIGNED(8) QtiRadioIncomingImsSms;


/*

struct RegistrationInfo {
    RegState state;
    uint32_t errorCode;
    string errorMessage;
    RadioTechType radioTech;
    string pAssociatedUris;
};
*/

typedef struct qti_radio_reg_info {
    QTI_RADIO_REG_STATE state RADIO_ALIGNED(4);
    guint32 error_code RADIO_ALIGNED(4);
    GBinderHidlString error_message RADIO_ALIGNED(8);
    guint32 radio_tech RADIO_ALIGNED(4);
    GBinderHidlString uri RADIO_ALIGNED(8);
} RADIO_ALIGNED(8) QtiRadioRegInfo;

/*
struct SipErrorInfo {
    uint32_t errorCode;
    string errorString;
};

*/

typedef struct qti_radio_sip_error_info {
    guint32 error_code RADIO_ALIGNED(4);
    GBinderHidlString error_string RADIO_ALIGNED(8);
} RADIO_ALIGNED(8) QtiRadioSipErrorInfo;

/*
struct CallFailCauseResponse {
    CallFailCause failCause;
    vec<uint8_t> errorinfo;
    string networkErrorString;
    bool hasErrorDetails;
    SipErrorInfo errorDetails;
};

*/

typedef struct qti_radio_call_fail_cause_response {
    guint32 fail_cause RADIO_ALIGNED(4);
    GBinderHidlVec errorinfo RADIO_ALIGNED(8);
    GBinderHidlString network_error_string RADIO_ALIGNED(8);
    guint8 has_error_details RADIO_ALIGNED(1);
    QtiRadioSipErrorInfo error_details RADIO_ALIGNED(8);
} RADIO_ALIGNED(8) QtiRadioCallFailCauseResponse;


/*

struct ServiceStatusInfo {
    bool hasIsValid;
    bool isValid;
    ServiceType type;
    CallType callType;
    StatusType status;
    vec<uint8_t> userdata;
    uint32_t restrictCause;
    vec<StatusForAccessTech> accTechStatus;
    RttMode rttMode;
};

    */

/*
struct StatusForAccessTech {
    RadioTechType networkMode;
    StatusType status;
    uint32_t restrictCause;
    bool hasRegistration;
    RegistrationInfo registration;
};

Offsets recovered from ims.apk's generated readEmbeddedFromParcel(): 0, 4, 8,
12, 16, total 64.
*/

typedef struct qti_radio_status_for_access_tech {
    guint32 network_mode RADIO_ALIGNED(4);
    guint32 status RADIO_ALIGNED(4);
    guint32 restrict_cause RADIO_ALIGNED(4);
    guint8 has_registration RADIO_ALIGNED(1);
    QtiRadioRegInfo registration RADIO_ALIGNED(8);
} RADIO_ALIGNED(8) QtiRadioStatusForAccessTech;

/*
 * hasIsValid and isValid are HIDL bool, which is one byte -- as guint32 they
 * pushed every following field 8 bytes out and made the struct 72 bytes where
 * the interface says 64, so it could never have matched a parcel. Nothing read
 * or wrote this struct before setServiceStatus, so the mistake was invisible.
 */
typedef struct qti_radio_service_status_info {
    guint8 has_is_valid RADIO_ALIGNED(1);
    guint8 is_valid RADIO_ALIGNED(1);
    guint32 type RADIO_ALIGNED(4);
    guint32 call_type RADIO_ALIGNED(4);
    guint32 status RADIO_ALIGNED(4);
    GBinderHidlVec userdata RADIO_ALIGNED(8);
    guint32 restrict_cause RADIO_ALIGNED(4);
    GBinderHidlVec acc_tech_status RADIO_ALIGNED(8);
    guint32 rtt_mode RADIO_ALIGNED(4);
} RADIO_ALIGNED(8) QtiRadioServiceStatusInfo;

/*
enum ConfigItem : int32_t {
    CONFIG_ITEM_NONE,
    ...
};

The full enum runs to 100-odd entries; only the ones this plugin sets are
spelled out. Values recovered from the device's own ims.apk, which is the
authority for this interface generation -- see scripts/hidl-from-apk.py.

VOLTE_USER_OPT_IN_STATUS is the provisioning flag Android's telephony
framework writes when the user turns VoLTE on, and it reaches QMI IMSS
"set client provisioning config". A modem that has IMS enabled and a service
status set will still decline to register while the subscriber has not opted
in, which is what the Xiaomi *#*#86583#*#* dialer code works around on stock
Android by forcing the framework's carrier check open.
*/
typedef enum qti_radio_config_item {
    QTI_RADIO_CONFIG_ITEM_NONE = 0,
    QTI_RADIO_CONFIG_ITEM_VLT_SETTING_ENABLED = 11,
    QTI_RADIO_CONFIG_ITEM_MOBILE_DATA_ENABLED = 26,
    QTI_RADIO_CONFIG_ITEM_VOLTE_USER_OPT_IN_STATUS = 33,
} QTI_RADIO_CONFIG_ITEM;

/*
struct ConfigInfo {
    ConfigItem item;
    bool hasBoolValue;
    bool boolValue;
    uint32_t intValue;
    string stringValue;
    ConfigFailureCause errorCause;
};

40 bytes. hasBoolValue and boolValue are HIDL bool, one byte each -- see the
note on QtiRadioServiceStatusInfo for what happens when that is got wrong.
*/
typedef struct qti_radio_config_info {
    guint32 item RADIO_ALIGNED(4);
    guint8 has_bool_value RADIO_ALIGNED(1);
    guint8 bool_value RADIO_ALIGNED(1);
    guint32 int_value RADIO_ALIGNED(4);
    GBinderHidlString string_value RADIO_ALIGNED(8);
    guint32 error_cause RADIO_ALIGNED(4);
} RADIO_ALIGNED(8) QtiRadioConfigInfo;

/*
struct CallDetails {
    CallType callType;
    CallDomain callDomain;
    uint32_t extrasLength;
    vec<string> extras;

    vec<ServiceStatusInfo> localAbility;
    vec<ServiceStatusInfo> peerAbility;
    uint32_t callSubstate;
    uint32_t mediaId;
    uint32_t causeCode;
    RttMode rttMode;
    string sipAlternateUri;
};
*/

typedef struct qti_radio_call_details {
    guint32 call_type RADIO_ALIGNED(4);
    guint32 call_domain RADIO_ALIGNED(4);
    guint32 extras_length RADIO_ALIGNED(4);

    GBinderHidlVec extras RADIO_ALIGNED(8);
    GBinderHidlVec local_ability RADIO_ALIGNED(8);
    GBinderHidlVec peer_ability RADIO_ALIGNED(8);

    guint32 call_substate RADIO_ALIGNED(4);
    guint32 media_id RADIO_ALIGNED(4);
    guint32 cause_code RADIO_ALIGNED(4);
    guint32 rtt_mode RADIO_ALIGNED(4);
    GBinderHidlString sip_alternate_uri RADIO_ALIGNED(8);
} RADIO_ALIGNED(8) QtiRadioCallDetails;


/*
struct CallInfo {
    CallState state;
    uint32_t index;
    uint32_t toa;
    bool hasIsMpty;
    bool isMpty;
    bool hasIsMT;
    bool isMT;
    uint32_t als;
    bool hasIsVoice;
    bool isVoice;
    bool hasIsVoicePrivacy;
    bool isVoicePrivacy;
    string number;
    uint32_t numberPresentation;
    string name;
    uint32_t namePresentation;
    bool hasCallDetails;
    CallDetails callDetails;
    bool hasFailCause;
    CallFailCauseResponse failCause;
    bool hasIsEncrypted;
    bool isEncrypted;
    bool hasIsCalledPartyRinging;
    bool isCalledPartyRinging;
    string historyInfo;
    bool hasIsVideoConfSupported;
    bool isVideoConfSupported;
};
*/

typedef struct qti_radio_call_info {
    QTI_RADIO_CALL_STATE state RADIO_ALIGNED(4);
    guint32 index RADIO_ALIGNED(4);
    guint32 toa RADIO_ALIGNED(4);
    guint8 has_is_mpty RADIO_ALIGNED(1);
    guint8 is_mpty RADIO_ALIGNED(1);
    guint8 has_is_mt RADIO_ALIGNED(1);
    guint8 is_mt RADIO_ALIGNED(1);
    guint32 als RADIO_ALIGNED(4);
    guint8 has_is_voice RADIO_ALIGNED(1);
    guint8 is_voice RADIO_ALIGNED(1);
    guint8 has_is_voice_privacy RADIO_ALIGNED(1);
    guint8 is_voice_privacy RADIO_ALIGNED(1);
    GBinderHidlString number RADIO_ALIGNED(8);
    guint32 number_presentation RADIO_ALIGNED(4);
    GBinderHidlString name RADIO_ALIGNED(8);
    guint32 name_presentation RADIO_ALIGNED(4);
    guint8 has_call_details RADIO_ALIGNED(1);
    QtiRadioCallDetails call_details RADIO_ALIGNED(8);
    guint8 has_fail_cause RADIO_ALIGNED(1);
    QtiRadioCallFailCauseResponse fail_cause RADIO_ALIGNED(8);
    guint8 has_is_encrypted RADIO_ALIGNED(1);
    guint8 is_encrypted RADIO_ALIGNED(1);
    guint8 has_is_called_party_ringing RADIO_ALIGNED(1);
    guint8 is_called_party_ringing RADIO_ALIGNED(1);
    GBinderHidlString history_info RADIO_ALIGNED(8);
    guint8 has_is_video_conf_supported RADIO_ALIGNED(1);
    guint8 is_video_conf_supported RADIO_ALIGNED(1);
} RADIO_ALIGNED(8) QtiRadioCallInfo;

/*
struct DialRequest {
    string address;
    uint32_t clirMode;
    IpPresentation presentation;
    bool hasCallDetails;
    CallDetails callDetails;
    bool hasIsConferenceUri;
    bool isConferenceUri;
    bool hasIsCallPull;
    bool isCallPull;
    bool hasIsEncrypted;
    bool isEncrypted;
};

*/

typedef struct qti_radio_dial_request {
    GBinderHidlString address RADIO_ALIGNED(8);
    guint32 clir_mode RADIO_ALIGNED(4);
    guint32 presentation RADIO_ALIGNED(4);
    guint8 has_call_details RADIO_ALIGNED(1);
    QtiRadioCallDetails call_details RADIO_ALIGNED(8);
    guint8 has_is_conference_uri RADIO_ALIGNED(1);
    guint8 is_conference_uri RADIO_ALIGNED(1);
    guint8 has_is_call_pull RADIO_ALIGNED(1);
    guint8 is_call_pull RADIO_ALIGNED(1);
    guint8 has_is_encrypted RADIO_ALIGNED(1);
    guint8 is_encrypted RADIO_ALIGNED(1);
} RADIO_ALIGNED(8) QtiRadioDialRequest;

/*
struct HangupRequestInfo {
    uint32_t connIndex;
    bool hasMultiParty;
    bool multiParty;
    string connUri;
    uint32_t conf_id;
    bool hasFailCauseResponse;
    CallFailCauseResponse failCauseResponse;
};
*/

typedef struct qti_radio_hangup_request_info {
    guint32 conn_index RADIO_ALIGNED(4);
    guint8 has_multi_party RADIO_ALIGNED(1);
    guint8 multi_party RADIO_ALIGNED(1);
    GBinderHidlString conn_uri RADIO_ALIGNED(8);
    guint32 conf_id RADIO_ALIGNED(4);
    guint8 has_fail_cause_response RADIO_ALIGNED(1);
    QtiRadioCallFailCauseResponse fail_cause_response RADIO_ALIGNED(8);
} RADIO_ALIGNED(8) QtiRadioHangupRequestInfo;


/* c(req, resp, callName, CALL_NAME) */
#define QTI_RADIO_EXT_IMS_CALL_1_0(c) \
    c(2, 1, dial, DIAL) \
    c(4, 11, getImsRegistrationState, GET_IMS_REG_STATE) \
    c(5, 2, answer, ANSWER) \
    c(6, 3, hangup, HANGUP) \
    c(7, 4, requestRegistrationChange, REQ_REG_CHANGE) \
    c(9, 6, setServiceStatus, SET_SERVICE_STATUS) \
    c(12, 9, setConfig, SET_CONFIG) \
    c(31, 28, setSuppServiceNotification, SET_SUPP_SVC_NOTIFICATION) \
    c(40, 29, cancelModifyCall, CANCEL_MODIFY_CALL) \

#define QTI_RADIO_EXT_IMS_CALL_1_1(c) \
    c(41, 103, hangup_1_1, HANGUP_1_1)

#define QTI_RADIO_EXT_IMS_CALL_1_2(c) \
    c(42, 203, hangup_1_2, HANGUP_1_2) \
    c(43, 37, sendImsSms, SEND_IMS_SMS) \
    c(44, 38, acknowledgeSms, ACK_SMS) \
    c(45, 39, acknowledgeSmsReport, ACK_SMS_REPORT) \
    c(46, 40, getSmsFormat, GET_SMS_FORMAT) \
    c(47, 41, sendGeolocationInfo_1_2, SEND_GEOLOCATION_INFO_1_2) \

typedef enum qti_radio_req {
    QTI_RADIO_REQ_SET_CALLBACK = 1, /* setCallback */
#define QTI_RADIO_REQ_(req,resp,Name,NAME) QTI_RADIO_REQ_##NAME = req,
    QTI_RADIO_EXT_IMS_CALL_1_0(QTI_RADIO_REQ_)
    QTI_RADIO_EXT_IMS_CALL_1_1(QTI_RADIO_REQ_)
    QTI_RADIO_EXT_IMS_CALL_1_2(QTI_RADIO_REQ_)
#undef QTI_RADIO_REQ_
} QTI_RADIO_REQ;

typedef enum ims_radio_resp {
#define QTI_RADIO_RESP_(req,resp,Name,NAME) QTI_RADIO_RESP_##NAME = resp,
    QTI_RADIO_EXT_IMS_CALL_1_0(QTI_RADIO_RESP_)
    QTI_RADIO_EXT_IMS_CALL_1_1(QTI_RADIO_RESP_)
    QTI_RADIO_EXT_IMS_CALL_1_2(QTI_RADIO_RESP_)
#undef QTI_RADIO_RESP_
} IMS_RADIO_RESP;

/* e(code, name, NAME) */
#define QTI_RADIO_IND_1_0(e) \
    e(1, onCallStateChanged, CALL_STATE_INDICATION) \
    e(2, onRing, RING_INDICATION) \
    e(3, onRingbackTone, RINGBACK_TONE_INDICATION) \
    e(4, onRegistrationChanged, REG_STATE_INDICATION) \
    e(5, onHandover, HANDOVER_INDICATION) \
    e(6, onServiceStatusChanged, SVC_STATUS_INDICATION) \
    e(7, radioStateChanged, RADIO_STATE_CHANGED_INDICATION)

#define QTI_RADIO_IND_1_1(e) \
    e(23, callStateChanged_1_1, CALL_STATE_INDICATION_1_1)

#define QTI_RADIO_IND_1_2(e) \
    e(24, callStateChanged_1_2, CALL_STATE_INDICATION_1_2) \
    e(25, onImsSmsStatusReport, SMS_STATUS_REPORT_INDICATION) \
    e(26, onIncomingImsSms, INCOMING_SMS_INDICATION) \
    e(27, onVopsChanged, VOPS_CHANGED_INDICATION)

typedef enum ims_radio_ind {
    /* vendor.mediatek.hardware.qtiradioex@3.0::IImsRadioIndication */
#define QTI_RADIO_IND_(code, name, NAME) QTI_RADIO_IND_##NAME = code,
    QTI_RADIO_IND_1_0(QTI_RADIO_IND_)
    QTI_RADIO_IND_1_1(QTI_RADIO_IND_)
    QTI_RADIO_IND_1_2(QTI_RADIO_IND_)
#undef QTI_RADIO_IND_
} IMS_RADIO_IND;

#endif /* QTI_RADIO_EXT_TYPES_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
