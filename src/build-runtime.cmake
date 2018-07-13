set(APPIMAGEKIT_RUNTIME_ENABLE_SETPROCTITLE OFF CACHE BOOL "Useful for $TARGET_APPIMAGE; see issue #763")

# if set to anything but ON, the magic bytes won't be embedded
# CAUTION: the magic bytes are a hard requirement for type 2 AppImages! This option should NEVER be used unless you are
# 100% sure what you are doing here!
set(APPIMAGEKIT_EMBED_MAGIC_BYTES ON CACHE BOOL "")
# mark as advanced so it won't show up in CMake GUIs etc., to prevent users from accidentally using this option
mark_as_advanced(APPIMAGEKIT_EMBED_MAGIC_BYTES)

set(runtime_cflags -Os -ffunction-sections -fdata-sections -DGIT_COMMIT=\"${GIT_COMMIT}\" -I${xz_INCLUDE_DIRS} -I${squashfuse_INCLUDE_DIRS} -I${PROJECT_SOURCE_DIR}/include)
set(runtime_ldflags -s -Wl,--gc-sections)

if(APPIMAGEKIT_RUNTIME_ENABLE_SETPROCTITLE)
    set(runtime_cflags "${runtime_cflags} -DENABLE_SETPROCTITLE")
endif()

# objcopy requires actual files for creating new sections to populate the new section
# therefore, we generate 3 suitable files containing blank bytes in the right sizes
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/16_blank_bytes
    COMMAND printf '\\0%.0s' {0..15} > ${CMAKE_CURRENT_BINARY_DIR}/16_blank_bytes
)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/1024_blank_bytes
    COMMAND printf '\\0%.0s' {0..1023} > ${CMAKE_CURRENT_BINARY_DIR}/1024_blank_bytes
)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/8192_blank_bytes
    COMMAND printf '\\0%.0s' {0..8191} > ${CMAKE_CURRENT_BINARY_DIR}/8192_blank_bytes
)

# compile first raw object (not linked yet) into which the sections will be embedded
# TODO: find out how this .o object can be generated using a normal add_executable call
# that'd allow us to get rid of the -I parameters in runtime_cflags
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/runtime.0.o
    COMMAND ${CMAKE_C_COMPILER} ${runtime_cflags} -c ${CMAKE_CURRENT_SOURCE_DIR}/runtime.c -o runtime.0.o
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# embed the sections, one by one
# TODO: find out whether all the sections can be embedded in a single objcopy call
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/runtime.1.o
    COMMAND objcopy --add-section .digest_md5=16_blank_bytes --set-section-flags .digest_md5=noload,readonly runtime.0.o runtime.1.o
    MAIN_DEPENDENCY ${CMAKE_CURRENT_BINARY_DIR}/runtime.0.o
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/16_blank_bytes
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/runtime.2.o
    COMMAND objcopy --add-section .upd_info=1024_blank_bytes --set-section-flags .digest_md5=noload,readonly runtime.1.o runtime.2.o
    MAIN_DEPENDENCY ${CMAKE_CURRENT_BINARY_DIR}/runtime.1.o
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/1024_blank_bytes
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/runtime.3.o
    COMMAND objcopy --add-section .sha256_sig=1024_blank_bytes --set-section-flags .digest_md5=noload,readonly runtime.2.o runtime.3.o
    MAIN_DEPENDENCY ${CMAKE_CURRENT_BINARY_DIR}/runtime.2.o
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/1024_blank_bytes
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/runtime.4.o
    COMMAND objcopy --add-section .sig_key=8192_blank_bytes --set-section-flags .digest_md5=noload,readonly runtime.3.o runtime.4.o
    MAIN_DEPENDENCY ${CMAKE_CURRENT_BINARY_DIR}/runtime.3.o
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/8192_blank_bytes
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# add the runtime as a normal executable
# CLion will recognize it as a normal executable, one can simply step into the code
add_executable(runtime ${CMAKE_CURRENT_BINARY_DIR}/runtime.4.o elf.c notify.c getsection.c)
# CMake gets confused by the .o object, therefore we need to tell it that it shall link everything using the C compiler
set_property(TARGET runtime PROPERTY LINKER_LANGUAGE C)
target_link_libraries(runtime PRIVATE squashfuse dl xz libzlib pthread)

# embed the magic bytes after the runtime's build has finished
if(APPIMAGEKIT_EMBED_MAGIC_BYTES)
    add_custom_command(
        TARGET runtime
        POST_BUILD
        COMMAND printf '\\x41\\x49\\x02' | dd of=runtime bs=1 seek=8 count=3 conv=notrunc
    )
endif()

# required for embedding in appimagetool
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/runtime_embed.o
    COMMAND xxd -i runtime | ${CMAKE_C_COMPILER} -c -x c - -o runtime_embed.o
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    MAIN_DEPENDENCY runtime
)
