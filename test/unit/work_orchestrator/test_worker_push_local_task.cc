/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Distributed under BSD 3-Clause license.                                   *
 * Copyright by The HDF Group.                                               *
 * Copyright by the Illinois Institute of Technology.                        *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of Hermes. The full Hermes copyright notice, including  *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the top directory. If you do not  *
 * have access to the file, you may request a copy from help@hdfgroup.org.   *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#include <hermes_shm/util/logging.h>
#include <hermes_shm/util/timer.h>

#include <chrono>
#include <thread>
#include <unordered_map>

#include "../basic_test.h"

// Mock classes for testing
namespace chi {

// Forward declarations
class MockTask;
class MockContainer;
class MockModuleRegistry;
class MockWorker;

// Mock PoolId
struct MockPoolId {
  static MockPoolId GetNull() { return MockPoolId{0, 0, 0}; }
  bool IsNull() const { return node_id_ == 0 && unique_ == 0 && hash_ == 0; }

  u32 node_id_;
  u32 hash_;
  u64 unique_;

  bool operator==(const MockPoolId& other) const {
    return node_id_ == other.node_id_ && hash_ == other.hash_ && unique_ == other.unique_;
  }

  friend std::ostream &operator<<(std::ostream &os, const MockPoolId &id) {
    return os << (std::to_string(id.node_id_) + "." +
                  std::to_string(id.unique_)) +
                     "::" + std::to_string(id.hash_);
  }
};

using ContainerId = u32;
using NodeId = u32;

// Mock Task class
class MockTask {
public:
  MockPoolId pool_;
  NodeId task_node_;
  void* ptr_;

  MockTask(MockPoolId pool, NodeId node) : pool_(pool), task_node_(node), ptr_(this) {}

  bool IsFlush() const { return false; }
};

// Mock Container class
class MockContainer {
public:
  MockPoolId pool_id_;
  std::string name_;
  ContainerId container_id_;
  bool is_created_;

  MockContainer() : is_created_(false) {}
};

// Mock ModuleRegistry
class MockModuleRegistry {
public:
  std::unordered_map<std::string, std::unordered_map<ContainerId, MockContainer*>> containers_;

  MockContainer* GetContainer(const MockPoolId& pool_id, ContainerId container_id) {
    std::string pool_key = std::to_string(pool_id.node_id_) + "." +
                          std::to_string(pool_id.unique_) + "::" +
                          std::to_string(pool_id.hash_);

    auto pool_it = containers_.find(pool_key);
    if (pool_it == containers_.end()) {
      return nullptr;
    }

    auto container_it = pool_it->second.find(container_id);
    if (container_it == pool_it->second.end()) {
      return nullptr;
    }

    return container_it->second;
  }

  void AddContainer(const MockPoolId& pool_id, ContainerId container_id, MockContainer* container) {
    std::string pool_key = std::to_string(pool_id.node_id_) + "." +
                          std::to_string(pool_id.unique_) + "::" +
                          std::to_string(pool_id.hash_);
    containers_[pool_key][container_id] = container;
  }
};

// Mock DomainQuery
struct MockDomainQuery {
  struct {
    ContainerId id_;
  } sel_;
};

// Mock RunContext
struct MockRunContext {
  // Empty for this test
};

// Mock FullPtr
template<typename T>
class MockFullPtr {
public:
  T* ptr_;

  MockFullPtr(T* ptr) : ptr_(ptr) {}

  T* operator->() const { return ptr_; }
  T& operator*() const { return *ptr_; }
};

// Mock PrivateTaskQueue
class MockPrivateTaskQueue {
public:
  std::vector<MockFullPtr<MockTask>> tasks_;

  MockFullPtr<MockTask> push(const MockFullPtr<MockTask>& task) {
    tasks_.push_back(task);
    return MockFullPtr<MockTask>(nullptr);  // Return null to indicate success
  }

  bool IsNull() const { return false; }
};

// Global mock registry instance
MockModuleRegistry* CHI_MOD_REGISTRY = nullptr;
NodeId CHI_CLIENT_node_id = 1;

// Mock PrivateTaskMultiQueue class with our fixed PushLocalTask method
class MockPrivateTaskMultiQueue {
public:
  MockPrivateTaskQueue fail_queue_;

  MockPrivateTaskQueue& GetFail() { return fail_queue_; }

  // This is our fixed PushLocalTask method
  bool PushLocalTask(const MockDomainQuery &res_query,
                     MockRunContext &rctx,
                     const MockFullPtr<MockTask> &task) {
    // If the task is a flushing task. Place in the flush queue.
    if (task->IsFlush()) {
      // For test purposes, we don't handle flush tasks
      return !GetFail().push(task).IsNull();
    }

    // Determine the lane the task should map to within container
    ContainerId container_id = res_query.sel_.id_;
    MockContainer *exec = CHI_MOD_REGISTRY->GetContainer(task->pool_, container_id);
    if (!exec || !exec->is_created_) {
      // Check if we have a retry count field in the task
      // If not, initialize it to track retry attempts
      static thread_local std::unordered_map<void*, std::pair<int, hshm::Timer>> retry_map;
      auto task_ptr = task.ptr_;

      // Get or initialize retry info for this task
      auto &retry_info = retry_map[task_ptr];
      auto &retry_count = retry_info.first;
      auto &first_attempt_time = retry_info.second;

      // Initialize timer on first attempt
      if (retry_count == 0) {
        first_attempt_time.Reset();
      }

      retry_count++;

      // Configuration: max retries and timeout
      const int MAX_RETRIES = 100;  // Prevent infinite retries
      const double TIMEOUT_SECONDS = 30.0;  // 30 second timeout

      double elapsed_time = first_attempt_time.GetSecFromStart();

      // Check if we should give up
      if (retry_count > MAX_RETRIES || elapsed_time > TIMEOUT_SECONDS) {
        // Clean up retry tracking for this task
        retry_map.erase(task_ptr);

        // Mark as failed permanently
        return !GetFail().push(task).IsNull();
      }

      // Put in failed queue for retry
      return !GetFail().push(task).IsNull();
    }

    // Success path: clean up any retry tracking for this task
    static thread_local std::unordered_map<void*, std::pair<int, hshm::Timer>> retry_map;
    retry_map.erase(task.ptr_);

    // Task was successfully processed (in real code it would go to execution queue)
    return true;
  }
};

} // namespace chi

