// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2024, Advanced Micro Devices, Inc. All rights reserved.

#ifndef _HWCTX_XDNA_H_
#define _HWCTX_XDNA_H_

#include "device.h"
#include "shared.h"
#include "shim_debug.h"
#include "cuidx_type.h"

#include "amdxdna_accel.h"

namespace shim_xdna {

class hw_q; // forward declaration
class bo; // forward declaration
class device; // forward declaration

class hw_ctx
{
public:
  using cfg_param_type = std::map<std::string, uint32_t>;
  using qos_type = cfg_param_type; //alias to old type
  enum class access_mode : uint8_t {
    exclusive = 0,
    shared = 1
  };
  using slot_id = uint32_t;
  const device& m_device;
  slot_id m_handle = AMDXDNA_INVALID_CTX_HANDLE;
  amdxdna_qos_info m_qos = {};
  struct cu_info {
    std::string m_name;
    size_t m_func;
    std::vector<uint8_t> m_pdi;
  };
  std::vector<cu_info> m_cu_info;
  std::unique_ptr<hw_q> m_q;
  uint32_t m_ops_per_cycle;
  uint32_t m_num_cols;
  uint32_t m_doorbell;
  std::unique_ptr<bo> m_log_bo;
  void *m_log_buf;

  hw_ctx(const device& dev, const qos_type& qos, std::unique_ptr<hw_q> q, const xrt::xclbin& xclbin);

  virtual ~hw_ctx();

  // TODO
  void
  update_qos(const qos_type&)
  { shim_not_supported_err(__func__); }

  void
  update_access_mode(access_mode)
  { shim_not_supported_err(__func__); }

  slot_id
  get_slotidx() const;

  hw_q*
  get_hw_queue();

  virtual std::unique_ptr<bo>
  alloc_bo(void* userptr, size_t size, uint64_t flags) = 0;

  std::unique_ptr<bo>
  alloc_bo(size_t size, uint64_t flags);

  std::unique_ptr<bo>
  import_bo(pid_t, shim_xdna::shared_handle::export_handle);

  xrt_core::cuidx_type
  open_cu_context(const std::string& cuname);

  void
  close_cu_context(xrt_core::cuidx_type cuidx);

  void
  exec_buf(bo *)
  { shim_not_supported_err(__func__); }

  uint32_t
  get_doorbell() const;

  const device&
  get_device();

  const std::vector<cu_info>&
  get_cu_info() const;

  void
  set_slotidx(slot_id id);

  void
  set_doorbell(uint32_t db);

  void
  create_ctx_on_device();

  void
  init_log_buf();

  void
  fini_log_buf();

  void
  delete_ctx_on_device();

  void
  init_qos_info(const qos_type& qos);

  void
  parse_xclbin(const xrt::xclbin& xclbin);

  void
  print_xclbin_info();
};

} // shim_xdna

#endif // _HWCTX_XDNA_H_
