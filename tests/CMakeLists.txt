# build and add test only if tests are enabled
if(BUILD_TESTING)
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
        gtest
    )

    add_test(test_libappimage test_libappimage)


    add_executable(test_shared
        test_shared.cpp
        fixtures.h
        ${PROJECT_SOURCE_DIR}/src/shared.c
        ${PROJECT_SOURCE_DIR}/src/elf.c
        ${PROJECT_SOURCE_DIR}/src/getsection.c
        ${PROJECT_SOURCE_DIR}/src/appimagetool_shared.c
    )

    target_link_libraries(test_shared
        PRIVATE
        squashfuse
        libarchive
        inotify-tools
        xz
        xdg-basedir
        pthread
        libglib
        libgio
        libzlib
        libcairo
        gtest
    )

    target_include_directories(test_shared
        # used to include shared.h
        PRIVATE ${PROJECT_SOURCE_DIR}/src/
        # need to include libappimage's header directory as well
        # as we're building this test manually from the source files, we need to replicate the build settings of
        # libappimage, appimagetool, ...
        PRIVATE ${PROJECT_SOURCE_DIR}/include/
    )

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
        squashfuse
        libarchive
        inotify-tools
        xz
        xdg-basedir
        pthread
        libglib
        libzlib
        libcairo
        gtest
    )

    target_include_directories(test_getsection
        # used to include getsection.h
        PRIVATE ${PROJECT_SOURCE_DIR}/src/
        # need to include libappimage's header directory as well
        # as we're building this test manually from the source files, we need to replicate the build settings of
        # libappimage, appimagetool, ...
        PRIVATE ${PROJECT_SOURCE_DIR}/include/
    )


    add_dependencies(test_getsection squashfuse)

    add_test(test_getsection test_getsection)

    target_compile_options(test_getsection PRIVATE -fpermissive)
endif()
