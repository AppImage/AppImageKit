###
# Registering external targets
###
add_library(squashfuse STATIC IMPORTED)
set_property(TARGET squashfuse PROPERTY IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../squashfuse/.libs/libsquashfuse.a")

add_library(fuseprivate STATIC IMPORTED)
set_property(TARGET fuseprivate PROPERTY IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../squashfuse/.libs/libfuseprivate.a")


if( ${STATIC_BUILD} )
    add_library(archive STATIC IMPORTED)
    set_property(TARGET archive
        PROPERTY IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../libarchive-3.3.1/.libs/libarchive.a")

    add_library(lzma STATIC IMPORTED)
    set_property(TARGET lzma 
        PROPERTY IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/../xz-5.2.3/build/lib/liblzma.a")
endif( ${STATIC_BUILD} )