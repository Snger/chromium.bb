// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_dialog_views.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"
#include "chrome/browser/ui/autofill/autofill_dialog_template.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"

namespace autofill {

namespace {

// Returns a label that describes a details section.
views::Label* CreateDetailsSectionLabel(const string16& text) {
  views::Label* label = new views::Label(text);
  label->SetHorizontalAlignment(views::Label::ALIGN_RIGHT);
  label->SetFont(label->font().DeriveFont(0, gfx::Font::BOLD));
  // TODO(estade): this should be made to match the native textfield top
  // inset. It's hard to get at, so for now it's hard-coded.
  label->set_border(views::Border::CreateEmptyBorder(4, 0, 0, 0));
  return label;
}

// Creates a detail section (Shipping, Billing, etc.) with the given label and
// inputs View.
views::View* CreateDetailsSection(const string16& label,
                                  views::View* inputs) {
  views::View* view = new views::View();
  views::GridLayout* layout = new views::GridLayout(view);
  view->SetLayoutManager(layout);

  const int column_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  // TODO(estade): pull out these constants, and figure out better values
  // for them.
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::LEADING,
                        0,
                        views::GridLayout::FIXED,
                        180,
                        0);
  column_set->AddPaddingColumn(0, 15);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::LEADING,
                        0,
                        views::GridLayout::FIXED,
                        300,
                        0);

  // Email section.
  layout->StartRow(0, column_set_id);
  layout->AddView(CreateDetailsSectionLabel(label));
  layout->AddView(inputs);
  return view;
}

}  // namespace

// static
AutofillDialogView* AutofillDialogView::Create(
    AutofillDialogController* controller) {
  return new AutofillDialogViews(controller);
}

AutofillDialogViews::AutofillDialogViews(AutofillDialogController* controller)
    : controller_(controller),
      window_(NULL),
      contents_(NULL) {
  DCHECK(controller);
}

AutofillDialogViews::~AutofillDialogViews() {
  DCHECK(!window_);
}

void AutofillDialogViews::Show() {
  InitChildViews();

  // Ownership of |contents_| is handed off by this call. The ConstrainedWindow
  // will take care of deleting itself after calling DeleteDelegate().
  window_ = new ConstrainedWindowViews(
      controller_->web_contents(), this,
      true, ConstrainedWindowViews::DEFAULT_INSETS);
}

string16 AutofillDialogViews::GetWindowTitle() const {
  return controller_->DialogTitle();
}

void AutofillDialogViews::DeleteDelegate() {
  window_ = NULL;
  // |this| belongs to |controller_|.
  controller_->ViewClosed(AutofillDialogController::AUTOFILL_ACTION_ABORT);
}

views::Widget* AutofillDialogViews::GetWidget() {
  return contents_->GetWidget();
}

const views::Widget* AutofillDialogViews::GetWidget() const {
  return contents_->GetWidget();
}

views::View* AutofillDialogViews::GetContentsView() {
  return contents_;
}

string16 AutofillDialogViews::GetDialogButtonLabel(ui::DialogButton button)
    const {
  return button == ui::DIALOG_BUTTON_OK ?
      controller_->ConfirmButtonText() : controller_->CancelButtonText();
}

bool AutofillDialogViews::IsDialogButtonEnabled(ui::DialogButton button) const {
  return button == ui::DIALOG_BUTTON_OK ?
      controller_->ConfirmButtonEnabled() : true;
}

bool AutofillDialogViews::UseChromeStyle() const {
  return true;
}

bool AutofillDialogViews::Cancel() {
  return true;
}

bool AutofillDialogViews::Accept() {
  NOTREACHED();
  return true;
}

void AutofillDialogViews::ButtonPressed(views::Button* sender,
                                        const ui::Event& event) {
  DCHECK_EQ(sender, use_billing_for_shipping_);
  shipping_section_->SetVisible(!use_billing_for_shipping_->checked());
  GetWidget()->SetSize(GetWidget()->non_client_view()->GetPreferredSize());
}

