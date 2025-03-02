set(BinName "mgspico3z")

add_executable(
	${BinName}
	global.cpp
	t_mmmspi.cpp
	t_si5351.cpp
	timerobj.cpp
	harz80/CHarz80Ctrl.cpp
	mgs/mgs_tools.cpp
	mgs/MusFiles.cpp
	ff/ff.c
	ff/ffsystem.c
	ff/ffunicode.c
	ff/mmc_pico_spi.c
	oled/oledssd1306.cpp
	sdfat.cpp
	CMsCount.cpp
	MgspicoSettings.cpp
	mgspico3z.cpp
)

include_directories(
	./
	ff
	mgs
	harz80
	harzlib
)

target_compile_options(
	${BinName}
	PUBLIC -O2 -Wall
)

# pico_generate_pio_header(
# 	${BinName}
# 	${CMAKE_CURRENT_LIST_DIR}/mgspico.pio
# )

## enable usb output, disable uart output
pico_enable_stdio_usb(${BinName} 1)
pico_enable_stdio_uart(${BinName} 0)

target_link_libraries(
	${BinName}
	PRIVATE
	pico_stdlib
	pico_rand
	hardware_i2c
	hardware_spi
#	hardware_pio pico_multicore hardware_flash
	harzlib
)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(${BinName})

# add url via pico_set_program_url
#example_auto_set_url(${BinName})

