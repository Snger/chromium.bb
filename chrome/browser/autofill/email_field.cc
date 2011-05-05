// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/email_field.h"

#include "chrome/browser/autofill/autofill_scanner.h"
#include "grit/autofill_resources.h"
#include "ui/base/l10n/l10n_util.h"

// static
EmailField* EmailField::Parse(AutofillScanner* scanner, bool is_ecml) {
  string16 pattern;
  if (is_ecml)
    pattern = GetEcmlPattern(kEcmlShipToEmail, kEcmlBillToEmail, '|');
  else
    pattern = l10n_util::GetStringUTF16(IDS_AUTOFILL_EMAIL_RE);

  const AutofillField* field;
  if (ParseText(scanner, pattern, &field))
    return new EmailField(field);

  return NULL;
}

bool EmailField::GetFieldInfo(FieldTypeMap* field_type_map) const {
  return Add(field_type_map, field_, EMAIL_ADDRESS);
}

EmailField::EmailField(const AutofillField* field) : field_(field) {
}
