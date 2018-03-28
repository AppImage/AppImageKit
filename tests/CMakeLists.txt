# build and add test only if tests are enabled
if(BUILD_TESTING)
    include_directories(${gtest_INCLUDE_DIRS})

    add_definitions(
        -DTEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data/"
        -DGIT_COMMIT="AppImageKit unit tests"
    )


    # Link runTests with what we want to test and the GTest and pthread library
    add_executable(test_libappimage
        test_libappimage.cpp
        fixtures.h
    )

    target_link_libraries(test_libappimage
        pthread
        libappimage
        xdg-basedir
        ${gtest_LIBRARIES}
    )

    if(NOT USE_SYSTEM_GTEST)
        add_dependencies(test_libappimage gtest)
    endif()

    add_test(test_libappimage test_libappimage)


    add_executable(test_shared
        test_shared.cpp
        fixtures.h
        ${PROJECT_SOURCE_DIR}/src/shared.c
        ${PROJECT_SOURCE_DIR}/src/elf.c
        ${PROJECT_SOURCE_DIR}/src/getsection.c
    )

    target_link_libraries(test_shared
        PRIVATE
        ${squashfuse_LIBRARIES}
        ${libarchive_LIBRARIES}
        ${inotify-tools_LIBRARIES}
        ${xz_LIBRARIES}
        xdg-basedir
        pthread
        ${GLIB_LIBRARIES}
        ${GIO_LIBRARIES}
        ${ZLIB_LIBRARIES}
        ${CAIRO_LIBRARIES}
        ${gtest_LIBRARIES}
    )

    target_include_directories(test_shared PRIVATE ../src/)

    add_dependencies(test_shared squashfuse)

    if(NOT USE_SYSTEM_LIBARCHIVE)
        add_dependencies(test_shared libarchive)
    endif()

    if(NOT USE_SYSTEM_INOTIFY_TOOLS)
        add_dependencies(test_shared inotify-tools)
    endif()

    if(NOT USE_SYSTEM_GTEST)
        add_dependencies(test_shared gtest)
    endif()

    add_test(test_shared test_shared)

    target_compile_options(test_shared PRIVATE -fpermissive)


    add_executable(test_getsection
        test_getsection.cpp
        fixtures.h
        ${PROJECT_SOURCE_DIR}/src/light_elf.h
        ${PROJECT_SOURCE_DIR}/src/light_byteswap.h
        ${PROJECT_SOURCE_DIR}/src/elf.c
        ${PROJECT_SOURCE_DIR}/src/getsection.c
    )

    target_link_libraries(test_getsection
        PRIVATE
        ${squashfuse_LIBRARIES}
        ${libarchive_LIBRARIES}
        ${inotify-tools_LIBRARIES}
        ${xz_LIBRARIES}
        xdg-basedir
        pthread
        ${GLIB_LIBRARIES}
        ${GIO_LIBRARIES}
        ${ZLIB_LIBRARIES}
        ${CAIRO_LIBRARIES}
        ${gtest_LIBRARIES}
    )

    target_include_directories(test_getsection PRIVATE ../src/)

    add_dependencies(test_getsection squashfuse)

    if(NOT USE_SYSTEM_LIBARCHIVE)
        add_dependencies(test_getsection libarchive)
    endif()

    if(NOT USE_SYSTEM_INOTIFY_TOOLS)
        add_dependencies(test_getsection inotify-tools)
    endif()

    if(NOT USE_SYSTEM_GTEST)
        add_dependencies(test_getsection gtest)
    endif()

    add_test(test_getsection test_getsection)

    target_compile_options(test_getsection PRIVATE -fpermissive)
endif()
