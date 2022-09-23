cmake_minimum_required(VERSION 3.21)
project(TinyHttpd C)

set(CMAKE_C_STANDARD 17)

add_executable(TinyHttpd main.c simpleclient.c)

