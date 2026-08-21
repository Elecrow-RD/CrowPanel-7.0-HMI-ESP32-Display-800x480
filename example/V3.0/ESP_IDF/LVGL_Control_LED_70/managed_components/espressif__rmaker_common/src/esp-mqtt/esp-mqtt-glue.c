/*
 * SPDX-FileCopyrightText: 2021 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_log.h>
#include <mqtt_client.h>
#include <esp_event.h>
#include <esp_rmaker_common_events.h>
#include <esp_rmaker_mqtt_glue.h>
#include <esp_idf_version.h>
#include <esp_rmaker_utils.h>
#ifdef CONFIG_ESP_RMAKER_MQTT_PORT_443
#define ESP_RMAKER_MQTT_USE_PORT_443
#endif

#ifdef CONFIG_ESP_RMAKER_MQTT_USE_CERT_BUNDLE
#define ESP_RMAKER_MQTT_USE_CERT_BUNDLE
#include <esp_crt_bundle.h>
#endif

static const char *TAG = "esp_mqtt_glue";

#define MAX_MQTT_SUBSCRIPTIONS      CONFIG_ESP_RMAKER_MAX_MQTT_SUBSCRIPTIONS

/* Subscription states for tracking subscription lifecycle */
typedef enum {
    MQTT_SUB_STATE_NONE = 0,        /* Not subscribed */
    MQTT_SUB_STATE_REQUESTED,       /* Subscription request sent, waiting for SUBACK */
    MQTT_SUB_STATE_ACKNOWLEDGED,    /* SUBACK received, subscription active */
    MQTT_SUB_STATE_FAILED           /* Subscription failed */
} mqtt_subscription_state_t;

typedef struct {
    char *topic;
    esp_rmaker_mqtt_subscribe_cb_t cb;
    void *priv;
    mqtt_subscription_state_t state;
    int msg_id;                     /* Message ID from last subscribe request */
    uint8_t qos;                    /* QoS level for this subscription */
} esp_mqtt_glue_subscription_t;

typedef struct {
    esp_mqtt_client_handle_t mqtt_client;
    esp_rmaker_mqtt_conn_params_t *conn_params;
    esp_mqtt_glue_subscription_t *subscriptions[MAX_MQTT_SUBSCRIPTIONS];
} esp_mqtt_glue_data_t;
esp_mqtt_glue_data_t *mqtt_data;

typedef struct {
    char *data;
    char *topic;
} esp_mqtt_glue_long_data_t;

static void esp_mqtt_glue_deinit(void);

/**
 * @brief Check if an MQTT topic matches a subscription pattern with wildcards
 * 
 * Supports MQTT single-level wildcard:
 * - '+' matches a single level (e.g., "node/+/params" matches "node/device1/params")
 * 
 * @param topic_filter The subscription pattern (may contain '+' wildcards)
 * @param topic_name The actual topic name to match
 * @param topic_len Length of the topic name
 * @return true if the topic matches the filter, false otherwise
 */
static bool mqtt_topic_matches(const char *topic_filter, const char *topic_name, int topic_len)
{
    const char *filter_pos = topic_filter;
    const char *topic_pos = topic_name;
    int topic_consumed = 0;

    while (*filter_pos && topic_consumed < topic_len) {
        if (*filter_pos == '+') {
            // Single-level wildcard - skip to next '/' or end of topic
            while (topic_consumed < topic_len && *topic_pos != '/') {
                topic_pos++;
                topic_consumed++;
            }
            filter_pos++;
        } else if (*filter_pos == *topic_pos) {
            // Characters match, advance both
            filter_pos++;
            topic_pos++;
            topic_consumed++;
        } else {
            // Characters don't match
            return false;
        }
    }

    // Both strings must be fully consumed
    return (*filter_pos == '\0' && topic_consumed == topic_len);
}

/* Helper function to reset all subscription states */
static void esp_mqtt_glue_reset_subscription_states(void)
{
    for (int i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
        if (mqtt_data->subscriptions[i]) {
            mqtt_data->subscriptions[i]->state = MQTT_SUB_STATE_NONE;
        }
    }
}

