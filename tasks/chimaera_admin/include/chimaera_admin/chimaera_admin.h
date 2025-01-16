#ifndef CHI_TASKS_CHI_ADMIN_CHI_ADMIN_H_
#define CHI_TASKS_CHI_ADMIN_CHI_ADMIN_H_

#include "chimaera_admin_tasks.h"

namespace chi::Admin {

/** Create admin requests */
class Client : public ModuleClient {
 public:
  /** Default constructor */
  HSHM_INLINE_CROSS_FUN
  Client() {
    id_ = PoolId(CHI_QM->admin_queue_id_);
    queue_id_ = CHI_QM->admin_queue_id_;
  }

  /** Destructor */
  HSHM_INLINE_CROSS_FUN
  ~Client() = default;

  /** Register a module */
  HSHM_INLINE_CROSS_FUN
  void RegisterModule(const hipc::MemContext &mctx,
                      const DomainQuery &dom_query,
                      const chi::string &lib_name) {
    FullPtr<RegisterModuleTask> task =
        AsyncRegisterModule(mctx, dom_query, lib_name);
    task->Wait();
    CHI_CLIENT->DelTask(mctx, task);
  }
  CHI_TASK_METHODS(RegisterModule)

  /** Unregister a module */
  HSHM_INLINE_CROSS_FUN
  void DestroyModule(const hipc::MemContext &mctx, const DomainQuery &dom_query,
                     const chi::string &lib_name) {
    FullPtr<DestroyModuleTask> task =
        AsyncDestroyModule(mctx, dom_query, lib_name);
    task->Wait();
    CHI_CLIENT->DelTask(mctx, task);
  }
  CHI_TASK_METHODS(DestroyModule)

  /** Register a task library */
  HSHM_INLINE_CROSS_FUN
  void UpgradeModule(const hipc::MemContext &mctx, const DomainQuery &dom_query,
                     const chi::string &lib_name) {
    FullPtr<UpgradeModuleTask> task =
        AsyncUpgradeModule(mctx, dom_query, lib_name);
    task->Wait();
    CHI_CLIENT->DelTask(mctx, task);
  }
  CHI_TASK_METHODS(UpgradeModule)

  /** Get the ID of a pool */
  HSHM_INLINE_CROSS_FUN
  PoolId GetPoolId(const hipc::MemContext &mctx, const DomainQuery &dom_query,
                   const chi::string &pool_name) {
    FullPtr<GetPoolIdTask> task = AsyncGetPoolId(mctx, dom_query, pool_name);
    task->Wait();
    PoolId new_id = task->id_;
    CHI_CLIENT->DelTask(mctx, task);
    return new_id;
  }
  CHI_TASK_METHODS(GetPoolId)

  /** Create a pool */
  HSHM_INLINE_CROSS_FUN
  void CreateContainer(const hipc::MemContext &mctx,
                       const DomainQuery &dom_query,
                       const CreateContainerTask &task) {
    FullPtr<CreateContainerTask> task_ptr =
        AsyncCreateContainer(mctx, dom_query, task);
    task_ptr->Wait();
    CHI_CLIENT->DelTask(mctx, task_ptr);
  }
  CHI_TASK_METHODS(CreateContainer)

  /** Terminate a pool */
  HSHM_INLINE_CROSS_FUN
  void DestroyContainer(const hipc::MemContext &mctx,
                        const DomainQuery &dom_query,
                        const PoolId &destroy_id) {
    FullPtr<DestroyContainerTask> task =
        AsyncDestroyContainer(mctx, dom_query, destroy_id);
    task->Wait();
    CHI_CLIENT->DelTask(mctx, task);
  }
  CHI_TASK_METHODS(DestroyContainer)

