// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2022-2023, Advanced Micro Devices, Inc. All rights reserved.
//
// WARNING: This file contains test cases calling XRT's SHIM layer APIs directly.
// These APIs are XRT's internal APIs and are not meant for any external XRT
// user to call. We can't provide any support if you use APIs here and run into issues.


#include "dev_info.h"
#include "io_param.h"
#include "hwctx.h"
#include "speed.h"
#include "bo.h"
#include "dlfcn.h"

#include "../../src/shim/pcidrv.h"
#include "../../src/shim/kmq/pcidev.h"
#include "core/common/query_requests.h"
#include "../../src/shim/shim.h"

#include <filesystem>
#include <libgen.h>
#include <fstream>
#include <set>

std::string cur_path;
std::string xclbin_path;

using arg_type = const std::vector<uint64_t>;
void TEST_export_import_bo(shim_xdna::device::id_type, std::shared_ptr<shim_xdna::device>, arg_type&);
void TEST_io(shim_xdna::device::id_type, std::shared_ptr<shim_xdna::device>, arg_type&);
void TEST_io_latency(shim_xdna::device::id_type, std::shared_ptr<shim_xdna::device>, arg_type&);
void TEST_io_throughput(shim_xdna::device::id_type, std::shared_ptr<shim_xdna::device>, arg_type&);
void TEST_io_runlist_latency(shim_xdna::device::id_type, std::shared_ptr<shim_xdna::device>, arg_type&);
void TEST_io_runlist_throughput(shim_xdna::device::id_type, std::shared_ptr<shim_xdna::device>, arg_type&);
void TEST_noop_io_with_dup_bo(shim_xdna::device::id_type, std::shared_ptr<shim_xdna::device>, arg_type&);
void TEST_shim_umq_vadd(shim_xdna::device::id_type, std::shared_ptr<shim_xdna::device>, arg_type&);
void TEST_shim_umq_memtiles(shim_xdna::device::id_type, std::shared_ptr<shim_xdna::device>, arg_type&);
void TEST_shim_umq_ddr_memtile(shim_xdna::device::id_type, std::shared_ptr<shim_xdna::device>, arg_type&);
void TEST_shim_umq_remote_barrier(shim_xdna::device::id_type, std::shared_ptr<shim_xdna::device>, arg_type&);
void TEST_txn_elf_flow(shim_xdna::device::id_type, std::shared_ptr<shim_xdna::device>, arg_type&);
void TEST_cmd_fence_host(shim_xdna::device::id_type, std::shared_ptr<shim_xdna::device>, arg_type&);
void TEST_cmd_fence_device(shim_xdna::device::id_type, std::shared_ptr<shim_xdna::device>, arg_type&);

#define REPEAT_RUNS 1

