// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/address.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace autofill {

class AddressTest : public testing::Test {
 public:
  // In order to access the application locale -- which the tested functions do
  // internally -- this test must run on the UI thread.
  AddressTest() : ui_thread_(BrowserThread::UI, &message_loop_) {}

 private:
  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(AddressTest);
};

// Test that country data can be properly returned as either a country code or a
// localized country name.
TEST_F(AddressTest, GetCountry) {
  Address address;
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_COUNTRY));

  // Make sure that nothing breaks when the country code is missing.
  base::string16 country =
      address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(base::string16(), country);

  address.SetInfo(
      AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("US"), "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(ASCIIToUTF16("United States"), country);
  country = address.GetInfo(
      AutofillType(HTML_TYPE_COUNTRY_NAME, HTML_MODE_NONE), "en-US");
  EXPECT_EQ(ASCIIToUTF16("United States"), country);
  country = address.GetInfo(
      AutofillType(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE), "en-US");
  EXPECT_EQ(ASCIIToUTF16("US"), country);

  address.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("CA"));
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(ASCIIToUTF16("Canada"), country);
  country = address.GetInfo(
      AutofillType(HTML_TYPE_COUNTRY_NAME, HTML_MODE_NONE), "en-US");
  EXPECT_EQ(ASCIIToUTF16("Canada"), country);
  country = address.GetInfo(
      AutofillType(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE), "en-US");
  EXPECT_EQ(ASCIIToUTF16("CA"), country);
}

// Test that we properly detect country codes appropriate for each country.
TEST_F(AddressTest, SetCountry) {
  Address address;
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_COUNTRY));

  // Test basic conversion.
  address.SetInfo(
      AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("United States"),
      "en-US");
  base::string16 country =
      address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(ASCIIToUTF16("US"), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("United States"), country);

  // Test basic synonym detection.
  address.SetInfo(
      AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("USA"), "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(ASCIIToUTF16("US"), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("United States"), country);

  // Test case-insensitivity.
  address.SetInfo(
      AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("canADA"), "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(ASCIIToUTF16("CA"), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("Canada"), country);

  // Test country code detection.
  address.SetInfo(
      AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("JP"), "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(ASCIIToUTF16("JP"), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("Japan"), country);

  // Test that we ignore unknown countries.
  address.SetInfo(
      AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("Unknown"), "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(base::string16(), country);

  // Test setting the country based on an HTML field type.
  AutofillType html_type_country_code =
      AutofillType(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE);
  address.SetInfo(html_type_country_code, ASCIIToUTF16("US"), "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(ASCIIToUTF16("US"), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("United States"), country);

  // Test case-insensitivity when setting the country based on an HTML field
  // type.
  address.SetInfo(html_type_country_code, ASCIIToUTF16("cA"), "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(ASCIIToUTF16("CA"), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("Canada"), country);

  // Test setting the country based on invalid data with an HTML field type.
  address.SetInfo(html_type_country_code, ASCIIToUTF16("unknown"), "en-US");
  country = address.GetInfo(AutofillType(ADDRESS_HOME_COUNTRY), "en-US");
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  EXPECT_EQ(base::string16(), country);
}

// Test that we properly match typed values to stored country data.
TEST_F(AddressTest, IsCountry) {
  Address address;
  address.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));

  const char* const kValidMatches[] = {
    "United States",
    "USA",
    "US",
    "United states",
    "us"
  };
  for (size_t i = 0; i < arraysize(kValidMatches); ++i) {
    SCOPED_TRACE(kValidMatches[i]);
    ServerFieldTypeSet matching_types;
    address.GetMatchingTypes(ASCIIToUTF16(kValidMatches[i]), "US",
                             &matching_types);
    ASSERT_EQ(1U, matching_types.size());
    EXPECT_EQ(ADDRESS_HOME_COUNTRY, *matching_types.begin());
  }

  const char* const kInvalidMatches[] = {
    "United",
    "Garbage"
  };
  for (size_t i = 0; i < arraysize(kInvalidMatches); ++i) {
    ServerFieldTypeSet matching_types;
    address.GetMatchingTypes(ASCIIToUTF16(kInvalidMatches[i]), "US",
                             &matching_types);
    EXPECT_EQ(0U, matching_types.size());
  }

  // Make sure that garbage values don't match when the country code is empty.
  address.SetRawInfo(ADDRESS_HOME_COUNTRY, base::string16());
  EXPECT_EQ(base::string16(), address.GetRawInfo(ADDRESS_HOME_COUNTRY));
  ServerFieldTypeSet matching_types;
  address.GetMatchingTypes(ASCIIToUTF16("Garbage"), "US", &matching_types);
  EXPECT_EQ(0U, matching_types.size());
}

}  // namespace autofill