static void esp_mqtt_glue_subscribe_callback(const char *topic, int topic_len, const char *data, int data_len)
{
    esp_mqtt_glue_subscription_t **subscriptions = mqtt_data->subscriptions;
    int i;
    for (i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
        if (subscriptions[i]) {
            if ((mqtt_topic_matches(subscriptions[i]->topic, topic, topic_len))) {
                char *actual_topic = strndup(topic, topic_len);
                if (!actual_topic) {
                    ESP_LOGE(TAG, "Failed to allocate memory for actual topic");
                    return;
                }
                /* send the actual topic to the callback */
                subscriptions[i]->cb(actual_topic, (void *)data, data_len, subscriptions[i]->priv);
                free(actual_topic);
            }
        }
    }
}

/*
 * Compatibility wrapper for ESP-IDF v5.1.2+ esp_mqtt_client_subscribe macro issue
 * The _Generic macro doesn't handle const char* topic type properly in older versions
 * esp_mqtt_client_subscribe_single was introduced in ESP-IDF v5.1.2 to fix this
 * See: https://github.com/espressif/esp-idf/issues/13414
 */
static inline int _esp_mqtt_client_subscribe(esp_mqtt_client_handle_t client, const char *topic, int qos)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 1, 2)
    return esp_mqtt_client_subscribe_single(client, topic, qos);
#else
    return esp_mqtt_client_subscribe(client, topic, qos);
#endif
}

static esp_err_t esp_mqtt_glue_subscribe(const char *topic, esp_rmaker_mqtt_subscribe_cb_t cb, uint8_t qos, void *priv_data)
{
    if (!mqtt_data || !topic || !cb) {
        return ESP_FAIL;
    }

    esp_mqtt_glue_subscription_t *existing_entry = NULL;
    bool topic_has_active_subscription = false;
    int empty_slot = -1;

    /* Single pass: gather all the info we need */
    for (int i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
        if (mqtt_data->subscriptions[i]) {
            if (strcmp(topic, mqtt_data->subscriptions[i]->topic) == 0) {
                /* Same topic found */
                if (cb == mqtt_data->subscriptions[i]->cb) {
                    /* Same callback too - this is an update */
                    existing_entry = mqtt_data->subscriptions[i];
                }
                /* Check if this topic has an active subscription */
                if (mqtt_data->subscriptions[i]->state == MQTT_SUB_STATE_ACKNOWLEDGED) {
                    topic_has_active_subscription = true;
                }
            }
        } else if (empty_slot == -1) {
            empty_slot = i;
        }
    }

    /* Handle existing entry (same topic + same callback) */
    if (existing_entry) {
        existing_entry->priv = priv_data;

        bool need_resubscribe = false;

        if (existing_entry->state != MQTT_SUB_STATE_ACKNOWLEDGED) {
            /* Not acknowledged yet, need to re-subscribe */
            need_resubscribe = true;
        } else if (existing_entry->qos < qos) {
            /* QoS upgrade needed, re-subscribe */
            need_resubscribe = true;
            ESP_LOGD(TAG, "QoS upgrade requested for topic: %s (%d->%d)", topic, existing_entry->qos, qos);
        }

        if (need_resubscribe) {
            int ret = _esp_mqtt_client_subscribe(mqtt_data->mqtt_client, topic, qos);
            if (ret >= 0) {
                existing_entry->msg_id = ret;
                existing_entry->state = MQTT_SUB_STATE_REQUESTED;
                existing_entry->qos = qos;
                ESP_LOGD(TAG, "Re-subscribing to topic: %s (msg_id: %d, QoS: %d)", topic, ret, qos);
            } else {
                existing_entry->state = MQTT_SUB_STATE_FAILED;
                ESP_LOGW(TAG, "Failed to re-subscribe to topic: %s", topic);
            }
        }
        return ESP_OK;
    }

    /* Need to create new entry */
    if (empty_slot == -1) {
        ESP_LOGE(TAG, "No space for new subscription to topic: %s", topic);
        return ESP_FAIL;
    }

    /* Create and populate new subscription */
    esp_mqtt_glue_subscription_t *subscription = calloc(1, sizeof(esp_mqtt_glue_subscription_t));
    if (!subscription) {
        ESP_LOGE(TAG, "Failed to allocate memory for subscription");
        return ESP_FAIL;
    }

    subscription->topic = strdup(topic);
    if (!subscription->topic) {
        free(subscription);
        ESP_LOGE(TAG, "Failed to allocate memory for topic string");
        return ESP_FAIL;
    }

    subscription->priv = priv_data;
    subscription->cb = cb;
    subscription->qos = qos;
    subscription->state = topic_has_active_subscription ? MQTT_SUB_STATE_ACKNOWLEDGED : MQTT_SUB_STATE_NONE;

    /* Add to database first */
    mqtt_data->subscriptions[empty_slot] = subscription;

    /* Send MQTT subscribe only if needed */
    if (!topic_has_active_subscription) {
        int ret = _esp_mqtt_client_subscribe(mqtt_data->mqtt_client, topic, qos);
        if (ret >= 0) {
            subscription->msg_id = ret;
            subscription->state = MQTT_SUB_STATE_REQUESTED;
            ESP_LOGD(TAG, "Subscribed to topic: %s (msg_id: %d)", topic, ret);
        } else {
            subscription->state = MQTT_SUB_STATE_FAILED;
            ESP_LOGW(TAG, "MQTT subscribe failed for topic: %s, keeping in DB for retry", topic);
        }
    } else {
        ESP_LOGD(TAG, "Added callback for already-subscribed topic: %s", topic);
    }

    return ESP_OK;
}

