# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

add_library(irama_engine INTERFACE)
add_library(irama::engine ALIAS irama_engine)

set(packages Fake::allowed)
if(CASE STREQUAL "imported")
    set(packages Fake::forbidden)
elseif(CASE STREQUAL "library")
    set(packages m)
elseif(CASE STREQUAL "target")
    set(packages project_helper)
elseif(CASE STREQUAL "expression")
    set(packages "$<$<CONFIG:Debug>:Fake::allowed>")
endif()

irama_add_component(base SOURCES ../empty.cpp)
irama_add_component(wav SOURCES ../empty.cpp DEPENDS base PACKAGES ${packages})
if(CASE STREQUAL "interface")
    target_link_libraries(irama_base INTERFACE Fake::forbidden)
endif()

irama_check_link_closure(Fake::allowed)
