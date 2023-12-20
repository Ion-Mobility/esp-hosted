idf_component_register(SRCS "tm_ble_gatts_server.c"
               INCLUDE_DIRS "."
                            "../../main"
               REQUIRES nvs_flash bt)

