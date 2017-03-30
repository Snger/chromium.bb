// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions for the Bookmarks page.
 */

cr.define('bookmarks.util', function() {
  /**
   * @param {!BookmarksPageState} state
   * @return {!Array<string>}
   */
  function getDisplayedList(state) {
    if (!isShowingSearch(state))
      return assert(state.nodes[assert(state.selectedFolder)].children);

    return state.search.results;
  }

  /**
   * @param {BookmarkTreeNode} rootNode
   * @return {NodeList}
   */
  function normalizeNodes(rootNode) {
    /** @type {NodeList} */
    var nodeList = {};
    var stack = [];
    stack.push(rootNode);

    while (stack.length > 0) {
      var node = stack.pop();
      // Node index is not necessary and not kept up-to-date. Remove it from the
      // data structure so we don't accidentally depend on the incorrect
      // information.
      delete node.index;
      nodeList[node.id] = node;
      if (!node.children)
        continue;

      var childIds = [];
      node.children.forEach(function(child) {
        childIds.push(child.id);
        stack.push(child);
      });
      node.children = childIds;
    }

    return nodeList;
  }

  /** @return {!BookmarksPageState} */
  function createEmptyState() {
    return {
      nodes: {},
      selectedFolder: '0',
      closedFolders: {},
      search: {
        term: '',
        inProgress: false,
        results: [],
      },
      selection: {
        items: {},
        anchor: null,
      },
    };
  }

  /**
   * @param {BookmarksPageState} state
   * @return boolean
   */
  function isShowingSearch(state) {
    return !state.selectedFolder;
  }

  /**
   * @param {string} id
   * @param {NodeList} nodes
   * @return {boolean}
   */
  function hasChildFolders(id, nodes) {
    var children = nodes[id].children;
    for (var i = 0; i < children.length; i++) {
      if (nodes[children[i]].children)
        return true;
    }
    return false;
  }

  return {
    createEmptyState: createEmptyState,
    getDisplayedList: getDisplayedList,
    hasChildFolders: hasChildFolders,
    isShowingSearch: isShowingSearch,
    normalizeNodes: normalizeNodes,
    ROOT_NODE_ID: '0',
  };
});