namespace {

int base_write_speed;
int base_read_speed;
std::string program;

inline void
set_xrt_path()
{
//  setenv("XILINX_XRT", (cur_path + "/../").c_str(), true);
}

enum test_mode {
  TEST_POSITIVE,
  TEST_NEGATIVE,
};

void
usage(const std::string& prog)
{
  std::cout << "\nUsage: " << prog << " [xclbin] [test case ID separated by space]\n";
  std::cout << "Examples:\n";
  std::cout << "\t" << prog << " - run all test cases\n";
  std::cout << "\t" << prog << " [test case ID separated by space] - run specified test cases\n";
  std::cout << "\t" << prog << " /path/to/a/test.xclbin - run all test cases with test.xclbin\n";
  std::cout << "\n";
  std::cout << std::endl;
}

struct test_case { // Definition of one test case
  const char *description;
  enum test_mode mode;
  bool (*dev_filter)(shim_xdna::device::id_type id, shim_xdna::device *dev);
  void (*func)(shim_xdna::device::id_type id, std::shared_ptr<shim_xdna::device> dev, arg_type& arg);
  arg_type arg;
};

// For overall test result evaluation
int test_passed = 0;
int test_skipped = 0;
int test_failed = 0;

// Device type filters
bool
is_xdna_dev(shim_xdna::device* dev)
{
  return true;
}

bool
no_dev_filter(shim_xdna::device::id_type id, shim_xdna::device* dev)
{
  return true;
}

bool
skip_dev_filter(shim_xdna::device::id_type id, shim_xdna::device* dev)
{
  return false;
}

bool
dev_filter_xdna(shim_xdna::device::id_type id, shim_xdna::device* dev)
{
  return is_xdna_dev(dev);
}

bool
dev_filter_not_xdna(shim_xdna::device::id_type id, shim_xdna::device* dev)
{
  return !is_xdna_dev(dev);
}

bool
dev_filter_is_aie2(shim_xdna::device::id_type id, shim_xdna::device* dev)
{
  if (!is_xdna_dev(dev))
    return false;
  //  auto device_id = device_query<query::pcie_device>(dev);
  uint16_t device_id = 5378;
  return device_id == npu1_device_id || device_id == npu2_device_id;
}

bool
dev_filter_is_aie4(shim_xdna::device::id_type id, shim_xdna::device* dev)
{
  if (!is_xdna_dev(dev))
    return false;
  //  auto device_id = device_query<query::pcie_device>(dev);
  uint16_t device_id = 5378;
  return device_id == npu3_device_id || device_id == npu3_device_id1;
}

bool
dev_filter_is_aie(shim_xdna::device::id_type id, shim_xdna::device* dev)
{
  return dev_filter_is_aie2(id, dev) || dev_filter_is_aie4(id, dev);
}

// All test case runners

void
TEST_get_xrt_info(shim_xdna::device::id_type id, std::shared_ptr<shim_xdna::device> sdev, arg_type& arg)
{
  throw std::runtime_error("TODO(max): re-enable TEST_get_xrt_info");
//  boost::property_tree::ptree pt;
//  const boost::property_tree::ptree empty_pt;
//  sysinfo::get_xrt_info(pt);
//  const boost::property_tree::ptree& drivers = pt.get_child("drivers", empty_pt);
//
//  for(const auto& drv : drivers) {
//    const boost::property_tree::ptree& driver = drv.second;
//    const std::string drv_name = driver.get<std::string>("name", "");
//    const std::string drv_version = driver.get<std::string>("version", "");
//    const std::string drv_hash = driver.get<std::string>("hash", "");
//    std::cout << "Driver: " << drv_name << std::endl;
//    std::cout << "Version: " << drv_version << std::endl;
//    std::cout << "HASH: " << drv_hash << std::endl;
//  }
}

void
TEST_get_os_info(shim_xdna::device::id_type id, std::shared_ptr<shim_xdna::device> sdev, arg_type& arg)
{
  throw std::runtime_error("TODO(max): re-enable TEST_get_os_info");
//  boost::property_tree::ptree pt;
//  sysinfo::get_os_info(pt);
//  std::cout << "Hostname: " << pt.get<std::string>("hostname", "N/A") << std::endl;
//  std::cout << "OS: " << pt.get<std::string>("distribution", "N/A") << std::endl;
}

void
TEST_get_total_devices(shim_xdna::device::id_type id, std::shared_ptr<shim_xdna::device> sdev, arg_type& arg)
{
  throw std::runtime_error("TODO(max): re-enable TEST_get_total_devices");
//  auto is_user = arg[0];
//  std::string pf { is_user ? "userpf" : "mgmtpf" };
//  auto info = get_total_devices(is_user);
//  std::cout << pf << " total: " << info.first << std::endl;
//  std::cout << pf << " ready: " << info.second << std::endl;
}

const std::string
bdf_info2str(std::tuple<uint16_t, uint16_t, uint16_t, uint16_t>& info)
{
  char buf[100] = {};

  snprintf(buf, sizeof(buf), "%04x:%02x:%02x.%x",
    std::get<0>(info), std::get<1>(info), std::get<2>(info), std::get<3>(info));
  return buf;
}

void
TEST_get_bdf_info_and_get_device_id(shim_xdna::device::id_type id, std::shared_ptr<shim_xdna::device> sdev, arg_type& arg)
{
  throw std::runtime_error("TODO(max): re-enable TEST_get_bdf_info_and_get_device_id");
//  auto is_user = arg[0];
//  auto devinfo = get_total_devices(is_user);
//  for (shim_xdna::device::id_type i = 0; i < devinfo.first; i++) {
//    auto info = get_bdf_info(i);
//    auto bdf = bdf_info2str(info);
//    std::cout << "device[" << i << "]: " << bdf << std::endl;
//    auto devid = get_device_id(bdf);
//    std::cout << "device[" << bdf << "]: " << devid << std::endl;
//  }
}

void
TEST_get_mgmtpf_device(shim_xdna::device::id_type id, std::shared_ptr<shim_xdna::device> sdev, arg_type& arg)
{
  throw std::runtime_error("TODO(max): re-enable TEST_get_mgmtpf_device");
//  auto devinfo = get_total_devices(false);
//  for (shim_xdna::device::id_type i = 0; i < devinfo.first; i++)
//    auto dev = get_mgmtpf_device(i);
}

template <typename QueryRequestType>
void
TEST_query_userpf(shim_xdna::device::id_type id, std::shared_ptr<shim_xdna::device> sdev, arg_type& arg)
{
  throw std::runtime_error("TODO(max): re-enable TEST_query_userpf");
//  auto query_result = device_query<QueryRequestType>(sdev);
//  std::cout << "dev[" << id << "]." << QueryRequestType::name() << ": "
//    << QueryRequestType::to_string(query_result) << std::endl;
}

void
TEST_create_destroy_hw_context(shim_xdna::device::id_type id, std::shared_ptr<shim_xdna::device> sdev, arg_type& arg)
{
  // Close existing device
  sdev.reset();

  // Try opening device and creating ctx twice
  {
    auto dev = shim_xdna::my_get_userpf_device(id);
    hw_ctx hwctx{dev.get()};
  }
  {
    auto dev = shim_xdna::my_get_userpf_device(id);
    hw_ctx hwctx{dev.get()};
  }
}

void
TEST_create_free_debug_bo(shim_xdna::device::id_type id, std::shared_ptr<shim_xdna::device> sdev, arg_type& arg)
{
  auto dev = sdev.get();
  auto boflags = XRT_BO_FLAGS_CACHEABLE;
  auto ext_boflags = XRT_BO_USE_DEBUG << 4;
  auto size = static_cast<size_t>(arg[0]);

  // Create ctx -> create bo -> destroy bo -> destroy ctx
  {
    hw_ctx hwctx{dev};
    auto bo = hwctx.get()->alloc_bo(size, get_bo_flags(boflags, ext_boflags));

    auto dbg_p = static_cast<uint32_t *>(bo->map(shim_xdna::bo::map_type::write));
    std::memset(dbg_p, 0xff, size);
    bo.get()->sync(shim_xdna::bo::direction::device2host, size, 0);
    if (std::memcmp(dbg_p, std::string(size, 0xff).c_str(), size) != 0)
      throw std::runtime_error("Debug buffer is not zero");
  }

  // Create ctx -> create bo -> destroy ctx -> destroy bo
  std::unique_ptr<shim_xdna::bo> bo;
  {
    hw_ctx hwctx{dev};
    bo = hwctx.get()->alloc_bo(size, get_bo_flags(boflags, ext_boflags));
  }
  try {
    bo.get()->sync(shim_xdna::bo::direction::device2host, size, 0);
  } catch (const std::system_error& e) {
    std::cout << e.what() << std::endl;
  }
}

void
get_and_show_bo_properties(shim_xdna::device* dev, shim_xdna::bo *boh)
{
  shim_xdna::bo::properties properties = boh->get_properties();
  std::cout << std::hex
    << "\tbo flags: 0x" << properties.flags << "\n"
    << "\tbo paddr: 0x" << properties.paddr << "\n"
    << "\tbo size: 0x" << properties.size << std::dec << std::endl;
}

void
TEST_create_free_bo(shim_xdna::device::id_type id, std::shared_ptr<shim_xdna::device> sdev, arg_type& arg)
{
  auto dev = sdev.get();
  uint32_t boflags = static_cast<unsigned int>(arg[0]);
  uint32_t ext_boflags = static_cast<unsigned int>(arg[1]);
  arg_type bos_size(arg.begin() + 2, arg.end());
  std::vector<std::unique_ptr<bo>> bos;

  for (auto& size : bos_size)
    bos.push_back(std::make_unique<bo>(dev, static_cast<size_t>(size), boflags, ext_boflags));

  for (auto& bo : bos)
    get_and_show_bo_properties(dev, bo->get());
}

void
TEST_sync_bo(shim_xdna::device::id_type id, std::shared_ptr<shim_xdna::device> sdev, arg_type& arg)
{
  auto boflags = static_cast<unsigned int>(arg[0]);
  auto ext_boflags = static_cast<unsigned int>(arg[1]);
  auto size = static_cast<size_t>(arg[2]);
  bo bo{sdev.get(), size, boflags, ext_boflags};

  auto start = clk::now();
  bo.get()->sync(shim_xdna::bo::direction::host2device, size / 2, 0);
  bo.get()->sync(shim_xdna::bo::direction::device2host, size / 2, size / 2);
  auto end = clk::now();

  get_speed_and_print("sync", size, start, end);
}

void
TEST_sync_bo_off_size(shim_xdna::device::id_type id, std::shared_ptr<shim_xdna::device> sdev, arg_type& arg)
{
  auto boflags = static_cast<unsigned int>(arg[0]);
  auto ext_boflags = static_cast<unsigned int>(arg[1]);
  auto size = static_cast<size_t>(arg[2]);
  auto sync_offset = static_cast<size_t>(arg[3]);
  auto sync_size = static_cast<size_t>(arg[4]);
  bo bo{sdev.get(), size, boflags, ext_boflags};

  auto start = clk::now();
  bo.get()->sync(shim_xdna::bo::direction::host2device, sync_size, sync_offset);
  auto end = clk::now();

  get_speed_and_print("sync", sync_size, start, end);
}

void
TEST_map_read_bo(shim_xdna::device::id_type id, std::shared_ptr<shim_xdna::device> sdev, arg_type& arg)
{
  auto dev = sdev.get();
  auto size = static_cast<size_t>(arg[0]);
  auto bo_hdl = dev->alloc_bo(size, get_bo_flags(XRT_BO_FLAGS_NONE, 0));

  auto buf = bo_hdl->map(shim_xdna::bo::map_type::read);
}

void speed_test_fill_buf(std::vector<int> &vec)
{
    auto buf = vec.data();

    for (int i = 0; i < vec.size(); i++)
       buf[i] = i;
}

int speed_test_copy_data(int *dst, int *src, size_t size)
{
  int num_element = size/sizeof(int);

  std::cout << "\tBuffer size 0x" << std::hex << size << std::dec << " bytes"
            << std::endl;
  auto start = clk::now();
  memcpy(dst, src, size);
  auto end = clk::now();
  return get_speed_and_print("move data (int type)", size, start, end);
}

void speed_test_base_line(size_t size)
{
  std::vector<int> ref_vec(size/sizeof(int));
  std::vector<int> trg_vec(size/sizeof(int));
  auto ref_buf = ref_vec.data();
  auto trg_buf = trg_vec.data();

  speed_test_fill_buf(ref_vec);

  std::cout << "\tBaseline *write* speed test start. vector -> vector " << std::endl;
  base_write_speed = speed_test_copy_data(trg_buf, ref_buf, size);

  std::cout << "\tBaseline *read* speed test start. vector -> vector" << std::endl;
  base_read_speed = speed_test_copy_data(ref_buf, trg_buf, size);
}

void
TEST_map_bo(shim_xdna::device::id_type id, std::shared_ptr<shim_xdna::device> sdev, arg_type& arg)
{
  auto boflags = static_cast<unsigned int>(arg[0]);
  auto ext_boflags = static_cast<unsigned int>(arg[1]);
  auto size = static_cast<size_t>(arg[2]);
  bo bo{sdev.get(), size, boflags, ext_boflags};

  // Intentionally not unmap to test error handling in driver
  bo.set_no_unmap();

  if (!base_write_speed || !base_read_speed)
    speed_test_base_line(size);
  if (!base_write_speed || !base_read_speed)
    throw std::runtime_error("Failed to obtain speed test baseline.");

  std::vector<int> ref_vec(size/sizeof(int));
  speed_test_fill_buf(ref_vec);
  auto ref_buf = ref_vec.data();

  auto buf = bo.map();
  memset(buf, 0, size); /* warm up */
  std::cout << "\tBO *write* speed test start. vector -> bo " << std::endl;
  auto write_speed = speed_test_copy_data(buf, ref_buf, size);

  std::cout << "\tBo *read* speed test start. bo -> vector" << std::endl;
  auto read_speed = speed_test_copy_data(ref_buf, buf, size);

  auto wpercent = (write_speed * 1.0) / base_write_speed;
  auto rpercent = (read_speed * 1.0) / base_read_speed;

  //if (wpercent < 0.85 || rpercent < 0.85) {
  //  std::cout << "write percent " << std::fixed << wpercent << std::endl;
  //  std::cout << "read percent " << std::fixed << rpercent << std::endl;
  //  throw std::runtime_error("BO access speed is obviously degrading");
  //}
}

void
TEST_open_close_cu_context(shim_xdna::device::id_type id, std::shared_ptr<shim_xdna::device> sdev, arg_type& arg)
{
  auto dev = sdev.get();
  hw_ctx hwctx{dev};

  for (auto& ip : get_xclbin_ip_name2index(dev)) {
    auto idx = hwctx.get()->open_cu_context(ip.first);
    hwctx.get()->close_cu_context(idx);
    auto r = idx.index;
    auto e = ip.second.index;
    if (r != e) {
      std::string s = std::string("Wrong CU(") +
                      std::string(ip.first) +
                      std::string(") index: ") +
                      std::to_string(r) +
                      std::string(", should be: ") +
                      std::to_string(e);
      throw std::runtime_error(s);
    }
  }
}

void
TEST_create_destroy_hw_queue(shim_xdna::device::id_type id, std::shared_ptr<shim_xdna::device> sdev, arg_type& arg)
{
  hw_ctx hwctx{sdev.get()};
  // Test to create > 1 queues
  auto hwq = hwctx.get()->get_hw_queue();
  auto hwq2 = hwctx.get()->get_hw_queue();
}

// List of all test cases
std::vector<test_case> test_list {
  test_case{ "get_xrt_info",
    TEST_POSITIVE, no_dev_filter, TEST_get_xrt_info, {}
  },
  test_case{ "get_os_info",
    TEST_POSITIVE, no_dev_filter, TEST_get_os_info, {}
  },
  test_case{ "get_total_devices",
    TEST_POSITIVE, no_dev_filter, TEST_get_total_devices, {true}
  },
  //test_case{ "get_total_devices(mgmtpf)",
  //  TEST_POSITIVE, no_dev_filter, TEST_get_total_devices, {false}
  //},
  test_case{ "get_bdf_info_and_get_device_id",
    TEST_POSITIVE, no_dev_filter, TEST_get_bdf_info_and_get_device_id, {true}
  },
  //test_case{ "get_bdf_info_and_get_device_id(mgmtpf)",
  //  TEST_POSITIVE, no_dev_filter, TEST_get_bdf_info_and_get_device_id, {false}
  //},
  //test_case{ "get_mgmtpf_device",
  //  TEST_POSITIVE, no_dev_filter, TEST_get_mgmtpf_device, {}
  //},
  test_case{ "query(pcie_vendor)",
    TEST_POSITIVE, dev_filter_xdna, TEST_query_userpf<query::pcie_vendor>, {}
  },
  //test_case{ "non_xdna_userpf: query(pcie_vendor)",
  //  TEST_POSITIVE, dev_filter_not_xdna, TEST_query_userpf<query::pcie_vendor>, {}
  //},
  test_case{ "query(rom_vbnv)",
    TEST_POSITIVE, dev_filter_xdna, TEST_query_userpf<query::rom_vbnv>, {}
  },
  test_case{ "query(rom_fpga_name)",
    TEST_NEGATIVE, dev_filter_xdna, TEST_query_userpf<query::rom_fpga_name>, {}
  },
  //test_case{ "non_xdna_userpf: query(rom_vbnv)",
  //  TEST_POSITIVE, dev_filter_not_xdna, TEST_query_userpf<query::rom_vbnv>, {}
  //},
  test_case{ "create_destroy_hw_context",
    TEST_POSITIVE, dev_filter_is_aie, TEST_create_destroy_hw_context, {}
  },
  test_case{ "create_invalid_bo",
    TEST_NEGATIVE, dev_filter_xdna, TEST_create_free_bo, {XCL_BO_FLAGS_P2P, 0, 128}
  },
  test_case{ "create_and_free_exec_buf_bo",
    TEST_POSITIVE, dev_filter_xdna, TEST_create_free_bo, {XCL_BO_FLAGS_EXECBUF, 0, 128}
  },
  test_case{ "create_and_free_dpu_sequence_bo 1 bo",
    TEST_POSITIVE, dev_filter_xdna, TEST_create_free_bo, {XCL_BO_FLAGS_CACHEABLE, 0, 128}
  },
  test_case{ "create_and_free_dpu_sequence_bo multiple bos",
    TEST_POSITIVE, dev_filter_xdna, TEST_create_free_bo,
    {XCL_BO_FLAGS_CACHEABLE, 0, 0x2000, 0x400, 0x3000, 0x100}
  },
  test_case{ "create_and_free_input_output_bo 1 pages",
    TEST_POSITIVE, dev_filter_xdna, TEST_create_free_bo, {XCL_BO_FLAGS_NONE, 0, 128}
  },
  test_case{ "create_and_free_input_output_bo multiple pages",
    TEST_POSITIVE, dev_filter_xdna, TEST_create_free_bo,
    {XCL_BO_FLAGS_NONE, 0, 0x10000, 0x23000, 0x2000}
  },
  test_case{ "create_and_free_input_output_bo huge pages",
    TEST_POSITIVE, dev_filter_is_aie, TEST_create_free_bo,
    {XCL_BO_FLAGS_NONE, 0, 0x20000000}
  },
  test_case{ "sync_bo for dpu sequence bo",
    TEST_POSITIVE, dev_filter_xdna, TEST_sync_bo, {XCL_BO_FLAGS_CACHEABLE, 0, 128}
  },
  test_case{ "sync_bo for input_output",
    TEST_POSITIVE, dev_filter_xdna, TEST_sync_bo, {XCL_BO_FLAGS_NONE, 0, 128}
  },
  test_case{ "map dpu sequence bo and test perf",
    TEST_POSITIVE, dev_filter_xdna, TEST_map_bo, {XCL_BO_FLAGS_CACHEABLE, 0, 361264 /*0x10000*/}
  },
  test_case{ "map input_output bo and test perf",
    TEST_POSITIVE, dev_filter_xdna, TEST_map_bo, {XCL_BO_FLAGS_NONE, 0, 361264}
  },
  test_case{ "map bo for read only",
    TEST_NEGATIVE, dev_filter_xdna, TEST_map_read_bo, {0x1000}
  },
  test_case{ "map exec_buf_bo and test perf",
    TEST_POSITIVE, dev_filter_xdna, TEST_create_free_bo, {XCL_BO_FLAGS_EXECBUF, 0, 0x1000}
  },
  test_case{ "open_close_cu_context",
    TEST_POSITIVE, dev_filter_is_aie2, TEST_open_close_cu_context, {}
  },
  test_case{ "create_destroy_hw_queue",
    TEST_POSITIVE, dev_filter_is_aie2, TEST_create_destroy_hw_queue, {}
  },
  // Keep bad run before normal run to test recovery of hw ctx
  test_case{ "io test real kernel bad run",
    TEST_NEGATIVE, dev_filter_is_aie2, TEST_io, { IO_TEST_BAD_RUN, 1 }
  },
  test_case{ "io test real kernel good run",
    TEST_POSITIVE, dev_filter_is_aie2, TEST_io, { IO_TEST_NORMAL_RUN, 1 }
  },
  test_case{ "measure no-op kernel latency",
    TEST_POSITIVE, dev_filter_is_aie2, TEST_io_latency, { IO_TEST_NOOP_RUN, IO_TEST_IOCTL_WAIT, REPEAT_RUNS }
  },
  test_case{ "measure real kernel latency",
    TEST_POSITIVE, dev_filter_is_aie2, TEST_io_latency, { IO_TEST_NORMAL_RUN, IO_TEST_IOCTL_WAIT, REPEAT_RUNS }
  },
  test_case{ "create and free debug bo",
    TEST_POSITIVE, dev_filter_is_aie2, TEST_create_free_debug_bo, { 0x1000 }
  },
  test_case{ "create and free large debug bo",
    TEST_POSITIVE, dev_filter_is_aie2, TEST_create_free_debug_bo, { 0x100000 }
  },
  test_case{ "multi-command io test real kernel good run",
    TEST_POSITIVE, dev_filter_is_aie2, TEST_io, { IO_TEST_NORMAL_RUN, 3 }
  },
  test_case{ "measure no-op kernel throughput chained command",
    TEST_POSITIVE, dev_filter_is_aie2, TEST_io_runlist_throughput, { IO_TEST_NOOP_RUN, IO_TEST_IOCTL_WAIT, REPEAT_RUNS }
  },
  test_case{ "npu3 shim vadd",
    TEST_POSITIVE, dev_filter_is_aie4, TEST_shim_umq_vadd, {}
  },
  test_case{ "export import BO",
    TEST_POSITIVE, dev_filter_is_aie2, TEST_export_import_bo, {}
  },
  test_case{ "txn elf flow",
    TEST_POSITIVE, dev_filter_is_aie2, TEST_txn_elf_flow, {}
  },
  test_case{ "Cmd fencing (user space side)",
    TEST_POSITIVE, dev_filter_is_aie2, TEST_cmd_fence_host, {}
  },
  test_case{ "npu3 shim move memory tiles",
    TEST_POSITIVE, dev_filter_is_aie4, TEST_shim_umq_memtiles, {}
  },
  test_case{ "npu3 shim move ddr memory tiles",
    TEST_POSITIVE, dev_filter_is_aie4, TEST_shim_umq_ddr_memtile, {}
  },
  test_case{ "npu3 shim multi col remote barrier",
    TEST_POSITIVE, dev_filter_is_aie4, TEST_shim_umq_remote_barrier, {}
  },
  test_case{ "io test no op with duplicated BOs",
    TEST_POSITIVE, dev_filter_is_aie2, TEST_noop_io_with_dup_bo, {}
  },
  test_case{ "measure no-op kernel latency chained command",
    TEST_POSITIVE, dev_filter_is_aie2, TEST_io_runlist_latency, { IO_TEST_NOOP_RUN, IO_TEST_IOCTL_WAIT, REPEAT_RUNS }
  },
  test_case{ "measure no-op kernel throuput",
    TEST_POSITIVE, dev_filter_is_aie2, TEST_io_throughput, { IO_TEST_NOOP_RUN, IO_TEST_IOCTL_WAIT, REPEAT_RUNS }
  },
  test_case{ "measure no-op kernel latency (polling)",
    TEST_POSITIVE, dev_filter_is_aie2, TEST_io_latency, { IO_TEST_NOOP_RUN, IO_TEST_POLL_WAIT, REPEAT_RUNS }
  },
  test_case{ "measure no-op kernel throuput (polling)",
    TEST_POSITIVE, dev_filter_is_aie2, TEST_io_throughput, { IO_TEST_NOOP_RUN, IO_TEST_POLL_WAIT, REPEAT_RUNS }
  },
  test_case{ "measure no-op kernel latency chained command (polling)",
    TEST_POSITIVE, dev_filter_is_aie2, TEST_io_runlist_latency, { IO_TEST_NOOP_RUN, IO_TEST_POLL_WAIT, REPEAT_RUNS }
  },
  test_case{ "measure no-op kernel throughput chained command (polling)",
    TEST_POSITIVE, dev_filter_is_aie2, TEST_io_runlist_throughput, { IO_TEST_NOOP_RUN, IO_TEST_POLL_WAIT, REPEAT_RUNS }
  },
  test_case{ "Cmd fencing (driver side)",
    TEST_POSITIVE, dev_filter_is_aie2, TEST_cmd_fence_device, {}
  },
  test_case{ "sync_bo for input_output 1MiB BO",
    TEST_POSITIVE, dev_filter_xdna, TEST_sync_bo, {XCL_BO_FLAGS_NONE, 0, 0x100000}
  },
  test_case{ "sync_bo for input_output 1MiB BO w/ offset and size",
    TEST_POSITIVE, dev_filter_xdna, TEST_sync_bo_off_size, {XCL_BO_FLAGS_NONE, 0, 0x100000, 0x1004, 0x3c}
  },
};

} // namespace

