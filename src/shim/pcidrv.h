// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2024, Advanced Micro Devices, Inc. All rights reserved.

#ifndef _PCIDRV_XDNA_H_
#define _PCIDRV_XDNA_H_

#include <string>

namespace shim_xdna {

class drv
{
public:
  std::string
  name() const;

  bool
  is_user() const;

  std::string
  dev_node_prefix() const;

  std::string
  dev_node_dir() const;

  std::string
  sysfs_dev_node_dir() const;
};

} // namespace shim_xdna

#endif
