set(BinName "harzlib")

set(Z80Asm AILZ80ASM.exe)
set(Z80AsmOpt -bin -lm simple -ts 4 -f)

add_custom_command(
	OUTPUT harzbios.bin
	DEPENDS harzbios.z80
	COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND ${Z80Asm} -i harzbios.z80 -o ${CMAKE_CURRENT_BINARY_DIR}/harzbios.bin -lst ${CMAKE_CURRENT_BINARY_DIR}/harzbios.lst ${Z80AsmOpt}
)
add_custom_command(
	OUTPUT hartplay.bin
	DEPENDS hartplay.z80
	COMMAND cd ${CMAKE_CURRENT_SOURCE_DIR}
	COMMAND ${Z80Asm} -i hartplay.z80 -o ${CMAKE_CURRENT_BINARY_DIR}/hartplay.bin -lst ${CMAKE_CURRENT_BINARY_DIR}/hartplay.lst ${Z80AsmOpt}
)

set(ObjCpy arm-none-eabi-objcopy.exe)
set(ObjCpyOpt -v -I binary -O elf32-littlearm --rename-section .data=.rodata --set-section-flags .rodata=alloc,load,readonly )

add_custom_command(
	OUTPUT harzbios.o
	DEPENDS harzbios.bin
	COMMAND ${ObjCpy} ${ObjCpyOpt} harzbios.bin ${CMAKE_CURRENT_BINARY_DIR}/harzbios.o
)
add_custom_command(
	OUTPUT hartplay.o
	DEPENDS hartplay.bin
	COMMAND ${ObjCpy} ${ObjCpyOpt} hartplay.bin ${CMAKE_CURRENT_BINARY_DIR}/hartplay.o
)

add_library(
	${BinName}
	STATIC
	harzlib.cpp
	harzbios.o
	hartplay.o
)

target_include_directories(
	${BinName}
	PUBLIC
	${PROJECT_SOURCE_DIR}/harzlib
)
