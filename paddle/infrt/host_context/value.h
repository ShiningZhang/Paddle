// Copyright (c) 2021 PaddlePaddle Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once
#include <glog/logging.h>
#include <llvm/ADT/SmallVector.h>

#include <string>
#include <utility>
#include <vector>

#include "paddle/infrt/common/object.h"
#include "paddle/infrt/common/shared.h"
#include "paddle/infrt/host_context/function.h"
#include "paddle/infrt/support/variant.h"
#include "paddle/infrt/tensor/dense_host_tensor.h"
#include "paddle/infrt/tensor/dense_tensor_view.h"
#include "paddle/infrt/tensor/tensor_map.h"
#include "paddle/infrt/tensor/tensor_shape.h"

#ifdef INFRT_WITH_PTEN
#include "paddle/infrt/backends/host/pten_allocator.h"
#include "paddle/infrt/backends/host/pten_context.h"
#include "paddle/pten/backends/all_context.h"
#include "paddle/pten/common/backend.h"
#include "paddle/pten/common/data_type.h"
#include "paddle/pten/common/layout.h"
#include "paddle/pten/common/scalar.h"
#include "paddle/pten/common/scalar_array.h"
#include "paddle/pten/core/dense_tensor.h"
#include "paddle/pten/core/meta_tensor.h"
#endif

namespace infrt {
namespace host_context {

struct MlirFunctionExecutable;

using ValueVariantType =
    Variant<int16_t,
            int32_t,
            int64_t,
            float,
            double,
            bool,
            uint32_t,
            uint64_t,
            std::string,
            tensor::TensorShape,
            tensor::DenseHostTensor,
            MlirFunctionExecutable*,
            tensor::TensorMap,
#ifdef INFRT_WITH_PTEN
            ::pten::MetaTensor,
            ::pten::DenseTensor,
            backends::CpuPtenAllocator,
            backends::CpuPtenContext,
            ::pten::CPUContext,
            std::vector<pten::DenseTensor>,
            paddle::experimental::ScalarBase<pten::DenseTensor>,
            paddle::experimental::ScalarArrayBase<pten::DenseTensor>,
            std::vector<pten::MetaTensor>,
            pten::MetaConfig,
            paddle::experimental::Backend,
            paddle::experimental::DataLayout,
            paddle::experimental::DataType,
#endif
            std::vector<int16_t>,
            std::vector<int32_t>,
            std::vector<int64_t>,
            std::vector<float>,
            std::vector<double>>;

//! Copy content from \param from to \param to.
void CopyTo(const Value& from, Value* to);

/**
 * Represents any data type for value in host context.
 */
class Value : public common::Object {
 public:
  using variant_type = ValueVariantType;

  explicit Value() {}  // NOLINT
  explicit Value(int32_t x) : data(x) {}
  explicit Value(int64_t x) : data(x) {}
  explicit Value(float x) : data(x) {}
  explicit Value(double x) : data(x) {}
  explicit Value(bool x) : data(x) {}
  explicit Value(std::string x) : data(x) {}
  explicit Value(tensor::TensorMap&& x) : data(x) {}
  explicit Value(std::vector<int16_t>&& x) : data(x) {}
  explicit Value(std::vector<int32_t>&& x) : data(x) {}
  explicit Value(std::vector<int64_t>&& x) : data(x) {}
  explicit Value(std::vector<float>&& x) : data(x) {}
  explicit Value(std::vector<double>&& x) : data(x) {}
  explicit Value(tensor::TensorShape&& x) : data(std::move(x)) {}
  explicit Value(tensor::DenseHostTensor&& x) : data(std::move(x)) {}
  explicit Value(MlirFunctionExecutable* x) : data(x) {}
#ifdef INFRT_WITH_PTEN
  explicit Value(backends::CpuPtenContext&& x) : data(std::move(x)) {}
  explicit Value(::pten::CPUContext&& x) : data(std::move(x)) {}
  explicit Value(::pten::DenseTensor&& x) : data(std::move(x)) {}
  explicit Value(::pten::MetaTensor&& x) : data(std::move(x)) {}
  explicit Value(backends::CpuPtenAllocator&& x) : data(std::move(x)) {}
#endif

  template <typename T>
  const T& get() const {
    CHECK(data.template is<T>());
    return data.get<T>();
  }

  template <typename T>
  T& get() {
    CHECK(data.template is<T>());
    return data.get<T>();
  }

  //! Get the value if assigned before or return a default value instead.
  template <class T>
  T& get_or_default() {
    if (!data.template is<T>()) {
      this->set(T{});
    }
    return get<T>();
  }

  template <typename T>
  void set(T&& v) {
    data = std::move(v);
  }

  void set(Value* v) { data = std::move(v->data); }

  bool valid() const { return true; }

  template <typename T>
  bool is_type() const {
    return data.template is<T>();
  }

  const char* type_info() const override;

  friend void CopyTo(const Value& from, Value* to);

 private:
  ValueVariantType data;
  static constexpr const char* __type_info__ = "host_context_value";
};

/**
 * Represents a counted reference of a Value.
 */
class ValueRef : common::Shared<Value> {
 public:
  ValueRef() = default;
  explicit ValueRef(Value* n) : common::Shared<Value>(n) {}
  explicit ValueRef(int32_t val);
  explicit ValueRef(int64_t val);
  explicit ValueRef(float val);
  explicit ValueRef(double val);
  explicit ValueRef(bool val);
  explicit ValueRef(::pten::MetaTensor&& val);
  explicit ValueRef(backends::CpuPtenContext&& x);
  explicit ValueRef(::pten::CPUContext&& x);
  explicit ValueRef(::pten::DenseTensor&& x);

  using common::Shared<Value>::get;
  using common::Shared<Value>::Reset;
  using common::Shared<Value>::operator->;
  using common::Shared<Value>::operator*;

  //! Get a readonly data.
  template <typename T>
  const T& get() const {
    CHECK(p_);
    return p_->get<T>();
  }

  template <typename T>
  T& get() {
    CHECK(p_);
    return p_->get<T>();
  }

  //! Assign a data.
  template <typename T>
  void Assign(const T& x) {
    if (!p_) {
      p_ = common::make_shared<Value>();
    }
    *p_ = x;
  }

  template <typename T, typename... Args>
  void Assign(Args... args) {
    p_ = common::make_shared<T>(std::forward<Args>(args)...);
  }

  inline bool IsValid() { return p_; }
};

}  // namespace host_context
}  // namespace infrt
