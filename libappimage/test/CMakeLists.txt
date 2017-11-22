# Locate GTest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

find_package(PkgConfig)

pkg_check_modules(GLIB REQUIRED glib-2.0)
include_directories(${GLIB_INCLUDE_DIRS})

# Link runTests with what we want to test and the GTest and pthread library
add_executable(test_libappimage
    test_libappimage.cpp
)
target_compile_definitions(test_libappimage
    PRIVATE TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/"
)

target_link_libraries(test_libappimage 
    appimage
    pthread  
    ${GTEST_LIBRARIES}
    ssl
    crypto
    lzma
    pthread
    ${GLIB_LIBRARIES}
    ${GIO_LIBRARIES}
    ${CAIRO_LIBRARIES}
)

add_dependencies(test_libappimage appimage)

add_test(test_libappimage test_libappimage)
