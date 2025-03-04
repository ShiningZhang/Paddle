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

USE_OP(assign);
USE_OP_DEVICE_KERNEL(assign, NPU);

template <typename T>
void Compare(f::Scope* scope, const p::DeviceContext& ctx,
             std::string op_type) {
  // init
  auto x = scope->Var("X");
  auto tensor_x = x->GetMutable<f::LoDTensor>();

  std::vector<T> init;
  init.push_back(static_cast<T>(1.0));
  init.push_back(static_cast<T>(2.0));
  init.push_back(static_cast<T>(3.0));
  init.push_back(static_cast<T>(4.0));

  paddle::framework::TensorFromVector(init, ctx, tensor_x);
  tensor_x->Resize({4});

  ctx.Wait();

  auto place = ctx.GetPlace();
  auto out = scope->Var("Out");
  auto tensor_out = out->GetMutable<f::LoDTensor>();

  auto op =
      f::OpRegistry::CreateOp(op_type, {{"X", {"X"}}}, {{"Out", {"Out"}}}, {});

  op->Run(*scope, place);

  std::vector<T> out_vec;
  paddle::framework::TensorToVector(*tensor_out, ctx, &out_vec);

  ctx.Wait();

  EXPECT_EQ((uint32_t)out_vec.size(), (uint32_t)4);
  EXPECT_EQ(out_vec[0], static_cast<T>(1.0));
  EXPECT_EQ(out_vec[1], static_cast<T>(2.0));
  EXPECT_EQ(out_vec[2], static_cast<T>(3.0));
  EXPECT_EQ(out_vec[3], static_cast<T>(4.0));
}

TEST(assign, NPU_fp32) {
  f::Scope scope;
  auto* ctx = p::DeviceContextPool::Instance().Get(p::NPUPlace(0));
  Compare<float>(&scope, *ctx, "assign");
}
