// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/manage_passwords_bubble_view.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/passwords/manage_password_item_view.h"
#include "chrome/browser/ui/views/passwords/manage_passwords_icon_view.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents_view.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/controls/button/blue_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"


// Helpers --------------------------------------------------------------------

namespace {

// Metrics: "PasswordBubble.DisplayDisposition"
enum BubbleDisplayDisposition {
  AUTOMATIC_WITH_PASSWORD_PENDING = 0,
  MANUAL_WITH_PASSWORD_PENDING,
  MANUAL_MANAGE_PASSWORDS,
  NUM_DISPLAY_DISPOSITIONS
};

// Upper limit on the length of fields displayed in the manage passwords bubble.
const int kMaxDisplayableStringWidth = 22;

// Returns the width of |password_form|'s |type| field, clamped to the maximum
// displayable string width.
int GetFieldWidth(const autofill::PasswordForm& password_form,
                  ManagePasswordsBubbleView::FieldType type) {
  const gfx::FontList font_list;
  base::string16 display_string(
      type == ManagePasswordsBubbleView::USERNAME_FIELD
          ? password_form.username_value
          : ManagePasswordItemView::GetPasswordDisplayString(
                password_form.password_value));
  return std::min(
      gfx::FontList().GetExpectedTextWidth(kMaxDisplayableStringWidth),
      gfx::GetStringWidth(display_string, font_list));
}

class SavePasswordRefusalComboboxModel : public ui::ComboboxModel {
 public:
  enum { INDEX_NOPE = 0, INDEX_NEVER_FOR_THIS_SITE = 1, };

  SavePasswordRefusalComboboxModel() {
    items_.push_back(
        l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CANCEL_BUTTON));
    items_.push_back(
        l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_BLACKLIST_BUTTON));
  }
  virtual ~SavePasswordRefusalComboboxModel() {}

 private:
  // Overridden from ui::ComboboxModel:
  virtual int GetItemCount() const OVERRIDE { return items_.size(); }
  virtual base::string16 GetItemAt(int index) OVERRIDE { return items_[index]; }
  virtual bool IsItemSeparatorAt(int index) OVERRIDE {
    return items_[index].empty();
  }
  virtual int GetDefaultIndex() const OVERRIDE { return 0; }

  std::vector<base::string16> items_;

  DISALLOW_COPY_AND_ASSIGN(SavePasswordRefusalComboboxModel);
};

}  // namespace


// ManagePasswordsBubbleView --------------------------------------------------

// static
ManagePasswordsBubbleView* ManagePasswordsBubbleView::manage_passwords_bubble_ =
    NULL;

// static
void ManagePasswordsBubbleView::ShowBubble(content::WebContents* web_contents,
                                           ManagePasswordsIconView* icon_view,
                                           BubbleDisplayReason reason) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  DCHECK(browser);
  DCHECK(browser->window());
  DCHECK(browser->fullscreen_controller());
  DCHECK(!IsShowing());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  bool is_fullscreen = browser_view->IsFullscreen();
  views::View* anchor_view = is_fullscreen ?
      NULL : browser_view->GetLocationBarView()->manage_passwords_icon_view();
  manage_passwords_bubble_ = new ManagePasswordsBubbleView(
      web_contents, anchor_view, icon_view, reason);

  if (is_fullscreen) {
    manage_passwords_bubble_->set_parent_window(
        web_contents->GetView()->GetTopLevelNativeWindow());
  }

  views::BubbleDelegateView::CreateBubble(manage_passwords_bubble_);

  // Adjust for fullscreen after creation as it relies on the content size.
  if (is_fullscreen) {
    manage_passwords_bubble_->AdjustForFullscreen(
        browser_view->GetBoundsInScreen());
  }

  manage_passwords_bubble_->GetWidget()->Show();
}

// static
void ManagePasswordsBubbleView::CloseBubble(BubbleDismissalReason reason) {
  if (manage_passwords_bubble_)
    manage_passwords_bubble_->Close(reason);
}

