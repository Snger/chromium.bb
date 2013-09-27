// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webcrypto/webcrypto_impl.h"

namespace content {

void WebCryptoImpl::Init() {
}

bool WebCryptoImpl::EncryptInternal(
    const WebKit::WebCryptoAlgorithm& algorithm,
    const WebKit::WebCryptoKey& key,
    const unsigned char* data,
    unsigned data_size,
    WebKit::WebArrayBuffer* buffer) {
  return false;
}

bool WebCryptoImpl::DigestInternal(
    const WebKit::WebCryptoAlgorithm& algorithm,
    const unsigned char* data,
    unsigned data_size,
    WebKit::WebArrayBuffer* buffer) {
  // TODO(bryaneyler): Placeholder for OpenSSL implementation.
  // Issue http://crbug.com/267888.
  return false;
}

bool WebCryptoImpl::ImportKeyInternal(
    WebKit::WebCryptoKeyFormat format,
    const unsigned char* key_data,
    unsigned key_data_size,
    const WebKit::WebCryptoAlgorithm& algorithm,
    WebKit::WebCryptoKeyUsageMask usage_mask,
    scoped_ptr<WebKit::WebCryptoKeyHandle>* handle,
    WebKit::WebCryptoKeyType* type) {
  // TODO(bryaneyler): Placeholder for OpenSSL implementation.
  // Issue http://crbug.com/267888.
  return false;
}

bool WebCryptoImpl::SignInternal(
    const WebKit::WebCryptoAlgorithm& algorithm,
    const WebKit::WebCryptoKey& key,
    const unsigned char* data,
    unsigned data_size,
    WebKit::WebArrayBuffer* buffer) {
  // TODO(bryaneyler): Placeholder for OpenSSL implementation.
  // Issue http://crbug.com/267888.
  return false;
}

bool WebCryptoImpl::VerifySignatureInternal(
    const WebKit::WebCryptoAlgorithm& algorithm,
    const WebKit::WebCryptoKey& key,
    const unsigned char* signature,
    unsigned signature_size,
    const unsigned char* data,
    unsigned data_size,
    bool* signature_match) {
  // TODO(bryaneyler): Placeholder for OpenSSL implementation.
  // Issue http://crbug.com/267888.
  return false;
}

}  // namespace content
