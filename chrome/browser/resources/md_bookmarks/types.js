// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Closure typedefs for MD Bookmarks.
 */

/**
 * A normalized version of chrome.bookmarks.BookmarkTreeNode.
 * @typedef{{
 *   id: string,
 *   parentId: (string|undefined),
 *   url: (string|undefined),
 *   title: string,
 *   dateAdded: (number|undefined),
 *   dateGroupModified: (number|undefined),
 *   unmodifiable: (string|undefined),
 *   children: (!Array<string>|undefined),
 * }}
 */
var BookmarkNode;

/**
 * @typedef{!Object<string, BookmarkNode>}
 */
var NodeList;

/** @typedef {!Object<string, boolean>} */
var ClosedFolderState;

/**
 * @typedef{{
 *   nodes: NodeList,
 *   selectedFolder: ?string,
 *   closedFolders: ClosedFolderState,
 * }}
 */
var BookmarksPageState;

/** @typedef {{name: string}} */
var Action;

/** @interface */
function StoreObserver(){};

/** @param {!BookmarksPageState} newState */
StoreObserver.prototype.onStateChanged = function(newState) {};
