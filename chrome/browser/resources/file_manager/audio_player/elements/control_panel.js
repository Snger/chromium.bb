// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  'use strict';

  /**
   * Moves |target| element above |anchor| element, in order to match the
   * bottom lines.
   * @param {HTMLElement} target Target element.
   * @param {HTMLElement} anchor Anchor element.
   */
  function matchBottomLine(target, anchor) {
    var targetRect = target.getBoundingClientRect();
    var anchorRect = anchor.getBoundingClientRect();

    var pos = {
      left: anchorRect.left + anchorRect.width / 2 - targetRect.width / 2,
      bottom: window.innerHeight - anchorRect.bottom,
    };

    target.style.position = 'fixed';
    target.style.left = pos.left + 'px';
    target.style.bottom = pos.bottom + 'px';
  }

  /**
   * Converts the time into human friendly string.
   * @param {number} time Time to be converted.
   * @return {string} String representation of the given time
   */
  function time2string(time) {
    return ~~(time / 60000) + ':' + ('0' + ~~(time / 1000 % 60)).slice(-2);
  }

  Polymer('control-panel', {
    /**
     * Initialize an element. This method is called automatically when the
     * element is ready.
     */
    ready: function() {
      this.$.volumeSlider.value = this.volume || 50;
      this.$.playlistButton.querySelector('input').checked =
          this.playlistExpanded;
    },

    /**
     * Current elapsed time in the current music in millisecond.
     * @type {number}
     */
    time: 0,

    /**
     * String representation of 'time'.
     * @type {number}
     * @private
     */
    get timeString_() {
      return time2string(this.time);
    },

    /**
     * Total length of the current music in millisecond.
     * @type {number}
     */
    duration: 0,

    /**
     * String representation of 'duration'.
     * @type {string}
     * @private
     */
    get durationString_() {
      return time2string(this.duration);
    },

    /**
     * Current volume. Must be between 0 to 100.
     * @type {number}
     */
    volume: 50,

    /**
     * Flag whether the playlist is expanded or not.
     * @type {boolean}
     */
    playlistExpanded: true,

    /**
     * Flag whether the volume slider is expanded or not.
     * @type {boolean}
     */
    volumeSliderShown: false,

    /**
     * Flag to enable shuffle mode.
     * @type {boolean}
     */
    shuffle: false,

    /*
     * Flag to enable repeat mode.
     * @type {boolean}
     */
    repeat: false,

    /*
     * Flag if the audio is playing or paused. True if playing, or false paused.
     * @type {boolean}
     */
    playing: false,

    /**
     * Invoked when the 'playlistExpanded' property is changed.
     * @param {boolean} oldValue old value.
     * @param {boolean} newValue new value.
     */
    playlistExpandedChanged: function(oldValue, newValue) {
      this.$.playlistButton.querySelector('input').checked = !!newValue;
    },

    /**
     * Invoked when the 'duration' property is changed.
     * @param {number} oldValue old value.
     * @param {number} newValue new value.
     */
    durationChanged: function(oldValue, newValue) {
      // Reset the current playback position.
      this.time = 0;
    },

    /**
     * Invoked when the next button is clicked.
     */
    nextClick: function() {
      this.fire('next-clicked');
    },

    /**
     * Invoked when the play button is clicked.
     */
    playClick: function() {
      this.playing = !this.playing;
    },

    /**
     * Invoked when the previous button is clicked.
     */
    previousClick: function() {
      this.fire('next-clicked');
    },

    /**
     * Invoked the volume button is clicked.
     * @type {Event} event The event.
     */
    volumeButtonClick: function(event) {
      if (this.volumeSliderShown) {
        matchBottomLine(this.$.volumeContainer, this.$.volumeButton);
        this.$.volumeContainer.style.visibility = 'visible';
      } else {
        this.$.volumeContainer.style.visibility = 'hidden';
      }
      event.stopPropagation();
    },

    /**
     * Invoked the value of the volume slider is changed.
     * @type {number}
     */
    volumeSliderChanged: function() {
      this.volume = this.$.volumeSlider.value;
      this.fire('volume-changed');
    },
  });
})();  // Anonymous closure
