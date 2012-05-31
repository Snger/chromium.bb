// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Print destination data object that holds data for both local and cloud
   * destinations.
   * @param {string} id ID of the destination.
   * @param {!print_preview.Destination.Type} type Type of the destination.
   * @param {string} displayName Display name of the destination.
   * @param {boolean} isRecent Whether the destination has been used recently.
   * @param {Array.<string>=} opt_tags Tags associated with the destination.
   * @param {boolean=} opt_isOwned Whether the destination is owned by the user.
   *     Only applies to cloud-based destinations.
   * @constructor
   */
  function Destination(id, type, displayName, isRecent, opt_tags, opt_isOwned) {
    /**
     * ID of the destination.
     * @type {string}
     * @private
     */
    this.id_ = id;

    /**
     * Type of the destination.
     * @type {!print_preview.Destination.Type}
     * @private
     */
    this.type_ = type;

    /**
     * Display name of the destination.
     * @type {string}
     * @private
     */
    this.displayName_ = displayName;

    /**
     * Whether the destination has been used recently.
     * @type {boolean}
     * @private
     */
    this.isRecent_ = isRecent;

    /**
     * Tags associated with the destination.
     * @type {!Array.<string>}
     * @private
     */
    this.tags_ = opt_tags || [];

    /**
     * Print capabilities of the destination.
     * @type {print_preview.ChromiumCapabilities}
     * @private
     */
    this.capabilities_ = null;

    /**
     * Whether the destination is owned by the user.
     * @type {boolean}
     * @private
     */
    this.isOwned_ = opt_isOwned || false;

    /**
     * Cache of destination location fetched from tags.
     * @type {?string}
     * @private
     */
    this.location_ = null;
  };

  /**
   * Prefix of the location destination tag.
   * @type {string}
   * @const
   */
  Destination.LOCATION_TAG_PREFIX = '__cp__printer-location=';

  /**
   * Enumeration of Google-promoted destination IDs.
   * @enum {string}
   */
  Destination.GooglePromotedId = {
    DOCS: '__google__docs',
    FEDEX: '__google__fedex',
    SAVE_AS_PDF: 'Save as PDF'
  };

  /**
   * Enumeration of the types of destinations.
   * @enum {string}
   */
  Destination.Type = {
    GOOGLE: 'google',
    LOCAL: 'local',
    MOBILE: 'mobile'
  };

  Destination.prototype = {
    /** @return {string} ID of the destination. */
    get id() {
      return this.id_;
    },

    /** @return {!print_preview.Destination.Type} Type of the destination. */
    get type() {
      return this.type_;
    },

    /** @return {string} Display name of the destination. */
    get displayName() {
      return this.displayName_;
    },

    /** @return {boolean} Whether the destination has been used recently. */
    get isRecent() {
      return this.isRecent_;
    },

    /**
     * @param {boolean} isRecent Whether the destination has been used recently.
     */
    set isRecent(isRecent) {
      this.isRecent_ = isRecent;
    },

    /**
     * @return {boolean} Whether the user owns the destination. Only applies to
     *     cloud-based destinations.
     */
    get isOwned() {
      return this.isOwned_;
    },

    /** @return {boolean} Whether the destination is local or cloud-based. */
    get isLocal() {
      return this.type_ == Destination.Type.LOCAL;
    },

    /** @return {boolean} Whether the destination is promoted by Google. */
    get isGooglePromoted() {
      for (var key in Destination.GooglePromotedId) {
        if (Destination.GooglePromotedId[key] == this.id_) {
          return true;
        }
      }
      return false;
    },

    /**
     * @return {string} The location of the destination, or an empty string if
     *     the location is unknown.
     */
    get location() {
      if (this.location_ == null) {
        for (var tag, i = 0; tag = this.tags_[i]; i++) {
          if (tag.indexOf(Destination.LOCATION_TAG_PREFIX) == 0) {
            this.location_ = tag.substring(
                Destination.LOCATION_TAG_PREFIX.length) || '';
            break;
          }
        }
      }
      return this.location_;
    },

    /** @return {!Array.<string>} Tags associated with the destination. */
    get tags() {
      return this.tags_.slice(0);
    },

    /**
     * @return {print_preview.ChromiumCapabilities} Print capabilities of the
     *     destination.
     */
    get capabilities() {
      return this.capabilities_;
    },

    /**
     * @param {!print_preview.ChromiumCapabilities} capabilities Print
     *     capabilities of the destination.
     */
    set capabilities(capabilities) {
      this.capabilities_ = capabilities;
    },

    /**
     * Matches a query against the destination.
     * @param {string} query Query to match against the destination.
     * @return {boolean} {@code true} if the query matches this destination,
     *     {@code false} otherwise.
     */
    matches: function(query) {
      return this.displayName_.toLowerCase().indexOf(
          query.toLowerCase().trim()) != -1;
    }
  };

  // Export
  return {
    Destination: Destination
  };
});