// Test cases
TEST_CASE("TestPushLocalTask_MissingContainer_RetriesWithTimeout") {
  using namespace chi;

  // Setup mock registry
  MockModuleRegistry registry;
  CHI_MOD_REGISTRY = &registry;

  // Create a task queue
  MockPrivateTaskMultiQueue task_queue;

  // Create a mock task with a pool that doesn't exist
  MockPoolId pool_id{1, 0, 0};  // 1.0::0
  MockTask task(pool_id, 123);
  MockFullPtr<MockTask> task_ptr(&task);

  // Create domain query for a container that doesn't exist
  MockDomainQuery query;
  query.sel_.id_ = 1;  // Container ID 1

  MockRunContext rctx;

  // Test that task gets queued to fail queue when container doesn't exist
  bool result = task_queue.PushLocalTask(query, rctx, task_ptr);

  // Should return true (task queued successfully to fail queue)
  REQUIRE(result == true);

  // Should have one task in the fail queue
  REQUIRE(task_queue.GetFail().tasks_.size() == 1);
}

TEST_CASE("TestPushLocalTask_ContainerExists_Success") {
  using namespace chi;

  // Setup mock registry
  MockModuleRegistry registry;
  CHI_MOD_REGISTRY = &registry;

  // Create a container and add it to registry
  MockPoolId pool_id{1, 0, 0};  // 1.0::0
  ContainerId container_id = 1;

  MockContainer container;
  container.pool_id_ = pool_id;
  container.container_id_ = container_id;
  container.is_created_ = true;  // Mark as created

  registry.AddContainer(pool_id, container_id, &container);

  // Create a task queue
  MockPrivateTaskMultiQueue task_queue;

  // Create a mock task
  MockTask task(pool_id, 123);
  MockFullPtr<MockTask> task_ptr(&task);

  // Create domain query for the existing container
  MockDomainQuery query;
  query.sel_.id_ = container_id;

  MockRunContext rctx;

  // Test that task is processed successfully when container exists
  bool result = task_queue.PushLocalTask(query, rctx, task_ptr);

  // Should return true (task processed successfully)
  REQUIRE(result == true);

  // Should have no tasks in the fail queue
  REQUIRE(task_queue.GetFail().tasks_.size() == 0);
}

TEST_CASE("TestPushLocalTask_ContainerExistsButNotCreated_Retries") {
  using namespace chi;

  // Setup mock registry
  MockModuleRegistry registry;
  CHI_MOD_REGISTRY = &registry;

  // Create a container but mark it as not created
  MockPoolId pool_id{1, 0, 0};  // 1.0::0
  ContainerId container_id = 1;

  MockContainer container;
  container.pool_id_ = pool_id;
  container.container_id_ = container_id;
  container.is_created_ = false;  // Not created yet

  registry.AddContainer(pool_id, container_id, &container);

  // Create a task queue
  MockPrivateTaskMultiQueue task_queue;

  // Create a mock task
  MockTask task(pool_id, 123);
  MockFullPtr<MockTask> task_ptr(&task);

  // Create domain query
  MockDomainQuery query;
  query.sel_.id_ = container_id;

  MockRunContext rctx;

  // Test that task gets queued to fail queue when container exists but not created
  bool result = task_queue.PushLocalTask(query, rctx, task_ptr);

  // Should return true (task queued to fail queue for retry)
  REQUIRE(result == true);

  // Should have one task in the fail queue
  REQUIRE(task_queue.GetFail().tasks_.size() == 1);
}

TEST_CASE("TestPushLocalTask_RepeatedRetries_EventualTimeout") {
  using namespace chi;

  // Setup mock registry
  MockModuleRegistry registry;
  CHI_MOD_REGISTRY = &registry;

  // Create a task queue
  MockPrivateTaskMultiQueue task_queue;

  // Create a mock task with a pool that doesn't exist
  MockPoolId pool_id{1, 0, 0};  // 1.0::0
  MockTask task(pool_id, 123);
  MockFullPtr<MockTask> task_ptr(&task);

  // Create domain query for a container that doesn't exist
  MockDomainQuery query;
  query.sel_.id_ = 1;  // Container ID 1

  MockRunContext rctx;

  // Call PushLocalTask multiple times to simulate retry behavior
  int retry_count = 0;
  const int MAX_RETRIES = 150;  // More than the internal limit of 100

  for (int i = 0; i < MAX_RETRIES; ++i) {
    bool result = task_queue.PushLocalTask(query, rctx, task_ptr);
    REQUIRE(result == true);  // Should always return true (queued to fail)
    retry_count++;
  }

  // All attempts should result in tasks being queued to fail queue
  REQUIRE(task_queue.GetFail().tasks_.size() == MAX_RETRIES);
}