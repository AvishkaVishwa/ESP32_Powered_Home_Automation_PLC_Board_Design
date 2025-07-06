idf_component_register(SRCS "wifi_config.c" "web_server.c" "auto_board_tasks.c" "auto_board.c" "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES esp_http_server json nvs_flash esp_wifi driver esp_timer freertos esp_system esp_netif esp_event mdns)