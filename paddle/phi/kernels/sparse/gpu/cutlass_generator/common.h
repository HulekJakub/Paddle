// Copyright (c) 2023 PaddlePaddle Authors. All Rights Reserved.
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

#ifdef PADDLE_WITH_CUTLASS
#include "cutlass/arch/mma.h"
#include "cutlass/epilogue/thread/linear_combination.h"
#include "cutlass/gemm/device/gemm_universal.h"
#include "cutlass/gemm/gemm.h"
#include "cutlass/half.h"
#include "cutlass/util/device_memory.h"
#include "examples/common/helper.h"
#include "paddle/phi/backends/gpu/gpu_context.h"
namespace phi {
namespace sparse {
#define TYPEDEF_KERNEL_POINTER(kernel, dtype)       \
  typedef void (*kernel)(dtype const alpha,         \
                         dtype const beta,          \
                         const GPUContext& dev_ctx, \
                         const dtype* const a,      \
                         const dtype* const b,      \
                         const dtype* const c,      \
                         dtype* const d,            \
                         const int m,               \
                         const int n,               \
                         const int k,               \
                         const int32_t* a_indices,  \
                         const int32_t* c_d_indices);
#define GATHER_GEMM_SCATTER_CHECK(status)                      \
  {                                                            \
    cutlass::Status error = status;                            \
    if (error != cutlass::Status::kSuccess) {                  \
      throw std::runtime_error(cutlassGetStatusString(error)); \
    }                                                          \
  }
#define DEFINE_LAUNCH_KERNEL(dtype, cutlass_type)                          \
  template <typename Gemm>                                                 \
  void launchKernel(dtype const alpha,                                     \
                    dtype const beta,                                      \
                    const GPUContext& dev_ctx,                             \
                    const dtype* const a,                                  \
                    const dtype* const b,                                  \
                    const dtype* const c,                                  \
                    dtype* const d,                                        \
                    const int m,                                           \
                    const int n,                                           \
                    const int k,                                           \
                    const int32_t* a_indices,                              \
                    const int32_t* c_d_indices) {                          \
    cutlass::gemm::GemmCoord problem_size_real({m, n, k});                 \
    int split_k_slices = 1;                                                \
    typename Gemm::Arguments arguments{                                    \
        cutlass::gemm::GemmUniversalMode::kGemm,                           \
        problem_size_real,                                                 \
        split_k_slices,                                                    \
        {static_cast<const cutlass_type>(static_cast<const float>(alpha)), \
         static_cast<const cutlass_type>(static_cast<const float>(beta))}, \
        reinterpret_cast<const cutlass_type* const>(a),                    \
        reinterpret_cast<const cutlass_type* const>(b),                    \
        reinterpret_cast<const cutlass_type* const>(c),                    \
        reinterpret_cast<cutlass_type* const>(d),                          \
        cutlass::layout::RowMajor().capacity(problem_size_real.mk()),      \
        cutlass::layout::RowMajor().capacity(problem_size_real.kn()),      \
        cutlass::layout::RowMajor().capacity(problem_size_real.mn()),      \
        cutlass::layout::RowMajor().capacity(problem_size_real.mn()),      \
        problem_size_real.k(),                                             \
        problem_size_real.n(),                                             \
        problem_size_real.n(),                                             \
        problem_size_real.n(),                                             \
        a_indices,                                                         \
        nullptr,                                                           \
        c_d_indices};                                                      \
    size_t workspace_size = Gemm::get_workspace_size(arguments);           \
    cutlass::device_memory::allocation<uint8_t> workspace(workspace_size); \
    Gemm gemm_op;                                                          \
    cutlass::Status status = gemm_op.can_implement(arguments);             \
    GATHER_GEMM_SCATTER_CHECK(status);                                     \
    status = gemm_op.initialize(arguments, workspace.get());               \
    GATHER_GEMM_SCATTER_CHECK(status);                                     \
    gemm_op(dev_ctx.stream());                                             \
  }

TYPEDEF_KERNEL_POINTER(fp16_gather_gemm_scatter, phi::dtype::float16)
TYPEDEF_KERNEL_POINTER(fp32_gather_gemm_scatter, float)

DEFINE_LAUNCH_KERNEL(phi::dtype::float16, cutlass::half_t)
DEFINE_LAUNCH_KERNEL(float, float)

}  // namespace sparse
}  // namespace phi
#endif
