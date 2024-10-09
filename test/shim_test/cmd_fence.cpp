// SPDX-License-Identifier: Apache-2.0
// Copyright (C) 2024, Advanced Micro Devices, Inc. All rights reserved.

#include "bo.h"
#include "io.h"
#include "hwctx.h"
#include "2proc.h"
#include "dev_info.h"

#include "../../src/shim/hwq.h"
#include <algorithm>

namespace {

using arg_type = const std::vector<uint64_t>;

class test_2proc_cmd_fence_host : public test_2proc
{
public:
  test_2proc_cmd_fence_host(uint32_t id) : test_2proc(id)
  {}

private:
  struct ipc_data {
    pid_t pid;
    int shdl; // for signaling
    int whdl; // for waiting
  };

  void
  run_test_parent() override
  {
    msg("user fence test started...");

    ipc_data idata = {};
    if (!recv_ipc_data(&idata, sizeof(idata)))
      return;
    msg("Received cmd fence fd %d %d from pid %d", idata.shdl, idata.whdl, idata.pid);

    auto dev = shim_xdna::my_get_userpf_device(get_dev_id());
    auto wfence = dev->import_fence(idata.pid, idata.whdl);
    auto sfence = dev->import_fence(idata.pid, idata.shdl);

    wfence->wait(0);
    sfence->signal();
    wfence->wait(0);
    sfence->signal();
    wfence->wait(0);
    sfence->signal();

    bool success = true;
    send_ipc_data(&success, sizeof(success));
  }

  void
  run_test_child() override
  {
    msg("user fence test started...");

    auto dev = shim_xdna::my_get_userpf_device(get_dev_id());
    auto sfence = dev->create_fence(shim_xdna::fence_handle::access_mode::process);
    auto wfence = dev->create_fence(shim_xdna::fence_handle::access_mode::process);
    auto sshare = sfence->share_handle();
    auto wshare = wfence->share_handle();
    ipc_data idata = { getpid(), sshare->get_export_handle(), wshare->get_export_handle() };
    send_ipc_data(&idata, sizeof(idata));

    wfence->signal();
    sfence->wait(0);
    wfence->signal();
    sfence->wait(0);
    wfence->signal();
    sfence->wait(0);

    bool success;
    recv_ipc_data(&success, sizeof(success));
  }
};

class test_2proc_cmd_fence_device : public test_2proc
{
public:
  test_2proc_cmd_fence_device(uint32_t id) : test_2proc(id)
  {}

private:
  struct ipc_data {
    pid_t pid;
    int hdl;
  };

  void
  run_test_parent() override
  {
    msg("device fence test started...");

    ipc_data idata = {};
    if (!recv_ipc_data(&idata, sizeof(idata)))
      return;
    msg("Received cmd fence fd %d from pid %d", idata.hdl, idata.pid);

    auto dev = shim_xdna::my_get_userpf_device(get_dev_id());
    auto fence = dev->import_fence(idata.pid, idata.hdl);
    const std::vector<shim_xdna::fence_handle*> wfences{fence.get()};
    const std::vector<shim_xdna::fence_handle*> sfences{};

    auto wrk = get_xclbin_workspace(dev.get());
    io_test_bo_set boset{dev.get(), wrk + "/data/"};
    boset.run(wfences, sfences, false);
    boset.run(wfences, sfences, false);
    boset.run(wfences, sfences, false);

    bool success = true;
    send_ipc_data(&success, sizeof(success));
  }

  void
  run_test_child() override
  {
    msg("device fence test started...");

    auto dev = shim_xdna::my_get_userpf_device(get_dev_id());
    auto fence = dev->create_fence(shim_xdna::fence_handle::access_mode::process);
    const std::vector<shim_xdna::fence_handle*> sfences{fence.get()};
    const std::vector<shim_xdna::fence_handle*> wfences{};
    auto share = fence->share_handle();
    ipc_data idata = { getpid(), share->get_export_handle() };
    send_ipc_data(&idata, sizeof(idata));

    hw_ctx hwctx{dev.get()};
    auto hwq = hwctx.get()->get_hw_queue();

    auto wrk = get_xclbin_workspace(dev.get());
    io_test_bo_set boset{dev.get(), wrk + "/data/"};
    hwq->submit_signal(fence.get());
    boset.run(wfences, sfences, false);
    boset.run(wfences, sfences, false);

    bool success;
    recv_ipc_data(&success, sizeof(success));
  }
};

}

void
TEST_cmd_fence_host(uint32_t id, std::shared_ptr<shim_xdna::device> sdev, arg_type& arg)
{
  // Can't fork with opened device.
  sdev.reset();

  test_2proc_cmd_fence_host t2p(id);
  t2p.run_test();
}

void
TEST_cmd_fence_device(uint32_t id, std::shared_ptr<shim_xdna::device> sdev, arg_type& arg)
{
  // Can't fork with opened device.
  sdev.reset();

  test_2proc_cmd_fence_device t2p(id);
  t2p.run_test();
}
