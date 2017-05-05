// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPLICATION_SRC_MANAGER_APPLICATION_CONTROLLER_IMPL_H_
#define APPLICATION_SRC_MANAGER_APPLICATION_CONTROLLER_IMPL_H_

#include <mx/process.h>

#include "application/services/application_controller.fidl.h"
#include "lib/fidl/cpp/bindings/binding.h"
#include "lib/ftl/macros.h"
#include "lib/mtl/tasks/message_loop.h"
#include "lib/mtl/tasks/message_loop_handler.h"

namespace app {
class ApplicationEnvironmentImpl;

class ApplicationControllerImpl : public ApplicationController,
                                  public mtl::MessageLoopHandler {
 public:
  ApplicationControllerImpl(
      fidl::InterfaceRequest<ApplicationController> request,
      ApplicationEnvironmentImpl* environment,
      mx::process process,
      std::string path);
  ~ApplicationControllerImpl() override;

  const std::string& path() const { return path_; }

  // |ApplicationController| implementation:
  void Kill() override;
  void Detach() override;

 private:
  // |mtl::MessageLoopHandler| implementation:
  void OnHandleReady(mx_handle_t handle, mx_signals_t pending) override;

  fidl::Binding<ApplicationController> binding_;
  ApplicationEnvironmentImpl* environment_;
  mx::process process_;
  std::string path_;

  mtl::MessageLoop::HandlerKey termination_handler_ = 0u;

  FTL_DISALLOW_COPY_AND_ASSIGN(ApplicationControllerImpl);
};

}  // namespace app

#endif  // APPLICATION_SRC_MANAGER_APPLICATION_CONTROLLER_IMPL_H_
