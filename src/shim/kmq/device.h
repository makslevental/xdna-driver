// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024, Advanced Micro Devices, Inc. All rights reserved.

#ifndef _DEVICE_KMQ_H_
#define _DEVICE_KMQ_H_

#include "../device.h"
#include "core/common/memalign.h"

namespace shim_xdna {

class device_kmq : public device {
public:
  device_kmq(const pdev& pdev, handle_type shim_handle, id_type device_id);

  ~device_kmq();

  std::unique_ptr<bo>
  alloc_bo(void* userptr, hw_ctx::slot_id ctx_id,
    size_t size, uint64_t flags) override;

protected:
  std::unique_ptr<hw_ctx>
  create_hw_context(const device& dev, const xrt::xclbin& xclbin,
    const xrt::hw_context::qos_type& qos) const override;

  std::unique_ptr<bo>
  import_bo(shim_xdna::shared_handle::export_handle ehdl) const override;
};

} // namespace shim_xdna

#endif // _DEVICE_KMQ_H_
