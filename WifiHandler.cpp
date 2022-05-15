#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_wpa2.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_netif.h"

#include "sdkconfig.h"
#include "esp_wifi_default.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "WifiHandler.hpp"

#define WIFI_SSID CONFIG_WIFI_SSID
#define EAP_ID CONFIG_EAP_ID
#define EAP_USERNAME CONFIG_EAP_USERNAME
#define EAP_PASSWORD CONFIG_EAP_PASSWORD

#define NR_OF_IP_ADDRESSES_TO_WAIT_FOR (s_active_interfaces)
#define WIFI_SCAN_METHOD WIFI_ALL_CHANNEL_SCAN
#define WIFI_CONNECT_AP_SORT_METHOD WIFI_CONNECT_AP_BY_SECURITY
#define WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK

static const char *TAG = "WiFi Custom Client";
static SemaphoreHandle_t s_semph_get_ip_addrs = NULL;
static int s_active_interfaces = 0;

#ifdef CONFIG_CONNECT_WIFI_WPA_WPA2_ENTERPRISE
esp_eap_ttls_phase2_types TTLS_PHASE2_METHOD = (esp_eap_ttls_phase2_types)CONFIG_EAP_METHOD_TTLS_PHASE_2;
#endif

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Wi-Fi disconnected, trying to reconnect...");
        esp_err_t err = esp_wifi_connect();
        if (err == ESP_ERR_WIFI_NOT_STARTED) {
            return;
        }
        ESP_ERROR_CHECK(err);
    } else if (event_id == IP_EVENT_STA_GOT_IP) {
        xSemaphoreGive(s_semph_get_ip_addrs);
    }
}

int _initialise_wifi(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );

#ifdef CONFIG_CONNECT_WIFI_WPA_WPA2_ENTERPRISE
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
#endif

#ifdef CONFIG_CONNECT_WIFI_WPA_WPA2_PSK
    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    esp_netif_config.route_prio = 128;
    esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
    esp_wifi_set_default_wifi_sta_handlers();
#endif

    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );
    
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    // C++ 17 não aceita nested Initializer  

#ifdef CONFIG_CONNECT_WIFI_WPA_WPA2_ENTERPRISE
    wifi_sta_config_t sta_config = {
        {.ssid = WIFI_SSID}
    };
#endif
#ifdef CONFIG_CONNECT_WIFI_WPA_WPA2_PSK
    wifi_sta_config_t sta_config = {
        {.ssid = CONFIG_WIFI_SSID},
        {.password = CONFIG_WIFI_PASSWORD},
        .scan_method = WIFI_SCAN_METHOD,
        .sort_method = WIFI_CONNECT_AP_SORT_METHOD,
    };
#endif
    wifi_config_t wifi_config = {
        .sta = sta_config
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );

    /*** big diference */
#ifdef CONFIG_CONNECT_WIFI_WPA_WPA2_ENTERPRISE
    ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)EAP_ID, strlen(EAP_ID)) );
    ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_username((uint8_t *)EAP_USERNAME, strlen(EAP_USERNAME)) );
    ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD)) );

    ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_set_ttls_phase2_method(TTLS_PHASE2_METHOD) );
    ESP_ERROR_CHECK( esp_wifi_sta_wpa2_ent_enable() );
#endif
    /**/
    ESP_ERROR_CHECK( esp_wifi_start() );
    esp_wifi_connect();
    s_active_interfaces++;
    s_semph_get_ip_addrs = xSemaphoreCreateCounting(NR_OF_IP_ADDRESSES_TO_WAIT_FOR, 0);
    return 0;
}

int initialise_wifi(void)
{   
    ESP_ERROR_CHECK(_initialise_wifi());
    if (s_semph_get_ip_addrs != NULL) {
        for (int i = 0; i < NR_OF_IP_ADDRESSES_TO_WAIT_FOR; ++i) {
            if (xSemaphoreTake(s_semph_get_ip_addrs, 60000 / portTICK_PERIOD_MS) == pdTRUE) {
                ESP_LOGI(TAG, "Got IP address.");
            } else {
                ESP_LOGE(TAG, "Failed to get IP address");
                return -1;
            }
        }
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Failed to create semaphore");
        return -1;
    }
}

void _deactivate_wifi(void){
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler));
    esp_err_t err = esp_wifi_stop();
    if (err == ESP_ERR_WIFI_NOT_INIT) {
        return;
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(esp_wifi_deinit());
}

int deactivate_wifi(void)
{
    s_active_interfaces--;
    if (s_semph_get_ip_addrs != NULL) {
        vSemaphoreDelete(s_semph_get_ip_addrs);
        s_semph_get_ip_addrs = NULL;
    }
    _deactivate_wifi();
    return ESP_OK;
}
