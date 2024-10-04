// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024, Advanced Micro Devices, Inc. All rights reserved.

#ifndef _BO_UMQ_H_
#define _BO_UMQ_H_

#include "../bo.h"

namespace shim_xdna {

class bo_umq : public bo {
public:
  bo_umq(const device& device, hw_ctx::slot_id ctx_id,
    size_t size, uint64_t flags);

  bo_umq(const device& device, shim_xdna::shared_handle::export_handle ehdl);

  ~bo_umq();

  void
  sync(direction dir, size_t size, size_t offset) override;

  void
  bind_at(size_t pos, const bo* bh, size_t offset, size_t size) override;

  bo_umq(const device& device, hw_ctx::slot_id ctx_id,
    size_t size, uint64_t flags, amdxdna_bo_type type);

};

} // namespace shim_xdna

#endif // _BO_UMQ_H_