  /** Terminate the runtime */
  HSHM_INLINE_CROSS_FUN
  void StopRuntime(const hipc::MemContext &mctx) {
    HILOG(kInfo,
          "Beginning to flush the runtime.\n"
          "If you did async I/O, this may take some time.\n"
          "All unflushed data will be written to the PFS.");
    Flush(mctx, DomainQuery::GetGlobalBcast());
    HILOG(kInfo, "Stopping the runtime");
    AsyncStopRuntime(
        mctx, DomainQuery::GetDirectHash(SubDomainId::kLocalContainers, 0),
        true);
    HILOG(kInfo, "All done!");
    exit(1);
  }
  CHI_TASK_METHODS(StopRuntime);

  /** Set work orchestrator queue policy */
  HSHM_INLINE_CROSS_FUN
  void SetWorkOrchQueuePolicy(const hipc::MemContext &mctx,
                              const DomainQuery &dom_query,
                              const PoolId &policy) {
    FullPtr<SetWorkOrchQueuePolicyTask> task =
        AsyncSetWorkOrchQueuePolicy(mctx, dom_query, policy);
    task->Wait();
    CHI_CLIENT->DelTask(mctx, task);
  }
#ifdef CHIMAERA_RUNTIME
  void SetWorkOrchQueuePolicyRN(const hipc::MemContext &mctx,
                                const DomainQuery &dom_query,
                                const PoolId &policy) {
    FullPtr<SetWorkOrchQueuePolicyTask> task = AsyncSetWorkOrchQueuePolicyBase(
        mctx, nullptr, CHI_CLIENT->MakeTaskNodeId(), dom_query, policy);
    task->SpinWait();
    CHI_CLIENT->DelTask(mctx, task);
  }
#endif
  CHI_TASK_METHODS(SetWorkOrchQueuePolicy);

  /** Set work orchestrator process policy */
  HSHM_INLINE_CROSS_FUN
  void SetWorkOrchProcPolicy(const hipc::MemContext &mctx,
                             const DomainQuery &dom_query,
                             const PoolId &policy) {
    FullPtr<SetWorkOrchProcPolicyTask> task =
        AsyncSetWorkOrchProcPolicy(mctx, dom_query, policy);
    task->Wait();
    CHI_CLIENT->DelTask(mctx, task);
  }
#ifdef CHIMAERA_RUNTIME
  void SetWorkOrchProcPolicyRN(const hipc::MemContext &mctx,
                               const DomainQuery &dom_query,
                               const PoolId &policy) {
    FullPtr<SetWorkOrchProcPolicyTask> task = AsyncSetWorkOrchProcPolicyBase(
        mctx, nullptr, CHI_CLIENT->MakeTaskNodeId(), dom_query, policy);
    task->SpinWait();
    CHI_CLIENT->DelTask(mctx, task);
  }
#endif
  CHI_TASK_METHODS(SetWorkOrchProcPolicy);

  /** Flush the runtime */
  HSHM_INLINE_CROSS_FUN
  void Flush(const hipc::MemContext &mctx, const DomainQuery &dom_query) {
    size_t work_done = 0;
    do {
      FullPtr<FlushTask> task = AsyncFlush(mctx, dom_query);
      task->Wait();
      work_done = task->work_done_;
      CHI_CLIENT->DelTask(mctx, task);
    } while (work_done > 0);
  }
  CHI_TASK_METHODS(Flush);

  /** Get size of a domain */
  HSHM_INLINE_CROSS_FUN
  size_t GetDomainSize(const hipc::MemContext &mctx,
                       const DomainQuery &dom_query, const DomainId &dom_id) {
    FullPtr<GetDomainSizeTask> task =
        AsyncGetDomainSize(mctx, dom_query, dom_id);
    task->Wait();
    size_t dom_size = task->dom_size_;
    CHI_CLIENT->DelTask(mctx, task);
    return dom_size;
  }
  CHI_TASK_METHODS(GetDomainSize)
};

}  // namespace chi::Admin

#define CHI_ADMIN hshm::CrossSingleton<chi::Admin::Client>::GetInstance()

#endif  // CHI_TASKS_CHI_ADMIN_CHI_ADMIN_H_
