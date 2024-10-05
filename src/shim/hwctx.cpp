// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2024, Advanced Micro Devices, Inc. All rights reserved.

#include "hwctx.h"
#include "bo.h"
#include "core/common/api/xclbin_int.h"
#include "hwq.h"

#include <cstring>

namespace {

std::vector<uint8_t> get_pdi(const xrt_core::xclbin::aie_partition_obj &aie,
                             uint16_t kernel_id) {
  for (auto &pdi : aie.pdis) {
    for (auto &cdo : pdi.cdo_groups) {
      for (auto kid : cdo.kernel_ids) {
        if (kid == kernel_id)
          return pdi.pdi;
      }
    }
  }
  shim_xdna::shim_err(ENOENT, "PDI for kernel ID 0x%x not found", kernel_id);
}

} // namespace

namespace shim_xdna {

hw_ctx::hw_ctx(const device &dev, const qos_type &qos, std::unique_ptr<hw_q> q,
               const xrt::xclbin &xclbin)
    : m_device(dev), m_q(std::move(q)), m_doorbell(0), m_log_buf(nullptr) {
  shim_debug("Creating HW context...");
  init_qos_info(qos);
  parse_xclbin(xclbin);
}

hw_ctx::~hw_ctx() {
  try {
    delete_ctx_on_device();
  } catch (const std::system_error &e) {
    shim_debug("Failed to delete context on device: %s", e.what());
  }
  shim_debug("Destroyed HW context (%d)...", m_handle);
}

hw_ctx::slot_id hw_ctx::get_slotidx() const { return m_handle; }
void hw_ctx::set_slotidx(slot_id id) { m_handle = id; }

xrt_core::cuidx_type hw_ctx::open_cu_context(const std::string &cu_name) {
  for (uint32_t i = 0; i < m_cu_info.size(); i++) {
    auto &ci = m_cu_info[i];
    if (ci.m_name == cu_name)
      return xrt_core::cuidx_type{.index = i};
  }

  shim_err(ENOENT, "CU name (%s) not found", cu_name.c_str());
}

void hw_ctx::close_cu_context(xrt_core::cuidx_type cuidx) {
  // Nothing to be done
}

std::unique_ptr<bo> hw_ctx::alloc_bo(size_t size, uint64_t flags) {
  return alloc_bo(nullptr, size, flags);
}

std::unique_ptr<bo>
hw_ctx::import_bo(pid_t pid, shim_xdna::shared_handle::export_handle ehdl) {
  // const_cast: import_bo() is not const yet in device class
  auto &dev = const_cast<device &>(get_device());
  return dev.import_bo(pid, ehdl);
}

hw_q *hw_ctx::get_hw_queue() { return m_q.get(); }

void hw_ctx::init_qos_info(const qos_type &qos) {
  for (auto &[key, value] : qos) {
    if (key == "gops")
      m_qos.gops = value;
    else if (key == "fps")
      m_qos.fps = value;
    else if (key == "dma_bandwidth")
      m_qos.dma_bandwidth = value;
    else if (key == "latency")
      m_qos.latency = value;
    else if (key == "frame_execution_time")
      m_qos.frame_exec_time = value;
    else if (key == "priority")
      m_qos.priority = value;
  }
}

void hw_ctx::print_xclbin_info() {
  if (m_cu_info.empty()) {
    shim_debug("CU INFO is empty");
    return;
  }

  for (int idx = 0; idx < m_cu_info.size(); idx++) {
    auto &e = m_cu_info[idx];
    shim_debug("index=%d, name=%s, func=%d, pdi(p=%p, sz=%ld)", idx,
               e.m_name.c_str(), e.m_func, e.m_pdi.data(), e.m_pdi.size());
  }
  shim_debug("OPs/cycle: %d", m_ops_per_cycle);
}

void hw_ctx::parse_xclbin(const xrt::xclbin &xclbin) {
  auto axlf = xclbin.get_axlf();
  auto aie_partition = xrt_core::xclbin::get_aie_partition(axlf);

  for (const auto &k : xclbin.get_kernels()) {
    auto &props = xrt_core::xclbin_int::get_properties(k);
    try {
      for (const auto &cu : k.get_cus()) {
        m_cu_info.push_back({.m_name = cu.get_name(),
                             .m_func = props.functional,
                             .m_pdi = get_pdi(aie_partition, props.kernel_id)});
      }
    } catch (std::system_error &ex) {
      if (ex.code().value() != ENOENT)
        throw;
      shim_debug("%s", ex.what());
    }
  }

  if (m_cu_info.empty())
    shim_err(EINVAL, "No valid DPU kernel found in xclbin");
  m_ops_per_cycle = aie_partition.ops_per_cycle;
  m_num_cols = aie_partition.ncol;
  print_xclbin_info();
}

const device &hw_ctx::get_device() { return m_device; }

const std::vector<hw_ctx::cu_info> &hw_ctx::get_cu_info() const {
  return m_cu_info;
}

void hw_ctx::create_ctx_on_device() {
  amdxdna_drm_create_hwctx arg = {};
  arg.qos_p = reinterpret_cast<uintptr_t>(&m_qos);
  arg.umq_bo = m_q->get_queue_bo();
  arg.max_opc = m_ops_per_cycle;
  // TODO(max)
  //  throw std::runtime_error("TODO(max): core_rows");
  //  arg.num_tiles = m_num_cols *
  //  xrt_core::device_query<xrt_core::query::aie_tiles_stats>(&m_device).core_rows;
  arg.num_tiles = m_num_cols * 4;
  arg.log_buf_bo = m_log_bo
                       ? static_cast<bo *>(m_log_bo.get())->get_drm_bo_handle()
                       : AMDXDNA_INVALID_BO_HANDLE;
  m_device.get_pdev().ioctl(DRM_IOCTL_AMDXDNA_CREATE_HWCTX, &arg);

  set_slotidx(arg.handle);
  set_doorbell(arg.umq_doorbell);

  m_q->bind_hwctx(this);
}

void hw_ctx::delete_ctx_on_device() {
  if (m_handle == AMDXDNA_INVALID_CTX_HANDLE)
    return;

  m_q->unbind_hwctx();
  amdxdna_drm_destroy_hwctx arg = {};
  arg.handle = m_handle;
  m_device.get_pdev().ioctl(DRM_IOCTL_AMDXDNA_DESTROY_HWCTX, &arg);

  fini_log_buf();
}

void hw_ctx::init_log_buf() {
  auto log_buf_size = m_num_cols * 1024;
  m_log_bo = alloc_bo(nullptr, log_buf_size, XCL_BO_FLAGS_EXECBUF);
  m_log_buf = m_log_bo->map(bo::map_type::write);
  std::memset(m_log_buf, 0, log_buf_size);
}

void hw_ctx::fini_log_buf() {
  if (m_log_bo)
    m_log_bo->unmap(m_log_buf);
}

void hw_ctx::set_doorbell(uint32_t db) { m_doorbell = db; }

uint32_t hw_ctx::get_doorbell() const { return m_doorbell; }

hw_ctx_kmq::hw_ctx_kmq(const device &device, const xrt::xclbin &xclbin,
                       const qos_type &qos)
    : hw_ctx(device, qos, std::make_unique<hw_q_kmq>(device), xclbin) {
  create_ctx_on_device();
  auto cu_info = get_cu_info();
  std::vector<char> cu_conf_param_buf(sizeof(amdxdna_hwctx_param_config_cu) +
                                      cu_info.size() *
                                          sizeof(amdxdna_cu_config));
  auto cu_conf_param = reinterpret_cast<amdxdna_hwctx_param_config_cu *>(
      cu_conf_param_buf.data());

  cu_conf_param->num_cus = cu_info.size();
  xcl_bo_flags f = {};
  f.flags = XRT_BO_FLAGS_CACHEABLE;
  for (int i = 0; i < cu_info.size(); i++) {
    auto &ci = cu_info[i];

    m_pdi_bos.push_back(alloc_bo(nullptr, ci.m_pdi.size(), f.all));
    auto &pdi_bo = m_pdi_bos[i];
    auto pdi_vaddr = reinterpret_cast<char *>(pdi_bo->map(bo::map_type::write));

    auto &cf = cu_conf_param->cu_configs[i];
    std::memcpy(pdi_vaddr, ci.m_pdi.data(), ci.m_pdi.size());
    pdi_bo->sync(bo::direction::host2device, pdi_bo->get_properties().size, 0);
    cf.cu_bo = pdi_bo.get()->get_drm_bo_handle();
    cf.cu_func = ci.m_func;
  }

  amdxdna_drm_config_hwctx arg = {};
  arg.handle = get_slotidx();
  arg.param_type = DRM_AMDXDNA_HWCTX_CONFIG_CU;
  arg.param_val = reinterpret_cast<uintptr_t>(cu_conf_param);
  arg.param_val_size = cu_conf_param_buf.size();
  get_device().get_pdev().ioctl(DRM_IOCTL_AMDXDNA_CONFIG_HWCTX, &arg);

  shim_debug("Created KMQ HW context (%d)", get_slotidx());
}

hw_ctx_kmq::~hw_ctx_kmq() {
  shim_debug("Destroying KMQ HW context (%d)...", get_slotidx());
}

std::unique_ptr<bo> hw_ctx_kmq::alloc_bo(void *userptr, size_t size,
                                         uint64_t flags) {
  // const_cast: alloc_bo() is not const yet in device class
  auto &dev = const_cast<device &>(get_device());

  // Debug buffer is specific to one context.
  if (xcl_bo_flags{flags}.use == XRT_BO_USE_DEBUG)
    return dev.alloc_bo(userptr, get_slotidx(), size, flags);
  // Other BOs are shared across all contexts.
  return dev.alloc_bo(userptr, AMDXDNA_INVALID_CTX_HANDLE, size, flags);
}

} // namespace shim_xdna
