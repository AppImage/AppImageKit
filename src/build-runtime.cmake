set(APPIMAGEKIT_RUNTIME_ENABLE_SETPROCTITLE OFF CACHE BOOL "Useful for $TARGET_APPIMAGE; see issue #763")

# if set to anything but ON, the magic bytes won't be embedded
# CAUTION: the magic bytes are a hard requirement for type 2 AppImages! This option should NEVER be used unless you are
# 100% sure what you are doing here!
set(APPIMAGEKIT_EMBED_MAGIC_BYTES ON CACHE BOOL "")
# mark as advanced so it won't show up in CMake GUIs etc., to prevent users from accidentally using this option
mark_as_advanced(APPIMAGEKIT_EMBED_MAGIC_BYTES)

# check type of current build
string(TOUPPER "${CMAKE_BUILD_TYPE}" BUILD_TYPE_UPPER)
if (BUILD_TYPE_UPPER STREQUAL DEBUG)
    set(BUILD_DEBUG TRUE)
else()
    set(BUILD_DEBUG FALSE)
endif()

if (NOT TARGET libsquashfuse)
    message(FATAL_ERROR "TARGET NOT found libsquashfuse")
else()
    get_target_property(squashfuse_INCLUDE_DIRS libsquashfuse INTERFACE_INCLUDE_DIRECTORIES)
endif()

# must not include -flto in the following flags, otherwise the data sections will be stripped out
set(runtime_cflags
    -std=c99 -ffunction-sections -fdata-sections
    -DGIT_COMMIT=\\"${GIT_COMMIT}\\"
    -I${squashfuse_INCLUDE_DIRS}
    -I${PROJECT_SOURCE_DIR}/include
    -I${PROJECT_SOURCE_DIR}/lib/libappimage/include
    -I${PROJECT_SOURCE_DIR}/lib/libappimage/src/libappimage_hashlib/include
    ${DEPENDENCIES_CFLAGS}
)
# must not include -Wl,--gc-sections in the following flags, otherwise the data sections will be stripped out
set(runtime_ldflags -s -ffunction-sections -fdata-sections -flto ${DEPENDENCIES_LDFLAGS})

if(BUILD_DEBUG)
    message(WARNING "Debug build, adding debug information")
    set(runtime_cflags -g ${runtime_cflags})
else()
    message(STATUS "Release build, optimizing runtime")
    set(runtime_cflags -Os ${runtime_cflags})
endif()

if(NOT xz_INCLUDE_DIRS STREQUAL "")
    list(APPEND runtime_cflags -I${xz_INCLUDE_DIRS})
endif()

if(APPIMAGEKIT_RUNTIME_ENABLE_SETPROCTITLE)
    set(runtime_cflags ${runtime_cflags} -DENABLE_SETPROCTITLE)
endif()

# objcopy requires actual files for creating new sections to populate the new section
# therefore, we generate 3 suitable files containing blank bytes in the right sizes
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/16_blank_bytes
    COMMAND dd if=/dev/zero bs=1 count=16 of=${CMAKE_CURRENT_BINARY_DIR}/16_blank_bytes
)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/1024_blank_bytes
    COMMAND dd if=/dev/zero bs=1 count=1024 of=${CMAKE_CURRENT_BINARY_DIR}/1024_blank_bytes
)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/8192_blank_bytes
    COMMAND dd if=/dev/zero bs=1 count=8192 of=${CMAKE_CURRENT_BINARY_DIR}/8192_blank_bytes
)

# compile first raw object (not linked yet) into which the sections will be embedded
# TODO: find out how this .o object can be generated using a normal add_executable call
# that'd allow us to get rid of the -I parameters in runtime_cflags
add_custom_command(
    MAIN_DEPENDENCY runtime.c
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/runtime.0.o
    COMMAND ${CMAKE_C_COMPILER} ${runtime_cflags} -c ${CMAKE_CURRENT_SOURCE_DIR}/runtime.c -o runtime.0.o
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# embed the sections, one by one
# TODO: find out whether all the sections can be embedded in a single objcopy call
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/runtime.1.o
    COMMAND ${OBJCOPY} --add-section .digest_md5=16_blank_bytes --set-section-flags .digest_md5=noload,readonly runtime.0.o runtime.1.o
    MAIN_DEPENDENCY ${CMAKE_CURRENT_BINARY_DIR}/runtime.0.o
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/16_blank_bytes
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/runtime.2.o
    COMMAND ${OBJCOPY} --add-section .upd_info=1024_blank_bytes --set-section-flags .upd_info=noload,readonly runtime.1.o runtime.2.o
    MAIN_DEPENDENCY ${CMAKE_CURRENT_BINARY_DIR}/runtime.1.o
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/1024_blank_bytes
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/runtime.3.o
    COMMAND ${OBJCOPY} --add-section .sha256_sig=1024_blank_bytes --set-section-flags .sha256_sig=noload,readonly runtime.2.o runtime.3.o
    MAIN_DEPENDENCY ${CMAKE_CURRENT_BINARY_DIR}/runtime.2.o
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/1024_blank_bytes
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/runtime.4.o
    COMMAND ${OBJCOPY} --add-section .sig_key=8192_blank_bytes --set-section-flags .sig_key=noload,readonly runtime.3.o runtime.4.o
    MAIN_DEPENDENCY ${CMAKE_CURRENT_BINARY_DIR}/runtime.3.o
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/8192_blank_bytes
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# add the runtime as a normal executable
# CLion will recognize it as a normal executable, one can simply step into the code
add_executable(runtime ${CMAKE_CURRENT_BINARY_DIR}/runtime.4.o notify.c)
# CMake gets confused by the .o object, therefore we need to tell it that it shall link everything using the C compiler
set_property(TARGET runtime PROPERTY LINKER_LANGUAGE C)
target_link_libraries(runtime PRIVATE libsquashfuse dl xz libzlib pthread libappimage_shared libappimage_hashlib)
if(COMMAND target_link_options)
    target_link_options(runtime PRIVATE ${runtime_ldflags})
else()
    message(WARNING "CMake version < 3.13, falling back to using target_link_libraries instead of target_link_options")
    target_link_libraries(runtime PRIVATE ${runtime_ldflags})
endif()
target_include_directories(runtime PRIVATE ${PROJECT_SOURCE_DIR}/include)

if(BUILD_DEBUG)
    message(WARNING "Debug build, not stripping runtime to allow debugging using gdb etc.")
else()
    add_custom_command(
        TARGET runtime
        POST_BUILD
        COMMAND ${STRIP} ${CMAKE_CURRENT_BINARY_DIR}/runtime
    )
endif()

# embed the magic bytes after the runtime's build has finished
if(APPIMAGEKIT_EMBED_MAGIC_BYTES)
    add_custom_command(
        TARGET runtime
        POST_BUILD
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/embed-magic-bytes-in-file.sh ${CMAKE_CURRENT_BINARY_DIR}/runtime
    )
endif()

# required for embedding in appimagetool
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/runtime_embed.o
    COMMAND ${XXD} -i runtime | ${CMAKE_C_COMPILER} -c -x c - -o runtime_embed.o
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    MAIN_DEPENDENCY runtime
)