// static
bool ManagePasswordsBubbleView::IsShowing() {
  // The bubble may be in the process of closing.
  return (manage_passwords_bubble_ != NULL) &&
      manage_passwords_bubble_->GetWidget()->IsVisible();
}

ManagePasswordsBubbleView::ManagePasswordsBubbleView(
    content::WebContents* web_contents,
    views::View* anchor_view,
    ManagePasswordsIconView* icon_view,
    BubbleDisplayReason reason)
    : BubbleDelegateView(anchor_view,
                         anchor_view ? views::BubbleBorder::TOP_RIGHT
                                     : views::BubbleBorder::NONE),
      manage_passwords_bubble_model_(
          new ManagePasswordsBubbleModel(web_contents)),
      icon_view_(icon_view) {
  // Compensate for built-in vertical padding in the anchor view's image.
  set_anchor_view_insets(gfx::Insets(5, 0, 5, 0));
  set_notify_enter_exit_on_child(true);

  BubbleDisplayDisposition disposition = AUTOMATIC_WITH_PASSWORD_PENDING;
  if (reason == USER_ACTION) {
    // TODO(mkwst): Deal with "Never save passwords" once we've decided how that
    // flow should work.
    disposition = manage_passwords_bubble_model_->WaitingToSavePassword()
                      ? MANUAL_WITH_PASSWORD_PENDING
                      : MANUAL_MANAGE_PASSWORDS;
  } else {
    DCHECK(manage_passwords_bubble_model_->WaitingToSavePassword());
  }

  UMA_HISTOGRAM_ENUMERATION("PasswordBubble.DisplayDisposition",
                            disposition,
                            NUM_DISPLAY_DISPOSITIONS);
}

ManagePasswordsBubbleView::~ManagePasswordsBubbleView() {
  if (dismissal_reason_ == NOT_DISPLAYED)
    return;

  UMA_HISTOGRAM_ENUMERATION("PasswordBubble.DismissalReason",
                            dismissal_reason_,
                            NUM_DISMISSAL_REASONS);
}

void ManagePasswordsBubbleView::BuildColumnSet(views::GridLayout* layout,
                                               ColumnSetType type) {
  views::ColumnSet* column_set = layout->AddColumnSet(type);
  column_set->AddPaddingColumn(0, views::kPanelHorizMargin);
  switch (type) {
    case SINGLE_VIEW_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::FILL,
                            views::GridLayout::FILL,
                            0,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      break;

    case DOUBLE_VIEW_COLUMN_SET:
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            1,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      column_set->AddPaddingColumn(0, views::kRelatedButtonHSpacing);
      column_set->AddColumn(views::GridLayout::TRAILING,
                            views::GridLayout::CENTER,
                            0,
                            views::GridLayout::USE_PREF,
                            0,
                            0);
      break;
  }
  column_set->AddPaddingColumn(0, views::kPanelHorizMargin);
}

int ManagePasswordsBubbleView::GetMaximumFieldWidth(FieldType type) {
  int maximum = 0;
  if (manage_passwords_bubble_model_->manage_passwords_bubble_state() !=
      ManagePasswordsBubbleModel::PASSWORD_TO_BE_SAVED) {
    // If we are in the PASSWORD_TO_BE_SAVED state we only display the
    // password that was just submitted and should not take these into account.
    for (autofill::PasswordFormMap::const_iterator i(
             manage_passwords_bubble_model_->best_matches().begin());
         i != manage_passwords_bubble_model_->best_matches().end(); ++i) {
      maximum = std::max(maximum, GetFieldWidth((*i->second), type));
    }
  }
  if (manage_passwords_bubble_model_->password_submitted()) {
    maximum = std::max(
        GetFieldWidth(manage_passwords_bubble_model_->pending_credentials(),
                      type),
        maximum);
  }
  return maximum;
}

