#include <cstdio>
#include <iostream>
#include <cstring>

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
#include "lwip/err.h"
#include "lwip/sys.h"
#include "WifiEventHandler.hpp"

#define WIFI_SSID CONFIG_WIFI_SSID
#define EAP_ID CONFIG_EAP_ID
#define EAP_USERNAME CONFIG_EAP_USERNAME
#define EAP_PASSWORD CONFIG_EAP_PASSWORD

#define NR_OF_IP_ADDRESSES_TO_WAIT_FOR (s_active_interfaces)
#define WIFI_SCAN_METHOD WIFI_ALL_CHANNEL_SCAN
#define WIFI_CONNECT_AP_SORT_METHOD WIFI_CONNECT_AP_BY_SECURITY
#define WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK

#ifdef CONFIG_CONNECT_WIFI_WPA_WPA2_ENTERPRISE
esp_eap_ttls_phase2_types TTLS_PHASE2_METHOD = (esp_eap_ttls_phase2_types)CONFIG_EAP_METHOD_TTLS_PHASE_2;
#endif

using namespace std;

WifiEventHandler::WifiEventHandler()
{
    this->s_semph_get_ip_addrs = NULL;
    this->s_active_interfaces =0;
}

void WifiEventHandler::initialise_wifi(){
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

#ifdef CONFIG_CONNECT_WIFI_WPA_WPA2_ENTERPRISE
    esp_netif_init();
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
#endif

#ifdef CONFIG_CONNECT_WIFI_WPA_WPA2_PSK
    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    esp_netif_config.route_prio = 128;
    esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);
    esp_wifi_set_default_wifi_sta_handlers();
#endif

    this->register_wifi_events();

    esp_wifi_set_storage(WIFI_STORAGE_RAM);

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
    this->log("Setting WiFi configuration SSID...");
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

    /*** big diference */
#ifdef CONFIG_CONNECT_WIFI_WPA_WPA2_ENTERPRISE
    esp_wifi_sta_wpa2_ent_set_identity((uint8_t *)EAP_ID, strlen(EAP_ID));
    esp_wifi_sta_wpa2_ent_set_username((uint8_t *)EAP_USERNAME, strlen(EAP_USERNAME));
    esp_wifi_sta_wpa2_ent_set_password((uint8_t *)EAP_PASSWORD, strlen(EAP_PASSWORD));

    esp_wifi_sta_wpa2_ent_set_ttls_phase2_method(TTLS_PHASE2_METHOD);
    esp_wifi_sta_wpa2_ent_enable();
#endif
    /**/
    esp_wifi_start();
    esp_wifi_connect();
    this->s_active_interfaces++;
    this->s_semph_get_ip_addrs = xSemaphoreCreateCounting(NR_OF_IP_ADDRESSES_TO_WAIT_FOR, 0);    

    ////////////////////////////////
    ////////////////////////////////
    ////////////////////////////////
    // Wait for Ips
    this->log( "Implemented.");
    if (this->s_semph_get_ip_addrs != NULL) {
        for (int i = 0; i < NR_OF_IP_ADDRESSES_TO_WAIT_FOR; ++i) {
            if (xSemaphoreTake(this->s_semph_get_ip_addrs, 60000 / portTICK_PERIOD_MS) == pdTRUE) {
                this->log( "Got IP address.");
            } else {
                this->log("Failed to get IP address");
                throw std::runtime_error("Failed to get IP address");
            }
        }
    } else {
        this->log("Failed to create semaphore");
        throw std::runtime_error("Failed to create semaphore");
    }
}

void WifiEventHandler::register_wifi_events(){
    esp_event_handler_register( WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, WifiEventHandler::onEventWifiDisconnected, this);
    esp_event_handler_register( IP_EVENT, IP_EVENT_STA_GOT_IP, WifiEventHandler::onEventWifiGotIP, this);
}

void WifiEventHandler::unregister_wifi_events(){
    esp_event_handler_unregister( WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, WifiEventHandler::onEventWifiDisconnected);
    esp_event_handler_unregister( IP_EVENT, IP_EVENT_STA_GOT_IP, WifiEventHandler::onEventWifiGotIP);
}

void WifiEventHandler::onEventWifiDisconnected(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data){
    WifiEventHandler* context = (WifiEventHandler*)handler_args;
    esp_err_t err = esp_wifi_connect();
    if (err == ESP_ERR_WIFI_NOT_STARTED) {
            return;
    }
    ESP_ERROR_CHECK(err);
    context->log("ESP Wifi disconnected");
}

void WifiEventHandler::onEventWifiGotIP(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data){
    WifiEventHandler* context = (WifiEventHandler*)handler_args;
    xSemaphoreGive(context->s_semph_get_ip_addrs);
    context->log("ESP Wifi Got IP");
}

string WifiEventHandler::getClassTag(){
    string tag(WifiEventHandler::CLASS_TAG);
    return tag;
}