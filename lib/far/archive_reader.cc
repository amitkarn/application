// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "application/lib/far/archive_reader.h"

#include <inttypes.h>
#include <unistd.h>

#include <limits>
#include <utility>

#include "application/lib/far/file_operations.h"
#include "application/lib/far/format.h"

namespace archive {
namespace {

struct PathComparator {
  ArchiveReader* reader = nullptr;

  bool operator()(const DirectoryTableEntry& lhs, const ftl::StringView& rhs) {
    return reader->GetPathView(lhs) < rhs;
  }
};

}  // namespace

ArchiveReader::ArchiveReader(ftl::UniqueFD fd) : fd_(std::move(fd)) {}

ArchiveReader::~ArchiveReader() = default;

bool ArchiveReader::Read() {
  return ReadIndex() && ReadDirectory();
}

bool ArchiveReader::ExtractFile(ftl::StringView archive_path,
                                const char* output_path) {
  PathComparator comparator;
  comparator.reader = this;

  auto it = std::lower_bound(directory_table_.begin(), directory_table_.end(),
                             archive_path, comparator);
  if (it == directory_table_.end() || GetPathView(*it) != archive_path)
    return false;
  if (lseek(fd_.get(), it->data_offset, SEEK_SET) < 0) {
    fprintf(stderr, "error: Failed to seek to offset of file.\n");
    return false;
  }
  if (!CopyFileToPath(fd_.get(), output_path, it->data_length)) {
    fprintf(stderr, "error: Failed write contents to '%s'.\n", output_path);
    return false;
  }
  return true;
}

ftl::UniqueFD ArchiveReader::TakeFileDescriptor() {
  return std::move(fd_);
}

ftl::StringView ArchiveReader::GetPathView(const DirectoryTableEntry& entry) {
  return ftl::StringView(path_data_.data() + entry.name_offset,
                         entry.name_length);
}

bool ArchiveReader::ReadIndex() {
  if (lseek(fd_.get(), 0, SEEK_SET) < 0) {
    fprintf(stderr, "error: Failed to seek to beginning of archive.\n");
    return false;
  }

  IndexChunk index_chunk;
  if (!ReadObject(fd_.get(), &index_chunk)) {
    fprintf(stderr,
            "error: Failed read index chunk. Is this file an archive?\n");
    return false;
  }

  if (index_chunk.magic != kMagic) {
    fprintf(stderr,
            "error: Index chunk missing magic. Is this file an archive?\n");
    return false;
  }

  if (index_chunk.length % sizeof(IndexEntry) != 0 ||
      index_chunk.length >
          std::numeric_limits<uint64_t>::max() - sizeof(IndexChunk)) {
    fprintf(stderr, "error: Invalid index chunk length.\n");
    return false;
  }

  index_.resize(index_chunk.length / sizeof(IndexEntry));
  if (!ReadVector(fd_.get(), &index_)) {
    fprintf(stderr, "error: Failed to read contents of index chunk.\n");
    return false;
  }

  uint64_t next_offset = sizeof(IndexChunk) + index_chunk.length;
  for (const auto& entry : index_) {
    if (entry.offset != next_offset) {
      fprintf(stderr,
              "error: Chunk at offset %" PRIu64 " not tightly packed.\n",
              entry.offset);
      return false;
    }
    if (entry.length % 8 != 0) {
      fprintf(stderr,
              "error: Chunk length %" PRIu64
              " not aligned to 8 byte boundary.\n",
              entry.length);
      return false;
    }
    if (entry.length > std::numeric_limits<uint64_t>::max() - entry.offset) {
      fprintf(stderr,
              "error: Chunk length %" PRIu64
              " overflowed total archive size.\n",
              entry.length);
      return false;
    }
    next_offset = entry.offset + entry.length;
  }

  return true;
}

bool ArchiveReader::ReadDirectory() {
  IndexEntry* dir_entry = GetIndexEntry(kDirType);
  if (!dir_entry) {
    fprintf(stderr, "error: Cannot find directory chunk.\n");
    return false;
  }
  if (dir_entry->length % sizeof(DirectoryTableEntry) != 0) {
    fprintf(stderr, "error: Invalid directory chunk length: %" PRIu64 ".\n",
            dir_entry->length);
    return false;
  }
  uint64_t file_count = dir_entry->length / sizeof(DirectoryTableEntry);
  directory_table_.resize(file_count);

  if (lseek(fd_.get(), dir_entry->offset, SEEK_SET) < 0) {
    fprintf(stderr, "error: Failed to seek to directory chunk.\n");
    return false;
  }
  if (!ReadVector(fd_.get(), &directory_table_)) {
    fprintf(stderr, "error: Failed to read directory table.\n");
    return false;
  }

  IndexEntry* dirnames_entry = GetIndexEntry(kDirnamesType);
  if (!dirnames_entry) {
    fprintf(stderr, "error: Cannot find directory names chunk.\n");
    return false;
  }
  path_data_.resize(dirnames_entry->length);

  if (lseek(fd_.get(), dirnames_entry->offset, SEEK_SET) < 0) {
    fprintf(stderr, "error: Failed to seek to directory names chunk.\n");
    return false;
  }
  if (!ReadVector(fd_.get(), &path_data_)) {
    fprintf(stderr, "error: Failed to read directory names.\n");
    return false;
  }

  return true;
}

IndexEntry* ArchiveReader::GetIndexEntry(uint64_t type) {
  for (auto& entry : index_) {
    if (entry.type == type)
      return &entry;
  }
  return nullptr;
}

}  // namespace archive
