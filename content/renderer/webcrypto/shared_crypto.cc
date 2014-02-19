// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/webcrypto/shared_crypto.h"

#include "base/logging.h"
#include "content/renderer/webcrypto/crypto_data.h"
#include "content/renderer/webcrypto/platform_crypto.h"
#include "content/renderer/webcrypto/webcrypto_util.h"
#include "crypto/secure_util.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithm.h"
#include "third_party/WebKit/public/platform/WebCryptoAlgorithmParams.h"
#include "third_party/WebKit/public/platform/WebCryptoKey.h"

namespace content {

namespace webcrypto {

namespace {

// TODO(eroman): Move this helper to WebCryptoKey.
bool KeyUsageAllows(const blink::WebCryptoKey& key,
                    const blink::WebCryptoKeyUsage usage) {
  return ((key.usages() & usage) != 0);
}

bool IsValidAesKeyLengthBits(unsigned int length_bits) {
  return length_bits == 128 || length_bits == 192 || length_bits == 256;
}

bool IsValidAesKeyLengthBytes(unsigned int length_bytes) {
  return length_bytes == 16 || length_bytes == 24 || length_bytes == 32;
}

Status ToPlatformSymKey(const blink::WebCryptoKey& key,
                        platform::SymKey** out) {
  *out = static_cast<platform::Key*>(key.handle())->AsSymKey();
  if (!*out)
    return Status::ErrorUnexpectedKeyType();
  return Status::Success();
}

Status ToPlatformPublicKey(const blink::WebCryptoKey& key,
                           platform::PublicKey** out) {
  *out = static_cast<platform::Key*>(key.handle())->AsPublicKey();
  if (!*out)
    return Status::ErrorUnexpectedKeyType();
  return Status::Success();
}

Status ToPlatformPrivateKey(const blink::WebCryptoKey& key,
                            platform::PrivateKey** out) {
  *out = static_cast<platform::Key*>(key.handle())->AsPrivateKey();
  if (!*out)
    return Status::ErrorUnexpectedKeyType();
  return Status::Success();
}

const size_t kAesBlockSizeBytes = 16;

Status EncryptDecryptAesCbc(EncryptOrDecrypt mode,
                            const blink::WebCryptoAlgorithm& algorithm,
                            const blink::WebCryptoKey& key,
                            const CryptoData& data,
                            blink::WebArrayBuffer* buffer) {
  platform::SymKey* sym_key;
  Status status = ToPlatformSymKey(key, &sym_key);
  if (status.IsError())
    return status;

  const blink::WebCryptoAesCbcParams* params = algorithm.aesCbcParams();
  if (!params)
    return Status::ErrorUnexpected();

  CryptoData iv(params->iv().data(), params->iv().size());
  if (iv.byte_length() != kAesBlockSizeBytes)
    return Status::ErrorIncorrectSizeAesCbcIv();

  return platform::EncryptDecryptAesCbc(mode, sym_key, data, iv, buffer);
}

Status EncryptDecryptAesGcm(EncryptOrDecrypt mode,
                            const blink::WebCryptoAlgorithm& algorithm,
                            const blink::WebCryptoKey& key,
                            const CryptoData& data,
                            blink::WebArrayBuffer* buffer) {
  platform::SymKey* sym_key;
  Status status = ToPlatformSymKey(key, &sym_key);
  if (status.IsError())
    return status;

  const blink::WebCryptoAesGcmParams* params = algorithm.aesGcmParams();
  if (!params)
    return Status::ErrorUnexpected();

  // TODO(eroman): The spec doesn't define the default value. Assume 128 for now
  // since that is the maximum tag length:
  // http://www.w3.org/2012/webcrypto/track/issues/46
  unsigned int tag_length_bits = 128;
  if (params->hasTagLengthBits())
    tag_length_bits = params->optionalTagLengthBits();
  if (tag_length_bits > 128)
    return Status::ErrorInvalidAesGcmTagLength();

  return platform::EncryptDecryptAesGcm(
      mode,
      sym_key,
      data,
      CryptoData(params->iv()),
      CryptoData(params->optionalAdditionalData()),
      tag_length_bits,
      buffer);
}

Status EncryptRsaEsPkcs1v1_5(const blink::WebCryptoAlgorithm& algorithm,
                             const blink::WebCryptoKey& key,
                             const CryptoData& data,
                             blink::WebArrayBuffer* buffer) {
  platform::PublicKey* public_key;
  Status status = ToPlatformPublicKey(key, &public_key);
  if (status.IsError())
    return status;

  // RSAES encryption does not support empty input
  if (!data.byte_length())
    return Status::Error();

  return platform::EncryptRsaEsPkcs1v1_5(public_key, data, buffer);
}

Status DecryptRsaEsPkcs1v1_5(const blink::WebCryptoAlgorithm& algorithm,
                             const blink::WebCryptoKey& key,
                             const CryptoData& data,
                             blink::WebArrayBuffer* buffer) {
  platform::PrivateKey* private_key;
  Status status = ToPlatformPrivateKey(key, &private_key);
  if (status.IsError())
    return status;

  // RSAES decryption does not support empty input
  if (!data.byte_length())
    return Status::Error();

  return platform::DecryptRsaEsPkcs1v1_5(private_key, data, buffer);
}

Status SignHmac(const blink::WebCryptoAlgorithm& algorithm,
                const blink::WebCryptoKey& key,
                const CryptoData& data,
                blink::WebArrayBuffer* buffer) {
  platform::SymKey* sym_key;
  Status status = ToPlatformSymKey(key, &sym_key);
  if (status.IsError())
    return status;

  const blink::WebCryptoHmacParams* params = algorithm.hmacParams();
  if (!params)
    return Status::ErrorUnexpected();

  if (!IsHashAlgorithm(params->hash().id()))
    return Status::ErrorUnexpected();

  if (params->hash().id() != GetInnerHashAlgorithm(key.algorithm()).id())
    return Status::ErrorUnexpected();

  return platform::SignHmac(sym_key, params->hash(), data, buffer);
}

Status VerifyHmac(const blink::WebCryptoAlgorithm& algorithm,
                  const blink::WebCryptoKey& key,
                  const CryptoData& signature,
                  const CryptoData& data,
                  bool* signature_match) {
  blink::WebArrayBuffer result;
  Status status = SignHmac(algorithm, key, data, &result);
  if (status.IsError())
    return status;

  // Do not allow verification of truncated MACs.
  *signature_match =
      result.byteLength() == signature.byte_length() &&
      crypto::SecureMemEqual(
          result.data(), signature.bytes(), signature.byte_length());

  return Status::Success();
}

Status SignRsaSsaPkcs1v1_5(const blink::WebCryptoAlgorithm& algorithm,
                           const blink::WebCryptoKey& key,
                           const CryptoData& data,
                           blink::WebArrayBuffer* buffer) {
  platform::PrivateKey* private_key;
  Status status = ToPlatformPrivateKey(key, &private_key);
  if (status.IsError())
    return status;

  const blink::WebCryptoRsaSsaParams* params = algorithm.rsaSsaParams();
  if (!params)
    return Status::ErrorUnexpected();

  if (!IsHashAlgorithm(params->hash().id()))
    return Status::ErrorUnexpected();

  // TODO(eroman): Verify the key has not been used with any other hash.

  return platform::SignRsaSsaPkcs1v1_5(
      private_key, params->hash(), data, buffer);
}

Status VerifyRsaSsaPkcs1v1_5(const blink::WebCryptoAlgorithm& algorithm,
                             const blink::WebCryptoKey& key,
                             const CryptoData& signature,
                             const CryptoData& data,
                             bool* signature_match) {
  platform::PublicKey* public_key;
  Status status = ToPlatformPublicKey(key, &public_key);
  if (status.IsError())
    return status;

  const blink::WebCryptoRsaSsaParams* params = algorithm.rsaSsaParams();
  if (!params)
    return Status::ErrorUnexpected();

  if (!IsHashAlgorithm(params->hash().id()))
    return Status::ErrorUnexpected();

  // TODO(eroman): Verify the key has not been used with any other hash.

  return platform::VerifyRsaSsaPkcs1v1_5(
      public_key, params->hash(), signature, data, signature_match);
}

Status ImportKeyRaw(const CryptoData& key_data,
                    const blink::WebCryptoAlgorithm& algorithm_or_null,
                    bool extractable,
                    blink::WebCryptoKeyUsageMask usage_mask,
                    blink::WebCryptoKey* key) {
  if (algorithm_or_null.isNull())
    return Status::ErrorMissingAlgorithmImportRawKey();

  switch (algorithm_or_null.id()) {
    case blink::WebCryptoAlgorithmIdAesCbc:
    case blink::WebCryptoAlgorithmIdAesGcm:
    case blink::WebCryptoAlgorithmIdAesKw:
      if (!IsValidAesKeyLengthBytes(key_data.byte_length()))
        return Status::Error();
    // Fallthrough intentional!
    case blink::WebCryptoAlgorithmIdHmac:
      return platform::ImportKeyRaw(
          algorithm_or_null, key_data, extractable, usage_mask, key);

    default:
      return Status::ErrorUnsupported();
  }
}

}  // namespace

void Init() { platform::Init(); }

Status Encrypt(const blink::WebCryptoAlgorithm& algorithm,
               const blink::WebCryptoKey& key,
               const CryptoData& data,
               blink::WebArrayBuffer* buffer) {
  if (!KeyUsageAllows(key, blink::WebCryptoKeyUsageEncrypt))
    return Status::ErrorUnexpected();
  if (algorithm.id() != key.algorithm().id())
    return Status::ErrorUnexpected();

  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdAesCbc:
      return EncryptDecryptAesCbc(ENCRYPT, algorithm, key, data, buffer);
    case blink::WebCryptoAlgorithmIdAesGcm:
      return EncryptDecryptAesGcm(ENCRYPT, algorithm, key, data, buffer);
    case blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5:
      return EncryptRsaEsPkcs1v1_5(algorithm, key, data, buffer);
    default:
      return Status::ErrorUnsupported();
  }
}

Status Decrypt(const blink::WebCryptoAlgorithm& algorithm,
               const blink::WebCryptoKey& key,
               const CryptoData& data,
               blink::WebArrayBuffer* buffer) {
  if (!KeyUsageAllows(key, blink::WebCryptoKeyUsageDecrypt))
    return Status::ErrorUnexpected();
  if (algorithm.id() != key.algorithm().id())
    return Status::ErrorUnexpected();

  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdAesCbc:
      return EncryptDecryptAesCbc(DECRYPT, algorithm, key, data, buffer);
    case blink::WebCryptoAlgorithmIdAesGcm:
      return EncryptDecryptAesGcm(DECRYPT, algorithm, key, data, buffer);
    case blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5:
      return DecryptRsaEsPkcs1v1_5(algorithm, key, data, buffer);
    default:
      return Status::ErrorUnsupported();
  }
}

