// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview User pod row implementation.
 */

cr.define('login', function() {
  // Pod width. 170px Pod + 10px padding + 10px margin on both sides.
  const POD_WIDTH = 170 + 2 * (10 + 10);

  /**
   * Helper function to remove a class from given element.
   * @param {!HTMLElement} el Element whose class list to change.
   * @param {string} cl Class to remove.
   */
  function removeClass(el, cl) {
    el.classList.remove(cl);
  }

  /**
   * Creates a user pod.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var UserPod = cr.ui.define(function() {
    return $('user-pod-template').cloneNode(true);
  });

  UserPod.prototype = {
    __proto__: HTMLDivElement.prototype,

    /** @inheritDoc */
    decorate: function() {
      this.addEventListener('click', this.handleClick_.bind(this));
      this.enterButtonElement.addEventListener('click',
          this.activate.bind(this));
      this.removeUserButtonElement.addEventListener('click',
          this.handleRemoveUserClick_.bind(this));
    },

    /**
     * Initializes the pod after its properties set and added to a pod row.
     */
    initialize: function() {
      if (this.isGuest) {
        this.imageElement.title = this.name;
        this.enterButtonElement.hidden = false;
        this.passwordElement.hidden = true;
      } else {
        this.imageElement.title = this.email;
        this.passwordElement.addEventListener('keydown',
            this.parentNode.handleKeyDown.bind(this.parentNode));
        this.passwordElement.hidden = false;
        this.enterButtonElement.hidden = true;
      }
    },

    /**
     * Gets image element.
     * @type {!HTMLImageElement}
     */
    get imageElement() {
      return this.firstElementChild;
    },

    /**
     * Gets name element.
     * @type {!HTMLDivElement}
     */
    get nameElement() {
      return this.imageElement.nextElementSibling;
    },

    /**
     * Gets password field.
     * @type {!HTMLInputElement}
     */
    get passwordElement() {
      return this.nameElement.nextElementSibling;
    },

    /**
     * Gets guest enter button.
     * @type {!HTMLInputElement}
     */
    get enterButtonElement() {
      return this.passwordElement.nextElementSibling;
    },

    /**
     * Gets remove user button.
     * @type {!HTMLInputElement}
     */
    get removeUserButtonElement() {
      return this.lastElementChild;
    },

    /**
     * User email of this pod.
     * @type {string}
     */
    email_ : '',
    get email() {
      return this.email_;
    },
    set email(email) {
      this.email_ = email;
    },

    /**
     * User name.
     * @type {string}
     */
    get name() {
      return this.nameElement.textContent;
    },
    set name(name) {
      this.nameElement.textContent = name;
    },

    /**
     * User image url.
     * @type {string}
     */
    get imageUrl()  {
      return this.imageElement.src;
    },
    set imageUrl(url) {
      this.imageElement.src = url;
    },

    /**
     * Whether we are a guest pod or not.
     */
    get isGuest() {
      return !this.email_;
    },

    /**
     * Whether the user can be removed.
     * @type {boolean}
     */
    get canRemove() {
      return !this.removeUserButtonElement.hidden;
    },
    set canRemove(canRemove) {
      if (this.canRemove == canRemove)
        return;

      this.removeUserButtonElement.hidden = !canRemove;
    },

    /**
     * Focuses on input element.
     */
    focusInput: function() {
      if (!this.isGuest) {
        this.passwordElement.focus();
      } else {
        this.enterButtonElement.focus();
      }
    },

    /**
     * Actiavtes the pod.
     */
    activate: function() {
      if (this.isGuest) {
        chrome.send('launchIncognito');
      } else {
        chrome.send('authenticateUser',
            [this.email_, this.passwordElement.value]);
      }
    },

    /**
     * Handles click event on remove user button.
     * @private
     */
    handleRemoveUserClick_: function(e) {
      chrome.send('removeUser', [this.email]);
      this.parentNode.removeChild(this);
    },

    /**
     * Handles click event.
     */
    handleClick_: function(e) {
      this.parentNode.focusPod(this);
      e.stopPropagation();
    }
  };


  /**
   * Creates a new pod row element.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var PodRow = cr.ui.define('podrow');

  PodRow.prototype = {
    __proto__: HTMLDivElement.prototype,

    // Focused pod.
    focusedPod_ : undefined,

    /** @inheritDoc */
    decorate: function() {
      // Make this focusable
      if (!this.hasAttribute('tabindex'))
        this.tabIndex = 0;

      this.style.left = 0;

      this.ownerDocument.addEventListener('click',
          this.handleClick_.bind(this));
      this.ownerDocument.addEventListener('keydown',
          this.handleKeyDown.bind(this));
    },

    /**
     * Returns all the pods in this pod row.
     */
    get pods() {
      return this.children;
    },

    /**
     * Creates a user pod from given email.
     * @param {string} email User's email.
     */
    createUserPod: function(user) {
      var userPod = new UserPod({
        email: user.emailAddress,
        name: user.name,
        imageUrl: user.imageUrl,
        canRemove: user.canRemove
      });
      userPod.hidden = false;
      return userPod;
    },

    /**
     * Add an existing user pod to this pod row.
     * @param {!Object} user User info dictionary.
     */
    addUserPod: function(user) {
      var userPod = this.createUserPod(user);
      this.appendChild(userPod);
      userPod.initialize();
    },

    /**
     * Ensures the given pod is visible.
     * @param {UserPod} pod Pod to scroll into view.
     */
    scrollPodIntoView: function(pod) {
      var podIndex = this.findIndex_(pod);
      if (podIndex == -1)
        return;

      var left = podIndex * POD_WIDTH;
      var right = left + POD_WIDTH;

      var viewportLeft = -parseInt(this.style.left);
      var viewportRight = viewportLeft + this.parentNode.clientWidth;

      if (left < viewportLeft) {
        this.style.left = -left + 'px';
      } else if (right > viewportRight) {
        var offset = right - viewportRight;
        this.style.left = (viewportLeft - offset) + 'px';
      }
    },

    /**
     * Gets index of given pod or -1 if not found.
     * @param {UserPod} pod Pod to look up.
     * @private
     */
    findIndex_: function(pod) {
      for (var i = 0; i < this.pods.length; ++i) {
        if (pod == this.pods[i])
          return i;
      }

      return -1;
    },

    /**
     * Start first time show animation.
     */
    startInitAnimation: function() {
      // Schedule init animation.
      for (var i = 0; i < this.pods.length; ++i) {
        window.setTimeout(removeClass, 500 + i * 70,
            this.pods[i], 'init');
        window.setTimeout(removeClass, 700 + i * 70,
            this.pods[i].nameElement, 'init');
      }
    },

    /**
     * Populates pod row with given existing users and
     * kick start init animiation.
     * @param {array} users Array of existing user emails.
     */
    loadPods: function(users) {
      // Clear existing pods.
      this.textContent = '';
      this.focusedPod_ = undefined;

      // Popoulate the pod row.
      for (var i = 0; i < users.length; ++i) {
        this.addUserPod(users[i]);
      }
    },

    /**
     * Focuses a given user pod or clear focus when given null.
     * @param {UserPod} pod User pod to focus or null to clear focus.
     */
    focusPod: function(pod) {
      for (var i = 0; i < this.pods.length; ++i) {
        this.pods[i].classList.remove('focused');
        this.pods[i].classList.add('faded');
      }

      if (pod) {
        pod.classList.remove("faded");
        pod.classList.add("focused");
        pod.focusInput();

        this.focusedPod_ = pod;
        this.scrollPodIntoView(pod);
      } else {
        for (var i = 0; i < this.pods.length; ++i) {
          this.pods[i].classList.remove('faded');
        }
        this.focusedPod_ = undefined;
      }
    },

    /**
     * Activates given pod.
     * @param {UserPod} pod Pod to activate.
     */
    activatePod: function(pod) {
      if (!pod)
        return;

      pod.activate();

      var activated = this.findIndex_(pod);
      if (activated == -1)
        return;

      for (var i = 0; i < this.pods.length; ++i) {
        if (i < activated)
          this.pods[i].classList.add('left');
        else if (i < activated)
          this.pods[i].classList.add('right');
        else
          this.pods[i].classList.add('zoom');
      }
    },

    /**
     * Handler of click event.
     * @private
     */
    handleClick_: function(e) {
      // Clears focus.
      this.focusPod();
    },

    /**
     * Handler of keydown event.
     * @public
     */
    handleKeyDown: function(e) {
      var editing = false;
      if (e.target.tagName == 'INPUT' && e.target.value)
        editing = true;

      switch (e.keyIdentifier) {
        case 'Left':
          if (!editing) {
            if (this.focusedPod_ && this.focusedPod_.previousElementSibling)
              this.focusPod(this.focusedPod_.previousElementSibling);
            else
              this.focusPod(this.lastElementChild);

            e.stopPropagation();
          }
          break;
        case 'Right':
          if (!editing) {
            if (this.focusedPod_ && this.focusedPod_.nextElementSibling)
              this.focusPod(this.focusedPod_.nextElementSibling);
            else
              this.focusPod(this.firstElementChild);

            e.stopPropagation();
          }
          break;
        case 'Enter':
          this.activatePod(this.focusedPod_);
          e.stopPropagation();
          break;
      }
    }
  };

  return {
    PodRow: PodRow
  };
});
