idf_component_register(
    SRCS "src/ppm_bootloader.c"
         "src/ppm_err.c"
         "src/ppm_session.c"
         "src/rmt_ppm.c"
         "src/rmt_ppm_encoder.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES driver
             esp_driver_rmt
             intelhex
             mlx_crc
    PRIV_REQUIRES mlx_chip
)
