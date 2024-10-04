//
// Created by mlevental on 10/3/24.
//

#include "shim.h"
#include "pcidrv.h"
#include "device.h"
#include "pcidev.h"

#include <filesystem>
#include <fstream>

namespace  {

static std::map<shim_xdna::device::handle_type, std::weak_ptr<shim_xdna::device>>
    userpf_device_map;

static std::vector<std::shared_ptr<shim_xdna::pdev>> user_ready_list;

// mutex to protect insertion
static std::mutex mutex;

} // namespace

namespace shim_xdna {
void add_to_user_ready_list(const std::shared_ptr<shim_xdna::pdev> &dev) {
  user_ready_list.push_back(dev);
}

std::shared_ptr<shim_xdna::pdev> get_dev(unsigned index) {
  return user_ready_list.at(index);
}

std::shared_ptr<shim_xdna::device>
my_get_userpf_device(shim_xdna::device::handle_type handle) {
  std::lock_guard lk(mutex);
  auto itr = userpf_device_map.find(handle);
  if (itr != userpf_device_map.end())
    return (*itr).second.lock();
  return nullptr;
}

std::shared_ptr<shim_xdna::device>
my_get_userpf_device(shim_xdna::device::handle_type handle,
                     shim_xdna::device::id_type id) {
  // Check device map cache
  if (auto device = my_get_userpf_device(handle)) {
    if (device->get_device_id() != id)
      throw std::runtime_error("my_get_userpf_device: id mismatch");
    return device;
  }

  auto pdev = get_dev(id);
  auto device = pdev->create_device(handle, id);

  std::lock_guard lk(mutex);
  userpf_device_map[handle] = device; // create or replace
  return device;
}

std::shared_ptr<shim_xdna::device>
my_get_userpf_device(shim_xdna::device::id_type id) {
  // Construct device by calling xclOpen, the returned
  // device is cached and unmanaged
  const auto &pdev = get_dev(id);
  auto device = my_get_userpf_device(pdev->create_shim(id));

  if (!device)
    throw std::runtime_error("Could not open device with index '" +
                             std::to_string(id) + "'");

  // Repackage raw ptr in new shared ptr with deleter that calls xclClose,
  // but leaves device object alone. The returned device is managed in that
  // it calls xclClose when going out of scope.
  auto close = [](shim_xdna::device *d) { d->close_device(); };
  std::shared_ptr<shim_xdna::device> ptr{device.get(), close};

  // The repackage raw ptr is the one that should be cached so
  // so that all references to device handles in application code
  // are tied to the shared ptr that ends up calling xclClose
  std::lock_guard lk(mutex);
  userpf_device_map[device->get_device_handle()] = ptr;
  return ptr;
}
}