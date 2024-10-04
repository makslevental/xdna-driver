// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2024, Advanced Micro Devices, Inc. All rights reserved.

#ifndef PCIE_DEVICE_LINUX_XDNA_H
#define PCIE_DEVICE_LINUX_XDNA_H

#include "shim_debug.h"
#include "core/pcie/linux/device_linux.h"
#include "core/common/ishim.h"

namespace shim_xdna {
class pdev;
class bo;
class hw_ctx;

class device
{
public:
  // device index type
  using id_type = unsigned int;
  using slot_id = uint32_t;
  using handle_type = void*;

  xclDeviceHandle m_handle = XRT_NULL_HANDLE;
  bool m_userpf;
  id_type m_device_id;
  xrt::xclbin m_xclbin;                       // currently loaded xclbin  (single-slot, default)
  mutable std::mutex m_mutex;

  class xclbin_map
  {
  public:

  private:
    std::map<slot_id, xrt::uuid> m_slot2uuid;
    std::map<xrt::uuid, xrt::xclbin> m_xclbins;

  public:
    // Reset the slot -> uuid mapping based on quieried slot info data
    void
    reset(std::map<slot_id, xrt::uuid>&& slot2uuid)
    {
      m_slot2uuid = std::move(slot2uuid);
    }

    // Cache an xclbin
    void
    insert(xrt::xclbin xclbin)
    {
      m_xclbins[xclbin.get_uuid()] = std::move(xclbin);
    }

    // Get an xclbin with specified uuid
    const xrt::xclbin&
    get(const xrt::uuid& uuid) const
    {
      auto itr = m_xclbins.find(uuid);
      if (itr == m_xclbins.end())
        throw xrt_core::error("No xclbin with uuid '" + uuid.to_string() + "'");

      return (*itr).second;
    }

    // Get the xclbin stored in specified slot
    // It is an error if the xclbin has not been explicitly loaded.
    const xrt::xclbin&
    get(slot_id slot) const
    {
      auto itr = m_slot2uuid.find(slot);
      if (itr == m_slot2uuid.end())
        throw xrt_core::error("No xclbin in slot");

      return get((*itr).second);
    }

    // Return slot indices sorted
    std::vector<slot_id>
    get_slots(const xrt::uuid& uuid) const
    {
      std::vector<slot_id> slots;
      for (auto& su : m_slot2uuid)
        if (su.second == uuid)
          slots.push_back(su.first);

      std::sort(slots.begin(), slots.end());
      return slots;
    }
  };

  xclbin_map m_xclbins;                       // currently loaded xclbins (multi-slot)


  device::id_type
  get_device_id() const;

  xclDeviceHandle
  get_device_handle() const;

  xrt::xclbin get_xclbin(const xrt::uuid& xclbin_id) const;

  // Private look up function for concrete query::request
  const xrt_core::query::request&
  lookup_query(xrt_core::query::key_type query_key) const;

  const pdev& m_pdev; // The pcidev that this device object is derived from

  std::map<uint32_t, bo *> m_bo_map;

  virtual std::unique_ptr<hw_ctx>
  create_hw_context(const device& dev,
    const xrt::xclbin& xclbin, const xrt::hw_context::qos_type& qos) const = 0;

  virtual std::unique_ptr<bo>
  import_bo(xrt_core::shared_handle::export_handle ehdl) const = 0;

  device(const pdev& pdev, handle_type shim_handle, id_type device_id);

  ~device();

  const pdev&
  get_pdev() const;

  virtual std::unique_ptr<bo>
  alloc_bo(void* userptr, slot_id ctx_id,
    size_t size, uint64_t flags) = 0;

// ISHIM APIs supported are listed below
  void
  close_device();

  std::unique_ptr<bo>
  alloc_bo(size_t size, uint64_t flags);

  virtual std::unique_ptr<bo>
  alloc_bo(void* userptr, size_t size, uint64_t flags);

  std::unique_ptr<bo>
  import_bo(pid_t, xrt_core::shared_handle::export_handle);

  std::unique_ptr<hw_ctx>
  create_hw_context(const xrt::uuid& xclbin_uuid, const xrt::hw_context::qos_type& qos,
    xrt::hw_context::access_mode mode) const;

  void
  register_xclbin(const xrt::xclbin& xclbin) const;

  void
  open_aie_context(xrt::aie::access_mode)
  {
    // return success everytime as this flow doesn't support
    // calling driver to open aie context
    // this is to satisfy xrt::aie::device class constructor
    // which calls open_aie_context of ishim
    return;
  }

  std::vector<char>
  read_aie_mem(uint16_t col, uint16_t row, uint32_t offset, uint32_t size);

  size_t
  write_aie_mem(uint16_t col, uint16_t row, uint32_t offset, const std::vector<char>& buf);

  uint32_t
  read_aie_reg(uint16_t col, uint16_t row, uint32_t reg_addr);

  bool
  write_aie_reg(uint16_t col, uint16_t row, uint32_t reg_addr, uint32_t reg_val);

  std::unique_ptr<xrt_core::fence_handle>
  create_fence(xrt::fence::access_mode);

  std::unique_ptr<xrt_core::fence_handle>
  import_fence(pid_t, xrt_core::shared_handle::export_handle);

  void
  load_axlf(const axlf* buffer);

  void
  load_xclbin(const xrt::xclbin& xclbin);

  void
  record_xclbin(const xrt::xclbin& xclbin);
};

} // namespace shim_xdna

#endif
