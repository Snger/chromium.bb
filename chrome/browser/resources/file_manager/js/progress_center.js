// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Progress center at the background page.
 * @constructor
 */
var ProgressCenter = function() {
  cr.EventTarget.call(this);

  /**
   * ID counter.
   * @type {number}
   * @private
   */
  this.idCounter_ = 1;

  /**
   * Default container.
   * @type {ProgressItemContainer}
   * @private
   */
  this.targetContainer_ = ProgressItemContainer.CLIENT;

  /**
   * Current items managed by the progress center.
   * @type {Array.<ProgressItem>}
   * @private
   */
  this.items_ = [];

  /**
   * Timeout callback to remove items.
   * @type {TimeoutManager}
   * @private
   */
  this.resetTimeout_ = new ProgressCenter.TimeoutManager(
      this.reset_.bind(this));
};

/**
 * The default amount of milliseconds time, before a progress item will reset
 * after the last complete.
 * @type {number}
 * @private
 * @const
 */
ProgressCenter.RESET_DELAY_TIME_MS_ = 5000;

/**
 * Utility for timeout callback.
 *
 * @param {function(*):*} callback Callbakc function.
 * @constructor
 */
ProgressCenter.TimeoutManager = function(callback) {
  this.callback_ = callback;
  this.id_ = null;
  Object.seal(this);
};

/**
 * Requests timeout. Previous request is canceled.
 * @param {number} milliseconds Time to invoke the callback function.
 */
ProgressCenter.TimeoutManager.prototype.request = function(milliseconds) {
  if (this.id_)
    clearTimeout(this.id_);
  this.id_ = setTimeout(function() {
    this.id_ = null;
    this.callback_();
  }.bind(this), milliseconds);
};

ProgressCenter.prototype = {
  __proto__: cr.EventTarget.prototype,

  /**
   * Obtains the items to be displayed in the application window.
   * @private
   */
  get applicationItems() {
    return this.items_.filter(function(item) {
      return item.container == ProgressItemContainer.CLIENT;
    });
  }
};

/**
 * Adds an item to the progress center.
 * @param {ProgressItem} item Item to be added.
 */
ProgressCenter.prototype.addItem = function(item) {
  // If application window is opening, the item is displayed in the window.
  // Otherwise the item is displayed in notification.
  item.id = this.idCounter_++;
  item.container = this.targetContainer_;
  this.items_.push(item);

  if (item.status !== ProgressItemState.PROGRESSING)
    this.resetTimeout_.request(ProgressCenter.RESET_DELAY_TIME_MS_);

  var event = new cr.Event(ProgressCenterEvent.ITEM_ADDED);
  event.item = item;
  this.dispatchEvent(event);
};

/**
 * Updates the item in the progress center.
 *
 * @param {ProgressCenterItem} item New contents of the item.
 */
ProgressCenter.prototype.updateItem = function(item) {
  var index = this.getItemIndex_(item.id);
  if (index === -1)
    return;
  this.items_[index] = item;

  if (item.status !== ProgressItemState.PROGRESSING)
    this.resetTimeout_.request(ProgressCenter.RESET_DELAY_TIME_MS_);

  var event = new cr.Event(ProgressCenterEvent.ITEM_UPDATED);
  event.item = item;
  this.dispatchEvent(event);
};

/**
 * Switches the default container.
 * @param {ProgressItemContainer} newContainer New value of the default
 *     container.
 */
ProgressCenter.prototype.switchContainer = function(newContainer) {
  if (this.targetContainer_ === newContainer)
    return;

  // Current items to be moved to the notification center.
  if (newContainer == ProgressItemContainer.NOTIFICATION) {
    var items = this.applicationItems;
    for (var i = 0; i < items.length; i++) {
      items[i].container = ProgressItemContainer.NOTIFICATION;
      this.postItemToNotification_(items);
    }
  }

  // The items in the notification center does not come back to the Files.app
  // clients.

  // Assign the new value.
  this.targetContainer_ = newContainer;
};

/**
 * Obtains item index that have the specifying ID.
 * @param {number} id Item ID.
 * @return {number} Item index. Returns -1 If the item is not found.
 * @private
 */
ProgressCenter.prototype.getItemIndex_ = function(id) {
  for (var i = 0; i < this.items_.length; i++) {
    if (this.items_[i].id === id)
      return i;
  }
  return -1;
};

/**
 * Obtains the summarized item to be displayed in the closed progress center
 * panel.
 * @return {ProgressCenterItem} Summarized item. Returns null if there is no
 *     item.
 */
ProgressCenter.prototype.getSummarizedItem = function() {
  var applicationItems = this.applicationItems;
  if (applicationItems.length == 0)
    return null;
  if (applicationItems.length == 1)
    return applicationItems[0];
  var summarizedItem = new ProgressCenterItem();
  summarizedItem.summarized = true;
  var completeCount = 0;
  var progressingCount = 0;
  var canceledCount = 0;
  var errorCount = 0;
  for (var i = 0; i < applicationItems.length; i++) {
    switch (applicationItems[i].state) {
      case ProgressItemState.COMPLETE:
        completeCount++;
        break;
      case ProgressItemState.PROGRESSING:
        progressingCount++;
        break;
      case ProgressItemState.ERROR:
        errorCount++;
        continue;
      case ProgressItemState.CANCELED:
        canceledCount++;
        continue;
    }
    summarizedItem.progressMax += applicationItems[i].progressMax;
    summarizedItem.progressValue += applicationItems[i].progressValue;
  }
  var messages = [];
  if (completeCount)
    messages.push(completeCount + ' complete');
  if (progressingCount)
    messages.push(progressingCount + ' active');
  if (canceledCount)
    messages.push(canceledCount + ' canceled');
  if (errorCount)
    messages.push(errorCount + ' error');
  summarizedItem.message = messages.join(', ') + '.';
  summarizedItem.state =
      completeCount + progressingCount == 0 ? ProgressItemState.CANCELED :
      progressingCount > 0 ? ProgressItemState.PROGRESSING :
      ProgressItemState.COMPLETE;
  return summarizedItem;
};

