# The part of the output system operating in the kernel mode.
########################################################################

set(TOP_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/include/kedr")
kbuild_include_directories("${CMAKE_SOURCE_DIR}/utils")
kbuild_include_directories("${KEDR_ST_REC_CONFIG_H_DIR}")

kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers") 
kbuild_add_dependencies("kedr_mem_core")
kbuild_add_module(${KMODULE_NAME} 
# sources
	module.c
	
# headers
	"${TOP_INCLUDE_DIR}/kedr_mem/core_api.h"
	"${TOP_INCLUDE_DIR}/object_types.h"
	"${KEDR_TR_INCLUDE_DIR}/recorder.h"
	"${KEDR_ST_REC_CONFIG_H_DIR}/kedr_st_rec_config.h"
)
kedr_install_kmodule(${KMODULE_NAME})
########################################################################