void AutofillDialogViews::InitChildViews() {
  contents_ = new views::View();
  views::GridLayout* layout = new views::GridLayout(contents_);
  contents_->SetLayoutManager(layout);

  const int single_column_set = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(single_column_set);
  column_set->AddColumn(views::GridLayout::FILL,
                        views::GridLayout::FILL,
                        1,
                        views::GridLayout::USE_PREF,
                        0,
                        0);

  layout->StartRow(0, single_column_set);
  views::Label* intro = new views::Label(controller_->IntroText());
  intro->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->AddView(intro);

  layout->StartRowWithPadding(0, single_column_set,
                              0, views::kUnrelatedControlVerticalSpacing);
  layout->AddView(CreateDetailsContainer());

  // Separator.
  layout->StartRowWithPadding(0, single_column_set,
                              0, views::kUnrelatedControlVerticalSpacing);
  layout->AddView(new views::Separator());

  // Wallet checkbox.
  layout->StartRowWithPadding(0, single_column_set,
                              0, views::kRelatedControlVerticalSpacing);
  layout->AddView(new views::Checkbox(controller_->WalletOptionText()));
}

views::View* AutofillDialogViews::CreateDetailsContainer() {
  views::View* view = new views::View();
  // A box layout is used because it respects widget visibility.
  view->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0,
                           views::kRelatedControlVerticalSpacing));

  // Email.
  view->AddChildView(CreateDetailsSection(
      controller_->EmailSectionLabel(), CreateEmailInputs()));
  // Billing.
  view->AddChildView(CreateDetailsSection(
      controller_->BillingSectionLabel(), CreateBillingInputs()));
  // Shipping.
  shipping_section_ = CreateDetailsSection(
      controller_->ShippingSectionLabel(), CreateShippingInputs());
  view->AddChildView(shipping_section_);
  shipping_section_->SetVisible(!use_billing_for_shipping_->checked());

  return view;
}

views::View* AutofillDialogViews::CreateEmailInputs() {
  views::Textfield* field = new views::Textfield();
  field->set_placeholder_text(ASCIIToUTF16("placeholder text"));
  return field;
}

views::View* AutofillDialogViews::CreateBillingInputs() {
  views::View* billing = new views::View();
  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0,
                           views::kRelatedControlVerticalSpacing);
  billing->SetLayoutManager(layout);

  billing->AddChildView(
      InitInputsFromTemplate(kBillingInputs, kBillingInputsSize));

  use_billing_for_shipping_ =
      new views::Checkbox(controller_->UseBillingForShippingText());
  use_billing_for_shipping_->SetChecked(true);
  use_billing_for_shipping_->set_listener(this);
  billing->AddChildView(use_billing_for_shipping_);

  return billing;
}

views::View* AutofillDialogViews::CreateShippingInputs() {
  return InitInputsFromTemplate(kShippingInputs, kShippingInputsSize);
}

// TODO(estade): we should be using Chrome-style constrained window padding
// values.
views::View* AutofillDialogViews::InitInputsFromTemplate(
    const DetailInput* inputs,
    size_t inputs_len) {
  views::View* view = new views::View();
  views::GridLayout* layout = new views::GridLayout(view);
  view->SetLayoutManager(layout);

  for (size_t i = 0; i < inputs_len; ++i) {
    const DetailInput& input = inputs[i];
    if (!controller_->ShouldShowInput(input))
      continue;

    int column_set_id = input.row_id;
    views::ColumnSet* column_set = layout->GetColumnSet(column_set_id);
    if (!column_set) {
      // Create a new column set and row.
      column_set = layout->AddColumnSet(column_set_id);
      if (i > 0)
        layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
      layout->StartRow(0, column_set_id);
    } else {
      // Add a new column to existing row.
      column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
      // Must explicitly skip the padding column since we've already started
      // adding views.
      layout->SkipColumns(1);
    }

    float expand = input.expand_weight;
    column_set->AddColumn(views::GridLayout::FILL,
                          views::GridLayout::BASELINE,
                          expand ? expand : 1,
                          views::GridLayout::USE_PREF,
                          0,
                          0);

    views::Textfield* field = new views::Textfield();
    field->set_placeholder_text(ASCIIToUTF16(input.placeholder_text));
    layout->AddView(field);
  }

  return view;
}

}  // namespace autofill
