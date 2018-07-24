if(TOOLS_PREFIX)
    message(STATUS "TOOLS_PREFIX ${TOOLS_PREFIX} detected")
endif()

# first of all, make sure required programs are available
function(check_program name)
    if(TOOLS_PREFIX)
        set(prefix ${TOOLS_PREFIX}_)
    endif()

    message(STATUS "Checking for program ${name}")

    string(TOUPPER ${name} name_upper)

    if(prefix)
        # try prefixed version first
        find_program(${name_upper} ${prefix}${name})
    endif()

    # try non-prefixed version
    if(NOT ${name_upper})
        find_program(${name_upper} ${name})

        if(NOT ${name_upper})
            message(FATAL_ERROR "Could not find required program ${name}.")
        endif()
    endif()

    message(STATUS "Found program ${name}: ${${name_upper}}")

    mark_as_advanced(${name_upper})
endfunction()

check_program(aclocal)
check_program(autoheader)
check_program(automake)
check_program(autoreconf)
check_program(libtoolize)
check_program(patch)
check_program(sed)
check_program(wget)
check_program(xxd)
check_program(desktop-file-validate)
check_program(objcopy)
check_program(objdump)
check_program(readelf)
check_program(strip)
check_program(make)
# TODO: add checks for remaining commands
