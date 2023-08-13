set(COMPONENT_SRCS "../../../../common/esp_hosted_config.pb-c.c"
                   "protocomm_pserial.c"
                   "app_main.c"
                   "slave_bt.c"
                   "mempool.c"
                   "tm/tm_atcmd_parser.c"
                   "tm/tm_ble_gatts_server.c"
                   "tm/tm_spi_slave_driver.c"
                   "tm/tm_ble.c"
                   "tm/tm_atcmd.c"
                   "crypto/lib/monocypher-ed25519.c"
                   "crypto/lib/monocypher.c"
                   "crypto/crypto.c")
                   set(COMPONENT_ADD_INCLUDEDIRS "."
                                                 "./tm"
                                                 "./crypto/lib"
                                                 "./crypto"
                                                 "../../../../common/include")

if(CONFIG_ESP_SDIO_HOST_INTERFACE)
    list(APPEND COMPONENT_SRCS sdio_slave_api.c)
else(CONFIG_ESP_SPI_HOST_INTERFACE)
    list(APPEND COMPONENT_SRCS spi_slave_api.c)
endif()


register_component()
set(PROJECT_VERSION "-DPROJECT_VERSION=0.4")
target_compile_definitions(${COMPONENT_LIB} PRIVATE ${PROJECT_VERSION})

# Add directory of protocomm_priv.h to include paths
idf_component_get_property(protocomm_dir protocomm COMPONENT_DIR)
target_include_directories(${COMPONENT_LIB} PRIVATE "${protocomm_dir}/src/common")
