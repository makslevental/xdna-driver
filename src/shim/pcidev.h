// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2024, Advanced Micro Devices, Inc. All rights reserved.

#ifndef PCIDEV_XDNA_H
#define PCIDEV_XDNA_H

#include "device.h"
#include "shim_debug.h"
#include <memory>
#include <mutex>
#include <vector>
#include <sys/mman.h>

namespace shim_xdna {

#define INVALID_ID 0xffff

// Forward declaration
class drv;
class device;

class pdev
{
public:
  uint16_t m_domain =           INVALID_ID;
  uint16_t m_bus =              INVALID_ID;
  uint16_t m_dev =              INVALID_ID;
  uint16_t m_func =             INVALID_ID;
  uint32_t m_instance =         INVALID_ID;
  std::string m_sysfs_name;     // dir name under /sys/bus/pci/devices
  int m_user_bar =              0;  // BAR mapped in by tools, default is BAR0
  size_t m_user_bar_size =      0;
  bool m_is_mgmt =              false;
  bool m_is_ready =             false;
  mutable std::mutex m_lock;
  mutable int m_dev_fd = -1;
  mutable int m_dev_users = 0;
  // Virtual address of memory mapped BAR0, mapped on first use, once mapped, never change.
  mutable char *m_user_bar_map = reinterpret_cast<char *>(MAP_FAILED);
  std::shared_ptr<const drv> m_driver;
  
  pdev(std::shared_ptr<const drv> driver, std::string sysfs_name);
  virtual ~pdev();

  using id_type = unsigned int;
  using slot_id = uint32_t;
  using handle_type = void*;

  virtual handle_type
  create_shim(id_type id) const;

  virtual std::shared_ptr<device>
  create_device(handle_type handle, id_type id) const
  { shim_not_supported_err(__func__); }

  std::string
  get_subdev_path(const std::string& subdev, uint32_t idx) const;

  void
  sysfs_get(const std::string& subdev, const std::string& entry,
            std::string& err, std::string& s);

  void
  sysfs_get(const std::string& subdev, const std::string& entry,
            std::string& err, std::vector<uint64_t>& iv);

  template <typename T>
  void
  sysfs_get(const std::string& subdev, const std::string& entry,
            std::string& err, T& i, const T& default_val)
  {
    std::vector<uint64_t> iv;
    sysfs_get(subdev, entry, err, iv);
    if (!iv.empty())
      i = static_cast<T>(iv[0]);
    else
      i = static_cast<T>(default_val); // default value
  }

  void
  sysfs_put(const std::string& subdev, const std::string& entry,
            std::string& err, const std::string& input);

  int
  open(const std::string& subdev, int flag) const;

  int
  open(const std::string& subdev, uint32_t idx, int flag) const;

  int
  ioctl(int devhdl, unsigned long cmd, void* arg = nullptr) const;

  void
  ioctl(unsigned long cmd, void* arg) const;

  void*
  mmap(void *addr, size_t len, int prot, int flags, off_t offset) const;

  void
  munmap(void* addr, size_t len) const;

  void
  open() const;

  void
  close() const;

  virtual void
  on_first_open() const {}
  virtual void
  on_last_close() const {}
};

} // namespace shim_xdna

#endif
