# Embed CA, certificate & key directly into binary
idf_component_register(SRCS "WifiEventHandler.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES wpa_supplicant esp_netif driver cpp-esp-e-handler)


set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
