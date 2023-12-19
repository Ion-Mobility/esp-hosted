set(COMPONENT_SRCS "app_main.c"
                   "mempool.c"
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
                                                 "./crypto")

register_component()
set(PROJECT_VERSION "-DPROJECT_VERSION=0.4")
target_compile_definitions(${COMPONENT_LIB} PRIVATE ${PROJECT_VERSION})