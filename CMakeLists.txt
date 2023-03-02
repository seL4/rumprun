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

cmake_minimum_required(VERSION 3.5.1)

project(rumprun NONE)

if(KernelArchX86)
    if(Kernel64)
        set(rumprun_arch "x86_64")
        set(rumprun_sel4_arch "${rumprun_arch}")
        set(rumprun_tuple "x86_64-rumprun-netbsd")
        list(APPEND RUMPKERNEL_FLAGS -F ACLFLAGS=-m64)
    else()
        set(rumprun_arch "i486")
        set(rumprun_sel4_arch "i386")
        set(rumprun_tuple "i486-rumprun-netbsdelf")
        list(
            APPEND
                RUMPKERNEL_FLAGS
                -F
                ACLFLAGS=-m32
                -F
                ACLFLAGS=-fno-pic
        )
    endif()
    set(rumprun_tool_prefix "${rumprun_sel4_arch}--netbsd")
elseif(KernelArchARM)
    if(Kernel32)
        set(rumprun_arch "arm")
        set(rumprun_sel4_arch "aarch32")
        set(rumprun_tuple "arm-rumprun-netbsdelf-eabi")
        set(rumprun_tool_prefix "arm--netbsdelf-eabi")
        # Suppress warnings that would otherwise stop the compilation
        list(APPEND RUMPKERNEL_FLAGS -F CWARNFLAGS=-w)
    endif()
    # Append -march flag to ensure Rump is built properly for the specific CPU arch
    list(APPEND RUMPKERNEL_FLAGS -F ACFLAGS='-march=${KernelArmArmV}')
else()
    # Stop processing this file the target platform isn't supported.
    return()
endif()

# Build for release (Without debug symbols)
if("${CMAKE_BUILD_TYPE}" IN_LIST "Release;MinSizeRel")
    list(APPEND RUMPKERNEL_FLAGS -r)
endif()

# Ignore errors that cause compile to fail for GCC 8 or above
if(NOT "${CMAKE_C_COMPILER_VERSION}" VERSION_LESS 8.0)
    list(
        APPEND
            RUMPKERNEL_FLAGS
            -F
            CFLAGS='-Wno-cast-function-type
            -Wno-packed-not-aligned
            -Wno-tautological-compare
            -fcommon'
    )
    # Suppress warnings that would otherwise stop the compilation
    list(APPEND RUMPKERNEL_FLAGS -F CWARNFLAGS=-w)

    # Avoid linker errors when build tools are generated
    set(host_cflags "-fcommon")
endif()

set(configure_string "")
config_string(RumprunTMPFSNumMiB RUMPRUN_TMPFS_NUM_MiB "Set this to the size of memory \
    you want to back the tmpfs at /tmp." DEFAULT 1 UNQUOTE)

config_string(RumprunCookfsDir RUMPRUN_COOKFS_DIR "cookfs directory" DEFAULT " ")

config_option(
    UseLargePages
    USE_LARGE_PAGES
    "Use large pages for rumprun backing memory.  This reduces the amount of book keeping required \
        to track the pages created and also reduces initialisation time.  However, mprotect depends on \
        4k pages in order to remap the pages with new permissions.  Thus, enabling large pages results in \
        disabling the stack guard page functionality.  Use at own risk."
    DEFAULT
    OFF
)

add_config_library(rumprun "${configure_string}")

