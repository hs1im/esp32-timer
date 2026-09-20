idf_component_register(SRCS "main.c" "ssd1351.c" "button.c" "stopwatch.c" "digit7seg.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_driver_spi esp_driver_gpio esp_driver_gptimer esp_timer)