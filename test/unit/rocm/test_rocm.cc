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

#include <hermes_shm/util/affinity.h>
#include <hermes_shm/util/timer.h>

#include "bdev/bdev.h"
#include "chimaera/api/chimaera_client.h"
#include "chimaera/api/chimaera_client_defn.h"
#include "chimaera_admin/chimaera_admin.h"
#include "omp.h"
#include "small_message/small_message.h"

HSHM_GPU_KERNEL void test_kernel() {
  printf("H1\n");
  chi::TaskNode task_node = CHI_CLIENT->MakeTaskNodeId();
  printf("H2: %p\n", CHI_CLIENT->main_alloc_);

  hipc::FullPtr<chi::Admin::RegisterModuleTask> ptr =
      CHI_CLIENT->main_alloc_->NewObjLocal<chi::Admin::RegisterModuleTask>(
          HSHM_DEFAULT_MEM_CTX,
          hipc::CtxAllocator<CHI_ALLOC_T>{(CHI_ALLOC_T*)CHI_CLIENT->main_alloc_,
                                          HSHM_DEFAULT_MEM_CTX},
          task_node, CHI_ADMIN->id_, chi::DomainQuery::GetGlobalBcast(),
          "small_message");

  // hipc::FullPtr<chi::Admin::RegisterModuleTask> task =
  //     CHI_CLIENT->NewTask<chi::Admin::RegisterModuleTask>(
  //         HSHM_DEFAULT_MEM_CTX, task_node, CHI_ADMIN->id_,
  //         chi::DomainQuery::GetGlobalBcast(), "small_message");
  // hipc::FullPtr<chi::Admin::RegisterModuleTask> task =
  //     CHI_ADMIN->AsyncRegisterModuleAlloc(HSHM_DEFAULT_MEM_CTX, task_node,
  //                                         chi::DomainQuery::GetGlobalBcast(),
  //                                         "small_message");
  // printf("H3: %p\n", task.ptr_);
  // chi::ingress::MultiQueue *queue =
  //     CHI_CLIENT->GetQueue(CHI_QM->process_queue_id_);
  // printf("H4: %p\n", queue);
  // queue->Emplace(chi::TaskPrioOpt::kLowLatency,
  //                hshm::hash<chi::DomainQuery>{}(task->dom_query_),
  //                task.shm_);
  // printf("H5\n");
  // CHI_ADMIN->RegisterModule(HSHM_DEFAULT_MEM_CTX,
  //                           chi::DomainQuery::GetGlobalBcast(),
  //                           "small_message");
  // client.Create(
  //     HSHM_DEFAULT_MEM_CTX,
  //     chi::DomainQuery::GetDirectHash(chi::SubDomainId::kGlobalContainers,
  //     0), chi::DomainQuery::GetGlobalBcast(), "ipc_test");
  // hshm::Timer t;
  // size_t domain_size = CHI_ADMIN->GetDomainSize(
  //     HSHM_DEFAULT_MEM_CTX,
  //     chi::DomainQuery::GetDirectHash(chi::SubDomainId::kLocalContainers, 0),
  //     chi::DomainId(client.id_, chi::SubDomainId::kGlobalContainers));

  // size_t ops = 256;
  // HILOG(kInfo, "OPS: {}", ops);
  // int depth = 0;
  // for (size_t i = 0; i < ops; ++i) {
  //   int cont_id = i;
  //   int ret = client.Md(HSHM_DEFAULT_MEM_CTX,
  //                       chi::DomainQuery::GetDirectHash(
  //                           chi::SubDomainId::kGlobalContainers, cont_id),
  //                       depth, 0);
  //   REQUIRE(ret == 1);
}

int main() {
  CHIMAERA_CLIENT_INIT();
  test_kernel<<<1, 1>>>();
  HIP_ERROR_CHECK(hipDeviceSynchronize());
}
