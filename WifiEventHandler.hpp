#pragma once
#include <string>
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "EventHandler.hpp"

class WifiEventHandler : public EventHandler
{
    private:
        inline static const char* CLASS_TAG = "WifiEventHandler";
        SemaphoreHandle_t s_semph_get_ip_addrs;
        int s_active_interfaces;

    public:
        WifiEventHandler();
        void register_events(void*){};
        void register_wifi_events();
        void unregister_wifi_events();
        
        void initialise_wifi();
        //int deactivate_wifi(void);

        static void onEventWifiDisconnected(void*, esp_event_base_t, int32_t, void*);
        static void onEventWifiGotIP(void*, esp_event_base_t, int32_t, void*);

        std::string getClassTag();
};


