// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2024, Advanced Micro Devices, Inc. All rights reserved.

#include "pcidev.h"
#include "amdxdna_accel.h"
#include "bo.h"
#include "device.h"
#include "shim.h"
#include "shim_debug.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <sys/ioctl.h>
#include <sys/mman.h>

namespace sfs = std::filesystem;

#define INVALID_ID 0xffff

namespace {

std::string ioctl_cmd2name(unsigned long cmd) {
  switch (cmd) {
  case DRM_IOCTL_AMDXDNA_CREATE_HWCTX:
    return "DRM_IOCTL_AMDXDNA_CREATE_HWCTX";
  case DRM_IOCTL_AMDXDNA_DESTROY_HWCTX:
    return "DRM_IOCTL_AMDXDNA_DESTROY_HWCTX";
  case DRM_IOCTL_AMDXDNA_CONFIG_HWCTX:
    return "DRM_IOCTL_AMDXDNA_CONFIG_HWCTX";
  case DRM_IOCTL_AMDXDNA_CREATE_BO:
    return "DRM_IOCTL_AMDXDNA_CREATE_BO";
  case DRM_IOCTL_AMDXDNA_GET_BO_INFO:
    return "DRM_IOCTL_AMDXDNA_GET_BO_INFO";
  case DRM_IOCTL_AMDXDNA_SYNC_BO:
    return "DRM_IOCTL_AMDXDNA_SYNC_BO";
  case DRM_IOCTL_AMDXDNA_EXEC_CMD:
    return "DRM_IOCTL_AMDXDNA_EXEC_CMD";
  case DRM_IOCTL_AMDXDNA_WAIT_CMD:
    return "DRM_IOCTL_AMDXDNA_WAIT_CMD";
  case DRM_IOCTL_AMDXDNA_GET_INFO:
    return "DRM_IOCTL_AMDXDNA_GET_INFO";
  case DRM_IOCTL_AMDXDNA_SET_STATE:
    return "DRM_IOCTL_AMDXDNA_SET_STATE";
  case DRM_IOCTL_GEM_CLOSE:
    return "DRM_IOCTL_GEM_CLOSE";
  case DRM_IOCTL_PRIME_HANDLE_TO_FD:
    return "DRM_IOCTL_PRIME_HANDLE_TO_FD";
  case DRM_IOCTL_PRIME_FD_TO_HANDLE:
    return "DRM_IOCTL_PRIME_FD_TO_HANDLE";
  case DRM_IOCTL_SYNCOBJ_CREATE:
    return "DRM_IOCTL_SYNCOBJ_CREATE";
  case DRM_IOCTL_SYNCOBJ_QUERY:
    return "DRM_IOCTL_SYNCOBJ_QUERY";
  case DRM_IOCTL_SYNCOBJ_DESTROY:
    return "DRM_IOCTL_SYNCOBJ_DESTROY";
  case DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD:
    return "DRM_IOCTL_SYNCOBJ_HANDLE_TO_FD";
  case DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE:
    return "DRM_IOCTL_SYNCOBJ_FD_TO_HANDLE";
  case DRM_IOCTL_SYNCOBJ_TIMELINE_SIGNAL:
    return "DRM_IOCTL_SYNCOBJ_TIMELINE_SIGNAL";
  case DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT:
    return "DRM_IOCTL_SYNCOBJ_TIMELINE_WAIT";
  }

  return "UNKNOWN(" + std::to_string(cmd) + ")";
}

// Device memory heap needs to be within one 64MB page. The maximum size is
// 64MB.
const size_t dev_mem_size = (64 << 20);

} // namespace

namespace shim_xdna {

pdev::pdev() {
  std::string err;
  m_is_ready = true; // We're always ready.
}

pdev::~pdev() {
  if (m_dev_fd != -1)
    shim_debug("Device node fd leaked!! fd=%d", m_dev_fd);
}

device::handle_type pdev::create_shim(device::id_type id) const {
  auto s = new shim(id);
  return static_cast<device::handle_type>(s);
}

int pdev::open(const std::string &subdev, uint32_t idx, int flag) const {
  return ::open("/dev/accel/accel0", flag);
}

int pdev::open(const std::string &subdev, int flag) const {
  return open(subdev, 0, flag);
}

void pdev::open() const {
  int fd;
  const std::lock_guard<std::mutex> lock(m_lock);

  if (m_dev_users == 0) {
    fd = open("", O_RDWR);
    if (fd < 0)
      shim_err(EINVAL, "Failed to open KMQ device");
    else
      shim_debug("Device opened, fd=%d", fd);
    // Publish the fd for other threads to use.
    m_dev_fd = fd;
  }
  ++m_dev_users;

  on_first_open();
}

void pdev::close() const {
  int fd;
  const std::lock_guard<std::mutex> lock(m_lock);

  --m_dev_users;
  if (m_dev_users == 0) {
    on_last_close();

    // Stop new users of the fd from other threads.
    fd = m_dev_fd;
    m_dev_fd = -1;
    // Kernel will wait for existing users to quit.
    ::close(fd);
    shim_debug("Device closed, fd=%d", fd);
  }
}

int pdev::ioctl(int dev_handle, unsigned long cmd, void *arg) const {
  if (dev_handle == -1) {
    errno = -EINVAL;
    return -1;
  }
  return ::ioctl(dev_handle, cmd, arg);
}

void pdev::ioctl(unsigned long cmd, void *arg) const {
  if (ioctl(m_dev_fd, cmd, arg) == -1)
    shim_err(errno, "%s IOCTL failed", ioctl_cmd2name(cmd).c_str());
}

void *pdev::mmap(void *addr, size_t len, int prot, int flags,
                 off_t offset) const {
  void *ret = ::mmap(addr, len, prot, flags, m_dev_fd, offset);

  if (ret == reinterpret_cast<void *>(-1))
    shim_err(errno,
             "mmap(addr=%p, len=%ld, prot=%d, flags=%d, offset=%ld) failed",
             addr, len, prot, flags, offset);
  return ret;
}

void pdev::munmap(void *addr, size_t len) const { ::munmap(addr, len); }

pdev_kmq::pdev_kmq() { shim_debug("Created KMQ pcidev"); }

pdev_kmq::~pdev_kmq() { shim_debug("Destroying KMQ pcidev"); }

std::shared_ptr<device> pdev_kmq::create_device(device::handle_type handle,
                                                device::id_type id) const {
  auto dev = std::make_shared<device_kmq>(*this, handle, id);
  try {
    // Alloc device memory on first device creation.
    // No locking is needed since driver will ensure only one heap BO is
    // created.
    if (m_dev_heap_bo == nullptr)
      m_dev_heap_bo =
          std::make_unique<bo_kmq>(*dev, dev_mem_size, AMDXDNA_BO_DEV_HEAP);
  } catch (const std::system_error &ex) {
    if (ex.code().value() != EBUSY)
      throw;
  }
  return dev;
}

void pdev_kmq::on_last_close() const { m_dev_heap_bo.reset(); }

std::shared_ptr<pdev> create_pcidev() { return std::make_shared<pdev_kmq>(); }

} // namespace shim_xdna
