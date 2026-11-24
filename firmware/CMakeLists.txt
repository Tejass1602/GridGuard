cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(gridguard_firmware C)
target_sources(app PRIVATE
  src/main.c src/hal_adc_relay.c src/nvs_store.c src/usb_comms.c
  lib/gridguard_core/src/gg_plant.c lib/gridguard_core/src/gg_metrology.c
  lib/gridguard_core/src/gg_protection.c lib/gridguard_core/src/gg_relay_fsm.c
  lib/gridguard_core/src/gg_nilm.c lib/gridguard_core/src/gg_protocol.c)
target_include_directories(app PRIVATE lib/gridguard_core/include src)
