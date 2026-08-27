idf_component_register(
    SRCS "main.c" "mcp2515.c" "bt_spp.c"
    INCLUDE_DIRS "."
    REQUIRES driver nvs_flash bt
)
