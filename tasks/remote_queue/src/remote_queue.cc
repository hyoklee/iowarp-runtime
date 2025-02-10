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

#include "remote_queue/remote_queue.h"

#include "chimaera/api/chimaera_runtime.h"
#include "chimaera/network/serialize.h"
#include "chimaera/work_orchestrator/work_orchestrator.h"
#include "chimaera_admin/chimaera_admin.h"

namespace chi::remote_queue {

struct RemoteEntry {
  ResolvedDomainQuery res_domain_;
  Task *task_;
};

class Server : public Module {
 public:
  std::vector<hshm::mpsc_queue<RemoteEntry>> submit_;
  std::vector<hshm::mpsc_queue<RemoteEntry>> complete_;
  std::vector<FullPtr<ClientSubmitTask>> submitters_;
  std::vector<FullPtr<ServerCompleteTask>> completers_;
  CLS_CONST int kNodeRpcLanes = 0;
  CLS_CONST int kInitRpcLanes = 1;

 public:
  Server() = default;

  /** Construct remote queue */
  void Create(CreateTask *task, RunContext &rctx) {
    // Registering RPCs
    CHI_THALLIUM->RegisterRpc(*CHI_WORK_ORCHESTRATOR->rpc_pool_,
                              "RpcTaskSubmit",
                              [this](const tl::request &req, tl::bulk &bulk,
                                     SegmentedTransfer &xfer) {
                                this->RpcTaskSubmit(req, bulk, xfer);
                              });
    CHI_THALLIUM->RegisterRpc(*CHI_WORK_ORCHESTRATOR->rpc_pool_,
                              "RpcTaskComplete",
                              [this](const tl::request &req, tl::bulk &bulk,
                                     SegmentedTransfer &xfer) {
                                this->RpcTaskComplete(req, bulk, xfer);
                              });
    CHI_REMOTE_QUEUE->Init(id_);

    // Create lanes
    CreateLaneGroup(kNodeRpcLanes, 1, QUEUE_HIGH_LATENCY);
    CreateLaneGroup(kInitRpcLanes, 4, QUEUE_LOW_LATENCY);

    // Creating submitter and completer queues
    DomainQuery dom_query =
        DomainQuery::GetDirectId(SubDomainId::kContainerSet, container_id_);
    QueueManagerInfo &qm = CHI_QM->config_->queue_manager_;
    submit_.emplace_back(qm.queue_depth_);
    complete_.emplace_back(qm.queue_depth_);
    submitters_.emplace_back(
        CHI_REMOTE_QUEUE->AsyncClientSubmit(HSHM_DEFAULT_MEM_CTX, dom_query));
    completers_.emplace_back(
        CHI_REMOTE_QUEUE->AsyncServerComplete(HSHM_DEFAULT_MEM_CTX, dom_query));
    task->SetModuleComplete();
  }
  void MonitorCreate(MonitorModeId mode, CreateTask *task, RunContext &rctx) {}

  /** Route a task to a lane */
  Lane *MapTaskToLane(const Task *task) override {
    if (task->IsLongRunning()) {
      return GetLaneByHash(kNodeRpcLanes, task->prio_,
                           hshm::hash<DomainQuery>()(task->dom_query_));
    } else {
      return GetLaneByHash(kInitRpcLanes, task->prio_,
                           hshm::hash<DomainQuery>()(task->dom_query_));
    }
  }

  /** Destroy remote queue */
  void Destroy(DestroyTask *task, RunContext &rctx) {
    task->SetModuleComplete();
  }
  void MonitorDestroy(MonitorModeId mode, DestroyTask *task, RunContext &rctx) {
  }

