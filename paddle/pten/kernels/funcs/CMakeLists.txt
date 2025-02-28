add_subdirectory(eigen)

function(math_library TARGET)
    # math_library is a function to create math library.
    # The interface is the same as cc_library.
    # But it handle split GPU/CPU code and link some common library.
    set(cc_srcs)
    set(cu_srcs)
    set(hip_srcs)
    set(math_common_deps device_context framework_proto enforce)
    if (WITH_GPU)
        if (${CMAKE_CUDA_COMPILER_VERSION} LESS 11.0)	
            list(APPEND math_common_deps cub)
	else()
            list(APPEND math_common_deps)
	endif()
    endif()
    set(multiValueArgs DEPS)
    cmake_parse_arguments(math_library "${options}" "${oneValueArgs}"
            "${multiValueArgs}" ${ARGN})

    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET}.cc)
        list(APPEND cc_srcs ${TARGET}.cc)
    endif()
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET}.cu)
        list(APPEND cu_srcs ${TARGET}.cu)
    endif()
    if (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${TARGET}.cu.cc)
        list(APPEND cu_srcs ${TARGET}.cu.cc)
    endif()

    list(LENGTH cc_srcs cc_srcs_len)
    if (WITH_GPU)
        nv_library(${TARGET} SRCS ${cc_srcs} ${cu_srcs} DEPS ${math_library_DEPS} ${math_common_deps})
    elseif (WITH_ROCM)
        hip_library(${TARGET} SRCS ${cc_srcs} ${cu_srcs} DEPS ${math_library_DEPS} ${math_common_deps})
    elseif(${cc_srcs_len} GREATER 0)
        cc_library(${TARGET} SRCS ${cc_srcs} DEPS ${math_library_DEPS} ${math_common_deps})
    endif()
endfunction()

math_library(math_function DEPS blas dense_tensor tensor)
