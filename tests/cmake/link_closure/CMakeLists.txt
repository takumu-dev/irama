# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# A project laid out as Irama is, for the tests of the link-closure check: libirama/ declares
# components with irama_add_component(), and links what CASE asks for.
#
#   allowed    its components link one another and the imported target it allows
#   imported   a component links an imported target it doesn't allow
#   library    a component links a library by name, rather than a target
#   target     a component links a target of the project outside libirama/
#   expression a component links through a generator expression
#   interface  a component passes an imported target it doesn't allow to what links it
#   late       cli/, configured after libirama/, adds a link to irama::engine
cmake_minimum_required(VERSION 4.4)
project(irama_link_closure_test LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../cmake")
include(IramaConventions)
include(IramaComponents)

add_library(Fake::allowed INTERFACE IMPORTED)
add_library(Fake::forbidden INTERFACE IMPORTED)
add_library(project_helper STATIC empty.cpp)

add_subdirectory(libirama)
add_subdirectory(cli)
