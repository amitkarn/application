// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPLICATION_SRC_ROOT_ENVIRONMENT_HOST_H_
#define APPLICATION_SRC_ROOT_ENVIRONMENT_HOST_H_

#include <memory>

#include "application/services/application_environment_host.fidl.h"
#include "application/services/service_provider.fidl.h"
#include "application/src/application_environment_impl.h"
#include "application/src/root_application_loader.h"
#include "lib/fidl/cpp/bindings/binding.h"
#include "lib/fidl/cpp/bindings/binding_set.h"
#include "lib/ftl/macros.h"

namespace app {

class RootEnvironmentHost : public ApplicationEnvironmentHost,
                            public ServiceProvider {
 public:
  explicit RootEnvironmentHost(std::vector<std::string> application_path);
  ~RootEnvironmentHost() override;

  ApplicationEnvironmentImpl* environment() const { return environment_.get(); }

  // ApplicationEnvironmentHost implementation:

  void GetApplicationEnvironmentServices(
      fidl::InterfaceRequest<ServiceProvider> environment_services) override;

  // ServiceProvider implementation:

  void ConnectToService(const fidl::String& interface_name,
                        mx::channel channel) override;

 private:
  RootApplicationLoader loader_;
  fidl::Binding<ApplicationEnvironmentHost> host_binding_;
  fidl::BindingSet<ApplicationLoader> loader_bindings_;
  fidl::BindingSet<ServiceProvider> service_provider_bindings_;

  std::vector<std::string> path_;
  std::unique_ptr<ApplicationEnvironmentImpl> environment_;

  FTL_DISALLOW_COPY_AND_ASSIGN(RootEnvironmentHost);
};

}  // namespace app

#endif  // APPLICATION_SRC_ROOT_ENVIRONMENT_HOST_H_
