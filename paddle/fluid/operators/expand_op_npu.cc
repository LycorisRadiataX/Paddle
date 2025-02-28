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

#include <iostream>
#include <memory>
#include <string>

#include "paddle/fluid/framework/op_registry.h"
#include "paddle/fluid/operators/expand_op.h"

namespace paddle {
namespace operators {

template <typename DeviceContext, typename T>
class ExpandNPUKernel : public framework::OpKernel<T> {
 public:
  void Compute(const framework::ExecutionContext& context) const override {
    auto rank = context.Input<phi::DenseTensor>("X")->dims().size();
    PADDLE_ENFORCE_GE(
        rank,
        1,
        platform::errors::InvalidArgument(
            "The number of dimensions of the input 'x' for Op(expand) "
            "must be greater than or equal to 1, but the value received is %d.",
            rank));
    PADDLE_ENFORCE_LE(
        rank,
        MAX_RANK_SUPPORTED,
        platform::errors::InvalidArgument(
            "The number of dimensions of the input 'x' for Op(expand) "
            "must be less than or equal to %d, but the value received is %d.",
            MAX_RANK_SUPPORTED,
            rank));
    switch (rank) {
      case 1:
        Expand<1>(context);
        break;
      case 2:
        Expand<2>(context);
        break;
      case 3:
        Expand<3>(context);
        break;
      case 4:
        Expand<4>(context);
        break;
      case 5:
        Expand<5>(context);
        break;
      case 6:
        Expand<6>(context);
        break;
    }
  }

 protected:
  template <int Rank>
  void Expand(const framework::ExecutionContext& context) const {
    auto* in0 = context.Input<phi::DenseTensor>("X");
    auto in_dims = in0->dims();
    auto expand_times = get_expand_times(context);
    PADDLE_ENFORCE_EQ(static_cast<size_t>(in_dims.size()),
                      expand_times.size(),
                      platform::errors::InvalidArgument(
                          "The number of elements (%d) of 'expand_times' for "
                          "Op(expand) must be equal to the number "
                          "of dimensions (%d) of the input.",
                          expand_times.size(),
                          static_cast<size_t>(in_dims.size())));
    auto* out0 = context.Output<phi::DenseTensor>("Out");
    framework::DDim out_dims(in_dims);

    for (size_t i = 0; i < expand_times.size(); ++i) {
      out_dims[i] *= expand_times[i];
    }

    auto place = context.GetPlace();
    auto stream =
        context.template device_context<paddle::platform::NPUDeviceContext>()
            .stream();

    out0->Resize(out_dims);
    out0->mutable_data<T>(place);

    bool is_expand_times_all_one =
        (out0->numel() == in0->numel()) ? true : false;

    if (is_expand_times_all_one) {
      memory::Copy(place,
                   out0->mutable_data<T>(place),
                   place,
                   in0->data<T>(),
                   in0->numel() * sizeof(T),
                   stream);
      if (out_dims != in_dims) {
        out0->Resize(out_dims);
      }
    } else {
      const auto& runner =
          NpuOpRunner("TileD", {*in0}, {*out0}, {{"multiples", expand_times}});
      runner.Run(stream);
    }
  }
};
}  // namespace operators
}  // namespace paddle

namespace ops = paddle::operators;
REGISTER_OP_NPU_KERNEL(
    expand,
    ops::ExpandNPUKernel<paddle::platform::NPUDeviceContext, float>,
    ops::ExpandNPUKernel<paddle::platform::NPUDeviceContext, int>,
    ops::ExpandNPUKernel<paddle::platform::NPUDeviceContext,
                         paddle::platform::float16>);
