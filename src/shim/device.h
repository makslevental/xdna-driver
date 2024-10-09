// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2024, Advanced Micro Devices, Inc. All rights reserved.

#ifndef PCIE_DEVICE_LINUX_XDNA_H
#define PCIE_DEVICE_LINUX_XDNA_H

#include "experimental/xrt_xclbin.h"
#include "fence.h"
#include "shim_debug.h"

#include <map>

namespace shim_xdna {
struct pdev;
struct bo;
using slot_id = uint32_t;

struct device {
  using qos_t = std::map<std::string, uint32_t>; // alias to old type
  enum class access_mode : uint8_t { exclusive = 0, shared = 1 };

  xrt::xclbin m_xclbin; // currently loaded xclbin  (single-slot, default)
  mutable std::mutex m_mutex;
  std::map<xrt::uuid, xrt::xclbin> m_xclbins;
  const pdev &m_pdev; // The pcidev that this device object is derived from

  device(const pdev &pdev);
  ~device();

  xrt::xclbin get_xclbin(const xrt::uuid &xclbin_id) const;

  std::unique_ptr<bo> import_bo(int ehdl) const;
  const pdev &get_pdev() const;

  std::unique_ptr<bo> alloc_bo(void *userptr, slot_id ctx_id, size_t size,
                               uint64_t flags);

  std::unique_ptr<bo> alloc_bo(size_t size, uint64_t flags);
  std::unique_ptr<bo> alloc_bo(void *userptr, size_t size, uint64_t flags);
  std::unique_ptr<bo> import_bo(pid_t, int);
  std::unique_ptr<hw_ctx> create_hw_context(const xrt::uuid &xclbin_uuid,
                                            const qos_t &qos);
  std::vector<char> read_aie_mem(uint16_t col, uint16_t row, uint32_t offset,
                                 uint32_t size);
  size_t write_aie_mem(uint16_t col, uint16_t row, uint32_t offset,
                       const std::vector<char> &buf);
  uint32_t read_aie_reg(uint16_t col, uint16_t row, uint32_t reg_addr);
  void write_aie_reg(uint16_t col, uint16_t row, uint32_t reg_addr,
                     uint32_t reg_val);
  std::unique_ptr<fence_handle> create_fence(fence_handle::access_mode);
  std::unique_ptr<fence_handle> import_fence(pid_t, int);
  void record_xclbin(const xrt::xclbin &xclbin);
};

struct pdev {
  mutable std::mutex m_lock;
  mutable int m_dev_fd = -1;
  mutable int m_dev_users = 0;

  // Create on first device creation and removed right before device is closed
  mutable std::unique_ptr<bo> m_dev_heap_bo;

  pdev();
  ~pdev();

  std::shared_ptr<device> create_device() const;

  void ioctl(unsigned long cmd, void *arg) const;
  void *mmap(void *addr, size_t len, int prot, int flags, off_t offset) const;
  void open() const;
  void close() const;
  void on_last_close() const;
};

std::shared_ptr<pdev> create_pcidev();

std::shared_ptr<device> my_get_userpf_device(uint32_t id);
void add_to_user_ready_list(const std::shared_ptr<pdev> &dev);

std::unique_ptr<hw_ctx> create_hw_context(device &dev,
                                          const xrt::xclbin &xclbin,
                                          const device::qos_t &qos);

} // namespace shim_xdna

#endif
