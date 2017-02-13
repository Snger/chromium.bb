// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-autofill-section' is the section containing saved
 * addresses and credit cards for use in autofill.
 */
(function() {
  'use strict';

  Polymer({
    is: 'settings-autofill-section',

    behaviors: [I18nBehavior],

    properties: {
      /**
       * An array of saved addresses.
       * @type {!Array<!chrome.autofillPrivate.AddressEntry>}
       */
      addresses: Array,

      /**
       * The model for any address related action menus or dialogs.
       * @private {?chrome.autofillPrivate.AddressEntry}
       */
      activeAddress: Object,

      /** @private */
      showAddressDialog_: Boolean,

      /**
       * An array of saved credit cards.
       * @type {!Array<!chrome.autofillPrivate.CreditCardEntry>}
       */
      creditCards: Array,

      /**
       * The model for any credit card related action menus or dialogs.
       * @private {?chrome.autofillPrivate.CreditCardEntry}
       */
      activeCreditCard: Object,

      /** @private */
      showCreditCardDialog_: Boolean,
    },

    /**
     * Formats the expiration date so it's displayed as MM/YYYY.
     * @param {!chrome.autofillPrivate.CreditCardEntry} item
     * @return {string}
     */
    expiration_: function(item) {
      return item.expirationMonth + '/' + item.expirationYear;
    },

    /**
     * Open the address action menu.
     * @param {!Event} e The polymer event.
     * @private
     */
    onAddressMenuTap_: function(e) {
      var menuEvent = /** @type {!{model: !{item: !Object}}} */(e);

      /* TODO(scottchen): drop the [dataHost][dataHost] once this bug is fixed:
       https://github.com/Polymer/polymer/issues/2574 */
      var item = menuEvent.model['dataHost']['dataHost'].item;

      // Copy item so dialog won't update model on cancel.
      this.activeAddress = /** @type {!chrome.autofillPrivate.AddressEntry} */(
          Object.assign({}, item));

      var dotsButton = /** @type {!HTMLElement} */ (Polymer.dom(e).localTarget);
      /** @type {!CrActionMenuElement} */ (
          this.$.addressSharedMenu).showAt(dotsButton);
    },

    /**
     * Handles tapping on the "Add address" button.
     * @param {!Event} e The polymer event.
     * @private
     */
    onAddAddressTap_: function(e) {
      e.preventDefault();
      this.activeAddress = {};
      this.showAddressDialog_ = true;
    },

    /** @private */
    onAddressDialogClosed_: function() {
      this.showAddressDialog_ = false;
    },

    /**
     * Handles tapping on the "Edit" address button.
     * @param {!Event} e The polymer event.
     * @private
     */
    onMenuEditAddressTap_: function(e) {
      e.preventDefault();
      this.showAddressDialog_ = true;
      this.$.addressSharedMenu.close();
    },

    /** @private */
    onRemoteEditAddressTap_: function() {
      window.open(this.i18n('manageAddressesUrl'));
    },

    /**
     * Handles tapping on the "Remove" address button.
     * @private
     */
    onMenuRemoveAddressTap_: function() {
      this.fire('remove-address', this.activeAddress);
      this.$.addressSharedMenu.close();
    },

    /**
     * Opens the credit card action menu.
     * @param {!Event} e The polymer event.
     * @private
     */
    onCreditCardMenuTap_: function(e) {
      var menuEvent = /** @type {!{model: !{item: !Object}}} */(e);

      /* TODO(scottchen): drop the [dataHost][dataHost] once this bug is fixed:
       https://github.com/Polymer/polymer/issues/2574 */
      var item = menuEvent.model['dataHost']['dataHost'].item;

      // Copy item so dialog won't update model on cancel.
      this.activeCreditCard =
          /** @type {!chrome.autofillPrivate.CreditCardEntry} */(
              Object.assign({}, item));

      var dotsButton = /** @type {!HTMLElement} */ (Polymer.dom(e).localTarget);
      /** @type {!CrActionMenuElement} */ (
          this.$.creditCardSharedMenu).showAt(dotsButton);
    },

    /**
     * Handles tapping on the "Add credit card" button.
     * @param {!Event} e
     * @private
     */
    onAddCreditCardTap_: function(e) {
      e.preventDefault();
      var date = new Date();  // Default to current month/year.
      var expirationMonth = date.getMonth() + 1;  // Months are 0 based.
      this.activeCreditCard = {
        expirationMonth: expirationMonth.toString(),
        expirationYear: date.getFullYear().toString(),
      };
      this.showCreditCardDialog_ = true;
    },

    /** @private */
    onCreditCardDialogClosed_: function() {
      this.showCreditCardDialog_ = false;
    },

    /**
     * Handles tapping on the "Edit" credit card button.
     * @param {!Event} e The polymer event.
     * @private
     */
    onMenuEditCreditCardTap_: function(e) {
      e.preventDefault();
      this.showCreditCardDialog_ = true;
      this.$.creditCardSharedMenu.close();
    },

    /** @private */
    onRemoteEditCreditCardTap_: function() {
      window.open(this.i18n('manageCreditCardsUrl'));
    },

    /**
     * Handles tapping on the "Remove" credit card button.
     * @private
     */
    onMenuRemoveCreditCardTap_: function() {
      this.fire('remove-credit-card', this.activeCreditCard);
      this.$.creditCardSharedMenu.close();
    },

    /**
     * Handles tapping on the "Clear copy" button for cached credit cards.
     * @private
     */
    onMenuClearCreditCardTap_: function() {
      this.fire('clear-credit-card', this.activeCreditCard);
      this.$.creditCardSharedMenu.close();
    },

    /**
     * Returns true if the list exists and has items.
     * @param {Array<Object>} list
     * @return {boolean}
     * @private
     */
    hasSome_: function(list) {
      return !!(list && list.length);
    },
  });
})();
