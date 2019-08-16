#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "freertos/ringbuf.h"
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "nvs_flash.h"
#include "tcpip_adapter.h"
#include "esp_eth.h"
#include "protocol_examples_common.h"

#include <esp_http_server.h>
#include "mbedtls/md5.h"
#include "mqtt_client.h"

#include "main.h"

esp_mqtt_client_handle_t mqtt_client = NULL;
static esp_mqtt_client_config_t mqtt_cfg = {0};
EventGroupHandle_t app_state = NULL;
RingbufHandle_t http_requests_event_queue = NULL;

static uint32_t mqtt_task2_counter = 0;
static uint64_t mqtt_stats_counter = 0ULL;
static uint64_t mqtt_climate_counter = 0ULL;
static uint64_t mqtt_ble_counter = 0ULL;
static const char *TAG = "example";

const file_res_type_t res_types[_RES_TYPE_MAX] =
    {
        [_RES_TYPE_JS] {
            .uri_type = "application/javascript; charset=UTF-8",
            .file_ext = ".js"
        },
        [_RES_TYPE_JSON] {
            .uri_type = "application/json",
            .file_ext = ".json"
        },
        [_RES_TYPE_HTML] {
            .uri_type = "text/html; charset=UTF-8",
            .file_ext = ".html"
        },
        [_RES_TYPE_CSS] {
            .uri_type = "text/css; charset=UTF-8",
            .file_ext = ".css"
        },
        [_RES_TYPE_ICO] {
            .uri_type = "image/x-icon",
            .file_ext = ".ico"
        },
        [_RES_TYPE_FONT_WOFF2] {
            .uri_type = "font/woff2",
            .file_ext = ".woff2"
        },
        [_RES_TYPE_PDF] {
            .uri_type = "application/pdf",
            .file_ext = ".pdf"
        }};

static_cache_t static_files_cache[_STATIC_MAX_FILE] =
    {
        [_STATIC_INDEX_HTML] {
            .filename = "index.html.gz",
            .uri = "index.html",
            .resptype = _RES_TYPE_HTML,
            .gzipped = true,
            .buf = NULL,
            .len = 10930
        },
        [_STATIC_INDEX_CSS] {
            .filename = "index.css.gz",
            .uri = "index.css",
            .resptype = _RES_TYPE_CSS,
            .gzipped = true,
            .buf = NULL,
            .len = 38099
        },
        [_STATIC_INDEX_JS] {
            .filename = "index.js.gz",
            .uri = "index.js",
            .resptype = _RES_TYPE_JS,
            .gzipped = true,
            .buf = NULL,
            .len = 75723
        }};

bool is_mqtt_client_connected(void)
{
    return xEventGroupWaitBits(app_state, MQTT_CLIENT_CONNECTED_BIT, false, true, 0) & MQTT_CLIENT_CONNECTED_BIT;
}

bool is_mqtt_client_started(void)
{
    return xEventGroupWaitBits(app_state, MQTT_CLIENT_STARTED_BIT, false, true, 0) & MQTT_CLIENT_STARTED_BIT;
}

esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        xEventGroupSetBits(app_state, MQTT_CLIENT_CONNECTED_BIT);
        ESP_LOGI(TAG, "MQTT client connected");
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        if (is_mqtt_client_connected())
        {
            xEventGroupClearBits(app_state, MQTT_CLIENT_CONNECTED_BIT);
            ESP_LOGW(TAG, "MQTT client reconnecting...(MQTT_EVENT_DISCONNECTED event with commented: esp_mqtt_client_reconnect(mqtt_client))");
            ESP_LOGI(TAG, "MQTT_CLIENT_CONNECTED_BIT state: %s", is_mqtt_client_connected() ? "True" : "False");
            esp_mqtt_client_reconnect(mqtt_client);
        }
        else
        {
            ESP_LOGI(TAG, "MQTT client disconnected");
        }
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED");
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED");
        break;
    case MQTT_EVENT_PUBLISHED:
        //ESP_LOGI(TAG, "MQTT message published: msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    case MQTT_EVENT_BEFORE_CONNECT:
        ESP_LOGI(TAG, "MQTT_EVENT_BEFORE_CONNECT");
        break;
    }
    return ESP_OK;
}

void mqtt_client_init(void)
{

    mqtt_cfg.event_handle = mqtt_event_handler;
    mqtt_cfg.disable_clean_session = true;
    mqtt_cfg.disable_auto_reconnect = false;
    mqtt_cfg.keepalive = 10;
    mqtt_cfg.refresh_connection_after_ms = 0;
    mqtt_cfg.buffer_size = 256;
    mqtt_cfg.username = MQTT_USER;
    mqtt_cfg.password = MQTT_PASSWORD;
    mqtt_cfg.client_id = MQTT_CLIENTID;
    mqtt_cfg.host = MQTT_SERVER;
    mqtt_cfg.port = 1883;
    mqtt_cfg.transport = MQTT_TRANSPORT_OVER_TCP,

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_LOGI(TAG, "MQTT Client init complete");
}

