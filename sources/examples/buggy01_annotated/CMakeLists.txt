set(KMODULE_NAME "buggy01")
set(MAIN_NAME "${KMODULE_NAME}_annotated")

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/makefile_installed.in"
	"${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
	@ONLY)

if (KEDR_ANNOTATIONS_ENABLED)
	set(KBUILD_IN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Kbuild_annotations_in_kernel.in")
else (KEDR_ANNOTATIONS_ENABLED)
	set(KBUILD_IN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Kbuild.in")
endif (KEDR_ANNOTATIONS_ENABLED)

configure_file(
	"${KBUILD_IN_FILE}"
	"${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
	@ONLY)

install(FILES
	"${CMAKE_CURRENT_SOURCE_DIR}/module.c"
	"${CMAKE_SOURCE_DIR}/core/annotations/kedr_annotations.c"
	"${CMAKE_SOURCE_DIR}/core/annotations/kedr_annotations.h"
	DESTINATION "${KEDR_EXAMPLE_PATH}/${MAIN_NAME}"
)
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/makefile_installed"
	DESTINATION "${KEDR_EXAMPLE_PATH}/${MAIN_NAME}"
	RENAME "Makefile"
)
install(FILES
	"${CMAKE_CURRENT_BINARY_DIR}/Kbuild"
	DESTINATION "${KEDR_EXAMPLE_PATH}/${MAIN_NAME}"
)

kedr_test_add_subdirectory(tests)
