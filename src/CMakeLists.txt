find_package(Sanitizers)

find_package(PkgConfig)
pkg_check_modules (libelf REQUIRED IMPORTED_TARGET libelf)

if(NOT COMMAND add_sanitizers)
    message(WARNING "Sanitizers CMake module not found, sanitizer support is not available")
    # defining a no-op function to avoid CMake errors
    function(add_sanitizers ...)
    endfunction()
endif()


# allow setting different path for mksquashfs after installation
set(AUXILIARY_FILES_DESTINATION "lib/appimagekit" CACHE STRING "Target install directory for mksquashfs")


# the runtime build configuration is quite long, therefore it's managed in a separate CMake script
include(build-runtime.cmake)


add_executable(appimagetool
    appimagetool.c
    binreloc.c
    runtime_embed.o
)

target_include_directories(appimagetool
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
    INTERFACE $<INSTALL_INTERFACE:include/>
)

# trick: list libraries on which imported static ones depend on in the PUBLIC section
# CMake then adds them after the PRIVATE ones in the linker command
target_link_libraries(appimagetool
    ${CMAKE_DL_LIBS}
    squashfuse
    xdg-basedir
    libappimage_shared
    libglib
    libzlib
    xz
    PkgConfig::libelf
)

target_compile_definitions(appimagetool
    PRIVATE -D_FILE_OFFSET_BITS=64
    PRIVATE -DGIT_COMMIT="${GIT_COMMIT}"
    PRIVATE -DBUILD_NUMBER="${BUILD_NUMBER}"
    PRIVATE -DBUILD_DATE="${DATE}"
    PRIVATE -DENABLE_BINRELOC
)

target_include_directories(appimagetool
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
    INTERFACE $<INSTALL_INTERFACE:include/>
)

if(AUXILIARY_FILES_DESTINATION)
    message(STATUS "Installing auxiliary files in path: ${AUXILIARY_FILES_DESTINATION}")
    target_compile_definitions(appimagetool
        PRIVATE -DAUXILIARY_FILES_DESTINATION="${AUXILIARY_FILES_DESTINATION}"
    )
endif()

add_sanitizers(appimagetool)


add_executable(AppRun AppRun.c)

target_compile_definitions(AppRun
    PRIVATE -D_FILE_OFFSET_BITS=64
    PRIVATE -DGIT_COMMIT="${GIT_COMMIT}"
    PRIVATE -DENABLE_BINRELOC
)

add_sanitizers(AppRun)


add_executable(validate validate.c)

target_link_libraries(validate
    libappimage_shared
    libglib
    libssl
)

target_include_directories(validate
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
    INTERFACE $<INSTALL_INTERFACE:include/
)


add_executable(digest digest.c)

target_link_libraries(digest
    libappimage_shared
    libssl
)

add_sanitizers(digest)


# digest_md5 demo application
# can be used to verify AppImages' MD5 digest
add_executable(digest_md5 EXCLUDE_FROM_ALL digest_md5.c)

target_include_directories(digest_md5
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
    INTERFACE $<INSTALL_INTERFACE:include/>
)

target_link_libraries(digest_md5
    PRIVATE libglib
    PRIVATE libappimage_shared
)


# install binaries
if(AUXILIARY_FILES_DESTINATION)
    install(
        PROGRAMS ${mksquashfs_INSTALL_DIR}/mksquashfs ${CMAKE_CURRENT_BINARY_DIR}/runtime
        DESTINATION ${AUXILIARY_FILES_DESTINATION}
        COMPONENT applications
    )
else()
    install(
        PROGRAMS ${mksquashfs_INSTALL_DIR}/mksquashfs ${CMAKE_CURRENT_BINARY_DIR}/runtime
        DESTINATION bin
        COMPONENT applications
    )
endif()

install(
    TARGETS AppRun appimagetool digest validate
    RUNTIME DESTINATION bin COMPONENT applications
    LIBRARY DESTINATION lib COMPONENT applications
    ARCHIVE DESTINATION lib/static COMPONENT applications
    INCLUDES DESTINATION include COMPONENT applications
)
