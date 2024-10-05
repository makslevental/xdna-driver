// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2024, Advanced Micro Devices, Inc. - All rights reserved

#include "device.h"
#include "bo.h"
#include "fence.h"
#include "hwctx.h"
#include "shim.h"

#include <cassert>
#include <sys/syscall.h>
#include <unistd.h>

namespace {

int import_fd(pid_t pid, int ehdl) {
  if (pid == 0 || getpid() == pid)
    return ehdl;

#if defined(SYS_pidfd_open) && defined(SYS_pidfd_getfd)
  auto pidfd = syscall(SYS_pidfd_open, pid, 0);
  if (pidfd < 0)
    throw std::system_error(errno, std::system_category(), "pidfd_open failed");

  auto fd = syscall(SYS_pidfd_getfd, pidfd, ehdl, 0);
  if (fd < 0) {
    if (errno == EPERM) {
      throw std::system_error(
          errno, std::system_category(),
          "pidfd_getfd failed, check that ptrace access mode "
          "allows PTRACE_MODE_ATTACH_REALCREDS.  For more details please "
          "check /etc/sysctl.d/10-ptrace.conf");
    } else {
      throw std::system_error(errno, std::system_category(),
                              "pidfd_getfd failed");
    }
  }
  return fd;
#else
  throw xrt_core::system_error(
      std::errc::not_supported,
      "Importing buffer object from different process requires XRT "
      " built and installed on a system with 'pidfd' kernel support");
  return -1;
#endif
}

} // namespace

