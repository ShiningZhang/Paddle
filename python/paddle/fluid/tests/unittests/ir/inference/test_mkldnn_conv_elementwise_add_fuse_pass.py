# Copyright (c) 2021 PaddlePaddle Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

from auto_scan_test import PassAutoScanTest, SkipReasons
from program_config import TensorConfig, ProgramConfig, OpConfig
import numpy as np
import paddle.inference as paddle_infer
from functools import partial
from typing import Optional, List, Callable, Dict, Any, Set
import unittest

import hypothesis
from hypothesis import given, settings, seed, example, assume
import hypothesis.strategies as st


class TestConvElementwiseAddMkldnnFusePass(PassAutoScanTest):
    def is_program_valid(self, program_config: ProgramConfig) -> bool:
        attrs = [
            program_config.ops[i].attrs
            for i in range(len(program_config.ops))
        ]
        # If the problem has been fixed, the judgment 
        # needs to be deleted!!!
        if attrs[1]['data_format'] == "NHWC":
            return False

        return True

    def sample_program_config(self, draw):
        data_format = draw(st.sampled_from(["NCHW", "NHWC"]))
        dilations = draw(st.sampled_from([[1, 1], [2, 2], [1, 2]]))
        padding_algorithm = draw(st.sampled_from(["EXPLICIT", "SAME", "VALID"]))
        groups = draw(st.sampled_from([1, 2, 4]))
        paddings = draw(st.sampled_from([[0, 3], [1, 1], [1, 2, 3, 4]]))
        strides = draw(st.sampled_from([[1, 1], [2, 2], [1, 2]]))
        axis = draw(st.sampled_from([-1, 0, 1]))
        batch_size = draw(st.integers(min_value=1, max_value=4))

        def generate_input1():
            if data_format == "NCHW":
                return np.random.random(
                    [batch_size, 48, 64, 64]).astype(np.float32)
            else:
                return np.random.random(
                    [batch_size, 64, 64, 48]).astype(np.float32)

        def generate_weight1():
            return np.random.random(
                [48, int(48 / groups), 3, 3]).astype(np.float32)

        def compute_out_shape(padding_alg):
            import paddle
            import paddle.nn as nn

            x_var = paddle.uniform(
                (batch_size, 48, 64, 64), dtype='float32', min=-1., max=1.)
            if padding_alg == "EXPLICIT":
                conv = nn.Conv2D(48, 48, (3, 3), strides, paddings, dilations,
                                 1)
            else:
                conv = nn.Conv2D(48, 48, (3, 3), strides, padding_alg,
                                 dilations, 1)
            y_var = conv(x_var)
            return y_var.shape

        def generate_weight2():
            return np.random.random([48]).astype(np.float32)

        if compute_out_shape(padding_algorithm) != (batch_size, 48, 64, 64):
            axis = 1

        relu_op = OpConfig(
            type="relu",
            inputs={"X": ["input_data1"]},
            outputs={"Out": ["sigmoid_out"]},
            attrs={})

        conv2d_op = OpConfig(
            type="conv2d",
            inputs={"Input": ["sigmoid_out"],
                    "Filter": ["conv_weight"]},
            outputs={"Output": ["conv_output"]},
            attrs={
                "data_format": data_format,
                "dilations": dilations,
                "padding_algorithm": padding_algorithm,
                "groups": groups,
                "paddings": paddings,
                "strides": strides
            })

        if axis == -1 or axis == 0:
            elt_op = OpConfig(
                type="elementwise_add",
                inputs={"X": ["input_data1"],
                        "Y": ["conv_output"]},
                outputs={"Out": ["elementwise_output"]},
                attrs={'axis': axis})
        else:
            elt_op = OpConfig(
                type="elementwise_add",
                inputs={"X": ["conv_output"],
                        "Y": ["elementwise_weight"]},
                outputs={"Out": ["elementwise_output"]},
                attrs={'axis': axis})

        model_net = [relu_op, conv2d_op, elt_op]

        if axis == 1:
            program_config = ProgramConfig(
                ops=model_net,
                weights={
                    "conv_weight":
                    TensorConfig(data_gen=partial(generate_weight1)),
                    "elementwise_weight":
                    TensorConfig(data_gen=partial(generate_weight2))
                },
                inputs={
                    "input_data1":
                    TensorConfig(data_gen=partial(generate_input1))
                },
                outputs=["elementwise_output"])
        else:
            program_config = ProgramConfig(
                ops=model_net,
                weights={
                    "conv_weight":
                    TensorConfig(data_gen=partial(generate_weight1))
                },
                inputs={
                    "input_data1":
                    TensorConfig(data_gen=partial(generate_input1))
                },
                outputs=["elementwise_output"])

        return program_config

    def sample_predictor_configs(self, program_config):
        config = self.create_inference_config(use_mkldnn=True)
        yield config, ["relu", "conv2d"], (1e-5, 1e-5)

    def test(self):
        self.run_and_statis(
            quant=False, passes=["conv_elementwise_add_mkldnn_fuse_pass"])


if __name__ == "__main__":
    unittest.main()
