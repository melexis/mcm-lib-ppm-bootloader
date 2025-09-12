idf_component_register(
    SRCS "src/rmt_ppm.c"
         "src/rmt_ppm_encoder.c"
         "src/ppm_bootloader.c"
         "src/ppm_session.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES driver
             esp_driver_rmt
             gen_bootloader
             intelhex
             mlx_crc
    PRIV_REQUIRES mlx_chip
                  slave_power
)
