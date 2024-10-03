//
// Created by mlevental on 10/3/24.
//

#include "get_user_pf.h"

#include "../../src/shim/pcidrv.h"
#include "core/common/device.h"

#include <filesystem>
#include <fstream>

namespace {

static std::map<xrt_core::device::handle_type, std::weak_ptr<xrt_core::device>>
    userpf_device_map;

static std::vector<std::shared_ptr<xrt_core::pci::dev>> user_ready_list;

// mutex to protect insertion
static std::mutex mutex;

} // namespace

void add_to_user_ready_list(const std::shared_ptr<xrt_core::pci::dev>& dev) {
  user_ready_list.push_back(dev);
}

std::shared_ptr<xrt_core::device>
my_get_userpf_device(xrt_core::device::handle_type handle)
{
  std::lock_guard lk(mutex);
  auto itr = userpf_device_map.find(handle);
  if (itr != userpf_device_map.end())
    return (*itr).second.lock();
  return nullptr;
}

std::shared_ptr<xrt_core::device>
my_get_userpf_device(xrt_core::device::handle_type handle, xrt_core::device::id_type id)
{
  // Check device map cache
  if (auto device = my_get_userpf_device(handle)) {
    if (device->get_device_id() != id)
        throw std::runtime_error("my_get_userpf_device: id mismatch");
    return device;
  }

  auto pdev = user_ready_list.at(id);
  auto device = pdev->create_device(handle, id);

  std::lock_guard lk(mutex);
  userpf_device_map[handle] = device;  // create or replace
  return device;
}

std::shared_ptr<xrt_core::device> my_get_userpf_device(xrt_core::device::id_type id) {
  // Construct device by calling xclOpen, the returned
  // device is cached and unmanaged
  const auto& pdev = user_ready_list.at(id);
  auto device = my_get_userpf_device(pdev->create_shim(id));

  if (!device)
    throw std::runtime_error("Could not open device with index '" +
                             std::to_string(id) + "'");

  // Repackage raw ptr in new shared ptr with deleter that calls xclClose,
  // but leaves device object alone. The returned device is managed in that
  // it calls xclClose when going out of scope.
  auto close = [](xrt_core::device *d) { d->close_device(); };
  std::shared_ptr<xrt_core::device> ptr{device.get(), close};

  // The repackage raw ptr is the one that should be cached so
  // so that all references to device handles in application code
  // are tied to the shared ptr that ends up calling xclClose
  std::lock_guard lk(mutex);
  userpf_device_map[device->get_device_handle()] = ptr;
  return ptr;
}