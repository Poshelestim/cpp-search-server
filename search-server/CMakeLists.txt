cmake_minimum_required(VERSION 3.5)

project(SearchServer
    VERSION 1.0.0
    DESCRIPTION "Template for C++ library built with CMake"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
#set (CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")

add_executable(SearchServer
    document.cpp
    document.h
    log_duration.h
    main.cpp
    paginator.h
    read_input_functions.h
    read_input_functions.cpp
    remove_duplicates.h
    remove_duplicates.cpp
    request_queue.h
    request_queue.cpp
    search_server.h
    search_server.cpp
    string_processing.h
    string_processing.cpp
    tests.h
    tests.cpp
    process_queries.h
    process_queries.cpp
    concurrent_map.h)
