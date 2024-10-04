// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2023-2024, Advanced Micro Devices, Inc. All rights reserved.

#ifndef _DEVICE_UMQ_H_
#define _DEVICE_UMQ_H_

#include "../device.h"

namespace shim_xdna {

class device_umq : public device {
public:
  device_umq(const pdev& pdev, handle_type shim_handle, id_type device_id);

  ~device_umq();

  std::unique_ptr<bo>
  alloc_bo(void* userptr, xrt_core::hwctx_handle::slot_id ctx_id,
    size_t size, uint64_t flags) override;

protected:
  std::unique_ptr<hw_ctx>
  create_hw_context(const device& dev, const xrt::xclbin& xclbin,
    const xrt::hw_context::qos_type& qos) const override;

  std::unique_ptr<bo>
  import_bo(xrt_core::shared_handle::export_handle ehdl) const override;
};

} // namespace shim_xdna

#endif // _DEVICE_UMQ_H_
