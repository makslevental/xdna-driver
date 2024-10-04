// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024, Advanced Micro Devices, Inc. All rights reserved.

#ifndef _HWQ_XDNA_H_
#define _HWQ_XDNA_H_

#include "hwctx.h"
#include "fence.h"
#include "shim_debug.h"

#include "core/common/shim/hwqueue_handle.h"

namespace shim_xdna {
class bo;
class hw_q
{
public:
  hw_q(const device& device);

  virtual ~hw_q() = default;

  void
  submit_command(bo *);

  int
  poll_command(bo *) const;

  int
  wait_command(bo *, uint32_t timeout_ms) const;

  void
  submit_wait(const shim_xdna::fence_handle*);

  void
  submit_wait(const std::vector<shim_xdna::fence_handle*>&);

  void
  submit_signal(const shim_xdna::fence_handle*);

  std::unique_ptr<shim_xdna::fence_handle>
  import(shim_xdna::fence_handle::export_handle)
  { shim_not_supported_err(__func__); }

public:
  virtual void
  bind_hwctx(const hw_ctx *ctx) = 0;

  void
  unbind_hwctx();

  uint32_t
  get_queue_bo();

protected:
  virtual void
  issue_command(bo *) = 0;

  const hw_ctx *m_hwctx;
  const pdev& m_pdev;
  uint32_t m_queue_boh;
};

} // shim_xdna

#endif // _HWQ_XDNA_H_
