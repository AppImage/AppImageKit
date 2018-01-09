find_package(Sanitizers)

# set up build script
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/build-runtime.sh.in
    ${CMAKE_CURRENT_BINARY_DIR}/build-runtime.sh
    @ONLY
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/runtime
    COMMAND bash ${CMAKE_CURRENT_BINARY_DIR}/build-runtime.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/runtime.c
    DEPENDS squashfuse
)

add_custom_command(OUTPUT data.o
    COMMAND ld -r -b binary -o data.o runtime
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/runtime
)

add_custom_target(runtime DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/runtime)


# allow setting different path for mksquashfs after installation
set(MKSQUASHFS_DIR "" CACHE STRING "Change target install directory for mksquashfs")

add_library(libappimage
    ${PROJECT_SOURCE_DIR}/include/appimage/libappimage.h
    shared.c
    getsection.c
    notify.c
    elf.c
)

set_target_properties(libappimage PROPERTIES PREFIX "")

target_compile_definitions(libappimage
    PRIVATE -D_FILE_OFFSET_BITS=64
    PRIVATE -DVERSION_NUMBER="${GIT_VERSION}"
    PRIVATE -DENABLE_BINRELOC
)

target_include_directories(libappimage PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(libappimage
    pthread
    ${squashfuse_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${libarchive_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${inotify-tools_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${GIO_LIBRARIES}
    ${CAIRO_LIBRARIES}
    ${xz_LIBRARIES}
)

add_dependencies(libappimage squashfuse)

if(NOT USE_SYSTEM_LIBARCHIVE)
    add_dependencies(libappimage libarchive)
endif()

if(NOT USE_SYSTEM_INOTIFY_TOOLS)
    add_dependencies(libappimage inotify-tools)
endif()


add_executable(appimagetool
    appimagetool.c
    getsection.c
    binreloc.c
    elf.c
    data.o
)

target_link_libraries(appimagetool
    ${GLIB_LIBRARIES}
    ${squashfuse_LIBRARIES}
    ${ZLIB_LIBRARIES}
)

target_link_libraries(appimagetool
    dl
    pthread
    ${ZLIB_LIBRARIES}
    ${xz_LIBRARIES}
)

target_compile_definitions(appimagetool
    PRIVATE -D_FILE_OFFSET_BITS=64
    PRIVATE -DGIT_COMMIT="${GIT_COMMIT}"
    PRIVATE -DBUILD_NUMBER="${BUILD_NUMBER}"
    PRIVATE -DBUILD_DATE="${DATE}"
    PRIVATE -DENABLE_BINRELOC
)

if(MKSQUASHFS_DIR)
    message(STATUS "Installing mksquashfs in path: ${MKSQUASHFS_DIR}")
    target_compile_definitions(appimagetool
        PRIVATE -DMKSQUASHFS_PATH="${MKSQUASHFS_DIR}/mksquashfs"
    )
endif()

add_dependencies(appimagetool mksquashfs squashfuse)

add_sanitizers(appimagetool)


add_executable(appimaged appimaged.c notify.c elf.c getsection.c)

target_link_libraries(appimaged
    pthread
    dl
    ${CAIRO_LIBRARIES}
    ${GLIB_LIBRARIES}
    ${GIO_LIBRARIES}
    ${inotify-tools_LIBRARIES}
    ${squashfuse_LIBRARIES}
    ${xz_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${libarchive_LIBRARIES}
)

target_compile_definitions(appimaged
    PRIVATE -DGIT_COMMIT="${GIT_COMMIT}"
    PRIVATE -DVERSION_NUMBER="${GIT_COMMIT}"  # legacy variable
    PRIVATE -DBUILD_NUMBER="${BUILD_NUMBER}"
    PRIVATE -DBUILD_DATE="${DATE}"
    PRIVATE -D_FILE_OFFSET_BITS=64
    PRIVATE -DHAVE_LIBARCHIVE3=0
)

add_dependencies(appimaged squashfuse)

if(NOT USE_SYSTEM_INOTIFY_TOOLS)
    add_dependencies(appimaged inotify-tools)
endif()
if(NOT USE_SYSTEM_LIBARCHIVE)
    add_dependencies(appimaged libarchive)
endif()

add_sanitizers(appimaged)


add_executable(AppRun AppRun.c)

add_sanitizers(AppRun)


add_executable(validate validate.c getsection.c)

target_link_libraries(validate
    ${GLIB_LIBRARIES}
    ${OPENSSL_LIBRARIES}
)


add_executable(digest digest.c getsection.c)

target_link_libraries(digest
    ${OPENSSL_LIBRARIES}
)

add_sanitizers(digest)


# install binaries
install(
    PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/runtime
    DESTINATION bin
)

if(MKSQUASHFS_DIR)
    install(
        PROGRAMS ${mksquashfs_INSTALL_DIR}/mksquashfs
        DESTINATION ${MKSQUASHFS_DIR}
    )
else()
    install(
        PROGRAMS ${mksquashfs_INSTALL_DIR}/mksquashfs
        DESTINATION bin
    )
endif()

install(
    TARGETS AppRun appimaged appimagetool digest validate
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
    INCLUDES DESTINATION include
)


# install libappimage
install(TARGETS libappimage
    EXPORT AppImageKit
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
    PUBLIC_HEADER DESTINATION include
)

# Add all targets to the build-tree export set
export(
    TARGETS libappimage
    FILE "${PROJECT_BINARY_DIR}/AppImageTargets.cmake"
)

# Export the package for use from the build-tree
# (this registers the build-tree with a global CMake-registry)
export(PACKAGE AppImageKit)

# Create the AppImageConfig.cmake and AppImageConfigVersion files
configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/AppImageKitConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/AppImageKitConfig.cmake"
    @ONLY
)
# ... for both
configure_file(
    "${PROJECT_SOURCE_DIR}/cmake/AppImageKitConfigVersion.cmake.in"
    "${PROJECT_BINARY_DIR}/AppImageKitConfigVersion.cmake"
    @ONLY
)

# Install the AppImageConfig.cmake and AppImageConfigVersion.cmake
install(FILES
    "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/AppImageKitConfig.cmake"
    "${PROJECT_BINARY_DIR}/AppImageKitConfigVersion.cmake"
    DESTINATION "/lib/CMake/AppImage"
    COMPONENT dev
)

# Install the export set for use with the install-tree
install(
    EXPORT AppImageKit
    DESTINATION "/lib/CMake/AppImage"
    COMPONENT dev
)
