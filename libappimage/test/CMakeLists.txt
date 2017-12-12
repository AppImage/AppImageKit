# Locate GTest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Link runTests with what we want to test and the GTest and pthread library
add_executable(test_libappimage
    test_appimagelib.cpp
)
target_compile_definitions(test_libappimage
    PRIVATE TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/"
)

target_link_libraries(test_libappimage
    pthread
    libappimage
    ${GTEST_LIBRARIES}
)

add_test(test_libappimage test_libappimage)
