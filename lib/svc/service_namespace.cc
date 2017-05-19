// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "application/lib/svc/service_namespace.h"

#include <fcntl.h>
#include <magenta/device/vfs.h>
#include <mxio/util.h>

#include <utility>

#include "lib/ftl/files/unique_fd.h"
#include "lib/mtl/vfs/vfs_handler.h"

namespace app {

ServiceNamespace::ServiceNamespace()
    : directory_(mxtl::AdoptRef(new svcfs::VnodeDir(&dispatcher_))) {
}

ServiceNamespace::ServiceNamespace(
    fidl::InterfaceRequest<app::ServiceProvider> request)
    : directory_(mxtl::AdoptRef(new svcfs::VnodeDir(&dispatcher_))) {
  AddBinding(std::move(request));
}

ServiceNamespace::~ServiceNamespace() {
  directory_->RemoveAllServices();
  directory_ = nullptr;
}

void ServiceNamespace::AddBinding(
    fidl::InterfaceRequest<app::ServiceProvider> request) {
  if (request)
    bindings_.AddBinding(this, std::move(request));
}

void ServiceNamespace::Close() {
  bindings_.CloseAllBindings();
}

void ServiceNamespace::AddServiceForName(ServiceConnector connector,
                                         const std::string& service_name) {
  name_to_service_connector_[service_name] = std::move(connector);
  directory_->AddService(service_name.data(), service_name.length(), this);
}

bool ServiceNamespace::ServeDirectory(mx::channel channel) {
  if (directory_->Open(O_DIRECTORY) < 0)
    return false;

  mx_handle_t h = channel.release();
  if (directory_->Serve(h, 0) < 0) {
    directory_->Close();
    return false;
  }

  // Setting this signal indicates that this directory is actively being served.
  mx_object_signal_peer(h, 0, MX_USER_SIGNAL_0);
  return true;
}

int ServiceNamespace::OpenAsFileDescriptor() {
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

bool ServiceNamespace::MountAtPath(const char* path) {
  mx::channel h1, h2;
  if (mx::channel::create(0, &h1, &h2) < 0)
    return false;

  if (!ServeDirectory(std::move(h1)))
    return false;

  ftl::UniqueFD fd(open(path, O_DIRECTORY | O_RDWR));
  if (fd.get() < 0)
    return false;

  mx_handle_t h = h2.release();
  return ioctl_vfs_mount_fs(fd.get(), &h) >= 0;
}

void ServiceNamespace::Connect(const char* name, size_t len,
                               mx::channel channel) {
  ConnectCommon(std::string(name, len), std::move(channel));
}

void ServiceNamespace::ConnectToService(const fidl::String& service_name,
                                        mx::channel channel) {
  ConnectCommon(service_name, std::move(channel));
}

void ServiceNamespace::ConnectCommon(const std::string& service_name,
                                     mx::channel channel) {
  auto it = name_to_service_connector_.find(service_name);
  if (it != name_to_service_connector_.end())
    it->second(std::move(channel));
}

}  // namespace app
