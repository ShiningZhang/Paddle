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

#pragma once

#include "paddle/fluid/framework/data_type.h"
#include "paddle/fluid/framework/op_registry.h"
#include "paddle/fluid/framework/var_type.h"
#include "paddle/fluid/platform/device_context.h"

namespace pten {
class DenseTensor;
}  // namespace pten

namespace paddle {
namespace framework {
class Variable;
class SelectedRows;
}  // namespace framework
}  // namespace paddle

namespace paddle {
namespace operators {
class MemcpyD2HFunctor {
 public:
  MemcpyD2HFunctor(framework::Variable *out,
                   const platform::DeviceContext &dev_ctx,
                   const int dst_place_type)
      : out_(out), dev_ctx_(dev_ctx), dst_place_type_(dst_place_type) {}

  void operator()(const framework::LoDTensor &lod_tensor) const {
    auto &out_tensor = *out_->GetMutable<framework::LoDTensor>();
    CopyLoDTensor(lod_tensor, out_tensor);
  }

  void operator()(const framework::LoDTensorArray &array) const {
    auto &out_array = *out_->GetMutable<framework::LoDTensorArray>();
    out_array.clear();
    out_array.resize(array.size());

    for (size_t i = 0; i < array.size(); i++) {
      CopyLoDTensor(array[i], out_array[i]);
    }
  }

  void operator()(const pten::SelectedRows &rows) const {
    // (JZ-LIANG) to support SelectedRows
    PADDLE_THROW(platform::errors::Unimplemented(
        "Memcpy for SelectedRows is NOT support yet."));
  }

  template <typename T>
  void operator()(const T &v) const {
    PADDLE_ENFORCE_EQ(
        true, false,
        platform::errors::PermissionDenied(
            "Not support type for Memcpy  op with type %s", typeid(T).name()));
  }

 private:
  static constexpr size_t WAIT_THRESHOLD = 64 * 1024;
  void CopyLoDTensor(const framework::LoDTensor &src,
                     framework::LoDTensor &dst) const {  // NOLINT
    if (dst_place_type_ == 1) {
      framework::TensorCopy(src, platform::CUDAPinnedPlace(), dev_ctx_, &dst);
    } else if (dst_place_type_ == 0) {
      framework::TensorCopy(src, platform::CPUPlace(), dev_ctx_, &dst);
    } else {
      PADDLE_THROW(platform::errors::Unimplemented(
          "memcpy dst_place_type: %d is not supported yet.", dst_place_type_));
    }
    // NOTE(Aurelius84): host <-> device memory copies of a memory block of 64
    // KB or less are asynchronous. See
    // https://forums.developer.nvidia.com/t/host-device-memory-copies-up-to-64-kb-are-asynchronous/17907
    if (src.memory_size() <= WAIT_THRESHOLD) {
      dev_ctx_.Wait();
    }

    dst.set_lod(src.lod());
  }

  framework::Variable *out_;
  const platform::DeviceContext &dev_ctx_;
  const int dst_place_type_;
};

}  // namespace operators
}  // namespace paddle
