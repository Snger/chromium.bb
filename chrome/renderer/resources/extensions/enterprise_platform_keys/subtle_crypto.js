// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var utils = require('utils');
var internalAPI = require('enterprise.platformKeys.internalAPI');
var intersect = require('enterprise.platformKeys.utils').intersect;
var KeyPair = require('enterprise.platformKeys.KeyPair').KeyPair;
var keyModule = require('enterprise.platformKeys.Key');
var getSpki = keyModule.getSpki;
var KeyUsage = keyModule.KeyUsage;

var normalizeAlgorithm =
    requireNative('enterprise_platform_keys_natives').NormalizeAlgorithm;

// This error is thrown by the internal and public API's token functions and
// must be rethrown by this custom binding. Keep this in sync with the C++ part
// of this API.
var errorInvalidToken = "The token is not valid.";

// The following errors are specified in WebCrypto.
// TODO(pneubeck): These should be DOMExceptions.
function CreateNotSupportedError() {
  return new Error('The algorithm is not supported');
}

function CreateInvalidAccessError() {
  return new Error('The requested operation is not valid for the provided key');
}

function CreateDataError() {
  return new Error('Data provided to an operation does not meet requirements');
}

function CreateSyntaxError() {
  return new Error('A required parameter was missing or out-of-range');
}

function CreateOperationError() {
  return new Error('The operation failed for an operation-specific reason');
}

// Catches an |internalErrorInvalidToken|. If so, forwards it to |reject| and
// returns true.
function catchInvalidTokenError(reject) {
  if (chrome.runtime.lastError &&
      chrome.runtime.lastError.message == errorInvalidToken) {
    reject(chrome.runtime.lastError);
    return true;
  }
  return false;
}

/**
 * Implementation of WebCrypto.SubtleCrypto used in enterprise.platformKeys.
 * @param {string} tokenId The id of the backing Token.
 * @constructor
 */
var SubtleCryptoImpl = function(tokenId) {
  this.tokenId = tokenId;
};

SubtleCryptoImpl.prototype.generateKey =
    function(algorithm, extractable, keyUsages) {
  var subtleCrypto = this;
  return new Promise(function(resolve, reject) {
    // TODO(pneubeck): Apply the algorithm normalization of the WebCrypto
    // implementation.

    if (extractable) {
      // Note: This deviates from WebCrypto.SubtleCrypto.
      throw CreateNotSupportedError();
    }
    if (intersect(keyUsages, [KeyUsage.sign, KeyUsage.verify]).length !=
        keyUsages.length) {
      throw CreateDataError();
    }
    var normalizedAlgorithmParameters =
        normalizeAlgorithm(algorithm, 'GenerateKey');
    if (!normalizedAlgorithmParameters) {
      // TODO(pneubeck): It's not clear from the WebCrypto spec which error to
      // throw here.
      throw CreateSyntaxError();
    }

    if (normalizedAlgorithmParameters.name !== 'RSASSA-PKCS1-v1_5') {
      // Note: This deviates from WebCrypto.SubtleCrypto.
      throw CreateNotSupportedError();
    }

    internalAPI.generateKey(subtleCrypto.tokenId,
                            normalizedAlgorithmParameters.modulusLength,
                            function(spki) {
      if (catchInvalidTokenError(reject))
        return;
      if (chrome.runtime.lastError) {
        reject(CreateOperationError());
        return;
      }
      resolve(new KeyPair(spki, algorithm, keyUsages));
    });
  });
};

SubtleCryptoImpl.prototype.sign = function(algorithm, key, dataView) {
  var subtleCrypto = this;
  return new Promise(function(resolve, reject) {
    if (key.type != 'private' || key.usages.indexOf(KeyUsage.sign) == -1)
      throw CreateInvalidAccessError();

    var normalizedAlgorithmParameters =
        normalizeAlgorithm(algorithm, 'Sign');
    if (!normalizedAlgorithmParameters) {
      // TODO(pneubeck): It's not clear from the WebCrypto spec which error to
      // throw here.
      throw CreateSyntaxError();
    }

    // Create an ArrayBuffer that equals the dataView. Note that dataView.buffer
    // might contain more data than dataView.
    var data = dataView.buffer.slice(dataView.byteOffset,
                                     dataView.byteOffset + dataView.byteLength);
    internalAPI.sign(
        subtleCrypto.tokenId, getSpki(key), data, function(signature) {
          if (catchInvalidTokenError(reject))
            return;
          if (chrome.runtime.lastError) {
            reject(CreateOperationError());
            return;
          }
          resolve(signature);
        });
  });
};

SubtleCryptoImpl.prototype.exportKey = function(format, key) {
  return new Promise(function(resolve, reject) {
    if (format == 'pkcs8') {
      // Either key.type is not 'private' or the key is not extractable. In both
      // cases the error is the same.
      throw CreateInvalidAccessError();
    } else if (format == 'spki') {
      if (key.type != 'public')
        throw CreateInvalidAccessError();
      resolve(getSpki(key));
    } else {
      // TODO(pneubeck): It should be possible to export to format 'jwk'.
      throw CreateNotSupportedError();
    }
  });
};

exports.SubtleCrypto =
    utils.expose('SubtleCrypto',
                 SubtleCryptoImpl,
                 {functions:['generateKey', 'sign', 'exportKey']});
