#
# Copyright 2017, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(DATA61_BSD)
#

cmake_minimum_required(VERSION 3.7.2)

# Force morecorebytes to 0.
set(LibSel4MuslcSysMorecoreBytes 0 CACHE STRING "" FORCE)

# Function for creating custom_target to invoke rumprun-bake to create a seL4 rumprun binary
# target_name is the name of the custom_target to be created.  The binary name will be ${target_name}.bin
# CONFIG is an optional argument that specifies a what rumprun config to be used. Its default value is sel4_generic
# RUMP_TARGETS are rumprun apps that need to be baked
# In addition, a custom_command to build each of the binaries needs to be in scope.
# See BakeExternalRumpkernelCMakeProject below for an example
function(CreateRumprunBakeCommand target_name)
    cmake_parse_arguments(PARSE_ARGV 1 RUMPRUN_BAKE "" "CONFIG" "RUMP_TARGETS")
    if (NOT "${RUMPRUN_BAKE_UNPARSED_ARGUMENTS}" STREQUAL "")
        message(FATAL_ERROR "Unknown arguments to CreateRumprunBakeCommand")
    endif()

    if("${RUMPRUN_BAKE_CONFIG}" STREQUAL "")
        set(RUMPRUN_BAKE_CONFIG sel4_generic)
    endif()

    # Add binaries from RUMP_TARGETS to rump_binaries list to be used in custom_command depends field
    foreach(RUMP_TARGET IN LISTS RUMPRUN_BAKE_RUMP_TARGETS)
        get_target_property(binaries ${RUMP_TARGET} RUMP_BINARIES)
        list(APPEND rump_binaries ${binaries})
    endforeach()

    set(output_binary ${target_name}.bin)
    # Add custom command for baking the rumprun image.
    add_custom_command(
        OUTPUT
            ${output_binary}
        COMMAND ${CMAKE_COMMAND} -E env
            PATH=$ENV{PATH}:$<TARGET_PROPERTY:rumprun_toplevel_support,RUMPRUN_TOOLCHAIN_PATH>
            RUMPRUN_BASEDIR=$<TARGET_PROPERTY:rumprun_bottomlevel_support,RUMPRUN_BASEDIR>
            rumprun-bake ${RUMPRUN_BAKE_CONFIG} ${output_binary} ${rump_binaries}
        DEPENDS rumprun_bottomlevel_support ${RUMPRUN_BAKE_RUMP_TARGETS} ${rump_binaries}
        COMMENT "[Rumprun baking ${target_name}]"
    )
    add_custom_target(${target_name}
        DEPENDS ${output_binary} ${RUMPRUN_BAKE_RUMP_TARGETS}
    )
    set_property(TARGET ${target_name} PROPERTY BAKED_LOCATION "${CMAKE_CURRENT_BINARY_DIR}/${output_binary}")

endfunction()

# Take a CMake posix project that outputs a list of binaries and turn it into a seL4 rumprun image
# target_name is the name of the target that will be returned. Location of the image is saved in a property by CreateRumprunBakeCommand
# SOURCE_DIR is the directory of the cmake project to build
# RUMPRUN_CONFIG is the config to use with rumprun-bake
# OUTPUT_BIN a list of binaries outputted by the CMake project
function(BakeExternalRumpkernelCMakeProject target_name)
    cmake_parse_arguments(PARSE_ARGV 1 EXTERNAL "" "SOURCE_DIR;RUMPRUN_CONFIG" "OUTPUT_BIN")
    if (NOT "${EXTERNAL_UNPARSED_ARGUMENTS}" STREQUAL "")
        message(FATAL_ERROR "Unknown arguments to BakeExternalRumpkernelProject")
    endif()
    if ("${EXTERNAL_SOURCE_DIR}" STREQUAL "")
        message(FATAL_ERROR "BakeExternalRumpkernelProject requires SOURCE_DIR argument")
    endif()
    if ("${EXTERNAL_OUTPUT_BIN}" STREQUAL "")
        message(FATAL_ERROR "BakeExternalRumpkernelProject requires OUTPUT_BIN argument")
    endif()

    # Wrap rumprun_toplevel_support in a custom target so that ExternalProject can depend on it
    add_custom_target(${target_name}_stamptarget)
    add_dependencies(${target_name}_stamptarget rumprun_toplevel_support)

    # Add ExternalProject for building a binary using rumprun toolchains
    set(external_target_name ${target_name}_external)
    set(stamp_dir ${CMAKE_CURRENT_BINARY_DIR}/${external_target_name}-stamp)
    ExternalProject_Add(${external_target_name}
        SOURCE_DIR ${EXTERNAL_SOURCE_DIR}
        INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}
        STAMP_DIR ${stamp_dir}
        DEPENDS ${target_name}_stamptarget
        BUILD_ALWAYS ON
        EXCLUDE_FROM_ALL
        CMAKE_ARGS -DCMAKE_TOOLCHAIN_FILE=$<TARGET_PROPERTY:rumprun_toplevel_support,RUMPRUN_TOOLCHAIN_CMAKE>
                   -G Ninja
                   -DCMAKE_INSTALL_PREFIX:STRING=${CMAKE_CURRENT_BINARY_DIR}
    )
    ExternalProject_Add_StepTargets(${external_target_name} install)

    # Create rumpbake_binfiles to hold list of output binaries that need to be given to RumprunBakeCommand
    foreach(RUMP_BIN IN LISTS EXTERNAL_OUTPUT_BIN)
        list(APPEND rumpbake_binfiles
            ${CMAKE_CURRENT_BINARY_DIR}/${RUMP_BIN}
        )
    endforeach()

    # Add custom_command that depends on the install stamp file of ExternalProject to force stale checking of
    # rumpbake_binfiles until after the install step of ExternalProject has been run
    add_custom_command(
        OUTPUT ${rumpbake_binfiles}
        COMMAND true
        DEPENDS ${external_target_name}-install ${stamp_dir}/${external_target_name}-install
    )

    # Save bin files as property on the ExternalProject target
    set_target_properties(${external_target_name} PROPERTIES RUMP_BINARIES ${rumpbake_binfiles})

    # Bake the binaries into a rumprun image
    CreateRumprunBakeCommand(${target_name}
            CONFIG ${EXTERNAL_RUMPRUN_CONFIG}
            RUMP_TARGETS ${target_name}_external
    )

endfunction()
