/* Copyright (c) 2021 PaddlePaddle Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. */

#include "paddle/pten/kernels/copy_kernel.h"

#include "paddle/pten/backends/xpu/xpu_context.h"
#include "paddle/pten/common/data_type.h"
#include "paddle/pten/core/compat/convert_utils.h"
#include "paddle/pten/core/kernel_registry.h"

// See Note [ Why still include the fluid headers? ]
#include "paddle/fluid/memory/memcpy.h"

namespace pten {

template <typename Context>
void Copy(const Context& dev_ctx,
          const DenseTensor& src,
          bool blocking,
          DenseTensor* dst) {
  auto* src_ptr = src.data();
  auto* dst_ptr = dev_ctx.Alloc(dst, src.dtype());
  const auto& src_place = src.place();
  const auto& dst_place = dst->place();

  if (src_ptr == dst_ptr && src_place == dst_place) {
    VLOG(3) << "Skip copy the same data async from " << src_place << " to "
            << dst_place;
    return;
  }
  VLOG(4) << "src:" << src_ptr << ", dst:" << dst_ptr;

  VLOG(3) << "TensorCopy " << src.dims() << " from " << src.place() << " to "
          << dst_place;
  dst->ResizeAndAllocate(src.dims());
  CHECK(dst->layout() == src.layout());
  auto size = src.numel() * paddle::experimental::SizeOf(src.dtype());

  if (paddle::platform::is_xpu_place(src_place) &&  // NOLINT
      paddle::platform::is_cpu_place(dst_place)) {
    paddle::memory::Copy(dst_place, dst_ptr, src_place, src_ptr, size);
  } else if (paddle::platform::is_cpu_place(src_place) &&
             paddle::platform::is_xpu_place(dst_place)) {
    paddle::memory::Copy(dst_place, dst_ptr, src_place, src_ptr, size);
  } else if (paddle::platform::is_xpu_place(src_place) &&
             paddle::platform::is_xpu_place(dst_place)) {
    if (src_ptr == dst_ptr) {
      VLOG(3) << "Skip copy the same data async from " << src_place << " to "
              << dst_place;
      return;
    }
    paddle::memory::Copy(dst_place, dst_ptr, src_place, src_ptr, size);
  } else {
    PADDLE_THROW(paddle::platform::errors::Unimplemented(
        "Copy from %s to %s is not supported.", src_place, dst_place));
  }
}

}  // namespace pten

PT_REGISTER_GENERAL_KERNEL(
    copy, XPU, ALL_LAYOUT, pten::Copy<pten::XPUContext>, ALL_DTYPE) {}
