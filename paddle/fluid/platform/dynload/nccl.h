/* Copyright (c) 2016 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */
#pragma once

#include <nccl.h>
#include <mutex>  // NOLINT

#include "paddle/pten/backends/dynload/nccl.h"

namespace paddle {
namespace platform {
namespace dynload {

#define PLATFORM_DECLARE_DYNAMIC_LOAD_NCCL_WRAP(__name)       \
  using DynLoad__##__name = pten::dynload::DynLoad__##__name; \
  extern DynLoad__##__name __name

#define NCCL_RAND_ROUTINE_EACH(__macro) \
  __macro(ncclCommInitAll);             \
  __macro(ncclGetUniqueId);             \
  __macro(ncclCommInitRank);            \
  __macro(ncclCommDestroy);             \
  __macro(ncclCommCount);               \
  __macro(ncclCommCuDevice);            \
  __macro(ncclCommUserRank);            \
  __macro(ncclAllReduce);               \
  __macro(ncclBcast);                   \
  __macro(ncclAllGather);               \
  __macro(ncclGroupStart);              \
  __macro(ncclGroupEnd);                \
  __macro(ncclReduce);                  \
  __macro(ncclReduceScatter);           \
  __macro(ncclGetErrorString);

NCCL_RAND_ROUTINE_EACH(PLATFORM_DECLARE_DYNAMIC_LOAD_NCCL_WRAP)

#if NCCL_VERSION_CODE >= 2212
#define NCCL_RAND_ROUTINE_EACH_AFTER_2212(__macro) __macro(ncclBroadcast);
NCCL_RAND_ROUTINE_EACH_AFTER_2212(PLATFORM_DECLARE_DYNAMIC_LOAD_NCCL_WRAP)
#endif

#if NCCL_VERSION_CODE >= 2304
#define NCCL_RAND_ROUTINE_EACH_AFTER_2304(__macro) __macro(ncclGetVersion);
NCCL_RAND_ROUTINE_EACH_AFTER_2304(PLATFORM_DECLARE_DYNAMIC_LOAD_NCCL_WRAP)
#endif

#if NCCL_VERSION_CODE >= 2703
#define NCCL_RAND_ROUTINE_EACH_AFTER_2703(__macro) \
  __macro(ncclSend);                               \
  __macro(ncclRecv);
NCCL_RAND_ROUTINE_EACH_AFTER_2703(PLATFORM_DECLARE_DYNAMIC_LOAD_NCCL_WRAP)
#endif

#if NCCL_VERSION_CODE >= 21100
#define NCCL_RAND_ROUTINE_EACH_AFTER_21100(__macro) \
  __macro(ncclRedOpCreatePreMulSum);                \
  __macro(ncclRedOpDestroy);
NCCL_RAND_ROUTINE_EACH_AFTER_21100(PLATFORM_DECLARE_DYNAMIC_LOAD_NCCL_WRAP)
#endif

}  // namespace dynload
}  // namespace platform
}  // namespace paddle
