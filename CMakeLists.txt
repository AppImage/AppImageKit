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

INCLUDE_DIRECTORIES(/usr/include/glib-2.0/ /usr/lib/glib-2.0/include/ .)

ADD_EXECUTABLE(AppRun AppRun.c)
add_custom_command(TARGET AppRun POST_BUILD COMMAND ${CMAKE_STRIP} AppRun)

ADD_LIBRARY(fuseiso fuseiso.c)
ADD_LIBRARY(isofs isofs.c)

ADD_EXECUTABLE(runtime runtime.c)
TARGET_LINK_LIBRARIES(runtime fuseiso isofs ${LIBFUSE} ${LIBPTHREAD} ${LIBGLIB2} ${LIBZ})
add_custom_command(TARGET runtime POST_BUILD COMMAND ${CMAKE_STRIP} runtime)

ADD_CUSTOM_TARGET(AppImageAssistant ALL DEPENDS runtime)
ADD_CUSTOM_COMMAND(TARGET AppImageAssistant
  COMMAND cp runtime AppImageAssistant.AppDir/
  COMMAND ./binary-dependencies/bundle
  COMMAND ./AppImageAssistant.AppDir/package ./AppImageAssistant.AppDir ./AppImageAssistant
  VERBATIM
)

ADD_CUSTOM_TARGET(AppImageExtract ALL DEPENDS AppRun AppImageAssistant)
ADD_CUSTOM_COMMAND(TARGET AppImageExtract
  COMMAND cp AppRun ./AppImageExtract.AppDir
  COMMAND ./binary-dependencies/bundle
  COMMAND ./AppImageAssistant ./AppImageExtract.AppDir ./AppImageExtract
  VERBATIM
)

SET_DIRECTORY_PROPERTIES(PROPERTIES   
 ADDITIONAL_MAKE_CLEAN_FILES "AppImageAssistant;AppImageExtract;./AppImageAssistant.AppDir/runtime;./AppImageAssistant.AppDir/usr/share/pyshared/vte.so;./AppImageAssistant.AppDir/usr/lib/;./AppImageExtract.AppDir/AppRun;./AppImageExtract.AppDir/usr/lib/;./AppImageAssistant.AppDir/usr/bin/xorriso;./AppImageExtract.AppDir/usr/bin/xorriso") 
