// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024, Advanced Micro Devices, Inc. All rights reserved.

#ifndef _HWQ_XDNA_H_
#define _HWQ_XDNA_H_

#include "fence.h"
#include "hwctx.h"
#include "shim_debug.h"

namespace shim_xdna {
class bo;
class hw_q {
public:
  const hw_ctx *m_hwctx;
  const pdev &m_pdev;
  uint32_t m_queue_boh;

  hw_q(const device &device);
  virtual ~hw_q() = default;
  void submit_command(bo *);
  int poll_command(bo *) const;
  int wait_command(bo *, uint32_t timeout_ms) const;
  void submit_wait(const fence_handle *);
  void submit_wait(const std::vector<fence_handle *> &);
  void submit_signal(const fence_handle *);

  std::unique_ptr<fence_handle> import(fence_handle::export_handle) {
    shim_not_supported_err(__func__);
  }

  virtual void bind_hwctx(const hw_ctx *ctx) = 0;
  void unbind_hwctx();
  uint32_t get_queue_bo();
  virtual void issue_command(bo *) = 0;
};

class hw_q_kmq : public hw_q {
public:
  hw_q_kmq(const device &device);
  ~hw_q_kmq() override;
  void bind_hwctx(const hw_ctx *ctx) override;
  void issue_command(bo *) override;
};

} // namespace shim_xdna

#endif // _HWQ_XDNA_H_