# Only set FULLDIRPATH if the COOKFS dir is set to something proper
if(NOT ${RumprunCookfsDir} STREQUAL " ")
    set(FULLDIRPATH ${CMAKE_SOURCE_DIR}/${RumprunCookfsDir})
    file(GLOB_RECURSE FULLDIR_DEPS ${FULLDIRPATH}/*)

endif()

file(GLOB_RECURSE ROOTFS_DEPS lib/librumprunfs_base/rootfs/*)

add_custom_command(
    OUTPUT build-temp/.librumrunfs.stamp
    COMMAND
        mkdir -p ${rumprun_sel4_arch}/sel4-obj/rootfs/ && rsync -av ${FULLDIRPATH}
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/librumprunfs_base/rootfs/
        ${rumprun_sel4_arch}/sel4-obj//rootfs/
        --delete
    COMMAND touch ${CMAKE_CURRENT_BINARY_DIR}/build-temp/.librumrunfs.stamp
    VERBATIM
    DEPENDS ${ROOTFS_DEPS} ${FULLDIR_DEPS}
    COMMENT "[Copying files to rootfs: ${FULLDIRPATH}]"
)

file(RELATIVE_PATH BUILD_DIRECTORY_RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(
    RR_ENV_VARS
    PATH=$ENV{PATH}
    SEL4_ARCH=${rumprun_sel4_arch}
    RUMPRUN_BASE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
    RUMP_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
    CC=${CMAKE_C_COMPILER}
    CXX=${CMAKE_CXX_COMPILER}
    HOST_CFLAGS=${host_cflags}
)

set(
    RUMPRUN_SEL4LIBS
    sel4
    sel4runtime
    sel4muslcsys
    sel4allocman
    platsupport
    sel4platsupport
    platsupport
    sel4serialserver
    sel4sync
    sel4utils
    sel4vspace
    sel4vka
    sel4simple-default
    sel4simple
    sel4debug
    utils
    cpio
    elf
)

set(LDFLAGS_SEL4 "")
set(CFLAGS_SEL4 "")
foreach(RUMPRUN_SEL4LIB IN LISTS RUMPRUN_SEL4LIBS)
    string(
        APPEND CFLAGS_SEL4
        " -I$<JOIN:$<TARGET_PROPERTY:${RUMPRUN_SEL4LIB},INTERFACE_INCLUDE_DIRECTORIES>, -I>"
    )
    string(APPEND LDFLAGS_SEL4 " -L$<TARGET_FILE_DIR:${RUMPRUN_SEL4LIB}> -l${RUMPRUN_SEL4LIB}")
endforeach()
string(APPEND LDFLAGS_SEL4 " $<TARGET_PROPERTY:muslc_imported,IMPORTED_LOCATION>")
string(APPEND CFLAGS_SEL4 " -I${CMAKE_CURRENT_SOURCE_DIR}/platform/sel4/include/sel4")

string(
    APPEND CFLAGS_SEL4
    " -I$<JOIN:$<TARGET_PROPERTY:sel4_autoconf,INTERFACE_INCLUDE_DIRECTORIES>, -I>"
)
string(
    APPEND CFLAGS_SEL4
    " -I$<JOIN:$<TARGET_PROPERTY:rumprun_Config,INTERFACE_INCLUDE_DIRECTORIES>, -I>"
)

# Create a custom target that invokes a rumprun build command from ./build-rr.sh
# command_name is the name of the command to run
# command_description is the command description printed out by ninja
# There are optional arguments:
# PHONY will cause the target to always be stale so that it will always rerun the command
# TARGET_NAME overrides the target name. Otherwise the command_name is used as the target name
# RUMP_TARGETS are
# FILE_DEPS are files that get passed to DEPENDS of the internal custom_command definition
# ENV_VARS are extra environment variables that get set when build-rr.sh is called
function(CreateRumprunBuildCommand command_name command_description)
    cmake_parse_arguments(
        PARSE_ARGV
        2
        RUMP_BUILD
        "PHONY"
        "TARGET_NAME"
        "RUMP_TARGETS;FILE_DEPS;ENV_VARS;OUTPUT_FILES"
    )
    if(NOT "${RUMP_BUILD_UNPARSED_ARGUMENTS}" STREQUAL "")
        message(FATAL_ERROR "Unknown arguments to CreateRumprunBuildCommand")
    endif()

    # Suppress rump build output
    # TODO find a way to disable this when the verbose flag is passed to ninja
    set(QUIET -q -q)

    # Create command line that will be invoked
    set(
        BUILD_RR_CMD_LINE
        ${CMAKE_COMMAND}
        -E
        env
        ${RR_ENV_VARS}
        ${RUMP_BUILD_ENV_VARS}
        ./build-rr.sh
        ${QUIET}
        -d
        ${BUILD_DIRECTORY_RELATIVE}/${rumprun_sel4_arch}/rumprun
        -o
        ${BUILD_DIRECTORY_RELATIVE}/${rumprun_sel4_arch}/sel4-obj
        sel4
        ${command_name}
        --
        ${RUMPKERNEL_FLAGS}
    )

    # Add stampfiles from other rump targets to rump_deps list to be used in custom_command depends field
    foreach(RUMP_TARGET IN LISTS RUMP_BUILD_RUMP_TARGETS)
        get_target_property(stamp ${RUMP_TARGET} STAMP_FILE)
        list(APPEND rump_deps ${stamp})
    endforeach()

    # We support overriding the target name if neccessary
    set(STAMP build-temp/${command_name}.stamp)
    Set(TARGET_NAME ${command_name})
    if(NOT "${RUMP_BUILD_TARGET_NAME}" STREQUAL "")
        set(TARGET_NAME ${RUMP_BUILD_TARGET_NAME})
        set(STAMP build-temp/${RUMP_BUILD_TARGET_NAME}.stamp)
    endif()

    # custom_command doesn't have a BUILD_ALWAYS option, so we emulate it by creating a phony dependency
    # That is then deleted at the end of each build.  This is used when we cannot correctly track dependencies
    if(RUMP_BUILD_PHONY)
        set(STAMP_PHONY ${CMAKE_CURRENT_BINARY_DIR}/build-temp/${TARGET_NAME}.phony.stamp)
        set(TOUCHSTAMP_PHONY ${CMAKE_COMMAND} -E remove ${STAMP_PHONY})
        add_custom_command(
            OUTPUT ${STAMP_PHONY}
            COMMAND
                ${CMAKE_COMMAND} -E touch ${STAMP_PHONY}
            COMMENT "[Calling phony rule]"
        )
    endif()

    # Create custom command.  It deletes the PHONY stampfile if one exists, runs the rump command and then touches the output ${STAMP} file
    add_custom_command(
        OUTPUT ${STAMP} ${RUMP_BUILD_OUTPUT_FILES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND ${TOUCHSTAMP_PHONY}
        COMMAND ${BUILD_RR_CMD_LINE}
        COMMAND
            ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/${STAMP}
        DEPENDS
            ${RUMP_BUILD_RUMP_TARGETS}
            ${rump_deps}
            ${RUMP_BUILD_FILE_DEPS}
            ${STAMP_PHONY}
        COMMENT "[Calling ./build-rr.sh - ${command_description}]"
    )

    # Add Target for custom_command.
    add_custom_target(
        ${TARGET_NAME}
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${STAMP} ${RUMP_BUILD_RUMP_TARGETS}
    )

    # Set STAMP_FILE property on custom target so that other RUMP_TARGETS can depend on it
    set_target_properties(${TARGET_NAME} PROPERTIES STAMP_FILE ${CMAKE_CURRENT_BINARY_DIR}/${STAMP})

endfunction()

# Glob all of the different files in the rumprun sources
# We use a glob as it isn't practical to keep a full list of files up to date
file(GLOB APP_TOOLS_FILES app-tools/*)
file(GLOB_RECURSE SEL4_PLATFORM_FILES platform/sel4/*)

set(
    RUMPRUN_GLOB_NETBSD_SOURCES OFF
    CACHE BOOL "This flag causes CMake to add lots of source files to its dependency lists
     which noticeably slows down its configuration times (up to 4 times slower).
     When not making changes in the rumprun directory, it is likely not necessary
     to have this enabled."
)
if(RUMPRUN_GLOB_NETBSD_SOURCES)
    file(GLOB_RECURSE RUMPRUN_LIB_FILES lib/*)
    file(GLOB_RECURSE RUMPRUN_INCLUDE_FILES include/*)
    file(GLOB_RECURSE SRC_NETBSD_FILES src-netbsd/*)
    file(GLOB_RECURSE BUILD_RR_FILES buildrump.sh/*)
endif()

# Create rumprun build targets
CreateRumprunBuildCommand(tools "Rump kernel tools" FILE_DEPS ${BUILD_RR_FILES} ${SRC_NETBSD_FILES})

CreateRumprunBuildCommand(toolsconfig "Extra Rumprun tools configuration" RUMP_TARGETS tools)

CreateRumprunBuildCommand(rumplibs "Rump kernel modules" RUMP_TARGETS toolsconfig)

CreateRumprunBuildCommand(
    apptools
    "Rumprun app toolchains"
    RUMP_TARGETS
    rumplibs
    FILE_DEPS
    ${APP_TOOLS_FILES}
)

CreateRumprunBuildCommand(userspace "Rumprun userspace libraries" RUMP_TARGETS rumplibs)

CreateRumprunBuildCommand(
    platformtoplevel
    "platform toplevel"
    RUMP_TARGETS
    userspace
    toolsconfig
    FILE_DEPS
    ${RUMPRUN_LIB_FILES}
    ${CMAKE_CURRENT_SOURCE_DIR}/platform/makepseudolinkstubs.sh
)

CreateRumprunBuildCommand(
    platformheaders
    "Platform headers"
    RUMP_TARGETS
    toolsconfig
    FILE_DEPS
    ${SEL4_PLATFORM_FILES}
    ${RUMPRUN_LIB_FILES}
    ${RUMPRUN_INCLUDE_FILES}
)

CreateRumprunBuildCommand(
    platformlibs
    "Rumprun platform libraries"
    RUMP_TARGETS
    platformheaders
    rumplibs
    userspace
    platformtoplevel
    FILE_DEPS
    ${CMAKE_CURRENT_BINARY_DIR}/build-temp/.librumrunfs.stamp
    ${RUMPRUN_LIB_FILES}
    ${RUMPRUN_INCLUDE_FILES}
)

# platformobj, rump_pci, and extralibs targets all depend on seL4 header files that we can't easily track
# therefore these rules need to be marked as PHONY and rerun everytime
CreateRumprunBuildCommand(
    platformobj
    "Platform object files"
    PHONY
    ENV_VARS
    "LDFLAGS_SEL4=${LDFLAGS_SEL4}"
    "CRTOBJFILES_SEL4=${CRTObjFiles}"
    "FINOBJFILES_SEL4=${FinObjFiles}"
    "CFLAGS_SEL4=${CFLAGS_SEL4}"
    RUMP_TARGETS
    userspace
    platformheaders
    rumplibs
    FILE_DEPS
    ${SEL4_PLATFORM_FILES}
    ${RUMPRUN_LIB_FILES}
    ${RUMPRUN_INCLUDE_FILES}
    ${RUMPRUN_SEL4LIBS}
    muslc
    sel4_autoconf
    rumprun_Config
    OUTPUT_FILES
    ${CMAKE_CURRENT_BINARY_DIR}/${rumprun_sel4_arch}/sel4-obj/rumprun-intermediate.o
)

CreateRumprunBuildCommand(
    extralibs
    "extra rump kernel modules"
    PHONY
    ENV_VARS
    "CFLAGS_SEL4=${CFLAGS_SEL4}"
    RUMP_TARGETS
    platformheaders
    rumplibs
    FILE_DEPS
    ${SEL4_PLATFORM_FILES}
    ${RUMPRUN_LIB_FILES}
    ${RUMPRUN_INCLUDE_FILES}
    ${SRC_NETBSD_FILES}
    ${RUMPRUN_SEL4LIBS}
    muslc
    sel4_autoconf
    rumprun_Config
)

CreateRumprunBuildCommand(
    platforminstall
    "Install Platform files"
    RUMP_TARGETS
    platformlibs
    platformobj
    extralibs
)

CreateRumprunBuildCommand(
    pci
    "PCI rump kernel modules"
    PHONY
    TARGET_NAME
    rump_pci
    ENV_VARS
    "CFLAGS_SEL4=${CFLAGS_SEL4}"
    RUMP_TARGETS
    platformheaders
    rumplibs
    FILE_DEPS
    ${SEL4_PLATFORM_FILES}
    ${RUMPRUN_LIB_FILES}
    ${RUMPRUN_INCLUDE_FILES}
    ${SRC_NETBSD_FILES}
    ${RUMPRUN_SEL4LIBS}
    muslc
    sel4_autoconf
    rumprun_Config
)

# Install commands install artifacts to the rumprun install directory.  Top level is everything required to build rumprun applications
# bottom level is everything required to link a rumprun app with the bottom level platform libraries and basefiles.
CreateRumprunBuildCommand(
    install
    "install toplevel"
    RUMP_TARGETS
    userspace
    apptools
    platformtoplevel
    TARGET_NAME
    rumprun_install_toplevel
)
CreateRumprunBuildCommand(
    install
    "install bottomlevel"
    PHONY
    RUMP_TARGETS
    platforminstall
    rump_pci
    rumplibs
    rumprun_install_toplevel
    TARGET_NAME
    rumprun_install_bottomlevel
)

# Add toplevel and bottomlevel libraries.  The IMPORTED_LOCATION property is important because it forces the downstream dependencies
# to be rebuilt if any of the rumprun commands have been run.  This is also why these are libraries and not custom targets.
add_library(rumprun_toplevel_support STATIC IMPORTED GLOBAL)
add_dependencies(rumprun_toplevel_support rumprun_install_toplevel)
get_target_property(stamp rumprun_install_toplevel STAMP_FILE)
set_property(TARGET rumprun_toplevel_support PROPERTY IMPORTED_LOCATION "${stamp}")

add_library(rumprun_bottomlevel_support STATIC IMPORTED GLOBAL)
add_dependencies(rumprun_bottomlevel_support rumprun_install_bottomlevel rumprun_toplevel_support)
get_target_property(stamp rumprun_install_bottomlevel STAMP_FILE)
set_property(TARGET rumprun_bottomlevel_support PROPERTY IMPORTED_LOCATION "${stamp}")

# Add interface library to make seL4 rumprun headers available to loader apps
add_library(rumprun INTERFACE)
add_dependencies(rumprun rumprun_bottomlevel_support)
target_include_directories(rumprun INTERFACE "platform/sel4/include/sel4")

# TODO: Figure out way to better handle target properties for install locations.  We currently hard code them as it would be too
# convoluted to pull them out of the generated config files and use in generator expressions.
set_property(
    TARGET rumprun_toplevel_support
    PROPERTY RUMPRUN_TOOLCHAIN_PATH "${CMAKE_CURRENT_BINARY_DIR}/${rumprun_sel4_arch}/rumprun/bin"
)
set_property(
    TARGET rumprun_toplevel_support
    PROPERTY
        RUMPRUN_TOOLCHAIN_CMAKE
        "${CMAKE_CURRENT_BINARY_DIR}/${rumprun_sel4_arch}/rumprun/rumprun-${rumprun_arch}/share/${rumprun_tuple}-toolchain.cmake"
)
set_property(TARGET rumprun_toplevel_support PROPERTY RUMPRUN_TOOLCHAIN_TUPLE "${rumprun_tuple}")
set_property(
    TARGET rumprun_bottomlevel_support
    PROPERTY RUMPRUN_BASEDIR "${CMAKE_CURRENT_BINARY_DIR}/${rumprun_sel4_arch}/sel4-obj"
)

set(
    RUMPRUN_TOOLS_DIR "${CMAKE_CURRENT_BINARY_DIR}/${rumprun_sel4_arch}/sel4-obj/rumptools/bin"
    CACHE INTERNAL ""
    FORCE
)
set(
    RUMPRUN_BASEDIR "${CMAKE_CURRENT_BINARY_DIR}/${rumprun_sel4_arch}/sel4-obj"
    CACHE INTERNAL ""
    FORCE
)
set(RUMPRUN_TOOLS_PREFIX "${rumprun_tool_prefix}" CACHE INTERNAL "" FORCE)
set(
    RUMPRUN_TOOLCHAIN_PATH "${CMAKE_CURRENT_BINARY_DIR}/${rumprun_sel4_arch}/rumprun/bin"
    CACHE INTERNAL ""
    FORCE
)

add_library(rumprun_intermediate_file STATIC IMPORTED GLOBAL)
target_link_libraries(
    rumprun_intermediate_file
    INTERFACE
        rumprun
        ${RUMPRUN_SEL4LIBS}
        rumprun_Config
        sel4_autoconf
)
set_property(
    TARGET rumprun_intermediate_file
    PROPERTY
        IMPORTED_LOCATION
        "${CMAKE_CURRENT_BINARY_DIR}/${rumprun_sel4_arch}/sel4-obj/rumprun-intermediate.o"
)
add_dependencies(rumprun_intermediate_file platformobj)