static void unsubscribe_helper(esp_mqtt_glue_subscription_t **subscription)
{
    if (subscription && *subscription) {
        /* Only send MQTT unsubscribe if this is the last subscription for this topic */
        bool other_subscription_exists = false;
        for (int i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
            if (mqtt_data->subscriptions[i] &&
                mqtt_data->subscriptions[i] != *subscription &&
                strcmp(mqtt_data->subscriptions[i]->topic, (*subscription)->topic) == 0) {
                other_subscription_exists = true;
                break;
            }
        }

        if (!other_subscription_exists) {
            if (esp_mqtt_client_unsubscribe(mqtt_data->mqtt_client, (*subscription)->topic) < 0) {
                ESP_LOGW(TAG, "Could not unsubscribe from topic: %s", (*subscription)->topic);
            } else {
                ESP_LOGD(TAG, "Unsubscribed from topic: %s", (*subscription)->topic);
            }
        } else {
            ESP_LOGD(TAG, "Not unsubscribing from topic %s - other callbacks still exist", (*subscription)->topic);
        }

        free((*subscription)->topic);
        free(*subscription);
        *subscription = NULL;
    }
}

static esp_err_t esp_mqtt_glue_unsubscribe(const char *topic)
{
    if (!mqtt_data || !topic) {
        return ESP_FAIL;
    }
    esp_mqtt_glue_subscription_t **subscriptions = mqtt_data->subscriptions;
    int i;
    for (i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
        if (subscriptions[i]) {
            if (strncmp(topic, subscriptions[i]->topic, strlen(topic)) == 0) {
                unsubscribe_helper(&subscriptions[i]);
                return ESP_OK;
            }
        }
    }
    return ESP_FAIL;
}

static esp_err_t esp_mqtt_glue_publish(const char *topic, void *data, size_t data_len, uint8_t qos, int *msg_id)
{
    if (!mqtt_data || !topic || !data) {
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "Publishing to %s", topic);
    int ret = esp_mqtt_client_publish(mqtt_data->mqtt_client, topic, data, data_len, qos, 0);
    if (ret < 0) {
        ESP_LOGE(TAG, "MQTT Publish failed");
        return ESP_FAIL;
    }
    if (msg_id) {
        *msg_id = ret;
    }
    return ESP_OK;
}

