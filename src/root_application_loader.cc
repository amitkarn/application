// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "application/src/root_application_loader.h"

#include <fcntl.h>

#include <utility>

#include "application/src/url_resolver.h"
#include "lib/ftl/files/unique_fd.h"
#include "lib/ftl/logging.h"
#include "lib/mtl/vmo/file.h"

namespace app {

RootApplicationLoader::RootApplicationLoader(std::vector<std::string> path)
    : path_(std::move(path)) {}

RootApplicationLoader::~RootApplicationLoader() {}

void RootApplicationLoader::LoadApplication(
    const fidl::String& url,
    const ApplicationLoader::LoadApplicationCallback& callback) {
  std::string path = GetPathFromURL(url);
  if (path.empty()) {
    // TODO(abarth): Support URL schemes other than file:// by querying the host
    // for an application runner.
    FTL_LOG(ERROR) << "Cannot load " << url
                   << " because the scheme is not supported.";
  } else {
    ftl::UniqueFD fd(open(path.c_str(), O_RDONLY));
    if (!fd.is_valid() && path[0] != '/') {
      for (const auto& entry : path_) {
        std::string qualified_path = entry + "/" + path;
        fd.reset(open(qualified_path.c_str(), O_RDONLY));
        if (fd.is_valid()) {
          path = qualified_path;
          break;
        }
      }
    }
    mx::vmo data;
    if (fd.is_valid() && mtl::VmoFromFd(std::move(fd), &data)) {
      ApplicationPackagePtr package = ApplicationPackage::New();
      package->data = std::move(data);
      callback(std::move(package));
      return;
    }
    FTL_LOG(ERROR) << "Could not load url: " << url;
  }

  callback(nullptr);
}

}  // namespace app
