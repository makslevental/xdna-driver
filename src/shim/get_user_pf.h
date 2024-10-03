//
// Created by mlevental on 10/3/24.
//

#ifndef AMD_XDNA_GET_USER_PF_H
#define AMD_XDNA_GET_USER_PF_H

#include "core/pcie/linux/pcidev.h"
std::shared_ptr<xrt_core::device> my_get_userpf_device(xrt_core::device::id_type id);
void add_to_user_ready_list(const std::shared_ptr<xrt_core::pci::dev> &dev);

std::shared_ptr<xrt_core::device>
my_get_userpf_device(xrt_core::device::handle_type handle, xrt_core::device::id_type id);

std::shared_ptr<xrt_core::device>
my_get_userpf_device(xrt_core::device::handle_type handle);

#endif // AMD_XDNA_GET_USER_PF_H