Status Digest(const blink::WebCryptoAlgorithm& algorithm,
              const CryptoData& data,
              blink::WebArrayBuffer* buffer) {
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdSha1:
    case blink::WebCryptoAlgorithmIdSha224:
    case blink::WebCryptoAlgorithmIdSha256:
    case blink::WebCryptoAlgorithmIdSha384:
    case blink::WebCryptoAlgorithmIdSha512:
      return platform::DigestSha(algorithm.id(), data, buffer);
    default:
      return Status::ErrorUnsupported();
  }
}

Status GenerateSecretKey(const blink::WebCryptoAlgorithm& algorithm,
                         bool extractable,
                         blink::WebCryptoKeyUsageMask usage_mask,
                         blink::WebCryptoKey* key) {
  unsigned int keylen_bytes = 0;

  // Get the secret key length in bytes from generation parameters.
  // This resolves any defaults.
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdAesCbc:
    case blink::WebCryptoAlgorithmIdAesGcm:
    case blink::WebCryptoAlgorithmIdAesKw: {
      if (!IsValidAesKeyLengthBits(algorithm.aesKeyGenParams()->lengthBits()))
        return Status::ErrorGenerateKeyLength();
      keylen_bytes = algorithm.aesKeyGenParams()->lengthBits() / 8;
      break;
    }
    case blink::WebCryptoAlgorithmIdHmac: {
      const blink::WebCryptoHmacKeyParams* params = algorithm.hmacKeyParams();
      DCHECK(params);
      if (params->hasLengthBytes()) {
        keylen_bytes = params->optionalLengthBytes();
      } else {
        keylen_bytes = ShaBlockSizeBytes(params->hash().id());
        if (keylen_bytes == 0)
          return Status::ErrorUnsupported();
      }
      break;
    }

    default:
      return Status::ErrorUnsupported();
  }

  // TODO(eroman): Is this correct? HMAC can import zero-length keys, so should
  //               probably be able to allowed to generate them too.
  if (keylen_bytes == 0)
    return Status::ErrorGenerateKeyLength();

  return platform::GenerateSecretKey(
      algorithm, extractable, usage_mask, keylen_bytes, key);
}

