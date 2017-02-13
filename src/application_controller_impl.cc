// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "application/src/application_controller_impl.h"

#include <utility>

#include "application/src/application_environment_impl.h"
#include "lib/ftl/functional/closure.h"
#include "lib/mtl/tasks/message_loop.h"

namespace app {

ApplicationControllerImpl::ApplicationControllerImpl(
    fidl::InterfaceRequest<ApplicationController> request,
    ApplicationEnvironmentImpl* environment,
    mx::process process,
    std::string path)
    : binding_(this),
      environment_(environment),
      process_(std::move(process)),
      path_(std::move(path)) {
  termination_handler_ = mtl::MessageLoop::GetCurrent()->AddHandler(
      this, process_.get(), MX_TASK_TERMINATED);
  if (request.is_pending()) {
    binding_.Bind(std::move(request));
    binding_.set_connection_error_handler([this] {
      environment_->ExtractApplication(this);
      // The destructor of the temporary returned by ExtractApplication destroys
      // |this| at the end of the previous statement.
    });
  }
}

ApplicationControllerImpl::~ApplicationControllerImpl() {
  RemoveTerminationHandlerIfNeeded();
  // NOTE(abdulla): This is a short-term fix until we better understand process,
  // module, and application life-cycles.
  process_.kill();
}

void ApplicationControllerImpl::Kill(const KillCallback& callback) {
  environment_->ExtractApplication(this);
  // The destructor of the temporary returned by ExtractApplication destroys
  // |this| at the end of the previous statement.
  callback();
}

void ApplicationControllerImpl::Detach() {
  binding_.set_connection_error_handler(ftl::Closure());
}

void ApplicationControllerImpl::OnHandleReady(mx_handle_t handle,
                                              mx_signals_t pending) {
  FTL_DCHECK(handle == process_.get());
  FTL_DCHECK(pending & MX_TASK_TERMINATED);
  environment_->ExtractApplication(this);
  // The destructor of the temporary returned by ExtractApplication destroys
  // |this| at the end of the previous statement.
}

void ApplicationControllerImpl::RemoveTerminationHandlerIfNeeded() {
  if (termination_handler_) {
    mtl::MessageLoop::GetCurrent()->RemoveHandler(termination_handler_);
    termination_handler_ = 0u;
  }
}

}  // namespace app
