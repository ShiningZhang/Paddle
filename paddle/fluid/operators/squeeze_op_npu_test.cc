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

#ifndef _WIN32
#include <unistd.h>
#endif

#include <string>
#include <thread>  // NOLINT
#include <vector>

#include "gtest/gtest.h"
#include "paddle/fluid/framework/op_registry.h"
#include "paddle/fluid/framework/operator.h"
#include "paddle/fluid/framework/program_desc.h"
#include "paddle/fluid/operators/dropout_op.h"
#include "paddle/fluid/string/printf.h"
#include "paddle/pten/kernels/funcs/math_function.h"

namespace f = paddle::framework;
namespace p = paddle::platform;

USE_OP(squeeze);
USE_OP_DEVICE_KERNEL(squeeze, NPU);

template <typename T>
void Compare(f::Scope* scope, const p::DeviceContext& ctx) {
  // init
  auto x = scope->Var("X");
  auto tensor_x = x->GetMutable<f::LoDTensor>();

  int dim0 = 1;
  int dim1 = 10;
  int dim2 = 1;

  std::vector<T> init;
  for (int64_t i = 0; i < dim0 * dim1 * dim2; ++i) {
    init.push_back(static_cast<T>(0.1));
  }

  paddle::framework::TensorFromVector(init, ctx, tensor_x);
  tensor_x->Resize({dim0, dim1, dim2});

  ctx.Wait();

  // run
  auto place = ctx.GetPlace();
  auto out = scope->Var("Out");
  auto tensor_out = out->GetMutable<f::LoDTensor>();

  std::vector<int> axis;
  axis.push_back(2);
  f::AttributeMap attrs = {{"axes", axis}};

  auto op = f::OpRegistry::CreateOp("squeeze", {{"X", {"X"}}},
                                    {{"Out", {"Out"}}}, attrs);

  op->Run(*scope, place);
  ctx.Wait();

  EXPECT_EQ((uint32_t)tensor_out->dims().size(), uint32_t(2));
  EXPECT_EQ((uint32_t)tensor_out->dims()[0], uint32_t(dim0));
  EXPECT_EQ((uint32_t)tensor_out->dims()[1], uint32_t(dim1));

  std::vector<T> out_vec;
  paddle::framework::TensorToVector(*tensor_out, ctx, &out_vec);
  for (uint32_t i = 0; i < out_vec.size(); i++) {
    EXPECT_EQ(out_vec[i], static_cast<T>(0.1));
  }

  ctx.Wait();
}

TEST(squeeze, NPU_fp32) {
  f::Scope scope;
  auto* ctx = p::DeviceContextPool::Instance().Get(p::NPUPlace(0));
  Compare<float>(&scope, *ctx);
}
