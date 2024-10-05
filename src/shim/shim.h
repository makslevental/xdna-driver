// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2024, Advanced Micro Devices, Inc. All rights reserved.

#ifndef _SHIM_XDNA_H_
#define _SHIM_XDNA_H_

#include "device.h"
#include <memory>

namespace shim_xdna {

std::shared_ptr<device> my_get_userpf_device(device::id_type id);
void add_to_user_ready_list(const std::shared_ptr<pdev> &dev);

std::shared_ptr<pdev> get_dev(unsigned index);

std::shared_ptr<device> my_get_userpf_device(device::handle_type handle,
                                             device::id_type id);

std::shared_ptr<device> my_get_userpf_device(device::handle_type handle);

class shim {
public:
  std::shared_ptr<device> m_device;

  shim(device::id_type id) : m_device(my_get_userpf_device(this, id)) {
    shim_debug("creating shim for id %d", id);
  }

  ~shim() { shim_debug("destroying shim"); }
};

} // namespace shim_xdna

#endif
