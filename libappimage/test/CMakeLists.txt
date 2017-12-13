# Locate GTest

enable_testing()

if(BUILD_TESTING)
    set(USE_SYSTEM_GTEST OFF CACHE BOOL "Use system GTest instead of downloading and building GTest")

    if(NOT USE_SYSTEM_GTEST)
        ExternalProject_Add(gtest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG release-1.8.0
            CONFIGURE_COMMAND ${CMAKE_COMMAND} -G${CMAKE_GENERATOR} -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> <SOURCE_DIR>/googletest
        )

        ExternalProject_Get_Property(gtest SOURCE_DIR)
        ExternalProject_Get_Property(gtest INSTALL_DIR)
        set(gtest_SOURCE_DIR "${SOURCE_DIR}")
        set(gtest_INSTALL_DIR "${INSTALL_DIR}")
        mark_as_advanced(gtest_SOURCE_DIR gtest_INSTALL_DIR)

        set(gtest_INCLUDE_DIRS "${gtest_INSTALL_DIR}/include/")
        set(gtest_LIBRARIES_DIR "${gtest_INSTALL_DIR}/lib")
        set(gtest_LIBRARIES "${gtest_LIBRARIES_DIR}/libgtest.a" "${gtest_LIBRARIES_DIR}/libgtest_main.a")
    else()
        find_package(GTest REQUIRED)

        set(gtest_INCLUDE_DIRS "${GTEST_INCLUDE_DIRS}")
        set(gtest_LIBRARIES "${GTEST_LIBRARIES}")
    endif()

    include_directories(${gtest_INCLUDE_DIRS})

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
        ${gtest_LIBRARIES}
    )

    if(NOT USE_SYSTEM_GTEST)
        add_dependencies(test_libappimage gtest)
    endif()

    add_test(test_libappimage test_libappimage)
endif()
