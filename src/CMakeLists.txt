find_package(Sanitizers)

if(NOT COMMAND add_sanitizers)
    message(WARNING "Sanitizers CMake module not found, sanitizer support is not available")
    # defining a no-op function to avoid CMake errors
    function(add_sanitizers ...)
    endfunction()
endif()


# include XDG basedir library
add_subdirectory(xdg-basedir)


# allow setting different path for mksquashfs after installation
set(AUXILIARY_FILES_DESTINATION "lib/appimagekit" CACHE STRING "Target install directory for mksquashfs")


add_library(libappimage SHARED
    ${PROJECT_SOURCE_DIR}/include/appimage/appimage.h
    shared.c
    getsection.c
    notify.c
    elf.c
    appimagetool_shared.c
    hexlify.c
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
    libgobject
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
    appimagetool_shared.c
    hexlify.c
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
    libgobject
    libgio
    libzlib
    libcairo
)


include(build-runtime.cmake)


add_executable(appimagetool
    ${PROJECT_SOURCE_DIR}/include/appimage/appimage.h
    appimagetool.c
    getsection.c
    binreloc.c
    elf.c
    runtime_embed.o
    appimagetool_shared.c
)

target_include_directories(appimagetool
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
    INTERFACE $<INSTALL_INTERFACE:include/>
)

# trick: list libraries on which imported static ones depend on in the PUBLIC section
# CMake then adds them after the PRIVATE ones in the linker command
target_link_libraries(appimagetool
    PRIVATE
    ${CMAKE_DL_LIBS}
    squashfuse
    xz
    xdg-basedir
    PUBLIC
    pthread
    libglib
    libgobject
    libzlib
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


add_executable(validate validate.c getsection.c)

target_link_libraries(validate
    libglib
    libopenssl
)

target_include_directories(validate
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
    INTERFACE $<INSTALL_INTERFACE:include/
)


add_executable(digest digest.c getsection.c)

target_link_libraries(digest
    libopenssl
)

add_sanitizers(digest)


# digest_md5 demo application
# can be used to verify AppImages' MD5 digest
add_executable(digest_md5 EXCLUDE_FROM_ALL digest_md5.c getsection.c appimagetool_shared.c)

target_include_directories(digest_md5
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include/>
    INTERFACE $<INSTALL_INTERFACE:include/>
)

target_link_libraries(digest_md5
    PRIVATE
    libglib
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
