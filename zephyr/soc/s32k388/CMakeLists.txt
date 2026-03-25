# Minimal SoC CMakeLists.txt for S32K388 Renode simulation.
# No real SoC init needed -- Renode handles hardware setup.

zephyr_include_directories(.)

zephyr_library()
zephyr_library_sources(gettimeofday.c)

# Use Zephyr's generic ARM Cortex-M linker script; memory regions come from DTS.
set(SOC_LINKER_SCRIPT ${ZEPHYR_BASE}/include/zephyr/arch/arm/cortex_m/scripts/linker.ld CACHE INTERNAL "")
