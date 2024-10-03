// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2024, Advanced Micro Devices, Inc. All rights reserved.
//
#include "drm_local/amdxdna_accel.h"
#include "pcidev.h"
#include "pcidrv.h"
#include <fstream>

namespace {

amdxdna_device_type
get_dev_type(const std::string& sysfs)
{
  const std::string sysfs_root{"/sys/bus/pci/devices/"};
  const std::string dev_type_path = sysfs_root + sysfs + "/device_type";

  std::ifstream ifs(dev_type_path);
  if (!ifs.is_open())
    throw std::invalid_argument(dev_type_path + " is missing?");

  std::string line;
  std::getline(ifs, line);
  return static_cast<amdxdna_device_type>(std::stoi(line));
}

}

namespace shim_xdna {

std::string
drv::
name() const
{
  return "amdxdna";
}

std::string
drv::
dev_node_prefix() const
{
  return "accel";
}

std::string
drv::
dev_node_dir() const
{
  return "accel";
}

std::string
drv::
sysfs_dev_node_dir() const
{
  return "accel";
}

bool
drv::
is_user() const
{
  return true;
}

} // namespace shim_xdna

