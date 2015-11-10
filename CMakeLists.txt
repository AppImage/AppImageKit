CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

PROJECT(AppImageKit C)

# Set required libraries.
set(LIBFUSE "fuse")
set(LIBPTHREAD "pthread")
set(LIBGLIB2 "glib-2.0")
set(LIBZ "z")

SET(REQUIRED_LIBRARIES ${LIBFUSE} ${LIBPTHREAD})
foreach(LIB ${REQUIRED_LIBRARIES})
  find_library(FOUND${LIB} ${LIB} PATHS "/lib64" "/usr/lib64")
  if (NOT FOUND${LIB})
    message(FATAL_ERROR "The required library '${LIB}' was not found. Please install it on your system first.")
  endif(NOT FOUND${LIB})
endforeach(LIB)

ADD_DEFINITIONS(-g -O2 -D_FILE_OFFSET_BITS=64)

# Begin find glib

if (GLIB_PKG_FOUND)
    find_path(GLIB_INCLUDE_DIR  NAMES glib.h PATH_SUFFIXES glib-2.0
       PATHS
       ${GLIB_PKG_INCLUDE_DIRS}
       /usr/include/glib-2.0
       /usr/include
       /usr/local/include
    )
    find_path(GLIB_CONFIG_INCLUDE_DIR NAMES glibconfig.h PATHS ${GLIB_PKG_LIBDIR} PATH_SUFFIXES glib-2.0/include)

    find_library(GLIB_LIBRARIES NAMES glib-2.0
       PATHS
       ${GLIB_PKG_LIBRARY_DIRS}
       /usr/lib
       /usr/local/lib
    )

else (GLIB_PKG_FOUND)
    # Find Glib even if pkg-config is not working (eg. cross compiling to Windows)
    find_library(GLIB_LIBRARIES NAMES glib-2.0)
    string (REGEX REPLACE "/[^/]*$" "" GLIB_LIBRARIES_DIR ${GLIB_LIBRARIES})

    find_path(GLIB_INCLUDE_DIR NAMES glib.h PATH_SUFFIXES glib-2.0)
    find_path(GLIB_CONFIG_INCLUDE_DIR NAMES glibconfig.h PATHS ${GLIB_LIBRARIES_DIR} PATH_SUFFIXES glib-2.0/include)

endif (GLIB_PKG_FOUND)

if (GLIB_INCLUDE_DIR AND GLIB_CONFIG_INCLUDE_DIR AND GLIB_LIBRARIES)
    set(GLIB_INCLUDE_DIRS ${GLIB_INCLUDE_DIR} ${GLIB_CONFIG_INCLUDE_DIR})
endif (GLIB_INCLUDE_DIR AND GLIB_CONFIG_INCLUDE_DIR AND GLIB_LIBRARIES)

if(GLIB_INCLUDE_DIRS AND GLIB_LIBRARIES)
   set(GLIB_FOUND TRUE CACHE INTERNAL "glib-2.0 found")
   message(STATUS "Found glib-2.0: ${GLIB_INCLUDE_DIR}, ${GLIB_LIBRARIES}")
else(GLIB_INCLUDE_DIRS AND GLIB_LIBRARIES)
   set(GLIB_FOUND FALSE CACHE INTERNAL "glib-2.0 found")
   message(STATUS "glib-2.0 not found.")
endif(GLIB_INCLUDE_DIRS AND GLIB_LIBRARIES)

mark_as_advanced(GLIB_INCLUDE_DIR GLIB_CONFIG_INCLUDE_DIR GLIB_INCLUDE_DIRS GLIB_LIBRARIES)

# End find glib

INCLUDE_DIRECTORIES(. ${GLIB_INCLUDE_DIRS})

ADD_SUBDIRECTORY(LibcWrapGenerator)
set(CMAKE_C_COMPILER "${CMAKE_CURRENT_SOURCE_DIR}/LibcWrapGenerator/cc")


ADD_EXECUTABLE(AppRun AppRun.c)
add_custom_command(TARGET AppRun POST_BUILD COMMAND ${CMAKE_STRIP} AppRun)
add_dependencies(AppRun libcwrap)

ADD_LIBRARY(fuseiso fuseiso.c)
add_dependencies(fuseiso libcwrap)

ADD_LIBRARY(isofs isofs.c)
add_dependencies(isofs libcwrap)

ADD_EXECUTABLE(runtime runtime.c)
TARGET_LINK_LIBRARIES(runtime fuseiso isofs ${LIBFUSE} ${LIBPTHREAD} ${LIBGLIB2} ${LIBZ})
add_custom_command(TARGET runtime POST_BUILD COMMAND ${CMAKE_STRIP} runtime)
add_dependencies(runtime libcwrap)


