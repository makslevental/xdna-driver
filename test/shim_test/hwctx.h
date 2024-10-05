// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024, Advanced Micro Devices, Inc. All rights reserved.

#ifndef _SHIMTEST_HWCTX_H_
#define _SHIMTEST_HWCTX_H_

#include "dev_info.h"
#include "../../src/shim/hwctx.h"
#include <iostream>


using namespace xrt_core;

class hw_ctx {
public:
  hw_ctx(shim_xdna::device* dev, const char *xclbin_name=nullptr)
  {
    auto path = get_xclbin_path(dev, xclbin_name);
    hw_ctx_init(dev, path);
  }

  shim_xdna::hw_ctx *
  get()
  {
    return m_handle.get();
  }

private:
  std::unique_ptr<shim_xdna::hw_ctx> m_handle;

  void
  hw_ctx_init(shim_xdna::device* dev, const std::string& xclbin_path)
  {
    xrt::xclbin xclbin;

    try {
      xclbin = xrt::xclbin(xclbin_path);
    } catch (...) {
      throw std::runtime_error(
        xclbin_path + " not found?\n"
        "specify xclbin path or run \"build.sh -xclbin_only\" to download them");
    }
    dev->record_xclbin(xclbin);
    auto xclbin_uuid = xclbin.get_uuid();
    shim_xdna::device::qos_type qos{ {"gops", 100} };
    m_handle = dev->create_hw_context(xclbin_uuid, qos);
    std::cout << "loaded " << xclbin_path << std::endl;
  }
};

#endif // _SHIMTEST_HWCTX_H_
