// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2024, Advanced Micro Devices, Inc. All rights reserved.

#ifndef PCIE_DEVICE_LINUX_XDNA_H
#define PCIE_DEVICE_LINUX_XDNA_H

#include "experimental/xrt_xclbin.h"
#include "fence.h"
#include "shim_debug.h"
#include "xrt_uuid.h"

#include <map>

namespace shim_xdna {
class pdev;
class bo;
using slot_id = uint32_t;

class xclbin_map {
public:
  std::map<slot_id, xrt::uuid> m_slot2uuid;
  std::map<xrt::uuid, xrt::xclbin> m_xclbins;
  // Reset the slot -> uuid mapping based on quieried slot info data
  void reset(std::map<slot_id, xrt::uuid> &&slot2uuid) {
    m_slot2uuid = std::move(slot2uuid);
  }

  // Cache an xclbin
  void insert(xrt::xclbin xclbin) {
    m_xclbins[xclbin.get_uuid()] = std::move(xclbin);
  }

  // Get an xclbin with specified uuid
  const xrt::xclbin &get(const xrt::uuid &uuid) const {
    auto itr = m_xclbins.find(uuid);
    if (itr == m_xclbins.end())
      throw std::system_error(EINVAL, std::system_category(),
                              "No xclbin with uuid '" + uuid.to_string() + "'");

    return (*itr).second;
  }

  // Get the xclbin stored in specified slot
  // It is an error if the xclbin has not been explicitly loaded.
  const xrt::xclbin &get(slot_id slot) const {
    auto itr = m_slot2uuid.find(slot);
    if (itr == m_slot2uuid.end())
      throw std::system_error(EINVAL, std::system_category(),
                              "No xclbin in slot");

    return get((*itr).second);
  }

  // Return slot indices sorted
  std::vector<slot_id> get_slots(const xrt::uuid &uuid) const {
    std::vector<slot_id> slots;
    for (auto &su : m_slot2uuid)
      if (su.second == uuid)
        slots.push_back(su.first);

    std::sort(slots.begin(), slots.end());
    return slots;
  }
};

class device {
public:
  // device index type
  using id_type = unsigned int;
  using handle_type = void *;
  using cfg_param_type = std::map<std::string, uint32_t>;
  using qos_type = cfg_param_type; // alias to old type
  enum class access_mode : uint8_t { exclusive = 0, shared = 1 };

  void *m_handle = nullptr;
  bool m_userpf;
  id_type m_device_id;
  xrt::xclbin m_xclbin; // currently loaded xclbin  (single-slot, default)
  mutable std::mutex m_mutex;
  xclbin_map m_xclbins; // currently loaded xclbins (multi-slot)
  const pdev &m_pdev;   // The pcidev that this device object is derived from
  std::map<uint32_t, bo *> m_bo_map;

  device(const pdev &pdev, handle_type shim_handle, id_type device_id);
  virtual ~device();

  id_type get_device_id() const;
  void *get_device_handle() const;
  xrt::xclbin get_xclbin(const xrt::uuid &xclbin_id) const;

  virtual std::unique_ptr<hw_ctx>
  create_hw_context(const device &dev, const xrt::xclbin &xclbin,
                    const qos_type &qos) const = 0;

  virtual std::unique_ptr<bo>
  import_bo(shared_handle::export_handle ehdl) const = 0;
  const pdev &get_pdev() const;

  virtual std::unique_ptr<bo> alloc_bo(void *userptr, slot_id ctx_id,
                                       size_t size, uint64_t flags) = 0;

  void close_device();
  std::unique_ptr<bo> alloc_bo(size_t size, uint64_t flags);
  virtual std::unique_ptr<bo> alloc_bo(void *userptr, size_t size,
                                       uint64_t flags);
  std::unique_ptr<bo> import_bo(pid_t, shared_handle::export_handle);
  std::unique_ptr<hw_ctx> create_hw_context(const xrt::uuid &xclbin_uuid,
                                            const qos_type &qos) const;
  std::vector<char> read_aie_mem(uint16_t col, uint16_t row, uint32_t offset,
                                 uint32_t size);
  size_t write_aie_mem(uint16_t col, uint16_t row, uint32_t offset,
                       const std::vector<char> &buf);
  uint32_t read_aie_reg(uint16_t col, uint16_t row, uint32_t reg_addr);
  void write_aie_reg(uint16_t col, uint16_t row, uint32_t reg_addr,
                     uint32_t reg_val);
  std::unique_ptr<fence_handle> create_fence(fence_handle::access_mode);
  std::unique_ptr<fence_handle> import_fence(pid_t,
                                             shared_handle::export_handle);
  void record_xclbin(const xrt::xclbin &xclbin);
};

class device_kmq : public device {
public:
  device_kmq(const pdev &pdev, handle_type shim_handle, id_type device_id);
  ~device_kmq() override;

  using slot_id = uint32_t;
  std::unique_ptr<bo> alloc_bo(void *userptr, slot_id ctx_id, size_t size,
                               uint64_t flags) override;

  std::unique_ptr<hw_ctx> create_hw_context(const device &dev,
                                            const xrt::xclbin &xclbin,
                                            const qos_type &qos) const override;

  std::unique_ptr<bo>
  import_bo(shared_handle::export_handle ehdl) const override;
};

} // namespace shim_xdna

#endif
