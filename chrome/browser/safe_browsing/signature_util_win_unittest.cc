// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/signature_util.h"

#include <string>
#include "base/base_paths.h"
#include "base/file_path.h"
#include "base/path_service.h"
#include "base/memory/ref_counted.h"
#include "chrome/common/safe_browsing/csd.pb.h"
#include "net/base/x509_cert_types.h"
#include "net/base/x509_certificate.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace safe_browsing {

TEST(SignatureUtilWinTest, CheckSignature) {
  FilePath source_path;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &source_path));

  FilePath testdata_path = source_path
      .AppendASCII("chrome")
      .AppendASCII("test")
      .AppendASCII("data")
      .AppendASCII("safe_browsing")
      .AppendASCII("download_protection");

  scoped_refptr<SignatureUtil> signature_util(new SignatureUtil());
  ClientDownloadRequest_SignatureInfo signature_info;
  signature_util->CheckSignature(testdata_path.Append(L"signed.exe"),
                                &signature_info);
  EXPECT_FALSE(signature_info.certificate_contents().empty());
  scoped_refptr<net::X509Certificate> cert(
      net::X509Certificate::CreateFromBytes(
          signature_info.certificate_contents().data(),
          signature_info.certificate_contents().size()));
  ASSERT_TRUE(cert.get());
  EXPECT_EQ("Joe's-Software-Emporium", cert->subject().common_name);

  signature_info.Clear();
  signature_util->CheckSignature(testdata_path.Append(L"unsigned.exe"),
                                 &signature_info);
  EXPECT_FALSE(signature_info.has_certificate_contents());

  signature_info.Clear();
  signature_util->CheckSignature(testdata_path.Append(L"doesnotexist.exe"),
                                 &signature_info);
  EXPECT_FALSE(signature_info.has_certificate_contents());
}

}  // namespace safe_browsing
