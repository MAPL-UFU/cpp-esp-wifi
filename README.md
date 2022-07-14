# cpp-esp-wifi

Standalone component for easy wifi connect configuration in esp32 this is inspired on the interface defined in Espressif documentation that uses Wifi,
[example connections](https://github.com/espressif/esp-idf/blob/master/examples/common_components/protocol_examples_common/connect.c), but it implements the 
necessary changes to implements WPA2 Enterprise connection protocol.

- extends [EventHandler](https://github.com/MAPL-UFU/cpp-esp-e-handler)

## **Class WifiEventHandler:**

Class that represents an wifi connection: 

### Constructor   
   
    - void WifiEventHandler()

Default Constructor
<br>
<br>




### <font color="yellow">Methods:</font>

#### Initialise Wifi

    - void initialise_wifi()

Init wifi connection with defined configuration, see main file for more information for how to configure.
<br>
<br>

### <font color="yellow">Static Methods:</font>

#### Lifecycle Callers 
    - static void onEventWifiConnected(void*, esp_event_base_t, int32_t, void*);
    - static void onEventWifiDisconnected(void*, esp_event_base_t, int32_t, void*);
    
    handler_args: Used to pass arguments to internal Esp class
    esp_event_base_t base: Group of events to listen
    int32_t event_id: Event id to listen
    event_data: data to be passed to event hendler

These are internal functions used by MQTT for MQTT conections, but you can use this to increment intermediary steps 
on conection. Keep in mind that these are static methods you can use it also to call a not normal step on the event loop, 
but the best use for this is to override this with a proper class.

<br>
<br>