Status GenerateKeyPair(const blink::WebCryptoAlgorithm& algorithm,
                       bool extractable,
                       blink::WebCryptoKeyUsageMask usage_mask,
                       blink::WebCryptoKey* public_key,
                       blink::WebCryptoKey* private_key) {
  // TODO(padolph): Handle other asymmetric algorithm key generation.
  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdRsaEsPkcs1v1_5:
    case blink::WebCryptoAlgorithmIdRsaOaep:
    case blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
      if (!algorithm.rsaKeyGenParams())
        return Status::ErrorUnexpected();
      return platform::GenerateRsaKeyPair(
          algorithm, extractable, usage_mask, public_key, private_key);
    default:
      return Status::ErrorUnsupported();
  }
}

Status ImportKey(blink::WebCryptoKeyFormat format,
                 const CryptoData& key_data,
                 const blink::WebCryptoAlgorithm& algorithm_or_null,
                 bool extractable,
                 blink::WebCryptoKeyUsageMask usage_mask,
                 blink::WebCryptoKey* key) {
  switch (format) {
    case blink::WebCryptoKeyFormatRaw:
      return ImportKeyRaw(
          key_data, algorithm_or_null, extractable, usage_mask, key);
      return platform::ImportKeyRaw(
          algorithm_or_null, key_data, extractable, usage_mask, key);
    case blink::WebCryptoKeyFormatSpki:
      return platform::ImportKeySpki(
          algorithm_or_null, key_data, extractable, usage_mask, key);
    case blink::WebCryptoKeyFormatPkcs8:
      return platform::ImportKeyPkcs8(
          algorithm_or_null, key_data, extractable, usage_mask, key);
    case blink::WebCryptoKeyFormatJwk:
      return ImportKeyJwk(
          key_data, algorithm_or_null, extractable, usage_mask, key);
    default:
      return Status::ErrorUnsupported();
  }
}

