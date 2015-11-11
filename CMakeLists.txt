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


ADD_SUBDIRECTORY(third-party-dependencies)
set(binary-dependencies_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/binary-dependencies/")
set(third-party-dependencies_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/third-party-dependencies/")


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


set(third-party-dependencies_clean
  "third-party-dependencies/build/;"
  "third-party-dependencies/install/;"
)

EXECUTE_PROCESS(COMMAND uname -m COMMAND tr -d '\n' OUTPUT_VARIABLE ARCHITECTURE)
ADD_CUSTOM_TARGET(AppImageAssistant ALL DEPENDS runtime xorriso)
ADD_CUSTOM_COMMAND(TARGET AppImageAssistant
  WORKING_DIRECTORY "AppImageAssistant.AppDir"
  COMMAND install ../runtime .
  COMMAND ../installBinary.sh ${third-party-dependencies_PREFIX}/install/bin/xorriso ${third-party-dependencies_PREFIX}/install/lib/ .
  COMMAND mkdir -p usr/lib/ usr/share/pyshared/
  COMMAND install -t usr/lib/ ${binary-dependencies_PREFIX}/${ARCHITECTURE}/libglade-2.0.so.0
  COMMAND install -t usr/lib/ ${binary-dependencies_PREFIX}/${ARCHITECTURE}/libvte.so.9
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
  COMMAND ../installBinary.sh ${third-party-dependencies_PREFIX}/install/bin/xorriso ${third-party-dependencies_PREFIX}/install/lib/ .
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
  COMMAND ../installBinary.sh ${third-party-dependencies_PREFIX}/install/bin/xorriso ${third-party-dependencies_PREFIX}/install/lib/ .
  COMMAND ../installBinary.sh ${third-party-dependencies_PREFIX}/install/bin/inotifywait ${third-party-dependencies_PREFIX}/install/lib/ .
  COMMAND ../installBinary.sh ${third-party-dependencies_PREFIX}/install/bin/update-desktop-database ${third-party-dependencies_PREFIX}/install/lib/ .
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