/**
 * Passes the item to the ChromeOS's message center.
 *
 * TODO(hirono): Implement the method.
 *
 * @private
 */
ProgressCenter.prototype.passItemsToNotification_ = function() {

};

/**
 * Hides the progress center if there is no progressing items.
 * @private
 */
ProgressCenter.prototype.reset_ = function() {
  // If we have a progressing item, stop reset.
  for (var i = 0; i < this.items_.length; i++) {
    if (this.items_[i].state == ProgressItemState.PROGRESSING)
      return;
  }

  // Reset items.
  this.items_.splice(0, this.items_.length);

  // Dispatch a event.
  this.dispatchEvent(new cr.Event(ProgressCenterEvent.RESET));
};

/**
 * An event handler for progress center.
 * @param {FileOperationManager} fileOperationManager File operation manager.
 * @param {ProgressCenter} progressCenter Progress center.
 * @constructor
 */
var ProgressCenterHandler = function(fileOperationManager, progressCenter) {
  /**
   * Copying progress item.
   * @type {ProgressCenterItem}
   * @private
   */
  this.copyingItem_ = null;

  /**
   * Deleting progress item.
   * @type {ProgressCenterItem}
   * @private
   */
  this.deletingItem_ = null;

  // Seal the object.
  Object.seal(this);

  // Register event.
  fileOperationManager.addEventListener('copy-progress',
                                        this.onCopyProgress_.bind(this));
  fileOperationManager.addEventListener('delete',
                                        this.onDeleteProgress_.bind(this));
};

/**
 * Handles the copy-progress event.
 * @param {Event} event The copy-progress event.
 * @private
 */
ProgressCenterHandler.prototype.onCopyProgress_ = function(event) {
  switch (event.reason) {
    case 'BEGIN':
      if (this.copyingItem_) {
        console.error('Previous copy is not completed.');
        return;
      }
      this.copyingItem_ = new ProgressCenterItem();
      // TODO(hirono): Specifying the correct message.
      this.copyingItem_.message = 'Copying ...';
      this.copyingItem_.progressMax = event.status.totalBytes;
      this.copyingItem_.progressValue = event.status.processedBytes;
      progressCenter.addItem(this.copyingItem_);
      break;

    case 'PROGRESS':
      if (!this.copyingItem_) {
        console.error('Cannot find copying item.');
        return;
      }
      this.copyingItem_.progressValue = event.status.processedBytes;
      progressCenter.updateItem(this.copyingItem_);
      break;

    case 'SUCCESS':
    case 'ERROR':
      if (!this.copyingItem_) {
        console.error('Cannot find copying item.');
        return;
      }
      // TODO(hirono): Replace the message with the string assets.
      if (event.reason === 'SUCCESS') {
        this.copyingItem_.message = 'Complete.';
        this.copyingItem_.state = ProgressItemState.COMPLETE;
        this.copyingItem_.progressValue = this.copyingItem_.progressMax;
      } else {
        this.copyingItem_.message = 'Error.';
        this.copyingItem_.state = ProgressItemState.ERROR;
      }
      progressCenter.updateItem(this.copyingItem_);
      this.copyingItem_ = null;
      break;
  }
};

/**
 * Handles the delete event.
 * @param {Event} event The delete event.
 * @private
 */
ProgressCenterHandler.prototype.onDeleteProgress_ = function(event) {
  switch (event.reason) {
    case 'BEGIN':
      if (this.deletingItem_) {
        console.error('Previous delete is not completed.');
        return;
      }
      this.deletingItem_ = new ProgressCenterItem();
      // TODO(hirono): Specifying the correct message.
      this.deletingItem_.message = 'Deleting...';
      this.deletingItem_.progressMax = 100;
      progressCenter.addItem(this.deletingItem_);
      break;

    case 'PROGRESS':
      if (!this.deletingItem_) {
        console.error('Cannot find deleting item.');
        return;
      }
      progressCenter.updateItem(this.deletingItem_);
      break;

    case 'SUCCESS':
    case 'ERROR':
      if (!this.deletingItem_) {
        console.error('Cannot find deleting item.');
        return;
      }
      if (event.reason === 'SUCCESS') {
        this.deletingItem_.message = 'Complete.';
        this.deletingItem_.state = ProgressItemState.COMPLETE;
        this.deletingItem_.progressValue = this.deletingItem_.progressMax;
      } else {
        this.deletingItem_.message = 'Error.';
        this.deletingItem_.state = ProgressItemState.ERROR;
      }
      progressCenter.updateItem(this.deletingItem_);
      this.deletingItem_ = null;
      break;
  }
};
