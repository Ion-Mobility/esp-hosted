idf_component_register(SRCS "lib/monocypher-ed25519.c"
                            "lib/monocypher.c"
                            "crypto.c"
               INCLUDE_DIRS "lib"
                            "."
               REQUIRES nvs_flash)