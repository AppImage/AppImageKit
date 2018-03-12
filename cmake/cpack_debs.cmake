# required for DEB-DEFAULT to work as intended
cmake_minimum_required(VERSION 3.6)

set(PROJECT_VERSION 1.0)
set(CPACK_GENERATOR "DEB")

set(CPACK_DEBIAN_PACKAGE_VERSION ${PROJECT_VERSION})
file(READ ${PROJECT_BINARY_DIR}/GIT_COMMIT GIT_COMMIT)
set(CPACK_DEBIAN_PACKAGE_RELEASE "git-${GIT_COMMIT}")

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "AppImage Team")
set(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://appimage.org/")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

set(CPACK_DEBIAN_LIBAPPIMAGE_PACKAGE_NAME "libappimage")
set(CPACK_DEBIAN_LIBAPPIMAGE_PACKAGE_DEPENDS "libarchive13, libc6 (>= 2.4), libglib2.0-0, zlib1g, fuse")
set(CPACK_DEBIAN_LIBAPPIMAGE_PACKAGE_DESCRIPTION "Utilities collection to handle AppImage files.")

set(CPACK_DEBIAN_LIBAPPIMAGE-DEV_PACKAGE_NAME "libappimage-dev")
set(CPACK_DEBIAN_LIBAPPIMAGE-DEV_PACKAGE_DEPENDS "libappimage")
set(CPACK_DEBIAN_LIBAPPIMAGE-DEV_PACKAGE_DESCRIPTION "Utilities collection to handle AppImage files.")

set(CPACK_DEBIAN_APPIMAGED_PACKAGE_NAME "appimaged")
set(CPACK_COMPONENT_APPIMAGED_DESCRIPTION
    "Optional AppImage daemon for desktop integration.\n  Integrates AppImages into the desktop, e.g., installs icons and menu entries.")

set(CPACK_DEBIAN_APPIMAGED_PACKAGE_DEPENDS "libarchive13, libc6 (>= 2.4), libglib2.0-0, zlib1g, fuse")

set(CPACK_COMPONENTS_ALL appimaged libappimage libappimage-dev)
set(CPACK_DEB_COMPONENT_INSTALL ON)

include(CPack)
