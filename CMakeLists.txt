cmake_minimum_required(VERSION 3.30)
project(cmdlc)

set(CMAKE_CXX_STANDARD 20)
find_package(Boost REQUIRED COMPONENTS program_options)

include(FetchContent)
fetchcontent_declare(
        Assimp
        GIT_REPOSITORY https://github.com/assimp/assimp
        GIT_TAG origin/master
        GIT_SHALLOW TRUE
        OVERRIDE_FIND_PACKAGE
)
fetchcontent_makeavailable(Assimp)

add_executable(cmdlc
        src/main.cpp
        third-party/lz4/lz4.c
)

target_link_libraries(cmdlc PRIVATE Boost::program_options assimp::assimp)

target_include_directories(cmdlc PRIVATE third-party/lz4)