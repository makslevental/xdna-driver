// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2024, Advanced Micro Devices, Inc. All rights reserved.
//
#include "pcidrv.h"
#include "amdxdna_accel.h"
#include "kmq/pcidev.h"
#include "pcidev.h"
#include "umq/pcidev.h"
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

std::shared_ptr<pdev>
drv::
create_pcidev(const std::string& sysfs) const
{
  auto t = get_dev_type(sysfs);
  auto driver = std::static_pointer_cast<const drv>(shared_from_this());
  if (t == AMDXDNA_DEV_TYPE_KMQ)
    return std::make_shared<pdev_kmq>(driver, sysfs);
  if (t == AMDXDNA_DEV_TYPE_UMQ)
    return std::make_shared<pdev_umq>(driver, sysfs);
  shim_err(-EINVAL, "Unknown device type: %d", t);
}

} // namespace shim_xdna

