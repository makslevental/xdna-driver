// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2024, Advanced Micro Devices, Inc. All rights reserved.

#ifndef _SHIM_XDNA_H_
#define _SHIM_XDNA_H_

#include "device.h"
#include <memory>

namespace shim_xdna {

std::shared_ptr<shim_xdna::device> my_get_userpf_device(shim_xdna::device::id_type id);
void add_to_user_ready_list(const std::shared_ptr<shim_xdna::pdev> &dev);

std::shared_ptr<shim_xdna::pdev> get_dev(unsigned index, bool user);

std::shared_ptr<shim_xdna::device>
my_get_userpf_device(shim_xdna::device::handle_type handle, shim_xdna::device::id_type id);

std::shared_ptr<shim_xdna::device>
my_get_userpf_device(shim_xdna::device::handle_type handle);

class shim
{
public:
  shim(device::id_type id) : m_device(my_get_userpf_device(this, id))
  {}

  ~shim()
  {}

private:
  std::shared_ptr<device> m_device;
};


} // namespace shim_xdna

#endif