Status ExportKey(blink::WebCryptoKeyFormat format,
                 const blink::WebCryptoKey& key,
                 blink::WebArrayBuffer* buffer) {
  if (!key.extractable())
    return Status::ErrorKeyNotExtractable();

  switch (format) {
    case blink::WebCryptoKeyFormatRaw: {
      platform::SymKey* sym_key;
      Status status = ToPlatformSymKey(key, &sym_key);
      if (status.IsError())
        return status;
      return platform::ExportKeyRaw(sym_key, buffer);
    }
    case blink::WebCryptoKeyFormatSpki: {
      platform::PublicKey* public_key;
      Status status = ToPlatformPublicKey(key, &public_key);
      if (status.IsError())
        return status;
      return platform::ExportKeySpki(public_key, buffer);
    }
    case blink::WebCryptoKeyFormatPkcs8:
    case blink::WebCryptoKeyFormatJwk:
      // TODO(eroman):
      return Status::ErrorUnsupported();
    default:
      return Status::ErrorUnsupported();
  }
}

Status Sign(const blink::WebCryptoAlgorithm& algorithm,
            const blink::WebCryptoKey& key,
            const CryptoData& data,
            blink::WebArrayBuffer* buffer) {
  if (!KeyUsageAllows(key, blink::WebCryptoKeyUsageSign))
    return Status::ErrorUnexpected();
  if (algorithm.id() != key.algorithm().id())
    return Status::ErrorUnexpected();

  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdHmac:
      return SignHmac(algorithm, key, data, buffer);
    case blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
      return SignRsaSsaPkcs1v1_5(algorithm, key, data, buffer);
    default:
      return Status::ErrorUnsupported();
  }
}

Status VerifySignature(const blink::WebCryptoAlgorithm& algorithm,
                       const blink::WebCryptoKey& key,
                       const CryptoData& signature,
                       const CryptoData& data,
                       bool* signature_match) {
  if (!KeyUsageAllows(key, blink::WebCryptoKeyUsageVerify))
    return Status::ErrorUnexpected();
  if (algorithm.id() != key.algorithm().id())
    return Status::ErrorUnexpected();

  if (!signature.byte_length()) {
    // None of the algorithms generate valid zero-length signatures so this
    // will necessarily fail verification. Early return to protect
    // implementations from dealing with a NULL signature pointer.
    *signature_match = false;
    return Status::Success();
  }

  switch (algorithm.id()) {
    case blink::WebCryptoAlgorithmIdHmac:
      return VerifyHmac(algorithm, key, signature, data, signature_match);
    case blink::WebCryptoAlgorithmIdRsaSsaPkcs1v1_5:
      return VerifyRsaSsaPkcs1v1_5(
          algorithm, key, signature, data, signature_match);
    default:
      return Status::ErrorUnsupported();
  }
}

}  // namespace webcrypto

}  // namespace content
