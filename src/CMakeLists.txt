find_package(Sanitizers)

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
    appimagetool_sign.c
    binreloc.c
    runtime_embed.o
)

target_include_directories(appimagetool
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
    INTERFACE $<INSTALL_INTERFACE:include/>
)

# required for appimagetool's signing
# we're using CMake's functionality directly, since we want to link those statically
find_package(PkgConfig)
pkg_check_modules(libgpgme REQUIRED gpgme IMPORTED_TARGET)
pkg_check_modules(libgcrypt REQUIRED libgcrypt IMPORTED_TARGET)

# trick: list libraries on which imported static ones depend on in the PUBLIC section
# CMake then adds them after the PRIVATE ones in the linker command
target_link_libraries(appimagetool
    ${CMAKE_DL_LIBS}
    libsquashfuse
    xdg-basedir
    libappimage_shared
    libglib
    libgio
    libzlib
    PkgConfig::libgcrypt
    PkgConfig::libgpgme
    xz
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


if (NOT TARGET libssl)
    message(WARNING "Could not find suitable libssl, skipping build of validate and digest")
else()
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
endif()


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
if(NOT USE_SYSTEM_MKSQUASHFS)
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
endif()

set(optional_targets "")
foreach(target validate digest)
    if(TARGET ${target})
        list(APPEND optional_targets ${target})
    endif()
endforeach()

install(
    TARGETS AppRun appimagetool ${optional_targets}
    RUNTIME DESTINATION bin COMPONENT applications
    LIBRARY DESTINATION lib COMPONENT applications
    ARCHIVE DESTINATION lib/static COMPONENT applications
    INCLUDES DESTINATION include COMPONENT applications
)

# Configure appimagetool component
install(
    TARGETS appimagetool AppRun
    RUNTIME DESTINATION bin COMPONENT appimagetool EXCLUDE_FROM_ALL
)

install(FILES "${PROJECT_SOURCE_DIR}/resources/appimagetool.desktop" DESTINATION share/applications COMPONENT appimagetool EXCLUDE_FROM_ALL)
install(FILES "${PROJECT_SOURCE_DIR}/resources/appimagetool.png" DESTINATION share/icons/hicolor/128x128/apps COMPONENT appimagetool EXCLUDE_FROM_ALL)
install(FILES "${PROJECT_SOURCE_DIR}/resources/usr/share/metainfo/appimagetool.appdata.xml" DESTINATION share/metainfo COMPONENT appimagetool EXCLUDE_FROM_ALL)
