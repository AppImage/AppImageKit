cmake_minimum_required(VERSION 3.6)

set(PROJECT_VERSION 1.0)
SET(CPACK_GENERATOR "DEB")

set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION}-${GIT_VERSION})

SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "AppImage Team")
SET(CPACK_DEBIAN_PACKAGE_HOMEPAGE "https://appimage.org/")
SET(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/README.md")
SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")


SET(CPACK_DEBIAN_LIBAPPIMAGE_PACKAGE_NAME "libappimage")
SET(CPACK_DEBIAN_LIBAPPIMAGE_PACKAGE_DEPENDS "libarchive13, libc6 (>= 2.4), libglib2.0-0, zlib1g, fuse")
SET(CPACK_DEBIAN_LIBAPPIMAGE_PACKAGE_DESCRIPTION "Utilities collection to handle AppImage files.")

SET(CPACK_DEBIAN_LIBAPPIMAGE-DEV_PACKAGE_NAME "libappimage-dev")
SET(CPACK_DEBIAN_LIBAPPIMAGE-DEV_PACKAGE_DEPENDS "libappimage")
SET(CPACK_DEBIAN_LIBAPPIMAGE-DEV_PACKAGE_DESCRIPTION "Utilities collection to handle AppImage files.")

SET(CPACK_DEBIAN_APPIMAGED_PACKAGE_NAME "appimaged")
SET(CPACK_COMPONENT_APPIMAGED_DESCRIPTION
    "Optional AppImage daemon for desktop integration.\n  Integrates AppImages into the desktop, e.g., installs icons and menu entries.")

SET(CPACK_DEBIAN_APPIMAGED_PACKAGE_DEPENDS "libarchive13, libc6 (>= 2.4), libglib2.0-0, zlib1g, fuse")

SET(CPACK_COMPONENTS_ALL appimaged libappimage libappimage-dev)
SET(CPACK_DEB_COMPONENT_INSTALL ON)
INCLUDE(CPack)