cmake_minimum_required(VERSION 3.26)
project(UDPServer)

set(CMAKE_CXX_STANDARD 17)

add_executable(UDPServer main.cpp
        Formatter.h
        Formatter.cpp)
