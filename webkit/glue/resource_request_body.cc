// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/resource_request_body.h"

#include "base/logging.h"
#include "net/base/upload_data.h"
#include "webkit/blob/blob_storage_controller.h"

using webkit_blob::BlobData;
using webkit_blob::BlobStorageController;

namespace webkit_glue {

ResourceRequestBody::Element::Element()
    : type_(TYPE_BYTES),
      bytes_start_(NULL),
      bytes_length_(0),
      file_range_offset_(0),
      file_range_length_(kuint64max) {
}

ResourceRequestBody::Element::~Element() {}

ResourceRequestBody::ResourceRequestBody() : identifier_(0) {}

void ResourceRequestBody::AppendBytes(const char* bytes, int bytes_len) {
  if (bytes_len > 0) {
    elements_.push_back(Element());
    elements_.back().SetToBytes(bytes, bytes_len);
  }
}

void ResourceRequestBody::AppendFileRange(
    const FilePath& file_path,
    uint64 offset, uint64 length,
    const base::Time& expected_modification_time) {
  elements_.push_back(Element());
  elements_.back().SetToFilePathRange(file_path, offset, length,
                                      expected_modification_time);
}

void ResourceRequestBody::AppendBlob(const GURL& blob_url) {
  elements_.push_back(Element());
  elements_.back().SetToBlobUrl(blob_url);
}

net::UploadData* ResourceRequestBody::ResolveElementsAndCreateUploadData(
    BlobStorageController* blob_controller) {
  net::UploadData* upload_data = new net::UploadData;
  // We attach 'this' to UploadData so that we do not need to copy
  // bytes for TYPE_BYTES.
  upload_data->SetUserData(
      this, new base::UserDataAdapter<ResourceRequestBody>(this));
  std::vector<net::UploadElement>* elements =
      upload_data->elements_mutable();
  for (size_t i = 0; i < elements_.size(); ++i) {
    const Element& element = elements_[i];
    switch (element.type()) {
      case TYPE_BYTES:
        elements->push_back(net::UploadElement());
        elements->back().SetToSharedBytes(element.bytes(),
                                        element.bytes_length());
        break;
      case TYPE_FILE:
        elements->push_back(net::UploadElement());
        elements->back().SetToFilePathRange(
            element.file_path(),
            element.file_range_offset(),
            element.file_range_length(),
            element.expected_file_modification_time());
        break;
      case TYPE_BLOB:
        ResolveBlobReference(blob_controller, element.blob_url(), elements);
        break;
    }
  }
  upload_data->set_identifier(identifier_);
  return upload_data;
}

ResourceRequestBody::~ResourceRequestBody() {}

void ResourceRequestBody::ResolveBlobReference(
    webkit_blob::BlobStorageController* blob_controller,
    const GURL& blob_url, std::vector<net::UploadElement>* elements) {
  DCHECK(blob_controller);
  BlobData* blob_data = blob_controller->GetBlobDataFromUrl(blob_url);
  DCHECK(blob_data);
  if (!blob_data)
    return;

  // If there is no element in the referred blob data, just return true.
  if (blob_data->items().empty())
    return;

  // Ensure the blob and any attached shareable files survive until
  // upload completion.
  SetUserData(blob_data, new base::UserDataAdapter<BlobData>(blob_data));

  // Append the elements in the referred blob data.
  for (size_t i = 0; i < blob_data->items().size(); ++i) {
    elements->push_back(net::UploadElement());
    net::UploadElement& element = elements->back();
    const BlobData::Item& item = blob_data->items().at(i);
    switch (item.type) {
      case BlobData::TYPE_DATA:
        element.SetToSharedBytes(
            &item.data.at(0) + static_cast<int>(item.offset),
            static_cast<int>(item.length));
        break;
      case BlobData::TYPE_FILE:
        element.SetToFilePathRange(
            item.file_path,
            item.offset,
            item.length,
            item.expected_modification_time);
        break;
      default:
        NOTREACHED();
        break;
    }
  }
}

}  // namespace webkit_glue
