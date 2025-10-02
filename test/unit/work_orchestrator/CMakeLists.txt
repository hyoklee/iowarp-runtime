project(chimaera)

set(CMAKE_CXX_STANDARD 17)

#------------------------------------------------------------------------------
# Build Tests
#------------------------------------------------------------------------------

add_executable(test_worker_push_local_task_exec
        ${TEST_MAIN}/main.cc
        test_worker_push_local_task.cc
)
target_link_libraries(test_worker_push_local_task_exec
        hshm::cxx Catch2::Catch2 MPI::MPI_CXX)

#------------------------------------------------------------------------------
# Test Cases
#------------------------------------------------------------------------------

add_test(NAME test_worker_push_local_task COMMAND
        ${CMAKE_BINARY_DIR}/bin/test_worker_push_local_task_exec)

#------------------------------------------------------------------------------
# Install Targets
#------------------------------------------------------------------------------
install(TARGETS
        test_worker_push_local_task_exec
        LIBRARY DESTINATION ${HSHM_INSTALL_LIB_DIR}
        ARCHIVE DESTINATION ${HSHM_INSTALL_LIB_DIR}
        RUNTIME DESTINATION ${HSHM_INSTALL_BIN_DIR})

#-----------------------------------------------------------------------------
# Coverage
#-----------------------------------------------------------------------------
if(HSHM_ENABLE_COVERAGE)
    set_coverage_flags(test_worker_push_local_task_exec)
endif()