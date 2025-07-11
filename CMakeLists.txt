cmake_minimum_required(VERSION 3.25)
project(chimaera)

# -----------------------------------------------------------------------------
# Define Options
# -----------------------------------------------------------------------------
set(CHIMAERA_IS_MAIN_PROJECT ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(BUILD_SHARED_LIBS "Build shared libraries (.dll/.so) instead of static ones (.lib/.a)" ON)
option(BUILD_TESTS "Build tests" ON)
option(BUILD_MPI_TESTS "Build tests which depend on MPI" ON)
option(BUILD_OpenMP_TESTS "Build tests which depend on OpenMP" ON)
option(BUILD_ZeroMQ_TESTS "Build tests which depend on ZeroMQ" OFF)
option(CHIMAERA_BUILD_BENCHMARKS "Build chimaera benchmarks" OFF)
option(CHIMAERA_ENABLE_COVERAGE "Check how well tests cover code" OFF)
option(CHIMAERA_ENABLE_DOXYGEN "Check how well the code is documented" OFF)
option(CHIMAERA_TASK_DEBUG "Debug tasks" OFF)
option(CHIMAERA_REMOTE_DEBUG "Debug remote tasks" OFF)
option(CHIMAERA_REMOTE_PRINT "Print remote tasks" OFF)
option(CHIMAERA_ENABLE_JEMALLOC "Use jemalloc as the allocator" OFF)
option(CHIMAERA_ENABLE_MIMALLOC "Use mimalloc as the allocator" OFF)
option(CHIMAERA_ENABLE_PYTHON "Use pybind11" ON)
option(CHIMAERA_ENABLE_ROCM "Enable ROCm support" OFF)
option(CHIMAERA_ENABLE_CUDA "Enable CUDA support" OFF)
option(CHIMAERA_ENABLE_CMAKE_DOTENV "Use cmake dotenv" OFF)

# A hack for spack to get dependencies
option(CHIMAERA_NO_COMPILE "Don't compile the code" OFF)

if(CHIMAERA_NO_COMPILE)
    # Copy tasks/MOD_NAME directory to ~/.chimaera during configure
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/tasks/MOD_NAME
        DESTINATION $ENV{HOME}/.chimaera)

    install(
        FILES
        ${CMAKE_SOURCE_DIR}/.gitignore
        DESTINATION
        ${CMAKE_INSTALL_PREFIX}/.gitignore
    )
    return()
endif()

# ------------------------------------------------------------------------------
# DOTENV
# ------------------------------------------------------------------------------
if(CHIMAERA_ENABLE_CMAKE_DOTENV)
    if(EXISTS ${CMAKE_SOURCE_DIR}/.env.cmake)
        include(.env.cmake)
    endif()
endif()

# -----------------------------------------------------------------------------
# Compiler Optimization
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message("IN DEBUG MODE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O0")
    set(CMAKE_HIP_FLAGS "${CMAKE_HIP_FLAGS} -g -O0")
    set(CMAKE_CUDA_FLAGS "${CMAKE_CUDA_FLAGS} -g -G")
    add_compile_definitions(HSHM_DEBUG)
else()
    message("IN RELEASE MODE")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O3")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -O3")
endif()

# add_compile_options(-march=native -fomit-frame-pointer)
if(CHIMAERA_REMOTE_DEBUG)
    message("Adding the chimaera remote debug option")
    add_compile_definitions(CHIMAERA_REMOTE_DEBUG)
    set(CHIMAERA_REMOTE_PRINT ON)
endif()

if(CHIMAERA_REMOTE_PRINT)
    add_compile_definitions(kRemoteQueue=0)
endif()

if(CHIMAERA_TASK_DEBUG)
    message("Adding the chimaera task debug option")
    add_compile_definitions(CHIMAERA_TASK_DEBUG)
endif()

# ------------------------------------------------------------------------------
# Setup CMake Environment
# ------------------------------------------------------------------------------
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY
    ${PROJECT_BINARY_DIR}/bin CACHE PATH "Single Directory for all Executables.")
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY
    ${PROJECT_BINARY_DIR}/bin CACHE PATH "Single Directory for all Libraries")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY
    ${PROJECT_BINARY_DIR}/bin CACHE PATH "Single Directory for all static libraries.")

# ------------------------------------------------------------------------------
# Setup install and output Directories
# ------------------------------------------------------------------------------
if(NOT CHIMAERA_INSTALL_BIN_DIR)
    set(CHIMAERA_INSTALL_BIN_DIR ${CMAKE_INSTALL_PREFIX}/bin)
endif()

if(NOT CHIMAERA_INSTALL_LIB_DIR)
    set(CHIMAERA_INSTALL_LIB_DIR ${CMAKE_INSTALL_PREFIX}/lib)
endif()

if(NOT CHIMAERA_INSTALL_INCLUDE_DIR)
    set(CHIMAERA_INSTALL_INCLUDE_DIR ${CMAKE_INSTALL_PREFIX}/include)
endif()