  /** Replicate the task across a node set */
  void Replicate(Task *submit_task, Task *orig_task,
                 std::vector<ResolvedDomainQuery> &dom_queries,
                 RunContext &rctx) {
    std::vector<FullPtr<Task>> replicas;
    replicas.reserve(dom_queries.size());

    // Get the container
    Container *exec = CHI_MOD_REGISTRY->GetStaticContainer(orig_task->pool_);

    // Register the block
    submit_task->SetBlocked(dom_queries.size());
    // CHI_WORK_ORCHESTRATOR->Block(submit_task, rctx);

    // Replicate task
    for (ResolvedDomainQuery &res_query : dom_queries) {
      FullPtr<Task> rep_task;
      exec->NewCopyStart(orig_task->method_, orig_task, rep_task, false);
      if (res_query.dom_.flags_.Any(DomainQuery::kLocal)) {
        exec->Monitor(MonitorMode::kReplicaStart, orig_task->method_, orig_task,
                      rctx);
      }
      rep_task->rctx_.pending_to_ = submit_task;
      size_t node_hash = hshm::hash<NodeId>{}(res_query.node_);
      auto &submit = submit_;
      HLOG(kDebug, kRemoteQueue, "[TASK_CHECK] Task replica addr {}",
           rep_task.ptr_);
      submit[node_hash % submit.size()].emplace(
          (RemoteEntry){res_query, rep_task.ptr_});
      replicas.emplace_back(rep_task);
    }
    HLOG(kDebug, kRemoteQueue, "[TASK_CHECK] Replicated the submit_task {}",
         submit_task);

    // Actually block
    submit_task->Yield();

    // Combine
    rctx.replicas_ = &replicas;
    exec->Monitor(MonitorMode::kReplicaAgg, orig_task->method_, orig_task,
                  rctx);
    HLOG(kDebug, kRemoteQueue, "[TASK_CHECK] Back in submit_task {}",
         submit_task);

    // Free
    for (FullPtr<Task> &replica : replicas) {
      CHI_CLIENT->DelTask(HSHM_DEFAULT_MEM_CTX, exec, replica.ptr_);
    }
  }

  /** Push operation called on client */
  void ClientPushSubmit(ClientPushSubmitTask *task, RunContext &rctx) {
    HLOG(kDebug, kRemoteQueue, "");
    // Get domain IDs
    Task *orig_task = task->orig_task_;
    std::vector<ResolvedDomainQuery> dom_queries = CHI_RPC->ResolveDomainQuery(
        orig_task->pool_, orig_task->dom_query_, false);
    if (dom_queries.size() == 0) {
      task->SetModuleComplete();
      orig_task->SetModuleComplete();
      CHI_CLIENT->ScheduleTask(nullptr, FullPtr<Task>(orig_task));
      return;
    }
    // Replicate task
    Replicate(task, orig_task, dom_queries, rctx);
    // Unblock original task
    if (!orig_task->IsLongRunning()) {
      orig_task->SetModuleComplete();
    }
    HLOG(kDebug, kRemoteQueue, "[TASK_CHECK] Pushing back to runtime {}",
         orig_task);
    CHI_CLIENT->ScheduleTask(nullptr, FullPtr<Task>(orig_task));

    // Set this task as complete
    task->SetModuleComplete();
  }
  void MonitorClientPushSubmit(MonitorModeId mode, ClientPushSubmitTask *task,
                               RunContext &rctx) {}

  /** Push operation called on client */
  void ClientSubmit(ClientSubmitTask *task, RunContext &rctx) {
    if (rctx.worker_props_.Any(CHI_WORKER_IS_FLUSHING)) {
      hshm::mpsc_queue<RemoteEntry> &submit = submit_[0];
      hshm::mpsc_queue<RemoteEntry> &complete = complete_[0];
      rctx.flush_->count_ += submit.GetSize() + complete.GetSize();
    }

    try {
      RemoteEntry entry;
      std::unordered_map<NodeId, BinaryOutputArchive<true>> entries;
      auto &submit = submit_;
      while (!submit[0].pop(entry).IsNull()) {
        if (entries.find(entry.res_domain_.node_) == entries.end()) {
          entries.emplace(entry.res_domain_.node_, BinaryOutputArchive<true>());
        }
        Task *rep_task = entry.task_;
        Container *exec = CHI_MOD_REGISTRY->GetStaticContainer(rep_task->pool_);
        if (exec == nullptr) {
          HELOG(kFatal, "(node {}) Could not find the pool {}",
                CHI_CLIENT->node_id_, rep_task->pool_);
          return;
        }
        rep_task->dom_query_ = entry.res_domain_.dom_;
        BinaryOutputArchive<true> &ar = entries[entry.res_domain_.node_];
        exec->SaveStart(rep_task->method_, ar, rep_task);
        HLOG(kDebug, kRemoteQueue,
             "[TASK_CHECK] Serializing rep_task {}({} -> {}) ", rep_task,
             CHI_RPC->node_id_, entry.res_domain_.node_);
      }

      for (auto it = entries.begin(); it != entries.end(); ++it) {
        SegmentedTransfer xfer = it->second.Get();
        xfer.ret_node_ = CHI_RPC->node_id_;
        CHI_THALLIUM->SyncIoCall<int>((i32)it->first, "RpcTaskSubmit", xfer,
                                      DT_WRITE);
      }
    } catch (hshm::Error &e) {
      HELOG(kError, "(node {}) Worker {} caught an error: {}",
            CHI_CLIENT->node_id_, id_, e.what());
    } catch (std::exception &e) {
      HELOG(kError, "(node {}) Worker {} caught an exception: {}",
            CHI_CLIENT->node_id_, id_, e.what());
    } catch (...) {
      HELOG(kError, "(node {}) Worker {} caught an unknown exception",
            CHI_CLIENT->node_id_, id_);
    }
    task->UnsetStarted();
  }
  void MonitorClientSubmit(MonitorModeId mode, ClientSubmitTask *task,
                           RunContext &rctx) {}

