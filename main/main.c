#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include <esp_http_server.h>

#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "driver/gpio.h"
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/api.h>
#include <lwip/netdb.h>

#include "DHT22.h"

#include "capacitive_moisture.h"
#include "driver/adc.h"

#include "driver/i2c.h"
#include "bh1750.h"

#define ESP_WIFI_SSID "vxlproject"
#define ESP_WIFI_PASSWORD "12345678"
#define ESP_MAXIMUM_RETRY 3

static const char *TAG = "esp32 webserver";

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static int s_retry_num = 0;
int wifi_connect_status = 0; // false no wifi

#define I2C_MASTER_SCL_GPIO 22    /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_GPIO 21    /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM I2C_NUM_0  /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ 100000 /*!< I2C master clock frequency */

#define relay1 33 // máy phun sương   nebulizer
#define relay2 25 // máy bơm  pumps
#define relay3 26 // led
int relay_case;

double dhtTemp = 0;
double hum = 0;
double mois = 0;
double light = 0;
char nebulizer[4] = "OFF";
char pumps[4] = "OFF";
char led[4] = "OFF";

char html_page[] = "<!DOCTYPE HTML><html>\n"
                   "<head>\n"
                   "  <title>KHU VUON THONG MINH</title>\n"
                   "  <meta http-equiv=\"refresh\" content=\"10\">\n"
                   "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
                   "  <link rel=\"stylesheet\" href=\"https://use.fontawesome.com/releases/v5.7.2/css/all.css\" integrity=\"sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr\" crossorigin=\"anonymous\">\n"
                   "  <link rel=\"icon\" href=\"data:,\">\n"
                   "  <style>\n"
                   "    html {font-family: Arial; display: inline-block; text-align: center;}\n"
                   "    p {  font-size: 1.2rem;}\n"
                   "    body {  margin: 0;}\n"
                   "    .topnav { overflow: hidden; background-color: #241d4b; color: white; font-size: 1.7rem; }\n"
                   "    .content { padding: 20px; }\n"
                   "    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }\n"
                   "    .cards { max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }\n"
                   "    .reading { font-size: 2.8rem; }\n"
                   "    .card.temperature { color: #0e7c7b; }\n"
                   "    .card.humidity { color: #17bebb; }\n"
                   "    .card.moisture { color: #17bebb; }\n"
                   "    .card.light intensity { color: #17bebb; }\n"
                   "    .card.nebulizer { color: #17bebb; }\n"
                   "    .card.pumps { color: #17bebb; }\n"
                   "    .card.led { color: #17bebb; }\n"
                   "  </style>\n"
                   "</head>\n"
                   "<body>\n"
                   "  <div class=\"topnav\">\n"
                   "    <h3>KHU VUON THONG MINH</h3>\n"
                   "  </div>\n"
                   "  <div class=\"content\">\n"
                   "    <div class=\"cards\">\n"
                   "      <div class=\"card temperature\">\n"
                   "        <h4><i class=\"fas fa-thermometer-half\"></i> TEMPERATURE</h4><p><span class=\"reading\">%.2f&deg;C</span></p>\n"
                   "      </div>\n"
                   "      <div class=\"card humidity\">\n"
                   "        <h4><i class=\"fas fa-tint\"></i> HUMIDITY</h4><p><span class=\"reading\">%.2f</span> &percnt;</span></p>\n"
                   "      </div>\n"
                   "      <div class=\"card moisture\">\n"
                   "        <h4><i class=\"fas fa-tint\"></i> MOISTURE</h4><p><span class=\"reading\">%.2f</span> &percnt;</span></p>\n"
                   "      </div>\n"
                   "      <div class=\"card light intensity\">\n"
                   "        <h4> LIGHT INTENSITY </h4><p><span class=\"reading\">%.2f</span></p>\n"
                   "      </div>\n"
                   "      <div class=\"card nebulizer\">\n"
                   "        <h4> NEBULIZER </h4><p><span class=\"reading\">%s</span></p>\n"
                   "      </div>\n"
                   "      <div class=\"card pumps\">\n"
                   "        <h4> PUMPS </h4><p><span class=\"reading\">%s</span></p>\n"
                   "      </div>\n"
                   "      <div class=\"card led\">\n"
                   "        <h4> LED </h4><p><span class=\"reading\">%s</span></p>\n"
                   "      </div>\n"
                   "    </div>\n"
                   "  </div>\n"
                   "</body>\n"
                   "</html>";

void DHT_readings()
{
    setDHTgpio(GPIO_NUM_4);

    int DHT_ret = readDHT();

    errorHandler(DHT_ret);

    dhtTemp = getTemperature();
    hum = getHumidity();
}

void CSMS_readings()
{
    int CM_ret = readCM();
    CM_ErrorHandler(CM_ret);
    mois = getMoisture();
}