include(ExternalProject)
set(binary-dependencies_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/binary-dependencies/")
set(third-party-dependencies_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/third-party-dependencies/")

ExternalProject_Add(inotify-tools
  DEPENDS libcwrap
  #GIT_REPOSITORY "git@github.com:rvoicilas/inotify-tools.git"
  #GIT_TAG "v3.14"
  URL "http://github.com/downloads/rvoicilas/inotify-tools/inotify-tools-3.14.tar.gz"
  PREFIX ${third-party-dependencies_PREFIX}
  CONFIGURE_COMMAND test -f autogen.sh && ./autogen.sh || true && ./configure --prefix=${third-party-dependencies_PREFIX}/install
  BUILD_IN_SOURCE 1
)

ExternalProject_Add(desktop-file-utils
  DEPENDS libcwrap
  #GIT_REPOSITORY "git://anongit.freedesktop.org/xdg/desktop-file-utils"
  #GIT_TAG "0.22"
  URL "http://www.freedesktop.org/software/desktop-file-utils/releases/desktop-file-utils-0.22.tar.xz"
  PREFIX ${third-party-dependencies_PREFIX}
  CONFIGURE_COMMAND test -f autogen.sh && ./autogen.sh || true && ./configure --prefix=${third-party-dependencies_PREFIX}/install
  BUILD_IN_SOURCE 1
)

ExternalProject_Add(xdg-utils
  URL "http://portland.freedesktop.org/download/xdg-utils-1.1.1.tar.gz"
  PREFIX ${third-party-dependencies_PREFIX}
  CONFIGURE_COMMAND ./configure --prefix=${third-party-dependencies_PREFIX}/install
  BUILD_IN_SOURCE 1
)

ExternalProject_Add(libburn
  DEPENDS libcwrap
  #SVN_REPOSITORY "http://svn.libburnia-project.org/libburn/trunk"
  URL "http://files.libburnia-project.org/releases/libburn-1.4.0.tar.gz"
  PREFIX ${third-party-dependencies_PREFIX}
  CONFIGURE_COMMAND ./configure --disable-ldconfig-at-install --prefix=${third-party-dependencies_PREFIX}/install
  BUILD_IN_SOURCE 1
)

ExternalProject_Add(libisofs
  DEPENDS libcwrap
  #HG_REPOSITORY "lp:~libburnia-team/libisofs/scdbackup"
  URL "http://files.libburnia-project.org/releases/libisofs-1.4.0.tar.gz"
  PREFIX ${third-party-dependencies_PREFIX}
  CONFIGURE_COMMAND ./configure --disable-ldconfig-at-install --prefix=${third-party-dependencies_PREFIX}/install
  BUILD_IN_SOURCE 1
)

ExternalProject_Add(libattr
  URL "http://download.savannah.gnu.org/releases/attr/attr-2.4.47.src.tar.gz"
  PREFIX ${third-party-dependencies_PREFIX}
  CONFIGURE_COMMAND ./configure --prefix=${third-party-dependencies_PREFIX}/install
  INSTALL_COMMAND make install-lib install-dev
  BUILD_IN_SOURCE 1
)

ExternalProject_Add(libacl
  DEPENDS libcwrap
  DEPENDS libattr
  URL "http://download.savannah.gnu.org/releases/acl/acl-2.2.52.src.tar.gz"
  PREFIX ${third-party-dependencies_PREFIX}
  CONFIGURE_COMMAND CPPFLAGS=-I${third-party-dependencies_PREFIX}/install/include
                    LDFLAGS=-L${third-party-dependencies_PREFIX}/install/lib
                    ./configure --prefix=${third-party-dependencies_PREFIX}/install
  BUILD_COMMAND CPPFLAGS=-I${third-party-dependencies_PREFIX}/install/include
                LDFLAGS=-L${third-party-dependencies_PREFIX}/install/lib
                make
  INSTALL_COMMAND make install-lib install-dev
  BUILD_IN_SOURCE 1
)

ExternalProject_Add(libisoburn
  DEPENDS libcwrap libisofs libburn libacl
  #SVN_REPOSITORY "http://svn.libburnia-project.org/libisoburn/trunk"
  URL "http://files.libburnia-project.org/releases/libisoburn-1.4.0.tar.gz"
  PREFIX ${third-party-dependencies_PREFIX}
  CONFIGURE_COMMAND CPPFLAGS=-I${third-party-dependencies_PREFIX}/install/include
                    LDFLAGS=-L${third-party-dependencies_PREFIX}/install/lib
                    ./configure --disable-ldconfig-at-install --disable-libacl --disable-xattr --disable-libjte --disable-libcdio --disable-libreadline --disable-libedit --prefix=${third-party-dependencies_PREFIX}/install
  BUILD_IN_SOURCE 1
)

# xorriso provided by libburnia
ADD_CUSTOM_TARGET(xorriso DEPENDS libisoburn)

# xorriso provided by GNU xorriso
#ExternalProject_Add(xorriso
#  DEPENDS libcwrap
#  URL "https://www.gnu.org/software/xorriso/xorriso-1.4.0.tar.gz"
#  PREFIX ${third-party-dependencies_PREFIX}
#  CONFIGURE_COMMAND ./configure --prefix=${third-party-dependencies_PREFIX}/install
#  BUILD_IN_SOURCE 1
#)

#ExternalProject_Add(libglade
#  DEPENDS libcwrap
#  URL "http://ftp.acc.umu.se/pub/GNOME/sources/libglade/2.6/libglade-2.6.4.tar.bz2"
#  PREFIX ${third-party-dependencies_PREFIX}
#  CONFIGURE_COMMAND ./configure --prefix=${third-party-dependencies_PREFIX}/install
#  BUILD_IN_SOURCE 1
#)

#ExternalProject_Add(libvte
#  DEPENDS libcwrap
#  URL "http://ftp.gnome.org/pub/GNOME/sources/vte/0.43/vte-0.43.0.tar.xz"
#  PREFIX ${third-party-dependencies_PREFIX}
#  CONFIGURE_COMMAND ./configure --disable-static --enable-introspection --with-gtk=2.0 --disable-gnome-pty-helper --prefix=${third-party-dependencies_PREFIX}/install
#  BUILD_IN_SOURCE 1
#)

set(third-party-dependencies_clean
  "third-party-dependencies/src/;"
  "third-party-dependencies/tmp/;"
  "third-party-dependencies/install/;"
)

EXECUTE_PROCESS(COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE)
ADD_CUSTOM_TARGET(AppImageAssistant ALL DEPENDS runtime xorriso)
ADD_CUSTOM_COMMAND(TARGET AppImageAssistant
  WORKING_DIRECTORY "AppImageAssistant.AppDir"
  COMMAND install ../runtime .
  COMMAND mkdir -p usr/bin/ usr/lib/ usr/share/pyshared/
  COMMAND install -t usr/bin/ ${third-party-dependencies_PREFIX}/install/bin/xorriso
  COMMAND install -t usr/lib/ ${third-party-dependencies_PREFIX}/install/lib/{libisofs.so.6,libisoburn.so.1,libburn.so.4}
  COMMAND install -t usr/lib/ ${binary-dependencies_PREFIX}/${ARCHITECTURE}/{libglade-2.0.so.0,libvte.so.9}
  COMMAND install -t usr/share/pyshared/ ${binary-dependencies_PREFIX}/${ARCHITECTURE}/vte.so
  COMMAND rm -vf ../AppImageAssistant
  COMMAND ../AppImageAssistant.AppDir/package . ../AppImageAssistant
  VERBATIM
)
set(AppImageAssistant_clean
  "AppImageAssistant;"
  "AppImageAssistant.AppDir/runtime;"
  "AppImageAssistant.AppDir/usr/bin/xorriso;"
  "AppImageAssistant.AppDir/usr/lib/;"
  "AppImageAssistant.AppDir/usr/share/pyshared/vte.so;"
)


ADD_CUSTOM_TARGET(AppImageExtract ALL DEPENDS AppImageAssistant AppRun xorriso)
ADD_CUSTOM_COMMAND(TARGET AppImageExtract
  WORKING_DIRECTORY "AppImageExtract.AppDir"
  COMMAND install ../AppRun .
  COMMAND mkdir -p usr/bin/ usr/lib/
  COMMAND install -t usr/bin/ ${third-party-dependencies_PREFIX}/install/bin/xorriso
  COMMAND install -t usr/lib/ ${third-party-dependencies_PREFIX}/install/lib/{libisofs.so.6,libisoburn.so.1,libburn.so.4}
  COMMAND rm -vf ../AppImageExtract
  COMMAND ../AppImageAssistant.AppDir/package . ../AppImageExtract
  VERBATIM
)
set(AppImageExtract_clean
  "AppImageExtract;"
  "AppImageExtract.AppDir/AppRun;"
  "AppImageExtract.AppDir/usr/bin/xorriso;"
  "AppImageExtract.AppDir/usr/lib/;"
)


ADD_CUSTOM_TARGET(AppImageMonitor ALL DEPENDS AppImageAssistant AppRun xorriso inotify-tools desktop-file-utils)
ADD_CUSTOM_COMMAND(TARGET AppImageMonitor
  WORKING_DIRECTORY "AppImageMonitor.AppDir"
  COMMAND install ../AppRun .
  COMMAND mkdir -p usr/bin/ usr/lib/
  COMMAND install -t usr/bin/ ${third-party-dependencies_PREFIX}/install/bin/{xorriso,inotifywait,xdg-desktop-menu,update-desktop-database}
  COMMAND install -t usr/lib/ ${third-party-dependencies_PREFIX}/install/lib/{libisofs.so.6,libisoburn.so.1,libburn.so.4}
  COMMAND rm -vf ../AppImageMonitor
  COMMAND ../AppImageAssistant.AppDir/package . ../AppImageMonitor
  VERBATIM
)
set(AppImageMonitor_clean
  "AppImageMonitor;"
  "AppImageMonitor.AppDir/AppRun;"
  "AppImageMonitor.AppDir/usr/bin/xorriso;"
  "AppImageMonitor.AppDir/usr/bin/inotifywait;"
  "AppImageMonitor.AppDir/usr/bin/update-desktop-database;"
  "AppImageMonitor.AppDir/usr/lib/;"
)

SET_DIRECTORY_PROPERTIES(PROPERTIES
  ADDITIONAL_MAKE_CLEAN_FILES "${AppImageAssistant_clean}${AppImageExtract_clean}${AppImageMonitor_clean}${third-party-dependencies_clean}")
