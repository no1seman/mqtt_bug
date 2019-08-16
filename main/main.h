#include <stdint.h>
#include <stddef.h>

#define MQTT_CLIENT_STARTED_BIT BIT1
#define MQTT_CLIENT_CONNECTED_BIT BIT2
#define MQTT_SERVER "192.168.3.211"
#define MQTT_USER "1yi5q0xqwl"
#define MQTT_PASSWORD "1yi5q0xqwl"
#define MQTT_CLIENTID "1yi5q0xqwl"
#define MQTT_STAT_MESSAGE_LEN (356)
#define MQTT_CLIMATE_MESSAGE_LEN (20)
#define MQTT_HTTP_MESSAGE_LEN_MAX (256)
#define MQTT_HTTP_MESSAGE_QUEUE_LEN (100)
#define DELAY_PERIOD (1000)
#define MQTT_TOPIC_STATS "1yi5q0xqwl/stats"
#define MQTT_TOPIC_CIMATE "1yi5q0xqwl/climate"
#define MQTT_TOPIC_HTTP "1yi5q0xqwl/http"

typedef enum
{
    _RES_TYPE_JS = 0,
    _RES_TYPE_JSON,
    _RES_TYPE_HTML,
    _RES_TYPE_CSS,
    _RES_TYPE_ICO,
    _RES_TYPE_FONT_WOFF2,
    _RES_TYPE_PDF,
    _RES_TYPE_MAX
} res_type_t;

typedef enum
{
    _STATIC_INDEX_HTML = 0,
    _STATIC_INDEX_CSS,
    _STATIC_INDEX_JS,
    _STATIC_MAX_FILE
} static_file_t;

typedef struct file_res_type
{
    char *uri_type;
    char *file_ext;
} file_res_type_t;

typedef struct static_cache
{
    const char *filename;
    const char *uri;
    const res_type_t resptype;
    bool gzipped;
    char *buf;
    size_t len;
} static_cache_t;