void
run_test(int id, const test_case& test, bool force, const shim_xdna::device::id_type& num_of_devices)
{
  bool failed = (test.mode == TEST_NEGATIVE);
  bool skipped = true;

  std::cout << "====== " << id << ": " << test.description << " started =====" << std::endl;
  try {
    if (test.dev_filter == no_dev_filter) { // system test
      skipped = false;
      test.func(0, nullptr, test.arg);
    } else { // per user device test
      for (shim_xdna::device::id_type i = 0; i < num_of_devices; i++) {
        auto dev = shim_xdna::my_get_userpf_device(i);
        if (!force && !test.dev_filter(i, dev.get()))
          continue;
        skipped = false;
        test.func(i, std::move(dev), test.arg);
      }
    }
  }
  catch (const std::exception& ex) {
    skipped = false;
    std::cerr << ex.what() << std::endl;
    failed = !failed;
  }

  std::string result;
  if (skipped)
    result = "skipped";
  else
    result = failed ? "\x1b[5m\x1b[31mFAILED\x1b[0m" : "passed";
  std::cout << "====== " << id << ": " << test.description << " " << result << "  =====" << std::endl;

  if (skipped)
    test_skipped++;
  else if (failed)
    test_failed++;
  else
    test_passed++;
}

void
run_all_test(std::set<int>& tests)
{
  auto all = tests.empty();
  shim_xdna::device::id_type total_dev = 1;

  for (int i = 0; i < test_list.size(); i++) {
    if (!all) {
      if (tests.find(i) == tests.end())
        continue;
      else
        tests.erase(i);
    }
    const auto& t = test_list[i];
    run_test(i, t, !all, total_dev);
    std::cout << std::endl;
  }
}

