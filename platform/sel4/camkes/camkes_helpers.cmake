#
# Copyright 2018, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(DATA61_BSD)

cmake_minimum_required(VERSION 3.7.2)

set(CAMKES_RUMPRUN_PATH ${CMAKE_CURRENT_LIST_DIR})

CAmkESAddImportPath(${CAMKES_RUMPRUN_PATH})

DeclareCAmkESComponent(rumprun_platform_layer)

set(CAmkESCPP ON CACHE BOOL "" FORCE)

check_c_compiler_flag("-no-pie" HAVE_NO_PIE_SUPPORT)
if(HAVE_NO_PIE_SUPPORT)
    set(no_pie "-no-pie")
else()
    set(no_pie "")
endif()

# This declares a custom link language that CMake will use to link an executable
# if the LINKER_LANGUAGE property on it is changed. This function was written
# for a scenario in camkes where a camkes component, represented in CMake as an executable
# needs to have custom linking behavior. In rumprun, we build the bottom half of the system,
# and then need to link this with camkes templates and make an initial component. Then the
# posix binary and rump kernel modules need to be linked in after. We add these on as part
# of the component link step, which is arguably what is happening semantically anyway.
# Currently only one POSIX binary is supported.
# bin: Full path to binary.
# config: rumprun-bake config parameter
# language_name: Some unique linker-language name for cmake to use.
# PUBLIC_SYMBOLS: list of wildcard symbols to be made public by objcopy in the linking step
#                 For if the POSIX binary needs to link to additional symbols, such as Camkes interfaces
function(DeclareCustomCamkesLanguage bin config language_name)
    cmake_parse_arguments(PARSE_ARGV 3 LANG
        "" # Option arguments
        "" # Single arguments
        "PUBLIC_SYMBOLS" # Multiple arguments
    )

    # Set 'cl_*' variables used by configure_file
    set(cl_rumprun_binary_file ${bin})
    set(cl_rumprunbake_config ${config})
    set(cl_tools_gcc ${RUMPRUN_TOOLS_DIR}/${RUMPRUN_TOOLS_PREFIX}-gcc)
    set(cl_tools_objcopy ${RUMPRUN_TOOLS_DIR}/${RUMPRUN_TOOLS_PREFIX}-objcopy)
    set(cl_rumprun_bake ${RUMPRUN_TOOLCHAIN_PATH}/rumprun-bake)
    foreach(sym IN LISTS LANG_PUBLIC_SYMBOLS)
        set(cl_extra_public_symbols "${cl_extra_public_symbols} -G ${sym}")
    endforeach()
    # Populate camkes_link.sh template with custom values.
    configure_file(${CAMKES_RUMPRUN_PATH}/camkes_link.sh.in ${CMAKE_CURRENT_BINARY_DIR}/${language_name}_camkes_link.sh @ONLY)

    set(BASEFILE_LIB_DIRS "-L${RUMPRUN_BASEDIR}/lib/libbmk_core -L${RUMPRUN_BASEDIR}/lib/libbmk_rumpuser")
    set(BASEFILE_LIBS "-lbmk_core -lbmk_rumpuser")
    set(CMAKE_${language_name}_LINK_EXECUTABLE "${CMAKE_CURRENT_BINARY_DIR}/${language_name}_camkes_link.sh \
            <FLAGS> <CMAKE_C_LINK_FLAGS> ${no_pie} \
            <LINK_FLAGS> \
            -Wl,-r -u __vsyscall_ptr  -Wl,-dT ${CAMKES_RUMPRUN_PATH}/../stage1.lds \
            ${CRTObjFiles} \
            <OBJECTS> \
            ${libgcc} \
            <LINK_LIBRARIES>  \
            ${BASEFILE_LIB_DIRS} \
            -Wl,--whole-archive ${BASEFILE_LIBS} -Wl,--no-whole-archive \
            ${FinObjFiles} \
            -o <TARGET>"
    CACHE INTERNAL "" FORCE)
endfunction()


function(DeclareRumprunCAmkESComponent name)
    cmake_parse_arguments(PARSE_ARGV 1 RUMP
        "" # Option arguments
        "POSIX_BIN;BAKE_CONFIG" # Single arguments
        "SOURCES;INCLUDES;C_FLAGS;LIBS;PUBLIC_SYMBOLS" # Multiple aguments
    )
    if (NOT "${RUMP_UNPARSED_ARGUMENTS}" STREQUAL "")
        message(FATAL_ERROR "Unknown arguments to DeclareRumprunCAmkESComponent")
    endif()
    DeclareCustomCamkesLanguage(${RUMP_POSIX_BIN} ${RUMP_BAKE_CONFIG} ${name}_LANG PUBLIC_SYMBOLS ${RUMP_PUBLIC_SYMBOLS})
    add_custom_command(OUTPUT ${RUMP_POSIX_BIN}.c
        DEPENDS ${RUMP_POSIX_BIN}
        COMMAND ${CMAKE_COMMAND} -E touch ${RUMP_POSIX_BIN}.c)
    add_custom_target(${name}_posix_bin DEPENDS ${RUMP_POSIX_BIN}.c)

    DeclareCAmkESComponent(${name}
        SOURCES  ${RUMP_POSIX_BIN}.c ${RUMP_SOURCES}
        LIBS rumprun_intermediate_file ${RUMP_LIBS}
        INCLUDES ${RUMP_INCLUDES}
        C_FLAGS ${RUMP_C_FLAGS}
        LINKER_LANGUAGE ${name}_LANG)
    add_dependencies(CAmkESComponent_${name} ${name}_posix_bin)

endfunction()