void BH1750_readings()
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = (gpio_num_t)I2C_MASTER_SDA_GPIO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = (gpio_num_t)I2C_MASTER_SCL_GPIO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    conf.clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL;

    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);

    bh1750_handle_t bh1750 = bh1750_create(I2C_MASTER_NUM, BH1750_I2C_ADDRESS_DEFAULT);

    bh1750_power_on(bh1750);

    bh1750_measure_mode_t cmd_measure;
    cmd_measure = BH1750_ONETIME_4LX_RES;
    bh1750_set_measure_mode(bh1750, cmd_measure);

    float data;
    bh1750_get_data(bh1750, &data);
    light = data;

    bh1750_delete(bh1750);

    i2c_driver_delete(I2C_MASTER_NUM);
}

void readings()
{
    DHT_readings();
    CSMS_readings();
    BH1750_readings();
}

void relaySetup(int relay_number, int relay_case, char relay_port[4])
{
    gpio_pad_select_gpio(relay_number);
    gpio_set_direction(relay_number, GPIO_MODE_OUTPUT);
    esp_err_t relay_ret;
    if (relay_case)
    {
        relay_ret = gpio_set_level(relay_number, 1);
        if (relay_ret)
        {
            strcpy(relay_port, "ON");
        }
    }
    else
    {
        relay_ret = gpio_set_level(relay_number, 0);
        if (relay_ret)
        {
            strcpy(relay_port, "OFF");
        }
    }
}

void relayControl()
{
    if (hum < 60 && dhtTemp > 25)
        relay_case = 1;
    else
        relay_case = 0;
    relaySetup(relay1, relay_case, nebulizer);
    if (60 < mois)
        relay_case = 0;
    else
        relay_case = 1;
    relaySetup(relay2, relay_case, pumps);
    if (3000 < light)
        relay_case = 0;
    else
        relay_case = 1;
    relaySetup(relay3, relay_case, led);
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < ESP_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        wifi_connect_status = 0;
        ESP_LOGI(TAG, "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        wifi_connect_status = 1;
    }
}

void connect_wifi(void)
{
    s_wifi_event_group = xEventGroupCreate();
    
        ESP_ERROR_CHECK(esp_netif_init());

        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();

        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        esp_event_handler_instance_t instance_any_id;
        esp_event_handler_instance_t instance_got_ip;
        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                            ESP_EVENT_ANY_ID,
                                                            &event_handler,
                                                            NULL,
                                                            &instance_any_id));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                            IP_EVENT_STA_GOT_IP,
                                                            &event_handler,
                                                            NULL,
                                                            &instance_got_ip));
    
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASSWORD,
            /* Setting a password implies station will connect to all security modes including WEP/WPA.
             * However these modes are deprecated and not advisable to be used. Incase your Access point
             * doesn't support WPA2, these mode can be enabled by commenting below line */
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 ESP_WIFI_SSID, ESP_WIFI_PASSWORD);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 ESP_WIFI_SSID, ESP_WIFI_PASSWORD);
    }
    else
    {
        ESP_LOGI(TAG, "UNEXPECTED EVENT");
    }
    vEventGroupDelete(s_wifi_event_group);
}

esp_err_t send_web_page(httpd_req_t *req)
{
    // Đọc các giá trị cảm biến và điều khiển relay
    readings();
    relayControl();

    // Tạo buffer đệm để chứa nội dung của trang web
    char response_data[sizeof(html_page) + 50];
    memset(response_data, 0, sizeof(response_data));

    // Định dạng nội dung của trang web trong response_data
    snprintf(response_data, sizeof(response_data), html_page, dhtTemp, hum, mois, light, nebulizer, pumps, led);

    // Gửi response_data đến client
    int response = httpd_resp_send(req, response_data, HTTPD_RESP_USE_STRLEN);

    return response;
}

esp_err_t get_req_handler(httpd_req_t *req)
{
    return send_web_page(req);
}

httpd_uri_t uri_get = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = get_req_handler,
    .user_ctx = NULL};

httpd_handle_t setup_server(void)
{
    httpd_config_t config = {
        .task_priority = tskIDLE_PRIORITY +5 ,
        .stack_size = 10000,
        .core_id = tskNO_AFFINITY,
        .server_port = 80,
        .ctrl_port = 32768,
        .max_open_sockets = 3,
        .max_uri_handlers = 2,
        .max_resp_headers = 5,
        .backlog_conn = 5,
        .lru_purge_enable = false,
        .recv_wait_timeout = 10,
        .send_wait_timeout = 10,
        .global_user_ctx = NULL,
        .global_user_ctx_free_fn = NULL,
        .global_transport_ctx = NULL,
        .global_transport_ctx_free_fn = NULL,
        .enable_so_linger = true,
        .linger_timeout = 3,
        .open_fn = NULL,
        .close_fn = NULL,
        .uri_match_fn = NULL};
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_register_uri_handler(server, &uri_get);
    }

    return server;
}

void app_main()
{
    // Initialize NVS
    esp_err_t main_ret = nvs_flash_init();
    if (main_ret == ESP_ERR_NVS_NO_FREE_PAGES || main_ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        main_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(main_ret);

    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");

    connect_wifi();

    if (wifi_connect_status == 1)
    {
        setup_server();
        ESP_LOGI(TAG, "Web Server is up and running\n");
    }
    else
    {
        ESP_LOGI(TAG, "Failed to connected with Wi-Fi, check your network Credentials\n");
    }
}