if(NOT CHIMAERA_INSTALL_DATA_DIR)
    set(CHIMAERA_INSTALL_DATA_DIR ${CMAKE_INSTALL_PREFIX}/share)
endif()

# -----------------------------------------------------------------------------
# Targets built within this project are exported at Install time for use
# by other projects.
# -----------------------------------------------------------------------------
set(CHIMAERA_EXPORTED_TARGETS "ChimaeraCore")

# -----------------------------------------------------------------------------
# Find Packages
# -----------------------------------------------------------------------------
include(CMake/ChimaeraCommonConfig.cmake)

# This is for compatability with CLANG + Spack
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
list(APPEND CMAKE_INSTALL_RPATH "${CHIMAERA_INSTALL_LIB_DIR}")
set(CMAKE_BUILD_WITH_INSTALL_RPATH ON)
SET(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Dependencies for the runtime
add_library(runtime_deps INTERFACE)
target_link_libraries(runtime_deps INTERFACE
    ${Boost_LIBRARIES}
#    ${Boost_CONTEXT_LIBRARY}    
    ${OPTIONAL_LIBS}
    ${ALLOCATOR_LIBRARIES})
target_include_directories(runtime_deps INTERFACE
    ${Boost_INCLUDE_DIRS})
target_link_directories(runtime_deps INTERFACE
    ${Boost_LIBRARY_DIRS})
target_compile_definitions(runtime_deps INTERFACE
    CHIMAERA_RUNTIME)

if(CHIMAERA_ENABLE_PYTHON)
    target_compile_definitions(runtime_deps INTERFACE
        CHIMAERA_ENABLE_PYTHON)
endif()

# Create an imported target for chimaera::run which links to chimaera_runtime
add_library(chimaera::run ALIAS runtime_deps)

# -----------------------------------------------------------------------------
# Build Chimaera Main Packages
# -----------------------------------------------------------------------------
# Main includes
include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Task includes
include_directories(${CMAKE_SOURCE_DIR}/tasks)
include_directories(${CMAKE_SOURCE_DIR}/tasks/chimaera_admin/include)
include_directories(${CMAKE_SOURCE_DIR}/tasks/small_message/include)
include_directories(${CMAKE_SOURCE_DIR}/tasks/remote_queue/include)
include_directories(${CMAKE_SOURCE_DIR}/tasks/worch_proc_round_robin/include)
include_directories(${CMAKE_SOURCE_DIR}/tasks/worch_queue_round_robin/include)
include_directories(${CMAKE_SOURCE_DIR}/tasks/proc_queue/include)
include_directories(${CMAKE_SOURCE_DIR}/tasks/bdev/include)

set(TEST_MAIN ${CMAKE_SOURCE_DIR}/test/unit)
add_subdirectory(src)
add_subdirectory(tasks)

if(BUILD_TESTS)
    add_subdirectory(test)
endif()

if(CHIMAERA_BUILD_BENCHMARKS)
    add_subdirectory(benchmark)
endif()

# -----------------------------------------------------------------------------
# Build + Enable Testing
# -----------------------------------------------------------------------------
add_custom_target(lint COMMAND bash ${CMAKE_SOURCE_DIR}/ci/lint.sh ${CMAKE_SOURCE_DIR})

# General function used to hook ctest to python test tool lib
function(jarvis_test test_type test_name)
    add_test(NAME ${test_name}
        COMMAND jarvis pipeline run yaml "${CMAKE_SOURCE_DIR}/test/unit/jarvis_chimaera/pipelines/${test_type}/${test_name}.yaml")
endfunction()

if(BUILD_TESTS)
    enable_testing()
endif()

# -----------------------------------------------------------------------------
# Install Chimaera Headers
# -----------------------------------------------------------------------------
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/CMake/ChimaeraConfig.cmake
    ${PROJECT_BINARY_DIR}/CMakeFiles/ChimaeraConfig.cmake @ONLY
)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/CMake/ChimaeraCommonConfig.cmake
    ${PROJECT_BINARY_DIR}/CMakeFiles/ChimaeraCommonConfig.cmake @ONLY
)
install(
    FILES
    ${PROJECT_BINARY_DIR}/CMakeFiles/ChimaeraConfig.cmake
    ${PROJECT_BINARY_DIR}/CMakeFiles/ChimaeraCommonConfig.cmake
    DESTINATION
    ${CMAKE_INSTALL_PREFIX}/cmake
)
install(DIRECTORY include DESTINATION ${CMAKE_INSTALL_PREFIX})

# copy tasks/MOD_NAME to HOME directory
# Create .chimaera directory in home if it doesn't exist
file(MAKE_DIRECTORY "$ENV{HOME}/.chimaera")

# Copy tasks/MOD_NAME directory to ~/.chimaera during configure
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/tasks/MOD_NAME
    DESTINATION $ENV{HOME}/.chimaera)

# Install jarvis repo
jarvis_repo_add("${CMAKE_SOURCE_DIR}/test/jarvis_chimaera" "${CMAKE_SOURCE_DIR}/test/jarvis_chimaera/pipelines")
