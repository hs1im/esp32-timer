# main/CMakeLists.txt (수정본)
idf_component_register(SRCS "main.c" "ssd1351.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_driver_spi esp_driver_gpio)