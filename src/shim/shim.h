// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2024, Advanced Micro Devices, Inc. All rights reserved.

#ifndef _SHIM_XDNA_H_
#define _SHIM_XDNA_H_

#include "get_user_pf.h"

namespace shim_xdna {

class shim
{
public:
  shim(xrt_core::device::id_type id) : m_device(my_get_userpf_device(this, id))
  {}

  ~shim()
  {}

private:
  std::shared_ptr<xrt_core::device> m_device;
};

} // namespace shim_xdna

#endif
