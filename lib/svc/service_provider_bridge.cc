// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "application/lib/svc/service_provider_bridge.h"

#include <fcntl.h>
#include <magenta/device/vfs.h>
#include <mxio/util.h>

#include <utility>

#include "lib/mtl/vfs/vfs_serve.h"

namespace app {

ServiceProviderBridge::ServiceProviderBridge()
    : directory_(mxtl::AdoptRef(new svcfs::VnodeProviderDir(&dispatcher_))) {
  directory_->SetServiceProvider(this);
}

ServiceProviderBridge::~ServiceProviderBridge() {
  directory_->SetServiceProvider(nullptr);
}

void ServiceProviderBridge::AddBinding(
    fidl::InterfaceRequest<app::ServiceProvider> request) {
  bindings_.AddBinding(this, std::move(request));
}

void ServiceProviderBridge::AddServiceForName(ServiceConnector connector,
                                              const std::string& service_name) {
  name_to_service_connector_[service_name] = std::move(connector);
}

bool ServiceProviderBridge::ServeDirectory(mx::channel channel) const {
  return mtl::VFSServe(directory_, std::move(channel));
}

mx::channel ServiceProviderBridge::OpenAsDirectory() {
  mx::channel h1, h2;
  if (mx::channel::create(0, &h1, &h2) < 0)
    return mx::channel();
  if (!ServeDirectory(std::move(h1)))
    return mx::channel();
  return h2;
}

int ServiceProviderBridge::OpenAsFileDescriptor() {
  mx::channel h1, h2;
  if (mx::channel::create(0, &h1, &h2) < 0)
    return -1;
  if (!ServeDirectory(std::move(h1)))
    return -1;
  mxio_t* io = mxio_remote_create(h2.release(), MX_HANDLE_INVALID);
  if (!io)
    return -1;
  return mxio_bind_to_fd(io, -1, 0);
}

void ServiceProviderBridge::Connect(const char* name,
                                    size_t len,
                                    mx::channel channel) {
  ConnectToService(fidl::String(name, len), std::move(channel));
}

void ServiceProviderBridge::ConnectToService(const fidl::String& service_name,
                                             mx::channel channel) {
  auto it = name_to_service_connector_.find(service_name.get());
  if (it != name_to_service_connector_.end())
    it->second(std::move(channel));
  else
    backend_->ConnectToService(std::move(service_name), std::move(channel));
}

}  // namespace app
