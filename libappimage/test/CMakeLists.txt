# Locate GTest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Link runTests with what we want to test and the GTest and pthread library
add_executable(test_appimagelib
    test_appimagelib.cpp
)
target_compile_definitions(test_appimagelib
    PRIVATE TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/"
)

target_link_libraries(test_appimagelib 
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

add_dependencies(test_appimagelib appimage)

add_test(test_appimagelib test_appimagelib)