namespace shim_xdna {

device::device(const pdev &pdev, handle_type shim_handle, id_type device_id)
    : m_handle(shim_handle), m_device_id(device_id), m_userpf(true),
      m_pdev(pdev) {
  m_pdev.open();
}

device::~device() { m_pdev.close(); }

const pdev &device::get_pdev() const { return m_pdev; }

void *device::get_device_handle() const { return m_handle; }

void device::close_device() {
  auto s = reinterpret_cast<shim *>(get_device_handle());
  if (s)
    delete s;
  // When shim is gone, the last ref to this device object will be removed
  // which will cause this object to be destruted. We're essentially committing
  // suicide here. Do not touch anything in this device object after this.
}

device::id_type device::get_device_id() const { return 0; }

xrt::xclbin device::get_xclbin(const xrt::uuid &xclbin_id) const {
  // Allow access to xclbin in process of loading via device::load_xclbin
  if (xclbin_id && xclbin_id == m_xclbin.get_uuid())
    return m_xclbin;
  throw std::runtime_error("TODO(max):multi-xclbin");
}

std::unique_ptr<hw_ctx> device::create_hw_context(const xrt::uuid &xclbin_uuid,
                                                  const qos_type &qos) const {
  return create_hw_context(*this, get_xclbin(xclbin_uuid), qos);
}

std::unique_ptr<bo> device::alloc_bo(size_t size, uint64_t flags) {
  return alloc_bo(nullptr, size, flags);
}

std::unique_ptr<bo> device::alloc_bo(void *userptr, size_t size,
                                     uint64_t flags) {
  return alloc_bo(userptr, AMDXDNA_INVALID_CTX_HANDLE, size, flags);
}

std::unique_ptr<bo> device::import_bo(pid_t pid,
                                      shared_handle::export_handle ehdl) {
  return import_bo(import_fd(pid, ehdl));
}

std::unique_ptr<fence_handle> device::create_fence(fence_handle::access_mode) {
  return std::make_unique<fence_handle>(*this);
}

std::unique_ptr<fence_handle>
device::import_fence(pid_t pid, shared_handle::export_handle ehdl) {
  return std::make_unique<fence_handle>(*this, import_fd(pid, ehdl));
}

void device::record_xclbin(const xrt::xclbin &xclbin) {
  std::lock_guard lk(m_mutex);
  m_xclbins.insert(xclbin);
  m_xclbin = xclbin;
}

device_kmq::device_kmq(const pdev &pdev, handle_type shim_handle,
                       id_type device_id)
    : device(pdev, shim_handle, device_id) {
  shim_debug("Created KMQ device (%s) ...");
}

device_kmq::~device_kmq() { shim_debug("Destroying KMQ device (%s) ..."); }

std::unique_ptr<hw_ctx>
device_kmq::create_hw_context(const device &dev, const xrt::xclbin &xclbin,
                              const hw_ctx::qos_type &qos) const {
  return std::make_unique<hw_ctx_kmq>(dev, xclbin, qos);
}

std::unique_ptr<bo> device_kmq::alloc_bo(void *userptr, hw_ctx::slot_id ctx_id,
                                         size_t size, uint64_t flags) {
  if (userptr)
    shim_not_supported_err("User ptr BO");

  return std::make_unique<bo_kmq>(*this, ctx_id, size, flags);
}

std::unique_ptr<bo>
device_kmq::import_bo(shared_handle::export_handle ehdl) const {
  return std::make_unique<bo_kmq>(*this, ehdl);
}

std::vector<char> device::read_aie_mem(uint16_t col, uint16_t row,
                                       uint32_t offset, uint32_t size) {
  amdxdna_drm_aie_mem mem;
  std::vector<char> store_buf(size);
  mem.col = col;
  mem.row = row;
  mem.addr = offset;
  mem.size = size;
  mem.buf_p = reinterpret_cast<uintptr_t>(store_buf.data());
  amdxdna_drm_get_info arg = {.param = DRM_AMDXDNA_READ_AIE_MEM,
                              .buffer_size = sizeof(mem),
                              .buffer = reinterpret_cast<uintptr_t>(&mem)};
  m_pdev.ioctl(DRM_IOCTL_AMDXDNA_GET_INFO, &arg);
  return store_buf;
}

uint32_t device::read_aie_reg(uint16_t col, uint16_t row, uint32_t reg_addr) {
  amdxdna_drm_aie_reg reg;
  reg.col = col;
  reg.row = row;
  reg.addr = reg_addr;
  reg.val = 0;
  amdxdna_drm_get_info arg = {.param = DRM_AMDXDNA_READ_AIE_REG,
                              .buffer_size = sizeof(reg),
                              .buffer = reinterpret_cast<uintptr_t>(&reg)};
  m_pdev.ioctl(DRM_IOCTL_AMDXDNA_GET_INFO, &arg);
  return reg.val;
}

size_t device::write_aie_mem(uint16_t col, uint16_t row, uint32_t offset,
                             const std::vector<char> &buf) {
  amdxdna_drm_aie_mem mem;
  uint32_t size = static_cast<uint32_t>(buf.size());
  mem.col = col;
  mem.row = row;
  mem.addr = offset;
  mem.size = size;
  mem.buf_p = reinterpret_cast<uintptr_t>(buf.data());
  amdxdna_drm_get_info arg = {.param = DRM_AMDXDNA_WRITE_AIE_MEM,
                              .buffer_size = sizeof(mem),
                              .buffer = reinterpret_cast<uintptr_t>(&mem)};
  m_pdev.ioctl(DRM_IOCTL_AMDXDNA_SET_STATE, &arg);
  return size;
}

void device::write_aie_reg(uint16_t col, uint16_t row, uint32_t reg_addr,
                           uint32_t reg_val) {
  amdxdna_drm_aie_reg reg;
  reg.col = col;
  reg.row = row;
  reg.addr = reg_addr;
  reg.val = reg_val;
  amdxdna_drm_get_info arg = {.param = DRM_AMDXDNA_WRITE_AIE_REG,
                              .buffer_size = sizeof(reg),
                              .buffer = reinterpret_cast<uintptr_t>(&reg)};
  m_pdev.ioctl(DRM_IOCTL_AMDXDNA_SET_STATE, &arg);
}

} // namespace shim_xdna
