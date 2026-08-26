cmake_minimum_required(VERSION 3.14)
include(pico-sdk/pico_sdk_init.cmake)
project(cable_tester)
pico_sdk_init()
add_executable(cable_tester)

target_compile_options(cable_tester PRIVATE -Wall -Wextra)

target_sources(cable_tester PUBLIC
	${CMAKE_CURRENT_LIST_DIR}/cable_tester.c
	${CMAKE_CURRENT_LIST_DIR}/usb_descriptors.c
)

# Get hash and date of last git commit and current date for IDENTIFY stats
find_package(Git)
if(Git_FOUND)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_COMMIT_HASH
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} log -1 --format=%ci
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		OUTPUT_VARIABLE GIT_COMMIT_DATE
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	execute_process(
		COMMAND ${GIT_EXECUTABLE} diff --quiet
		WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
		RESULT_VARIABLE GIT_IS_DIRTY
	)
	if(GIT_IS_DIRTY)
		set(GIT_COMMIT_HASH "${GIT_COMMIT_HASH}-dirty")
	endif()
else()
	set(GIT_COMMIT_HASH "unknown")
	set(GIT_COMMIT_DATE "unknown")
endif()
string(TIMESTAMP BUILD_DATE "%Y-%m-%d")
configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/version.h.in
	${CMAKE_CURRENT_BINARY_DIR}/version.h
	@ONLY
)

# Show size after link
SET(SHOW_SIZE_INFO "-Wl,--print-memory-usage")
SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${SHOW_SIZE_INFO}")

target_include_directories(cable_tester PUBLIC
	${CMAKE_CURRENT_LIST_DIR} # for TinyUSB to find tusb_config.h
	${CMAKE_CURRENT_BINARY_DIR} # for cable_tester.c to find version.h
)

target_link_libraries(cable_tester PUBLIC
	pico_stdlib
	tinyusb_device
	tinyusb_board
	pico_unique_id
	hardware_gpio
	hardware_adc
	hardware_i2c
	)

pico_add_extra_outputs(cable_tester)
