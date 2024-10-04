// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024, Advanced Micro Devices, Inc. All rights reserved.

#ifndef _FENCE_XDNA_H_
#define _FENCE_XDNA_H_

#include "shared.h"

#include "shim_debug.h"
#include <mutex>
#include <vector>

namespace shim_xdna {
class pdev;
class device;
class hw_ctx;
class fence_handle
{
public:
  using export_handle = int;
  enum class access_mode : uint8_t { local, shared, process, hybrid };

  fence_handle(const device& device);

  fence_handle(const device& device, shim_xdna::shared_handle::export_handle ehdl);

  fence_handle(const fence_handle&);

  ~fence_handle();

  std::unique_ptr<shim_xdna::fence_handle>
  clone() const;

  std::unique_ptr<shim_xdna::shared_handle>
  share_handle() const;

  void
  wait(uint32_t timeout_ms) const;

  uint64_t
  get_next_state() const;

  void
  signal() const;

  void
  submit_wait(const hw_ctx*) const;

  static void
  submit_wait(const pdev& dev, const hw_ctx*, const std::vector<shim_xdna::fence_handle*>& fences);

  void
  submit_signal(const hw_ctx*) const;

  uint64_t
  wait_next_state() const;

  uint64_t
  signal_next_state() const;

  const pdev& m_pdev;
  const std::unique_ptr<shim_xdna::shared_handle> m_import;
  uint32_t m_syncobj_hdl;

  // Protecting below mutables
  mutable std::mutex m_lock;
  // Set once at first signal
  mutable bool m_signaled = false;
  // Ever incrementing at each wait/signal
  static constexpr uint64_t initial_state = 0;
  mutable uint64_t m_state = initial_state;
};

} // shim_xdna

#endif // _FENCE_XDNA_H_
