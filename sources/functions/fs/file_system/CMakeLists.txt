# Model for character devices.
set(plugin_name "kedr_fh_drd_file_system")

kbuild_include_directories("${CMAKE_CURRENT_BINARY_DIR}")
kbuild_include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
kbuild_include_directories("${KEDR_COI_INSTALL_DIR}/include")

kbuild_use_symbols("${KEDR_COI_INSTALL_DIR}/lib/modules/${KBUILD_VERSION_STRING}/symvers/kedr_coi.symvers")

kbuild_use_symbols("${CMAKE_BINARY_DIR}/core/Module.symvers")
kbuild_add_dependencies("kedr_mem_core")

kbuild_add_module(${plugin_name}
    "plugin.c"
    "file_operations_model.c"
    "file_operations_model.h"
    
    "fs_interception.c"
    "fs_interception.h"
    
    "file_operations_interceptor.c"
    "inode_file_operations_interceptor.c"
    "file_system_type_interceptor.c"
    "super_operations_interceptor.c"
    "inode_operations_interceptor.c"
    "dentry_operations_interceptor.c"
    
    "vma_operations_interceptor.c"
    "vma_operations_interceptor.h"
    "vma_operations_interceptor_internal.c"
    "vma_operations_interceptor_internal.h"
    "vma_operations_clone_interceptor.c"
    "vma_operations_clone_interceptor.h"
    
    "cdev_file_operations_interceptor.c"
    "cdev_file_operations_interceptor.h"
    
    "miscdev_file_operations_interceptor.h"
    
    "miscdev_file_operations_interceptor_internal.c"
    "miscdev_file_operations_interceptor_internal.h"
    
    "file_operations_model.c"
    "file_operations_model.h"
    "inode_operations_model.c"
    "inode_operations_model.h"
    "dentry_operations_model.c"
    "dentry_operations_model.h"
    "super_operations_model.c"
    "super_operations_model.h"
    "file_system_type_model.c"
    "file_system_type_model.h"
    
    "vma_operations_model.c"
    "vma_operations_model.h"
    
    "cdev_model.h"
    "miscdev_model.h"
    
    "fill_super_interceptor.c"
    "fill_super_interceptor.h"
    
    "callback_interceptor.h"
    "callback_interceptor.c"
)

# Plugin code generation
rule_generate_file("plugin.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/module.data"
    "${model_templates_dir}/plugin_module.c")


# Interception for whole file system is taken from example in KEDR COI
rule_copy_file("fs_interception.c"
    "${KEDR_COI_INSTALL_DIR}/share/kedr-coi/examples/file_access_counter/fs_interception.c")
rule_copy_file("fs_interception.h"
    "${KEDR_COI_INSTALL_DIR}/share/kedr-coi/examples/file_access_counter/fs_interception.h")

# Interceptors itself taken from correponded places
rule_copy_file("file_operations_interceptor.c"
    "${interceptors_dir}/file_operations_interceptor.c")
rule_copy_file("inode_file_operations_interceptor.c"
    "${interceptors_dir}/inode_file_operations_interceptor.c")
rule_copy_file("file_system_type_interceptor.c"
    "${interceptors_dir}/file_system_type_interceptor.c")
rule_copy_file("super_operations_interceptor.c"
    "${interceptors_dir}/super_operations_interceptor.c")
rule_copy_file("inode_operations_interceptor.c"
    "${interceptors_dir}/inode_operations_interceptor.c")
rule_copy_file("dentry_operations_interceptor.c"
    "${interceptors_dir}/dentry_operations_interceptor.c")

# Interception for vma operations(internal)
rule_generate_file("vma_operations_interceptor_internal.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vma_operations_interceptor.data"
    "${KEDR_COI_INSTALL_DIR}/share/kedr-coi/templates/kedr_coi_interceptor.c")

rule_generate_file("vma_operations_interceptor_internal.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/vma_operations_interceptor.data"
    "${KEDR_COI_INSTALL_DIR}/share/kedr-coi/templates/kedr_coi_interceptor.h")

# Interception for vma operations as cloned(internal)
rule_generate_file("vma_operations_clone_interceptor.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vma_operations_clone_interceptor.data"
    "${KEDR_COI_INSTALL_DIR}/share/kedr-coi/templates/kedr_coi_creation_interceptor.c")