namespace sfs = std::filesystem;

int
main(int argc, char **argv)
{
  program = std::filesystem::path(argv[0]).filename();
  std::set<int> tests;

  try {
    int first_test_id = 1;

    if (argc >= 2) {
      std::ifstream xclbin(argv[first_test_id]);
      if (xclbin) {
        xclbin_path = argv[1];
        std::cout << "Xclbin file: " << xclbin_path << std::endl;
        first_test_id++;
      }
    }

    for (int i = first_test_id; i < argc; i++)
      tests.insert(std::stoi(argv[i]));
  }
  catch (...) {
    usage(program);
    return 2;
  }

  std::shared_ptr<shim_xdna::drv> driver = std::make_shared<shim_xdna::drv>();
  const sfs::path drvpath = "/sys/bus/pci/drivers/amdxdna/0000:c5:00.1";
  std::shared_ptr<shim_xdna::pdev> pf = std::make_shared<shim_xdna::pdev_kmq>(driver, drvpath.filename().string());

  add_to_user_ready_list(pf);

  cur_path = dirname(argv[0]);
  set_xrt_path();

  run_all_test(tests);

  if (!tests.empty()) {
    std::cout << tests.size() << "\ttest(s) not found:";
    for (auto i : tests)
      std::cout << " " << i;
    std::cout << std::endl;
  }

  if (test_skipped)
    std::cout << test_skipped << "\ttest(s) skipped" << std::endl;

  if (test_passed + test_failed == 0)
    return 0;

  std::cout << test_passed + test_failed << "\ttest(s) executed" << std::endl;
  if (test_failed == 0) {
    std::cout << "ALL " << test_passed << " executed test(s) PASSED!" << std::endl;
    return 0;
  }
  std::cout << test_failed << "\ttest(s) \x1b[5m\x1b[31mFAILED\x1b[0m!" << std::endl;
  return 1;
}

// vim: ts=2 sw=2 expandtab
