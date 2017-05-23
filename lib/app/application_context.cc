// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <magenta/process.h>
#include <magenta/processargs.h>
#include <mxio/util.h>

#include "application/lib/app/application_context.h"
#include "application/lib/app/connect.h"
#include "lib/ftl/logging.h"

namespace app {
namespace {

mx::channel GetServiceRoot() {
  mx::channel h1, h2;
  if (mx::channel::create(0, &h1, &h2) != NO_ERROR)
    return mx::channel();

  if (mxio_service_connect("/svc/.", h1.release()) != NO_ERROR)
    return mx::channel();

  return h2;
}

}  // namespace

ApplicationContext::ApplicationContext(
    mx::channel service_root,
    fidl::InterfaceRequest<ServiceProvider> outgoing_services)
    : outgoing_services_(std::move(outgoing_services)),
      service_root_(std::move(service_root)) {
  ConnectToEnvironmentService(environment_.NewRequest());
  ConnectToEnvironmentService(launcher_.NewRequest());
}

ApplicationContext::~ApplicationContext() = default;

std::unique_ptr<ApplicationContext>
ApplicationContext::CreateFromStartupInfo() {
  auto startup_info = CreateFromStartupInfoNotChecked();
  FTL_CHECK(startup_info->environment().get() != nullptr)
      << "The ApplicationEnvironment is null. Usually this means you need to "
         "use @boot on the Magenta command line. Otherwise, use "
         "CreateFromStartupInfoNotChecked() to allow |environment| to be null.";
  return startup_info;
}

std::unique_ptr<ApplicationContext>
ApplicationContext::CreateFromStartupInfoNotChecked() {
  mx_handle_t services = mx_get_startup_handle(PA_APP_SERVICES);
  return std::make_unique<ApplicationContext>(
      GetServiceRoot(),
      fidl::InterfaceRequest<ServiceProvider>(mx::channel(services)));
}

void ApplicationContext::ConnectToEnvironmentService(
    const std::string& interface_name,
    mx::channel channel) {
  mxio_service_connect_at(service_root_.get(), interface_name.c_str(),
                          channel.release());
}

}  // namespace app