void ManagePasswordsBubbleView::AdjustForFullscreen(
    const gfx::Rect& screen_bounds) {
  if (GetAnchorView())
    return;

  // The bubble's padding from the screen edge, used in fullscreen.
  const int kFullscreenPaddingEnd = 20;
  const size_t bubble_half_width = width() / 2;
  const int x_pos = base::i18n::IsRTL() ?
      screen_bounds.x() + bubble_half_width + kFullscreenPaddingEnd :
      screen_bounds.right() - bubble_half_width - kFullscreenPaddingEnd;
  SetAnchorRect(gfx::Rect(x_pos, screen_bounds.y(), 0, 0));
}

void ManagePasswordsBubbleView::Close(BubbleDismissalReason reason) {
  dismissal_reason_ = reason;
  icon_view_->SetTooltip(
      manage_passwords_bubble_model_->manage_passwords_bubble_state() ==
      ManagePasswordsBubbleModel::PASSWORD_TO_BE_SAVED);
  GetWidget()->Close();
}

void ManagePasswordsBubbleView::Init() {
  using views::GridLayout;

  // Default to a dismissal reason of "lost focus". If the user interacts with
  // the button in such a way that it closes, we'll reset this value
  // accordingly.
  dismissal_reason_ = BUBBLE_LOST_FOCUS;

  GridLayout* layout = new GridLayout(this);
  SetFocusable(true);
  SetLayoutManager(layout);
  BuildColumnSet(layout, SINGLE_VIEW_COLUMN_SET);
  BuildColumnSet(layout, DOUBLE_VIEW_COLUMN_SET);

  // This calculates the necessary widths for credential columns in the bubble.
  const int first_field_width = std::max(
      GetMaximumFieldWidth(USERNAME_FIELD),
      views::Label(l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_DELETED))
          .GetPreferredSize()
          .width());

  const int second_field_width = std::max(
      GetMaximumFieldWidth(PASSWORD_FIELD),
      views::Label(l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_UNDO))
          .GetPreferredSize()
          .width());

  // Build and populate the header.
  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  views::Label* title_label =
      new views::Label(manage_passwords_bubble_model_->title());
  title_label->SetMultiLine(true);
  title_label->SetFontList(rb->GetFontList(ui::ResourceBundle::MediumFont));

  layout->StartRowWithPadding(
      0, SINGLE_VIEW_COLUMN_SET, 0, views::kRelatedControlSmallVerticalSpacing);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  if (manage_passwords_bubble_model_->WaitingToSavePassword()) {
    // If we've got a password that we're deciding whether or not to save,
    // then we need to display a single-view columnset containing the
    // ManagePasswordItemView, followed by double-view columnset containing
    // a "Save" and "Reject" button.
    ManagePasswordItemView* item = new ManagePasswordItemView(
        manage_passwords_bubble_model_,
        manage_passwords_bubble_model_->pending_credentials(),
        first_field_width,
        second_field_width,
        ManagePasswordItemView::FIRST_ITEM);
    layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
    layout->AddView(item);

    refuse_combobox_ =
        new views::Combobox(new SavePasswordRefusalComboboxModel());
    refuse_combobox_->set_listener(this);
    refuse_combobox_->SetStyle(views::Combobox::STYLE_ACTION);

    save_button_ = new views::BlueButton(
        this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_BUTTON));

    layout->StartRowWithPadding(
        0, DOUBLE_VIEW_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);
    layout->AddView(save_button_);
    layout->AddView(refuse_combobox_);
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  } else {
    // If we have a list of passwords to store for the current site, display
    // them to the user for management. Otherwise, render a "No passwords for
    // this site" message.
    //
    // TODO(mkwst): Do we really want the "No passwords" case? It would probably
    // be better to only clear the pending password upon navigation, rather than
    // as soon as the bubble closes.
    int num_items_displayed = 0;
    if (!manage_passwords_bubble_model_->best_matches().empty()) {
      for (autofill::PasswordFormMap::const_iterator i(
               manage_passwords_bubble_model_->best_matches().begin());
           i != manage_passwords_bubble_model_->best_matches().end(); ++i) {
        ManagePasswordItemView* item = new ManagePasswordItemView(
            manage_passwords_bubble_model_,
            *i->second,
            first_field_width,
            second_field_width,
            num_items_displayed == 0 ? ManagePasswordItemView::FIRST_ITEM
                                     : ManagePasswordItemView::SUBSEQUENT_ITEM);

        layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
        layout->AddView(item);
        num_items_displayed++;
      }
    } else if (!manage_passwords_bubble_model_->password_submitted()) {
        views::Label* empty_label = new views::Label(
            l10n_util::GetStringUTF16(IDS_MANAGE_PASSWORDS_NO_PASSWORDS));
        empty_label->SetMultiLine(true);

        layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
        layout->AddView(empty_label);
    }

    // If the user just saved a password, it won't be in the 'best matches' list
    // we just walked through. Display it explicitly.
    if (manage_passwords_bubble_model_->password_submitted()) {
      ManagePasswordItemView* item = new ManagePasswordItemView(
          manage_passwords_bubble_model_,
          manage_passwords_bubble_model_->pending_credentials(),
          first_field_width,
          second_field_width,
          num_items_displayed ? ManagePasswordItemView::FIRST_ITEM
                              : ManagePasswordItemView::SUBSEQUENT_ITEM);

      layout->StartRow(0, SINGLE_VIEW_COLUMN_SET);
      layout->AddView(item);
      num_items_displayed++;
    }

    // Build a "manage" link and "done" button, and throw them both into a new
    // row
    // containing a double-view columnset.
    manage_link_ =
        new views::Link(manage_passwords_bubble_model_->manage_link());
    manage_link_->SetUnderline(false);
    manage_link_->set_listener(this);

    done_button_ =
        new views::LabelButton(this, l10n_util::GetStringUTF16(IDS_DONE));
    done_button_->SetStyle(views::Button::STYLE_BUTTON);

    layout->StartRowWithPadding(
        0, DOUBLE_VIEW_COLUMN_SET, 0, views::kRelatedControlVerticalSpacing);
    layout->AddView(manage_link_);
    layout->AddView(done_button_);
  }
}