void mqtt_client_start(void)
{
    ESP_LOGI(TAG, "MQTT client starting...");
    if (!is_mqtt_client_started())
    {
        esp_mqtt_client_start(mqtt_client);
        xEventGroupSetBits(app_state, MQTT_CLIENT_STARTED_BIT);
        ESP_LOGI(TAG, "MQTT client started");
    }
}

void mqtt_client_stop(void)
{
    if (is_mqtt_client_started())
    {
        xEventGroupClearBits(app_state, MQTT_CLIENT_STARTED_BIT);
        esp_mqtt_client_stop(mqtt_client);
        ESP_LOGI(TAG, "MQTT client stopped");
    }
}

char atoc(uint8_t value, bool capital)
{
    if (value < 10)
    {
        return (char)(value + 48);
    }
    else
    {
        if (capital)
            return (char)(value + 55);
        else
            return (char)(value + 87);
    }
}

char *array2hex(const uint8_t *hex, char *target, uint8_t len, bool capital)
{
    int i, hop = 0;
    for (i = 0; i < len; i++)
    {
        target[hop] = atoc((hex[i] & 0xf0) >> 4, capital);
        target[hop + 1] = atoc((hex[i] & 0x0f), capital);
        hop += 2;
    }
    target[hop] = '\0';
    return (target);
}

esp_err_t get_static_file(static_file_t static_file, httpd_req_t *req)
{
    esp_err_t res;
    //uint32_t start_time = esp_log_timestamp();
    //ESP_LOGI(TAG, "Get static file: %s", static_files_cache[static_file].filename);
    if (static_files_cache[static_file].buf)
    {
        if (static_files_cache[static_file].gzipped)
            httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
        httpd_resp_set_type(req, res_types[static_files_cache[static_file].resptype].uri_type);
        httpd_resp_set_status(req, HTTPD_200);
        unsigned char md5[16];
        char md5hex[34];
        mbedtls_md5_ret((const unsigned char *)static_files_cache[static_file].buf, static_files_cache[static_file].len, md5);
        httpd_resp_set_hdr(req, "MD5HASH", array2hex((const uint8_t *)&md5, (char *)&md5hex, 16, false));
        res = httpd_resp_send(req, static_files_cache[static_file].buf, static_files_cache[static_file].len);
        //uint32_t end_time = esp_log_timestamp();
        if (res != ESP_OK)
            //ESP_LOGI(TAG, "Sending file '%s' took %u ms", static_files_cache[static_file].filename, end_time - start_time);
            //else
            ESP_LOGI(TAG, "Sending file '%s' failed", static_files_cache[static_file].filename);
        return res;
    }
    res = httpd_resp_send_404(req);
    return res;
}

esp_err_t get_file_handler(httpd_req_t *req)
{
    esp_err_t res;
    //uint32_t start_time = esp_log_timestamp();
    //ESP_LOGI(TAG, "GET %s", req->uri);

    for (int i = 0; i < _STATIC_MAX_FILE; i++)
    {
        if (strncasecmp(req->uri + 1, static_files_cache[i].uri, strlen(static_files_cache[i].uri)) == 0)
        {
            res = get_static_file(i, req);
            if (res == ESP_OK)
            {
                UBaseType_t res = xRingbufferSend(http_requests_event_queue, req->uri, strlen(req->uri), pdMS_TO_TICKS(1000));
                if (res != pdTRUE)
                {
                    ESP_LOGE(TAG, "Failed to send item to MQTT HTTP QUEUE");
                }
            }
            //uint32_t end_time = esp_log_timestamp();
            //ESP_LOGI(TAG, "Processing request '%s' took %u ms", static_files_cache[i].filename, end_time - start_time);
            return res;
        }
    }
    res = httpd_resp_send_404(req);
    return res;
}

static const httpd_uri_t get_file_uri = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = get_file_handler,
    .user_ctx = NULL};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_open_sockets = 6;
    config.max_uri_handlers = 8;
    config.max_resp_headers = 8;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &get_file_uri);

        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}

static void disconnect_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    mqtt_client_stop();
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server)
    {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
}

static void connect_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "WiFi connected");
    mqtt_client_start();
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server == NULL)
    {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}

