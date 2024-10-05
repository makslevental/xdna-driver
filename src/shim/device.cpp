// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2024, Advanced Micro Devices, Inc. - All rights reserved

#include "device.h"
#include "bo.h"
#include "fence.h"
#include "hwctx.h"
#include "shim.h"

#include <any>
#include <cassert>
#include <sys/syscall.h>
#include <unistd.h>

namespace {

int
import_fd(pid_t pid, int ehdl)
{
  if (pid == 0 || getpid() == pid)
    return ehdl;

#if defined(SYS_pidfd_open) && defined(SYS_pidfd_getfd)
  auto pidfd = syscall(SYS_pidfd_open, pid, 0);
  if (pidfd < 0)
    throw std::system_error(errno, std::system_category(), "pidfd_open failed");

  auto fd = syscall(SYS_pidfd_getfd, pidfd, ehdl, 0);
  if (fd < 0) {
    if (errno == EPERM) {
      throw std::system_error
        (errno, std::system_category(), "pidfd_getfd failed, check that ptrace access mode "
        "allows PTRACE_MODE_ATTACH_REALCREDS.  For more details please "
        "check /etc/sysctl.d/10-ptrace.conf");
    } else {
      throw std::system_error(errno, std::system_category(), "pidfd_getfd failed");
    }
  }
  return fd;
#else
  throw xrt_core::system_error
    (std::errc::not_supported,
     "Importing buffer object from different process requires XRT "
     " built and installed on a system with 'pidfd' kernel support");
  return -1;
#endif
}

}

namespace shim_xdna {

device::
device(const pdev& pdev, handle_type shim_handle, id_type device_id)
  : m_handle(shim_handle), m_device_id(device_id), m_userpf(!pdev.m_is_mgmt)
  , m_pdev(pdev)
{
  m_pdev.open();
}

device::
~device()
{
  m_pdev.close();
}


const pdev&
device::
get_pdev() const
{
  return m_pdev;
}

xclDeviceHandle
device::
get_device_handle() const
{
  return m_handle;
}

void
device::
close_device()
{
  auto s = reinterpret_cast<shim_xdna::shim*>(get_device_handle());
  if (s)
    delete s;
  // When shim is gone, the last ref to this device object will be removed
  // which will cause this object to be destruted. We're essentially committing
  // suicide here. Do not touch anything in this device object after this.
}

device::id_type
device::get_device_id() const {
    return 0;
}

xrt::xclbin
device::
get_xclbin(const xrt::uuid& xclbin_id) const
{
  // Allow access to xclbin in process of loading via device::load_xclbin
  if (xclbin_id && xclbin_id == m_xclbin.get_uuid())
    return m_xclbin;
  //  if (xclbin_id) {
  //    std::lock_guard lk(m_mutex);
  //    return m_xclbins.get(xclbin_id);
  //  }
  throw std::runtime_error("TODO(max):multi-xclbin");

  // Single xclbin case
  return m_xclbin;
}

std::unique_ptr<hw_ctx>
device::
create_hw_context(const xrt::uuid& xclbin_uuid, const qos_type& qos) const
{
  return create_hw_context(*this, get_xclbin(xclbin_uuid), qos);
}

std::unique_ptr<bo>
device::
alloc_bo(size_t size, uint64_t flags)
{
  return alloc_bo(nullptr, size, flags);
}

std::unique_ptr<bo>
device::
alloc_bo(void* userptr, size_t size, uint64_t flags)
{
  return alloc_bo(userptr, AMDXDNA_INVALID_CTX_HANDLE, size, flags);
}

std::unique_ptr<bo>
device::
import_bo(pid_t pid, shim_xdna::shared_handle::export_handle ehdl)
{
  return import_bo(import_fd(pid, ehdl));
}

std::unique_ptr<shim_xdna::fence_handle>
device::
create_fence(fence_handle::access_mode)
{
  return std::make_unique<fence_handle>(*this);
}

std::unique_ptr<shim_xdna::fence_handle>
device::
import_fence(pid_t pid, shim_xdna::shared_handle::export_handle ehdl)
{
  return std::make_unique<fence_handle>(*this, import_fd(pid, ehdl));
}

void
device::
record_xclbin(const xrt::xclbin& xclbin)
{
  std::lock_guard lk(m_mutex);
  m_xclbins.insert(xclbin);
  // For single xclbin case, where shim doesn't implement
  // kds_cu_info, we need the current xclbin stored here
  // as a temporary 'global'.  This variable is used when
  // update_cu_info() is called and query:kds_cu_info is not
  // implemented
  m_xclbin = xclbin;
}

} // namespace shim_xdna