  /** Complete the task (on the remote node) */
  void ServerPushComplete(ServerPushCompleteTask *task, RunContext &rctx) {
    HLOG(kDebug, kRemoteQueue, "");
    NodeId ret_node = task->rctx_.ret_node_;
    size_t node_hash = hshm::hash<NodeId>{}(ret_node);
    auto &complete = complete_;
    complete[node_hash % complete.size()].emplace(
        (RemoteEntry){ret_node, task});
  }
  void MonitorServerPushComplete(MonitorModeId mode,
                                 ServerPushCompleteTask *task,
                                 RunContext &rctx) {}

  /** Complete the task (on the remote node) */
  void ServerComplete(ServerCompleteTask *task, RunContext &rctx) {
    try {
      // Serialize task completions
      RemoteEntry entry;
      std::unordered_map<NodeId, BinaryOutputArchive<false>> entries;
      auto &complete = complete_;
      while (!complete[0].pop(entry).IsNull()) {
        if (entries.find(entry.res_domain_.node_) == entries.end()) {
          entries.emplace(entry.res_domain_.node_,
                          BinaryOutputArchive<false>());
        }
        Task *done_task = entry.task_;
        Container *exec =
            CHI_MOD_REGISTRY->GetStaticContainer(done_task->pool_);
        BinaryOutputArchive<false> &ar = entries[entry.res_domain_.node_];
        exec->SaveEnd(done_task->method_, ar, done_task);
        CHI_CLIENT->DelTask(HSHM_DEFAULT_MEM_CTX, exec, done_task);
      }

      // Do transfers
      for (auto it = entries.begin(); it != entries.end(); ++it) {
        SegmentedTransfer xfer = it->second.Get();
        CHI_THALLIUM->SyncIoCall<int>((i32)it->first, "RpcTaskComplete", xfer,
                                      DT_WRITE);
      }
    } catch (hshm::Error &e) {
      HELOG(kError, "(node {}) Worker {} caught an error: {}",
            CHI_CLIENT->node_id_, id_, e.what());
    } catch (std::exception &e) {
      HELOG(kError, "(node {}) Worker {} caught an exception: {}",
            CHI_CLIENT->node_id_, id_, e.what());
    } catch (...) {
      HELOG(kError, "(node {}) Worker {} caught an unknown exception",
            CHI_CLIENT->node_id_, id_);
    }
    task->UnsetStarted();
  }
  void MonitorServerComplete(MonitorModeId mode, ServerCompleteTask *task,
                             RunContext &rctx) {}

 private:
  /** The RPC for processing a message with data */
  void RpcTaskSubmit(const tl::request &req, tl::bulk &bulk,
                     SegmentedTransfer &xfer) {
    try {
      HLOG(kDebug, kRemoteQueue, "");
      xfer.AllocateBulksServer();
      CHI_THALLIUM->IoCallServerWrite(req, bulk, xfer);
      BinaryInputArchive<true> ar(xfer);
      for (size_t i = 0; i < xfer.tasks_.size(); ++i) {
        DeserializeTask(i, ar, xfer);
      }
    } catch (hshm::Error &e) {
      HELOG(kError, "(node {}) Worker {} caught an error: {}",
            CHI_CLIENT->node_id_, id_, e.what());
    } catch (std::exception &e) {
      HELOG(kError, "(node {}) Worker {} caught an exception: {}",
            CHI_CLIENT->node_id_, id_, e.what());
    } catch (...) {
      HELOG(kError, "(node {}) Worker {} caught an unknown exception",
            CHI_CLIENT->node_id_, id_);
    }
    req.respond(0);
  }

