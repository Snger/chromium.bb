// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// require: event_tracker.js

cr.define('cr.ui', function() {

  /**
   * Bubble is a free-floating informational bubble with a triangular arrow
   * that points at a place of interest on the page. Currently the arrow is
   * always positioned at the bottom left and points down.
   */
  var Bubble = cr.ui.define('div');

  Bubble.prototype = {
    __proto__: HTMLDivElement.prototype,

    decorate: function() {
      this.className = 'bubble';
      this.innerHTML =
          '<div class=\"bubble-contents\"></div>' +
          '<div class=\"bubble-shadow\"></div>' +
          '<div class=\"bubble-arrow\"></div>';

      this.hidden = true;
    },

    /**
     * Sets the text message within the bubble.
     * @param {String} text The message string.
     */
    set text(text) {
      this.querySelector('.bubble-contents').textContent = text;
    },

    /**
     * Sets the anchor node, i.e. the node that this bubble points at.
     * @param {HTMLElement} node The new anchor node.
     */
    set anchorNode(node) {
      this.anchorNode_ = node;

      if (!this.hidden)
        this.reposition();
    },

    /**
     * Updates the position of the bubble. This is automatically called when
     * the window is resized, but should also be called any time the layout
     * may have changed.
     */
    reposition: function() {
      var node = this.anchorNode_;
      var clientRect = node.getBoundingClientRect();

      this.style.left = (clientRect.left + clientRect.right) / 2 + 'px';
      this.style.top = (clientRect.top - this.clientHeight) + 'px';
    },

    /**
     * Starts showing the bubble. The bubble will grab input and show until the
     * user clicks away.
     */
    show: function() {
      if (!this.hidden)
        return;

      document.body.appendChild(this);
      this.hidden = false;
      this.reposition();

      this.eventTracker_ = new EventTracker;
      this.eventTracker_.add(window, 'resize', this.reposition.bind(this));

      var doc = this.ownerDocument;
      this.eventTracker_.add(doc, 'keydown', this, true);
      this.eventTracker_.add(doc, 'mousedown', this, true);
    },

    /**
     * Hides the bubble from view.
     */
    hide: function() {
      this.hidden = true;
      this.eventTracker_.removeAll();
      this.parentNode.removeChild(this);
    },

    /**
     * Handles keydown and mousedown events, dismissing the bubble if
     * necessary.
     * @param {Event} e The event.
     */
    handleEvent: function(e) {
      switch (e.type) {
        case 'keydown':
          if (e.keyCode == 27)  // Esc
            this.hide();
          break;

        case 'mousedown':
          if (!this.contains(e.target))
            this.hide();
          break;
      }

      e.stopPropagation();
      e.preventDefault();
      return;
    },
  };

  return {
    Bubble: Bubble
  };
});
