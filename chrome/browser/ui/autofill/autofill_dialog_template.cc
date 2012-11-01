// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_template.h"

#include "base/basictypes.h"

namespace autofill {

static int row_count = 0;

const DetailInput kBillingInputs[] = {
  { ++row_count, CREDIT_CARD_NUMBER, "Card number" },
  { ++row_count, CREDIT_CARD_EXP_2_DIGIT_YEAR, "Expiration MM/YY" },
  {   row_count, CREDIT_CARD_VERIFICATION_CODE, "CVC" },
  { ++row_count, CREDIT_CARD_NAME, "Cardholder name" },
  { ++row_count, ADDRESS_BILLING_LINE1, "Street address" },
  { ++row_count, ADDRESS_BILLING_LINE2, "Street address (optional)" },
  { ++row_count, ADDRESS_BILLING_CITY, "City" },
  { ++row_count, ADDRESS_BILLING_STATE, "State" },
  {   row_count, ADDRESS_BILLING_ZIP, "ZIP code", 0.5 },
};

const size_t kBillingInputsSize = arraysize(kBillingInputs);

}  // namespace