static esp_mqtt_glue_long_data_t *esp_mqtt_glue_free_long_data(esp_mqtt_glue_long_data_t *long_data)
{
    if (long_data) {
        if (long_data->topic) {
            free(long_data->topic);
        }
        if (long_data->data) {
            free(long_data->data);
        }
        free(long_data);
    }
    return NULL;
}

static esp_mqtt_glue_long_data_t *esp_mqtt_glue_manage_long_data(esp_mqtt_glue_long_data_t *long_data,
        esp_mqtt_event_handle_t event)
{
    if (event->topic) {
        /* This is new data. Free any earlier data, if present. */
        esp_mqtt_glue_free_long_data(long_data);
        long_data = calloc(1, sizeof(esp_mqtt_glue_long_data_t));
        if (!long_data) {
            ESP_LOGE(TAG, "Could not allocate memory for esp_mqtt_glue_long_data_t");
            return NULL;
        }
        long_data->data = MEM_CALLOC_EXTRAM(1, event->total_data_len);
        if (!long_data->data) {
            ESP_LOGE(TAG, "Could not allocate %d bytes for received data.", event->total_data_len);
            return esp_mqtt_glue_free_long_data(long_data);
        }
        long_data->topic = strndup(event->topic, event->topic_len);
        if (!long_data->topic) {
            ESP_LOGE(TAG, "Could not allocate %d bytes for received topic.", event->topic_len);
            return esp_mqtt_glue_free_long_data(long_data);
        }
    }
    if (long_data) {
        memcpy(long_data->data + event->current_data_offset, event->data, event->data_len);

        if ((event->current_data_offset + event->data_len) == event->total_data_len) {
            esp_mqtt_glue_subscribe_callback(long_data->topic, strlen(long_data->topic),
                        long_data->data, event->total_data_len);
            return esp_mqtt_glue_free_long_data(long_data);
        }
    }
    return long_data;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT Connected");
            /* Reset all subscription states on reconnection */
            esp_mqtt_glue_reset_subscription_states();

            /* Re-subscribe to unique topics only */
            for (int i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
                if (!mqtt_data->subscriptions[i]) continue;

                /* Skip if we already processed this topic */
                bool topic_already_processed = false;
                for (int j = 0; j < i; j++) {
                    if (mqtt_data->subscriptions[j] &&
                        strcmp(mqtt_data->subscriptions[i]->topic, mqtt_data->subscriptions[j]->topic) == 0) {
                        topic_already_processed = true;
                        break;
                    }
                }
                if (topic_already_processed) continue;

                /* Find highest QoS for this topic */
                uint8_t max_qos = mqtt_data->subscriptions[i]->qos;
                for (int j = i + 1; j < MAX_MQTT_SUBSCRIPTIONS; j++) {
                    if (mqtt_data->subscriptions[j] &&
                        strcmp(mqtt_data->subscriptions[i]->topic, mqtt_data->subscriptions[j]->topic) == 0 &&
                        mqtt_data->subscriptions[j]->qos > max_qos) {
                        max_qos = mqtt_data->subscriptions[j]->qos;
                    }
                }

                /* Subscribe once with highest QoS */
                int ret = _esp_mqtt_client_subscribe(event->client, mqtt_data->subscriptions[i]->topic, max_qos);
                mqtt_subscription_state_t new_state = (ret >= 0) ? MQTT_SUB_STATE_REQUESTED : MQTT_SUB_STATE_FAILED;

                /* Update all subscriptions for this topic */
                for (int j = i; j < MAX_MQTT_SUBSCRIPTIONS; j++) {
                    if (mqtt_data->subscriptions[j] &&
                        strcmp(mqtt_data->subscriptions[i]->topic, mqtt_data->subscriptions[j]->topic) == 0) {
                        mqtt_data->subscriptions[j]->msg_id = (ret >= 0) ? ret : -1;
                        mqtt_data->subscriptions[j]->state = new_state;
                    }
                }

                if (ret >= 0) {
                    ESP_LOGD(TAG, "Reconnect: Subscribed to %s (msg_id: %d, QoS: %d)",
                             mqtt_data->subscriptions[i]->topic, ret, max_qos);
                } else {
                    ESP_LOGW(TAG, "Reconnect: Failed to subscribe to %s", mqtt_data->subscriptions[i]->topic);
                }
            }
            esp_event_post(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_CONNECTED, NULL, 0, portMAX_DELAY);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT Disconnected. Will try reconnecting in a while...");
            /* Mark all subscriptions as disconnected - they'll need re-acknowledgment */
            esp_mqtt_glue_reset_subscription_states();
            esp_event_post(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_DISCONNECTED, NULL, 0, portMAX_DELAY);
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGD(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            /* Mark matching subscriptions as acknowledged */
            for (int i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
                if (mqtt_data->subscriptions[i] &&
                    mqtt_data->subscriptions[i]->msg_id == event->msg_id) {
                    mqtt_data->subscriptions[i]->state = MQTT_SUB_STATE_ACKNOWLEDGED;
                    ESP_LOGD(TAG, "Subscription acknowledged for topic: %s", mqtt_data->subscriptions[i]->topic);
                }
            }
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGD(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            esp_event_post(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_PUBLISHED, &event->msg_id, sizeof(event->msg_id), portMAX_DELAY);
            break;
#ifdef CONFIG_MQTT_REPORT_DELETED_MESSAGES
        case MQTT_EVENT_DELETED:
            ESP_LOGD(TAG, "MQTT_EVENT_DELETED, msg_id=%d", event->msg_id);
            esp_event_post(RMAKER_COMMON_EVENT, RMAKER_MQTT_EVENT_MSG_DELETED, &event->msg_id, sizeof(event->msg_id), portMAX_DELAY);
            break;
#endif /* CONFIG_MQTT_REPORT_DELETED_MESSAGES */
        case MQTT_EVENT_DATA: {
            ESP_LOGD(TAG, "MQTT_EVENT_DATA");
            static esp_mqtt_glue_long_data_t *long_data;
            /* Topic can be NULL, for data longer than the MQTT buffer */
            if (event->topic) {
                ESP_LOGD(TAG, "TOPIC=%.*s\r\n", event->topic_len, event->topic);
            }
            ESP_LOGD(TAG, "DATA=%.*s\r\n", event->data_len, event->data);
            if (event->data_len == event->total_data_len) {
                /* If long_data still exists, it means there was some issue getting the
                 * long data, and so, it needs to be freed up.
                 */
                if (long_data) {
                    long_data = esp_mqtt_glue_free_long_data(long_data);
                }
                esp_mqtt_glue_subscribe_callback(event->topic, event->topic_len, event->data, event->data_len);
            } else {
                long_data = esp_mqtt_glue_manage_long_data(long_data, event);
            }
            break;
        }
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGD(TAG, "Other event id:%d", event->event_id);
            break;
    }
}

static esp_err_t esp_mqtt_glue_connect(void)
{
    if (!mqtt_data) {
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Connecting to %s", mqtt_data->conn_params->mqtt_host);
    esp_err_t ret = esp_mqtt_client_start(mqtt_data->mqtt_client);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_mqtt_client_start() failed with err = %d", ret);
        return ret;
    }
    return ESP_OK;
}

static void esp_mqtt_glue_unsubscribe_all(void)
{
    if (!mqtt_data) {
        return;
    }
    int i;
    for (i = 0; i < MAX_MQTT_SUBSCRIPTIONS; i++) {
        if (mqtt_data->subscriptions[i]) {
            unsubscribe_helper(&(mqtt_data->subscriptions[i]));
        }
    }
}

static esp_err_t esp_mqtt_glue_disconnect(void)
{
    if (!mqtt_data) {
        return ESP_FAIL;
    }
    esp_mqtt_glue_unsubscribe_all();
    esp_err_t err = esp_mqtt_client_stop(mqtt_data->mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to disconnect from MQTT");
    } else {
        ESP_LOGI(TAG, "MQTT Disconnected.");
    }
    return err;
}
#ifdef ESP_RMAKER_MQTT_USE_PORT_443
static const char *alpn_protocols[] = { "x-amzn-mqtt-ca", NULL };
#endif /* ESP_RMAKER_MQTT_USE_PORT_443 */

/* Static helper to create MQTT client config from connection params */
static esp_mqtt_client_config_t esp_mqtt_glue_create_client_config(esp_rmaker_mqtt_conn_params_t *conn_params)
{
#ifdef CONFIG_ESP_RMAKER_MQTT_SEND_USERNAME
    const char *username = esp_get_aws_ppi();
#endif
    esp_mqtt_client_config_t mqtt_client_cfg = {
        .broker = {
            .address = {
                .hostname = conn_params->mqtt_host,
#ifdef ESP_RMAKER_MQTT_USE_PORT_443
                .port = 443,
#else
                .port = 8883,
#endif
                .transport = MQTT_TRANSPORT_OVER_SSL,
            },
            .verification = {
#ifdef ESP_RMAKER_MQTT_USE_PORT_443
                .alpn_protos = alpn_protocols,
#endif
#ifdef ESP_RMAKER_MQTT_USE_CERT_BUNDLE
                .crt_bundle_attach = esp_crt_bundle_attach,
#else
                .certificate = (const char *)conn_params->server_cert,
                .certificate_len = conn_params->server_cert_len,
#endif
            }
        },
        .credentials = {
#ifdef CONFIG_ESP_RMAKER_MQTT_SEND_USERNAME
            .username = username,
#endif
            .client_id = (const char *)conn_params->client_id,
            .authentication = {
                .certificate = (const char *)conn_params->client_cert,
                .certificate_len = conn_params->client_cert_len,
                .key = (const char *)conn_params->client_key,
                .key_len = conn_params->client_key_len,
                .ds_data = conn_params->ds_data
            },
        },
        .session = {
            .keepalive = CONFIG_ESP_RMAKER_MQTT_KEEP_ALIVE_INTERVAL,
            .last_will = {
                .topic = (const char *)conn_params->mqtt_last_will_topic,
                .msg = (const char *)conn_params->mqtt_last_will_message,
                .msg_len = conn_params->mqtt_last_will_message_len,
                .qos = RMAKER_MQTT_QOS1,
                .retain = false
            },
#ifdef CONFIG_ESP_RMAKER_MQTT_PERSISTENT_SESSION
            .disable_clean_session = 1,
#endif /* CONFIG_ESP_RMAKER_MQTT_PERSISTENT_SESSION */
        },
    };
    if (conn_params->use_ecdsa_peripheral) {
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 1)
        mqtt_client_cfg.credentials.authentication.use_ecdsa_peripheral = conn_params->use_ecdsa_peripheral;
        mqtt_client_cfg.credentials.authentication.ecdsa_key_efuse_blk = conn_params->ecdsa_key_efuse_blk;
#else
        ESP_LOGW(TAG, "MQTT ECDSA peripheral is supported only on ESP-IDF >= v5.5.1");
#endif
    }
    return mqtt_client_cfg;
}

