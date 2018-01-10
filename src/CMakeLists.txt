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
set(AUXILIARY_FILES_DESTINATION "lib/appimagekit" CACHE STRING "Target install directory for mksquashfs")


add_library(libappimage SHARED
    ${PROJECT_SOURCE_DIR}/include/appimage/appimage.h
    shared.c
    getsection.c
    notify.c
    elf.c
)

set_target_properties(libappimage PROPERTIES PREFIX "")
set_target_properties(libappimage PROPERTIES PUBLIC_HEADER "${PROJECT_SOURCE_DIR}/include/appimage/appimage.h")

target_compile_definitions(libappimage
    PRIVATE -D_FILE_OFFSET_BITS=64
    PRIVATE -DGIT_COMMIT="${GIT_COMMIT}"
    PRIVATE -DENABLE_BINRELOC
)

target_include_directories(libappimage
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
    INTERFACE $<INSTALL_INTERFACE:include/>
)

target_link_libraries(libappimage
    PRIVATE
    ${squashfuse_LIBRARIES}
    ${libarchive_LIBRARIES}
    ${inotify-tools_LIBRARIES}
    ${xz_LIBRARIES}
    PUBLIC
    pthread
    ${GLIB_LIBRARIES}
    ${GIO_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${CAIRO_LIBRARIES}
)

add_dependencies(libappimage squashfuse)

if(NOT USE_SYSTEM_LIBARCHIVE)
    add_dependencies(libappimage libarchive)
endif()

if(NOT USE_SYSTEM_INOTIFY_TOOLS)
    add_dependencies(libappimage inotify-tools)
endif()


add_library(libappimage_static STATIC
    ${PROJECT_SOURCE_DIR}/include/appimage/appimage.h
    shared.c
    getsection.c
    notify.c
    elf.c
)

set_target_properties(libappimage_static PROPERTIES PREFIX "")
set_target_properties(libappimage_static PROPERTIES LIBRARY_OUTPUT_NAME libappimage)
set_target_properties(libappimage_static PROPERTIES PUBLIC_HEADER "${PROJECT_SOURCE_DIR}/include/appimage/appimage.h")

target_compile_definitions(libappimage_static
    PRIVATE -D_FILE_OFFSET_BITS=64
    PRIVATE -DGIT_COMMIT="${GIT_COMMIT}"
    PRIVATE -DENABLE_BINRELOC
)

target_include_directories(libappimage_static
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
    INTERFACE $<INSTALL_INTERFACE:include/>
)

target_link_libraries(libappimage_static
    PRIVATE
    ${squashfuse_LIBRARIES}
    ${libarchive_LIBRARIES}
    ${inotify-tools_LIBRARIES}
    ${xz_LIBRARIES}
    PUBLIC
    pthread
    ${GLIB_LIBRARIES}
    ${GIO_LIBRARIES}
    ${ZLIB_LIBRARIES}
    ${CAIRO_LIBRARIES}
)

add_dependencies(libappimage_static squashfuse)

if(NOT USE_SYSTEM_LIBARCHIVE)
    add_dependencies(libappimage_static libarchive)
endif()

if(NOT USE_SYSTEM_INOTIFY_TOOLS)
    add_dependencies(libappimage_static inotify-tools)
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

if(AUXILIARY_FILES_DESTINATION)
    message(STATUS "Installing auxiliary files in path: ${AUXILIARY_FILES_DESTINATION}")
    target_compile_definitions(appimagetool
        PRIVATE -DAUXILIARY_FILES_DESTINATION="${AUXILIARY_FILES_DESTINATION}"
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
    TARGETS AppRun appimaged appimagetool digest validate
    RUNTIME DESTINATION bin COMPONENT applications
    LIBRARY DESTINATION lib COMPONENT applications
    ARCHIVE DESTINATION lib/static COMPONENT applications
    INCLUDES DESTINATION include COMPONENT applications
)


# install libappimage
install(TARGETS libappimage
    EXPORT AppImageKitTargets
    LIBRARY DESTINATION lib COMPONENT libappimage
    ARCHIVE DESTINATION lib/static COMPONENT libappimage
    PUBLIC_HEADER DESTINATION include/appimage COMPONENT libappimage-dev
)

# Add all targets to the build-tree export set
export(
    TARGETS libappimage
    FILE "${PROJECT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/AppImageKitTargets.cmake"
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
    "${PROJECT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/AppImageKitConfigVersion.cmake"
    @ONLY
)

# Install the AppImageConfig.cmake and AppImageConfigVersion.cmake
install(FILES
    "${PROJECT_BINARY_DIR}${CMAKE_FILES_DIRECTORY}/AppImageKitConfig.cmake"
    "${PROJECT_BINARY_DIR}/${CMAKE_FILES_DIRECTORY}/AppImageKitConfigVersion.cmake"
    DESTINATION "lib/cmake/AppImageKit"
    COMPONENT libappimage-dev
)

# Install the export set for use with the install-tree
install(EXPORT AppImageKitTargets
    DESTINATION "lib/cmake/AppImageKit"
    COMPONENT libappimage-dev
)
