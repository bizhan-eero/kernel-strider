cmake_minimum_required (VERSION 2.6)
#######################################################################

# Name of the subproject and of the application
project (tsan_process_trace)
enable_language (C)
enable_language(CXX)
#######################################################################

include_directories (
	"${CMAKE_SOURCE_DIR}"
	"${CMAKE_SOURCE_DIR}/include"
	"${CMAKE_CURRENT_BINARY_DIR}"
)

check_dwfl_report_elf()

# [NB] This file is not named config.h to avoid conflicts with the top-level
# config.h of the project.
configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/config_process_trace.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/config_process_trace.h")

set(APP_SOURCES
	main.cpp
	process_trace.h
	module_info.h
	module_info.cpp
	rc_ptr.h
	trace_processor.h
	trace_processor.cpp
	"${CMAKE_CURRENT_BINARY_DIR}/config_process_trace.h"

	# LZO mini
	"${CMAKE_SOURCE_DIR}/lzo/minilzo.c"
	"${CMAKE_SOURCE_DIR}/lzo/minilzo.h"
	"${CMAKE_SOURCE_DIR}/lzo/lzoconf.h"
	"${CMAKE_SOURCE_DIR}/lzo/lzodefs.h"
)
add_executable(${PROJECT_NAME} ${APP_SOURCES})

# _GNU_SOURCE is needed because at least getopt_long() is GNU extension.
set_target_properties(${PROJECT_NAME} PROPERTIES
	COMPILE_FLAGS "-Wall -Wextra -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64"
)

target_link_libraries(${PROJECT_NAME} "elf" "dw")
#######################################################################

install(TARGETS ${PROJECT_NAME}
	DESTINATION ${KEDR_INSTALL_PREFIX_EXEC})
#######################################################################

kedr_test_add_subdirectory(tests)
########################################################################
