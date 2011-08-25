// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('ntp4', function() {
  'use strict';

  var localStrings = new LocalStrings;

  /**
   * A running count of bookmark tiles that we create so that each will have
   * a unique ID.
   */
  var tileId = 0;

  /**
   * The maximum number of tiles that we will display on this page.  If there
   * are not enough spaces to show all bookmarks, we'll include a link to the
   * Bookmarks manager.
   * TODO(csilv): Eliminate the need for this restraint.
   * @type {number}
   * @const
   */
  var MAX_BOOKMARK_TILES = 18;

  /**
   * Creates a new bookmark object.
   * @param {Object} data The url and title.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function Bookmark(data) {
    var el = $('bookmark-template').cloneNode(true);
    el.__proto__ = Bookmark.prototype;
    el.data = data;
    el.initialize();

    return el;
  }

  Bookmark.prototype = {
    __proto__: HTMLDivElement.prototype,

    initialize: function() {
      var id = tileId++;
      this.id = 'bookmark_tile_' + id;

      var title = this.querySelector('.title');
      title.textContent = this.data.title;

      if (this.data.url) {
        var button = this.querySelector('.button');
        button.href = title.href = this.data.url;
      }

      var faviconDiv = this.querySelector('.favicon');
      var faviconUrl;
      if (this.data.url) {
        faviconUrl = 'chrome://favicon/size/32/' + this.data.url;
        chrome.send('getFaviconDominantColor',
                    [faviconUrl, id, 'ntp4.setBookmarksFaviconDominantColor']);
      } else {
        // TODO(csilv): We need a large (32px) icon for this URL.
        faviconUrl = 'chrome://theme/IDR_BOOKMARK_BAR_FOLDER';
        // TODO(csilv): Should we vary this color by platform?
        this.stripeColor = '#919191';
      }
      faviconDiv.style.backgroundImage = url(faviconUrl);

      this.addEventListener('click', this.handleClick_.bind(this));
    },

    /**
     * Sets the color of the favicon dominant color bar.
     * @param {string} color The css-parsable value for the color.
     */
    set stripeColor(color) {
      this.querySelector('.color-stripe').style.backgroundColor = color;
    },

    /**
     * Set the size and position of the bookmark tile.
     * @param {number} size The total size of |this|.
     * @param {number} x The x-position.
     * @param {number} y The y-position.
     *     animate.
     */
    setBounds: function(size, x, y) {
      this.style.width = this.style.height = size + 'px';
      this.style.left = x + 'px';
      this.style.top = y + 'px';
    },

    /**
     * Invoked when a bookmark is clicked
     * @param {Event} e The click event.
     * @private
     */
    handleClick_: function(e) {
      if (e.target.classList.contains('close-button')) {
        this.handleDelete_();
        e.preventDefault();
      } else if (!this.data.url) {
        chrome.send('getBookmarksData', [this.data.id]);
        e.preventDefault();
      }
    },

    /**
     * Delete a bookmark from the data model.
     * @private
     */
    handleDelete_: function() {
      // TODO(csilv): add support for deleting bookmarks
    },
  };

  /**
   * Creates a new bookmark title object.
   * @param {Object} data The url and title.
   * @constructor
   * @extends {HTMLDivElement}
   */
  function BookmarkTitle(data) {
    var el = cr.doc.createElement('div');
    el.__proto__ = BookmarkTitle.prototype;
    el.initialize(data);

    return el;
  }

  BookmarkTitle.prototype = {
    __proto__: HTMLDivElement.prototype,

    initialize: function(data) {
      this.className = 'title-crumb';
      this.folderId_ = data.id;
      this.textContent = data.parentId ? data.title :
          localStrings.getString('bookmarksPage');

      this.addEventListener('click', this.handleClick_);
    },

    /**
     * Invoked when a bookmark title is clicked
     * @param {Event} e The click event.
     * @private
     */
    handleClick_: function(e) {
      chrome.send('getBookmarksData', [this.folderId_]);
    },
  };

  var TilePage = ntp4.TilePage;

  var bookmarksPageGridValues = {
    // The fewest tiles we will show in a row.
    minColCount: 3,
    // The most tiles we will show in a row.
    maxColCount: 6,

    // The smallest a tile can be.
    minTileWidth: 150,
    // The biggest a tile can be.
    maxTileWidth: 150,
  };
  TilePage.initGridValues(bookmarksPageGridValues);

  /**
   * Creates a new BookmarksPage object.
   * @constructor
   * @extends {TilePage}
   */
  function BookmarksPage() {
    var el = new TilePage(bookmarksPageGridValues);
    el.__proto__ = BookmarksPage.prototype;
    el.initialize();

    return el;
  }

  BookmarksPage.prototype = {
    __proto__: TilePage.prototype,

    initialize: function() {
      this.classList.add('bookmarks-page');

      // insert the bookmark titles header which is unique to bookmark pages.
      this.insertBefore($('bookmarks-title-wrapper'), this.firstChild);

      // insert a container for a link to a Bookmarks Manager page.
      var link = document.createElement('a');
      link.className = 'bookmarks-manager-link';
      link.textContent = localStrings.getString('bookmarksManagerLinkTitle');
      var container = document.createElement('div');
      container.className = 'bookmarks-manager-link-container';
      container.hidden = true;
      container.appendChild(link);
      this.querySelector('.tile-page-content').appendChild(container);
    },

    /**
     * Build the bookmark titles bar (ie, navigation hiearchy).
     * @param {Array} items The parent hiearchy of the current folder.
     * @private
     */
    updateBookmarkTitles_: function(items) {
      var wrapper = $('bookmarks-title-wrapper');
      var title = wrapper.querySelector('.section-title');
      title.innerHTML = '';

      for (var i = items.length - 1; i > 0; i--) {
        title.appendChild(new BookmarkTitle(items[i]));

        var separator = document.createElement('hr');
        separator.className = 'bookmark-separator';
        title.appendChild(separator);
      }

      var titleCrumb = new BookmarkTitle(items[0]);
      titleCrumb.classList.add('title-crumb-active');
      title.appendChild(titleCrumb);
    },

    /**
     * Build the bookmark tiles.
     * @param {Array} items The contents of the current folder.
     * @private
     */
    updateBookmarkTiles_: function(items) {
      this.removeAllTiles();
      var tile_count = Math.min(items.length, MAX_BOOKMARK_TILES);
      for (var i = 0; i < tile_count; i++)
        this.appendTile(new Bookmark(items[i]), false);

      var container = this.querySelector('.bookmarks-manager-link-container');
      if (items.length > MAX_BOOKMARK_TILES) {
        var link = container.querySelector('.bookmarks-manager-link');
        link.href = 'chrome://bookmarks/#' + this.id;
        container.hidden = false;
      } else {
        container.hidden = true;
      }
    },

    /** @inheritDoc */
    shouldAcceptDrag: function(dataTransfer) {
      return false;
    },

    /**
     * Set the bookmark data that should be displayed, replacing any existing
     * data.
     */
    set data(data) {
      this.id = data.navigationItems[0].id;
      this.updateBookmarkTiles_(data.items);
      this.updateBookmarkTitles_(data.navigationItems);
    },
  };

  /**
   * Initializes and renders the bookmark chevron canvas.  This needs to be
   * performed after the page has been loaded so that we have access to the
   * style sheet values.
   */
  function initBookmarkChevron() {
    var wrapperStyle = window.getComputedStyle($('bookmarks-title-wrapper'));
    var width = 10;
    var height = parseInt(wrapperStyle.height, 10);
    var ctx = document.getCSSCanvasContext('2d', 'bookmark-chevron',
                                           width, height);
    ctx.strokeStyle = wrapperStyle.borderBottomColor;
    ctx.beginPath();
    ctx.moveTo(0, 0);
    ctx.lineTo(width, height / 2);
    ctx.lineTo(0, height);
    ctx.stroke();
  };

  /**
   * Set the dominant color for a bookmark tile.  This is the callback method
   * from a request made when the tile was created.
   * @param {number} id The numeric ID of the bookmark tile.
   * @param {string} color The color represented as a CSS string.
   */
  function setBookmarksFaviconDominantColor(id, color) {
    var tile = $('bookmark_tile_' + id);
    if (tile)
      tile.stripeColor = color;
  };

  return {
    BookmarksPage: BookmarksPage,
    initBookmarkChevron: initBookmarkChevron,
    setBookmarksFaviconDominantColor: setBookmarksFaviconDominantColor
  };
});

window.addEventListener('load', ntp4.initBookmarkChevron);
