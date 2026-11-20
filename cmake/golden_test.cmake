# SPDX-FileCopyrightText: 2026 takumu-dev
# SPDX-License-Identifier: Zlib

# Runs one golden case: irama-cli writes a WAV file, which must match the case's golden.
#
#   CLI        irama-cli
#   ARGUMENTS  its arguments, as a list, after which it is given the path of the file to write
#   COMPARE    irama_wav_compare
#   GOLDEN     the golden
#   ACTUAL     where irama-cli writes its file
#   MAX_ERROR  the largest difference allowed between a sample and its golden
#   MAX_SIZE   the largest a golden may be, in bytes
#   FAILURES   the folder that keeps the file of a case that fails, named after its golden
#   MAY_UPDATE whether this build may write goldens, which only gcc-release's may, so that goldens
#              don't drift between compilers
#
# With the environment variable IRAMA_UPDATE_GOLDENS set to 1, the case writes its golden instead of
# checking it: it copies the file irama-cli wrote over the golden, or creates a missing one, prints
# the largest difference from the old golden and the peak of the new one, and passes.

set(update_command "IRAMA_UPDATE_GOLDENS=1 ctest --preset gcc-release -L golden")
cmake_path(GET GOLDEN FILENAME golden_name)
set(kept "${FAILURES}/${golden_name}")

# Sets `out` to `text` with each line indented, which CMake's messages then print as they are,
# rather than wrapping them.
function(irama_indent out text)
    string(REGEX REPLACE "\n$" "" text "${text}")
    string(REPLACE "\n" "\n  " text "${text}")
    set(${out} "  ${text}\n" PARENT_SCOPE)
endfunction()

# Runs irama_wav_compare on the file irama-cli wrote and the file at `golden`, setting `result` to
# its exit status and `report` to what it printed, indented.
function(irama_compare result report golden)
    execute_process(
        COMMAND "${COMPARE}" "${ACTUAL}" "${golden}" --max-error "${MAX_ERROR}"
        RESULT_VARIABLE status
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    irama_indent(indented "${output}${error}")
    set(${result} "${status}" PARENT_SCOPE)
    set(${report} "${indented}" PARENT_SCOPE)
endfunction()

# Stops if the file at `path`, called `what`, is larger than MAX_SIZE.
function(irama_check_size what path)
    file(SIZE "${path}" size)
    if(size GREATER MAX_SIZE)
        message(FATAL_ERROR "${what} is ${size} bytes, over the case's limit of ${MAX_SIZE}:\n"
            "  ${path}\n"
            "Make the case shorter, or give it a larger MAX_SIZE with a comment saying why."
        )
    endif()
endfunction()

file(REMOVE "${ACTUAL}")
execute_process(
    COMMAND "${CLI}" ${ARGUMENTS} "${ACTUAL}"
    RESULT_VARIABLE result
    ERROR_VARIABLE error
)
if(NOT result STREQUAL "0")
    irama_indent(error "${error}")
    message(FATAL_ERROR "irama-cli exited with ${result}:\n${error}")
endif()

if("$ENV{IRAMA_UPDATE_GOLDENS}")
    if(NOT MAY_UPDATE)
        message(FATAL_ERROR "Only gcc-release writes goldens, so that they don't drift between "
            "compilers. Run:\n  ${update_command}"
        )
    endif()
    irama_check_size("The new golden" "${ACTUAL}")
    if(EXISTS "${GOLDEN}")
        irama_compare(result report "${GOLDEN}")
        set(action "Updated")
    else()
        # Compared with itself, the new file reports its peak alone.
        irama_compare(result report "${ACTUAL}")
        string(REGEX MATCH "  peak: [^\n]*\n" report "${report}")
        set(action "Created")
    endif()
    file(COPY_FILE "${ACTUAL}" "${GOLDEN}")
    message("${action} the golden:\n  ${GOLDEN}\n${report}")
    return()
endif()

if(NOT EXISTS "${GOLDEN}")
    message(FATAL_ERROR
        "There is no golden at:\n  ${GOLDEN}\nCreate it with:\n  ${update_command}"
    )
endif()
irama_check_size("The golden" "${GOLDEN}")
irama_compare(result report "${GOLDEN}")
if(NOT result STREQUAL "0")
    file(MAKE_DIRECTORY "${FAILURES}")
    file(COPY_FILE "${ACTUAL}" "${kept}" RESULT copied)
    if(copied STREQUAL "0")
        string(APPEND report "Kept the file irama-cli wrote at:\n  ${kept}\n")
    endif()
    message(FATAL_ERROR "${report}")
endif()
# A failure kept from an earlier run no longer stands.
file(REMOVE "${kept}")
