# target_include_libraries was introduced in version 3.0
cmake_minimum_required(VERSION 3.0)

include_directories(${gtest_INCLUDE_DIRS})

# force static linking
add_library(xdg-basedir STATIC xdg-basedir.h xdg-basedir.c)
target_include_directories(xdg-basedir PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

add_executable(test-xdg-basedir test-xdg-basedir.cpp)
target_link_libraries(test-xdg-basedir ${gtest_LIBRARIES} pthread xdg-basedir)
if(NOT USE_SYSTEM_GTEST)
    add_dependencies(test-xdg-basedir gtest)
endif()