  /** Push operation called at the remote server */
  void DeserializeTask(size_t task_off, BinaryInputArchive<true> &ar,
                       SegmentedTransfer &xfer) {
    // Deserialize task
    PoolId pool_id = xfer.tasks_[task_off].pool_;
    u32 method = xfer.tasks_[task_off].method_;
    Container *exec = CHI_MOD_REGISTRY->GetStaticContainer(pool_id);
    if (exec == nullptr) {
      HELOG(kFatal, "(node {}) Could not find the pool {}",
            CHI_CLIENT->node_id_, pool_id);
      return;
    }
    TaskPointer rep_task = exec->LoadStart(method, ar);
    rep_task->dom_query_ = xfer.tasks_[task_off].dom_;
    rep_task->rctx_.ret_task_addr_ = xfer.tasks_[task_off].task_addr_;
    rep_task->rctx_.ret_node_ = xfer.ret_node_;
    if (rep_task->rctx_.ret_task_addr_ == (size_t)rep_task.ptr_) {
      HELOG(kFatal, "This shouldn't happen ever");
    }
    HLOG(kDebug, kRemoteQueue,
         "[TASK_CHECK] (node {}) Deserialized task {} with replica addr {} "
         "(pool={}, method={})",
         CHI_CLIENT->node_id_, rep_task.ptr_,
         (void *)rep_task->rctx_.ret_task_addr_, pool_id, method);

    // Unset task flags
    // NOTE(llogan): Remote tasks are executed to completion and
    // return values sent back to the remote host. This is
    // for things like long-running monitoring tasks.
    rep_task->SetDataOwner();
    rep_task->UnsetFireAndForget();
    rep_task->UnsetLongRunning();
    rep_task->SetSignalRemoteComplete();
    rep_task->task_flags_.SetBits(TASK_REMOTE_DEBUG_MARK);

    // Execute task
    CHI_CLIENT->ScheduleTask(nullptr, rep_task);
  }

  /** Receive task completion */
  void RpcTaskComplete(const tl::request &req, tl::bulk &bulk,
                       SegmentedTransfer &xfer) {
    try {
      // Deserialize message parameters
      BinaryInputArchive<false> ar(xfer);
      for (size_t i = 0; i < xfer.tasks_.size(); ++i) {
        Task *rep_task = (Task *)xfer.tasks_[i].task_addr_;
        Container *exec = CHI_MOD_REGISTRY->GetStaticContainer(rep_task->pool_);
        if (exec == nullptr) {
          HELOG(kFatal, "(node {}) Could not find the pool {}",
                CHI_CLIENT->node_id_, rep_task->pool_);
          return;
        }
        exec->LoadEnd(rep_task->method_, ar, rep_task);
        HLOG(kDebug, kRemoteQueue, "[TASK_CHECK] Completing replica {}",
             rep_task);
      }
      // Process bulk message
      CHI_THALLIUM->IoCallServerWrite(req, bulk, xfer);
      // Unblock completed tasks
      for (size_t i = 0; i < xfer.tasks_.size(); ++i) {
        Task *rep_task = (Task *)xfer.tasks_[i].task_addr_;
        Task *submit_task = rep_task->rctx_.pending_to_;
        HLOG(kDebug, kRemoteQueue, "[TASK_CHECK] Unblocking the submit_task {}",
             submit_task);
        if (submit_task->pool_ != id_) {
          HELOG(kFatal, "This shouldn't happen ever");
        }
        CHI_WORK_ORCHESTRATOR->SignalUnblock(submit_task, submit_task->rctx_);
      }
    } catch (hshm::Error &e) {
      HELOG(kError, "(node {}) Worker {} caught an error: {}",
            CHI_CLIENT->node_id_, id_, e.what());
    } catch (std::exception &e) {
      HELOG(kError, "(node {}) Worker {} caught an exception: {}",
            CHI_CLIENT->node_id_, id_, e.what());
    } catch (...) {
      HELOG(kError, "(node {}) Worker {} caught an unknown exception",
            CHI_CLIENT->node_id_, id_);
    }
    req.respond(0);
  }

 public:
#include "remote_queue/remote_queue_lib_exec.h"
};
}  // namespace chi::remote_queue

CHI_TASK_CC(chi::remote_queue::Server, "remote_queue");
