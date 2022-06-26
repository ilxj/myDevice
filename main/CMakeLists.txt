idf_component_register(SRCS "main.c"
INCLUDE_DIRS "." "../components/myDeviceLib"
PRIV_REQUIRES nvs_flash app_update nghttp json)