void make_http_cache(void)
{
    unsigned char md5[16];
    char tgt[34];
    ESP_LOGI(TAG, "Caching HTTP static files...");
    for (int i = 0; i < _STATIC_MAX_FILE; i++)
    {

        static_files_cache[i].buf = pvPortMalloc(static_files_cache[i].len);
        for (int j = 0; j < static_files_cache[j].len; j++)
            static_files_cache[i].buf[j] = (char)rand();

        mbedtls_md5_ret((const unsigned char *)static_files_cache[i].buf, static_files_cache[i].len, md5);
        ESP_LOGI(TAG, "%s file md5 summ: %s", static_files_cache[i].filename, array2hex((const uint8_t *)&md5, (char *)&tgt, 16, false));
    }
    ESP_LOGI(TAG, "Caching HTTP static files completed");
}

void mqtt_task1(void *pvParameter)
{
    ESP_LOGI(TAG, "mqtt_task1() starting...");
    char *http_message;
    size_t item_size;
    int msg_id;
    bool cnt = true;

    while (1)
    {
        do
        {
            http_message = (char *)xRingbufferReceive(http_requests_event_queue, &item_size, pdMS_TO_TICKS(1000));

            if (http_message && item_size > 0)
            {
                if (is_mqtt_client_connected())
                {
                    ESP_LOGI(TAG, "PUBLISHING HTTP MESSAGE");
                    msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_HTTP, http_message, item_size, 1, 0);
                    if (msg_id != -1)
                    {
                        vRingbufferReturnItem(http_requests_event_queue, (void *)http_message);
                        mqtt_ble_counter++;
                        cnt = true;
                    }
                    else
                    {
                        ESP_LOGI(TAG, "Message sent failed");
                        cnt = false;
                    }
                }
            }
            else
            {
                cnt = false;
            }
        } while (cnt);

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void mqtt_task2(void *pvParameter)
{
    ESP_LOGI(TAG, "mqtt_task2() starting...");
    char *stats_message = NULL;
    int i;
    stats_message = pvPortMalloc(MQTT_STAT_MESSAGE_LEN);
    for (i = 0; i < MQTT_STAT_MESSAGE_LEN; i++)
        stats_message[i] = (char)i;

    char *climate_message = NULL;
    climate_message = pvPortMalloc(MQTT_CLIMATE_MESSAGE_LEN);
    for (i = 0; i < MQTT_CLIMATE_MESSAGE_LEN; i++)
        climate_message[i] = (char)i;

    while (1)
    {
        mqtt_task2_counter++;
        if (mqtt_task2_counter % 10 == 0)
        {
            int msg_id;
            if (is_mqtt_client_connected())
            {
                ESP_LOGI(TAG, "PUBLISHING STATS MESSAGE");
                msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_STATS, stats_message, MQTT_STAT_MESSAGE_LEN, 1, 0);
                if (msg_id != -1)
                {
                    mqtt_stats_counter++;
                }
                else
                {
                    ESP_LOGI(TAG, "Stats message send failed");
                }
            }
            else
            {
                ESP_LOGI(TAG, "MQTT client is not connected");
            }

            if (is_mqtt_client_connected())
            {
                ESP_LOGI(TAG, "PUBLISHING CLIMATE MESSAGE");
                msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC_CIMATE, climate_message, MQTT_CLIMATE_MESSAGE_LEN, 1, 0);
                if (msg_id != -1)
                {
                    mqtt_climate_counter++;
                }
                else
                {
                    ESP_LOGI(TAG, "Climate message send failed");
                }
            }
            else
            {
                ESP_LOGI(TAG, "MQTT client is not connected");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(DELAY_PERIOD));
    }
}

void app_main(void)
{
    static httpd_handle_t server = NULL;
    esp_log_level_set("wifi", ESP_LOG_WARN);
    http_requests_event_queue = xRingbufferCreate(MQTT_HTTP_MESSAGE_QUEUE_LEN * MQTT_HTTP_MESSAGE_LEN_MAX, RINGBUF_TYPE_NOSPLIT);
    make_http_cache();
    ESP_ERROR_CHECK(nvs_flash_init());
    app_state = xEventGroupCreate();
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    mqtt_client_init();
    ESP_ERROR_CHECK(example_connect());

#ifdef CONFIG_EXAMPLE_CONNECT_WIFI
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_WIFI
#ifdef CONFIG_EXAMPLE_CONNECT_ETHERNET
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_ETHERNET

    /* Start the server for the first time */
    server = start_webserver();
    mqtt_client_start();
    xTaskCreate(mqtt_task1, "MQTT_TASK1", 4096, NULL, 14, NULL);
    xTaskCreate(mqtt_task2, "MQTT_TASK2", 4096, NULL, 5, NULL);
}
