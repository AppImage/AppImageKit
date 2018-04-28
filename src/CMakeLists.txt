find_package(Sanitizers)

# set up build script

# if set to anything but ON, the magic bytes won't be embedded
# CAUTION: the magic bytes are a hard requirement for type 2 AppImages! This option should NEVER be used unless you are
# 100% sure what you are doing here!
set(APPIMAGEKIT_EMBED_MAGIC_BYTES ON CACHE BOOL "")
# mark as advanced so it won't show up in CMake GUIs etc., to prevent users from accidentally using this option
mark_as_advanced(APPIMAGEKIT_EMBED_MAGIC_BYTES)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/build-runtime.sh.in
    ${CMAKE_CURRENT_BINARY_DIR}/build-runtime.sh
    @ONLY
)

add_custom_command(OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/runtime
    COMMAND bash ${CMAKE_CURRENT_BINARY_DIR}/build-runtime.sh
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/build-runtime.sh
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/runtime.c
    DEPENDS squashfuse
)

add_custom_command(OUTPUT data.o
    COMMAND ld -r -b binary -o data.o runtime
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/runtime
)

add_custom_target(runtime DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/runtime)


# include XDG basedir library
add_subdirectory(xdg-basedir)


# allow setting different path for mksquashfs after installation
set(AUXILIARY_FILES_DESTINATION "lib/appimagekit" CACHE STRING "Target install directory for mksquashfs")


add_library(libappimage SHARED
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
    squashfuse
    libarchive
    inotify-tools
    xz
    xdg-basedir
    PUBLIC
    pthread
    libglib
    libgio
    libzlib
    libcairo
)


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
    squashfuse
    libarchive
    inotify-tools
    xz
    xdg-basedir
    PUBLIC
    pthread
    libglib
    libgio
    libzlib
    libcairo
)


add_executable(appimagetool
    appimagetool.c
    getsection.c
    binreloc.c
    elf.c
    data.o
)

target_link_libraries(appimagetool
    libglib
    libzlib
    squashfuse
)

target_link_libraries(appimagetool
    ${CMAKE_DL_LIBS}
    pthread
    libzlib
    xz
    xdg-basedir
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

add_sanitizers(appimagetool)


add_executable(appimaged appimaged.c notify.c elf.c getsection.c)

target_link_libraries(appimaged
    pthread
    ${CMAKE_DL_LIBS}
    libcairo
    libglib
    libgio
    inotify-tools
    squashfuse
    xz
    libzlib
    libarchive
    xdg-basedir
)

target_compile_definitions(appimaged
    PRIVATE -DGIT_COMMIT="${GIT_COMMIT}"
    PRIVATE -DBUILD_NUMBER="${BUILD_NUMBER}"
    PRIVATE -DBUILD_DATE="${DATE}"
    PRIVATE -D_FILE_OFFSET_BITS=64
    PRIVATE -DHAVE_LIBARCHIVE3=0
    PRIVATE -DSTANDALONE=1
    PRIVATE -DAPPIMAGED=1
)
# TODO: remove -DAPPIMAGED=1, move these code snippets to appimaged's source code files

target_include_directories(appimaged
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
    INTERFACE $<INSTALL_INTERFACE:include/>
)

add_sanitizers(appimaged)


add_executable(AppRun AppRun.c)

add_sanitizers(AppRun)


add_executable(validate validate.c getsection.c)

target_link_libraries(validate
    libglib
    libopenssl
)


add_executable(digest digest.c getsection.c)

target_link_libraries(digest
    libopenssl
)

add_sanitizers(digest)


add_executable(runtime-debug EXCLUDE_FROM_ALL elf.c notify.c getsection.c ${CMAKE_CURRENT_SOURCE_DIR}/runtime.c)

target_link_libraries(runtime-debug
    squashfuse
    libarchive
    xz
    xdg-basedir
    pthread
    libglib
    libgio
    libzlib
    libcairo
    ${CMAKE_DL_LIBS}
    bsd
)

set_target_properties(runtime-debug PROPERTIES LINK_FLAGS "-ffunction-sections -fdata-sections -s -Wl,--gc-sections")

target_compile_options(runtime-debug
    PUBLIC -DGIT_COMMIT='${GIT_COMMIT}' -D_FILE_OFFSET_BITS=64
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
    TARGETS appimaged
    RUNTIME DESTINATION bin COMPONENT appimaged
)

install(
    FILES ${PROJECT_SOURCE_DIR}/resources/appimaged.service
    DESTINATION lib/systemd/user/
    COMPONENT appimaged
)
install(
    TARGETS AppRun appimagetool digest validate
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