rule_generate_file("vma_operations_clone_interceptor.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/vma_operations_clone_interceptor.data"
    "${KEDR_COI_INSTALL_DIR}/share/kedr-coi/templates/kedr_coi_creation_interceptor.h")


# Interception for file operations from character device
rule_generate_file("cdev_file_operations_interceptor.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/cdev_file_operations_interceptor.data"
    "${KEDR_COI_INSTALL_DIR}/share/kedr-coi/templates/kedr_coi_factory_interceptor.c")

rule_generate_file("cdev_file_operations_interceptor.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/cdev_file_operations_interceptor.data"
    "${KEDR_COI_INSTALL_DIR}/share/kedr-coi/templates/kedr_coi_factory_interceptor.h")

# Interception for file operations from misc device(internal)
rule_generate_file("miscdev_file_operations_interceptor_internal.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/miscdev_file_operations_interceptor.data"
    "${KEDR_COI_INSTALL_DIR}/share/kedr-coi/templates/kedr_coi_factory_interceptor.c")

rule_generate_file("miscdev_file_operations_interceptor_internal.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/miscdev_file_operations_interceptor.data"
    "${KEDR_COI_INSTALL_DIR}/share/kedr-coi/templates/kedr_coi_factory_interceptor.h")


rule_copy_file("fs_interception.c"
    "${KEDR_COI_INSTALL_DIR}/share/kedr-coi/examples/file_access_counter/fs_interception.c")
rule_copy_file("fs_interception.h"
    "${KEDR_COI_INSTALL_DIR}/share/kedr-coi/examples/file_access_counter/fs_interception.h")



rule_generate_file("file_operations_model.c"
    "${CMAKE_CURRENT_BINARY_DIR}/file_operations_all.data"
    "${model_templates_dir}/operations_model.c")

rule_generate_file("file_operations_model.h"
    "${CMAKE_CURRENT_BINARY_DIR}/file_operations_all.data"
    "${model_templates_dir}/operations_model.h")


# Rule for concatenation files in one.
# In the future may be in cmake_useful.cmake
function(rule_concat_files target_file source1)
    to_abs_path(source_files ${source1} ${ARGN})
    add_custom_command(OUTPUT ${target_file}
        COMMAND cat ${source_files} > ${target_file}
    DEPENDS ${source_files})
endfunction(rule_concat_files target_file source1)

# Combine data of file operations itself and their connection with inode
# and cdev objects into one datafile.
# 
# In the future connections will use different templates
rule_concat_files("file_operations_all.data"
    "file_operations.data"
    "inode_file_operations.data"
    "cdev_file_operations.data"
    "miscdev_file_operations.data")


rule_generate_file("dentry_operations_model.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/dentry_operations.data"
    "${model_templates_dir}/operations_model.c")

rule_generate_file("dentry_operations_model.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/dentry_operations.data"
    "${model_templates_dir}/operations_model.h")


rule_generate_file("inode_operations_model.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/inode_operations.data"
    "${model_templates_dir}/operations_model.c")

rule_generate_file("inode_operations_model.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/inode_operations.data"
    "${model_templates_dir}/operations_model.h")

rule_generate_file("super_operations_model.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/super_operations.data"
    "${model_templates_dir}/operations_model.c")

rule_generate_file("super_operations_model.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/super_operations.data"
    "${model_templates_dir}/operations_model.h")


rule_generate_file("file_system_type_model.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/file_system_type.data"
    "${model_templates_dir}/operations_model.c")

rule_generate_file("file_system_type_model.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/file_system_type.data"
    "${model_templates_dir}/operations_model.h")


rule_generate_file("vma_operations_model.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/vma_operations.data"
    "${model_templates_dir}/operations_model.c")

rule_generate_file("vma_operations_model.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/vma_operations.data"
    "${model_templates_dir}/operations_model.h")


rule_generate_file("fill_super_interceptor.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/fill_super_interceptor.data"
    "${model_templates_dir}/callback_interceptor.c")

rule_generate_file("fill_super_interceptor.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/fill_super_interceptor.data"
    "${model_templates_dir}/callback_interceptor.h")

kedr_install_kmodule(${plugin_name})
