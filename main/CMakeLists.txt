# Sources are globbed so new app folders (main/apps/app_*/) need no edits
# here. CMake only re-globs on reconfigure — scripts/new_app.py touches this
# file after generating an app to force that. (CONFIGURE_DEPENDS is not
# usable: IDF also runs this file in script mode, where it is invalid.)
file(GLOB_RECURSE APP_SRCS *.cpp *.c)

idf_component_register(
    SRCS ${APP_SRCS}
    INCLUDE_DIRS "."
    REQUIRES lvgl mooncake
    PRIV_REQUIRES
        driver              # legacy touch_pad driver (ESP32 v1 touch)
        esp_driver_gpio
        esp_driver_spi
        esp_driver_ledc
        esp_timer
        nvs_flash
        esp_wifi
        esp_netif
        esp_event
        esp_http_client
        esp_http_server
        esp-tls
        mbedtls             # esp_crt_bundle for HTTPS
        bt                  # NimBLE
        lwip
        json                # cJSON for API responses (see app_weather)
        esp_app_format      # esp_app_get_description (version in app_info)
)