/* Static helper to log LWT configuration */
static void esp_mqtt_glue_log_lwt(esp_rmaker_mqtt_conn_params_t *conn_params)
{
    if (conn_params->mqtt_last_will_topic) {
        ESP_LOGI(TAG, "MQTT LWT topic: %s", conn_params->mqtt_last_will_topic);
        if (conn_params->mqtt_last_will_message && conn_params->mqtt_last_will_message_len > 0) {
            ESP_LOGI(TAG, "MQTT LWT message: %.*s", (int)conn_params->mqtt_last_will_message_len,
                     conn_params->mqtt_last_will_message);
        }
    } else {
        ESP_LOGI(TAG, "MQTT LWT not configured");
    }
}

static esp_err_t esp_mqtt_glue_init(esp_rmaker_mqtt_conn_params_t *conn_params)
{
#ifdef CONFIG_ESP_RMAKER_MQTT_SEND_USERNAME
    const char *username = esp_get_aws_ppi();
    if (!username) {
        ESP_LOGE(TAG, "username received is NULL");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "AWS PPI: %s", username);
#endif
    if (mqtt_data) {
        ESP_LOGE(TAG, "MQTT already initialized");
        return ESP_OK;
    }
    if (!conn_params) {
        ESP_LOGE(TAG, "Connection params are mandatory for esp_mqtt_glue_init");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Initialising MQTT");
    mqtt_data = calloc(1, sizeof(esp_mqtt_glue_data_t));
    if (!mqtt_data) {
        ESP_LOGE(TAG, "Failed to allocate memory for esp_mqtt_glue_data_t");
        return ESP_ERR_NO_MEM;
    }
    mqtt_data->conn_params = conn_params;

    esp_mqtt_client_config_t mqtt_client_cfg = esp_mqtt_glue_create_client_config(conn_params);
    esp_mqtt_glue_log_lwt(conn_params);

    mqtt_data->mqtt_client = esp_mqtt_client_init(&mqtt_client_cfg);
    if (!mqtt_data->mqtt_client) {
        ESP_LOGE(TAG, "esp_mqtt_client_init failed");
        esp_mqtt_glue_deinit();
        return ESP_FAIL;
    }
    esp_mqtt_client_register_event(mqtt_data->mqtt_client , ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    return ESP_OK;
}

static void esp_mqtt_glue_deinit(void)
{
    esp_mqtt_glue_unsubscribe_all();
    if (mqtt_data && mqtt_data->mqtt_client) {
        esp_mqtt_client_destroy(mqtt_data->mqtt_client);
    }
    if (mqtt_data) {
        free(mqtt_data);
        mqtt_data = NULL;
    }
}

/* Update MQTT config (including LWT) and reconnect.
 * Subscriptions are preserved across the reconnection.
 */
static esp_err_t esp_mqtt_glue_update_config(esp_rmaker_mqtt_conn_params_t *conn_params)
{
    if (!mqtt_data || !mqtt_data->mqtt_client) {
        ESP_LOGE(TAG, "MQTT not initialized, cannot update config");
        return ESP_ERR_INVALID_STATE;
    }
    if (!conn_params) {
        ESP_LOGE(TAG, "Connection params are mandatory for update_config");
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Updating MQTT config and reconnecting");

    /* Stop the MQTT client (disconnect) */
    esp_err_t err = esp_mqtt_client_stop(mqtt_data->mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to stop MQTT client: %d", err);
        /* Continue anyway - try to update config */
    }

    /* Update the stored conn_params */
    mqtt_data->conn_params = conn_params;

    /* Create new config with updated params */
    esp_mqtt_client_config_t mqtt_client_cfg = esp_mqtt_glue_create_client_config(conn_params);
    esp_mqtt_glue_log_lwt(conn_params);

    /* Update the client config using esp_mqtt_set_config */
    err = esp_mqtt_set_config(mqtt_data->mqtt_client, &mqtt_client_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update MQTT config: %d", err);
        return err;
    }

    /* Start the client again (connect) */
    err = esp_mqtt_client_start(mqtt_data->mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %d", err);
        return err;
    }

    ESP_LOGI(TAG, "MQTT config updated, reconnecting...");
    return ESP_OK;
}

esp_err_t esp_rmaker_mqtt_glue_setup(esp_rmaker_mqtt_config_t *mqtt_config)
{
    mqtt_config->init           = esp_mqtt_glue_init;
    mqtt_config->deinit         = esp_mqtt_glue_deinit;
    mqtt_config->connect        = esp_mqtt_glue_connect;
    mqtt_config->disconnect     = esp_mqtt_glue_disconnect;
    mqtt_config->publish        = esp_mqtt_glue_publish;
    mqtt_config->subscribe      = esp_mqtt_glue_subscribe;
    mqtt_config->unsubscribe    = esp_mqtt_glue_unsubscribe;
    mqtt_config->update_config  = esp_mqtt_glue_update_config;
    mqtt_config->setup_done     = true;
    return ESP_OK;
}
