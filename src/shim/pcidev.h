// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2024, Advanced Micro Devices, Inc. All rights reserved.

#ifndef PCIDEV_XDNA_H
#define PCIDEV_XDNA_H

#include "device.h"
#include "shim_debug.h"
#include <memory>
#include <mutex>
#include <vector>

namespace shim_xdna {

#define INVALID_ID 0xffff

// Forward declaration
class drv;
class device;

class pdev {
public:
  bool m_is_ready = false;
  mutable std::mutex m_lock;
  mutable int m_dev_fd = -1;
  mutable int m_dev_users = 0;
  using id_type = unsigned int;
  using slot_id = uint32_t;
  using handle_type = void *;

  pdev();
  virtual ~pdev();

  virtual handle_type create_shim(id_type id) const;
  virtual std::shared_ptr<device> create_device(handle_type handle,
                                                id_type id) const {
    shim_not_supported_err(__func__);
  }

  int open(const std::string &subdev, int flag) const;
  int open(const std::string &subdev, uint32_t idx, int flag) const;
  int ioctl(int devhdl, unsigned long cmd, void *arg = nullptr) const;
  void ioctl(unsigned long cmd, void *arg) const;
  void *mmap(void *addr, size_t len, int prot, int flags, off_t offset) const;
  void munmap(void *addr, size_t len) const;
  void open() const;
  void close() const;
  virtual void on_first_open() const {}
  virtual void on_last_close() const {}
};

class pdev_kmq : public pdev {
public:
  pdev_kmq();
  ~pdev_kmq() override;

  std::shared_ptr<device> create_device(device::handle_type handle,
                                        device::id_type id) const override;

  // Create on first device creation and removed right before device is closed
  mutable std::unique_ptr<bo> m_dev_heap_bo;

  void on_last_close() const override;
};

std::shared_ptr<pdev> create_pcidev();

} // namespace shim_xdna

#endif