void ManagePasswordsBubbleView::WindowClosing() {
  // Close() closes the window asynchronously, so by the time we reach here,
  // |manage_passwords_bubble_| may have already been reset.
  if (manage_passwords_bubble_ == this)
    manage_passwords_bubble_ = NULL;
}

void ManagePasswordsBubbleView::ButtonPressed(views::Button* sender,
                                              const ui::Event& event) {
  DCHECK(sender == save_button_ || sender == done_button_);

  BubbleDismissalReason reason;
  if (sender == save_button_) {
    manage_passwords_bubble_model_->OnSaveClicked();
    reason = CLICKED_SAVE;
  } else {
    reason = CLICKED_DONE;
  }
  Close(reason);
}

void ManagePasswordsBubbleView::LinkClicked(views::Link* source,
                                            int event_flags) {
  DCHECK_EQ(source, manage_link_);
  manage_passwords_bubble_model_->OnManageLinkClicked();
  Close(CLICKED_MANAGE);
}

void ManagePasswordsBubbleView::OnPerformAction(views::Combobox* source) {
  DCHECK_EQ(source, refuse_combobox_);
  BubbleDismissalReason reason = NOT_DISPLAYED;
  switch (refuse_combobox_->selected_index()) {
    case SavePasswordRefusalComboboxModel::INDEX_NOPE:
      manage_passwords_bubble_model_->OnNopeClicked();
      reason = CLICKED_NOPE;
      break;
    case SavePasswordRefusalComboboxModel::INDEX_NEVER_FOR_THIS_SITE:
      manage_passwords_bubble_model_->OnNeverForThisSiteClicked();
      reason = CLICKED_NEVER;
      break;
    default:
      NOTREACHED();
      break;
  }
  Close(reason);
}
