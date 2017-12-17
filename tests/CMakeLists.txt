# build and add test only if tests are enabled
if(BUILD_TESTING)
    include_directories(${gtest_INCLUDE_DIRS})

    # Link runTests with what we want to test and the GTest and pthread library
    add_executable(test_libappimage
        test_libappimage.cpp
    )
    target_compile_definitions(test_libappimage
        PRIVATE TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/"
    )

    target_link_libraries(test_libappimage
        pthread
        libappimage
        ${gtest_LIBRARIES}
    )

    if(NOT USE_SYSTEM_GTEST)
        add_dependencies(test_libappimage gtest)
    endif()

    add_test(test_libappimage test_libappimage)
endif()
