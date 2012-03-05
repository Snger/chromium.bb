// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options.network', function() {

  var ArrayDataModel = cr.ui.ArrayDataModel;
  var List = cr.ui.List;
  var ListItem = cr.ui.ListItem;
  var Menu = cr.ui.Menu;
  var MenuItem = cr.ui.MenuItem;

  /**
   * Network settings constants. These enums usually match their C++
   * counterparts.
   */
  function Constants() {}

  // Network types:
  Constants.TYPE_UNKNOWN = 0;
  Constants.TYPE_ETHERNET = 1;
  Constants.TYPE_WIFI = 2;
  Constants.TYPE_WIMAX = 3;
  Constants.TYPE_BLUETOOTH = 4;
  Constants.TYPE_CELLULAR = 5;
  Constants.TYPE_VPN = 6;

  /**
   * Order in which controls are to appear in the network list sorted by key.
   */
  Constants.NETWORK_ORDER = ['ethernet',
                             'wifi',
                             'cellular',
                             'vpn',
                             'airplaneMode',
                             'useSharedProxies',
                             'addConnection'];

  /**
   * Mapping of network category titles to the network type.
   */
  var categoryMap = {
    'cellular': Constants.TYPE_CELLULAR,
    'ethernet': Constants.TYPE_ETHERNET,
    'wifi': Constants.TYPE_WIFI,
    'vpn': Constants.TYPE_VPN
  };

  /**
   * ID of the menu that is currently visible.
   * @type {?string}
   * @private
   */
  var activeMenu_ = null;

  /**
   * Indicates if cellular networks are available.
   * @type {boolean}
   * @private
   */
  var cellularAvailable_ = false;

  /**
   * Indicates if cellular networks are enabled.
   * @type {boolean}
   * @private
   */
  var cellularEnabled_ = false;

  /**
   * Indicates if shared proxies are enabled.
   * @type {boolean}
   * @private
   */
  var useSharedProxies_ = false;

  /**
   * Create an element in the network list for controlling network
   * connectivity.
   * @param {Object} data Description of the network list or command.
   * @constructor
   */
  function NetworkListItem(data) {
    var el = cr.doc.createElement('li');
    el.data_ = {};
    for (var key in data)
      el.data_[key] = data[key];
    NetworkListItem.decorate(el);
    return el;
  }

  /**
   * Decorate an element as a NetworkListItem.
   * @param {!Element} el The element to decorate.
   */
  NetworkListItem.decorate = function(el) {
    el.__proto__ = NetworkListItem.prototype;
    el.decorate();
  };

  NetworkListItem.prototype = {
    __proto__: ListItem.prototype,

    /**
     * Description of the network group or control.
     * @type {Object.<string,Object>}
     * @private
     */
    data_: null,

    /**
     * Element for the control's subtitle.
     * @type {?Element}
     * @private
     */
    subtitle_: null,

    /**
     * Icon for the network control.
     * @type {?Element}
     * @private
     */
    icon_: null,

    /**
     * Indicates if in the process of connecting to a network.
     * @type {boolean}
     * @private
     */
    connecting_: false,

    /**
     * Description of the network control.
     * @type {Object}
     */
    get data() {
      return this.data_;
    },

    /**
     * Text label for the subtitle.
     * @type {string}
     */
    set subtitle(text) {
      if (text)
        this.subtitle_.textContent = text;
      this.subtitle_.hidden = !text;
    },

    /**
     * URL for the network icon.
     * @type {string}
     */
    set iconURL(iconURL) {
      this.icon_.style.backgroundImage = url(iconURL);
    },

    /**
     * Type of network icon.  Each type corresponds to a CSS rule.
     * @type {string}
     */
    set iconType(type) {
      this.icon_.classList.add('network-' + type);
    },

    /**
     * Indicates if the network is in the process of being connected.
     * @type {boolean}
     */
    set connecting(state) {
      this.connecting_ = state;
      if (state)
        this.icon_.classList.add('network-connecting');
      else
        this.icon_.classList.remove('network-connecting');
    },

    /**
     * Indicates if the network is in the process of being connected.
     * @type {boolean}
     */
    get connecting() {
      return this.connecting_;
    },

    /**
     * Indicate that the selector arrow should be shown.
     */
    showSelector: function() {
      this.subtitle_.classList.add('network-selector');
    },

    /* @inheritDoc */
    decorate: function() {
      ListItem.prototype.decorate.call(this);
      this.className = 'network-group';
      this.icon_ = this.ownerDocument.createElement('div');
      this.icon_.className = 'network-icon';
      this.appendChild(this.icon_);
      var textContent = this.ownerDocument.createElement('div');
      textContent.className = 'network-group-labels';
      this.appendChild(textContent);
      var categoryLabel = this.ownerDocument.createElement('div');
      var title = this.data_.key + 'Title';
      categoryLabel.className = 'network-title';
      categoryLabel.textContent = templateData[title];
      textContent.appendChild(categoryLabel);
      this.subtitle_ = this.ownerDocument.createElement('div');
      this.subtitle_.className = 'network-subtitle';
      textContent.appendChild(this.subtitle_);
    },
  };

  /**
   * Creates a control that displays a popup menu when clicked.
   * @param {Object} data  Description of the control.
   */
  function NetworkMenuItem(data) {
    var el = new NetworkListItem(data);
    el.__proto__ = NetworkMenuItem.prototype;
    el.decorate();
    return el;
  }

  NetworkMenuItem.prototype = {
    __proto__: NetworkListItem.prototype,

    /**
     * Popup menu element.
     * @type {?Element}
     * @private
     */
    menu_: null,

    /* @inheritDoc */
    decorate: function() {
      this.subtitle = null;
      if (this.data.iconType)
        this.iconType = this.data.iconType;
      if (!this.connecting) {
        this.addEventListener('click', function() {
          this.showMenu();
        });
      }
    },

    /**
     * Retrieves the ID for the menu.
     * @private
     */
    getMenuName_: function() {
      return this.data_.key + '-network-menu';
    },

    /**
     * Creates a popup menu for the control.
     * @return {Element} The newly created menu.
     */
    createMenu: function() {
      if (this.data.menu) {
        var menu = this.ownerDocument.createElement('div');
        menu.id = this.getMenuName_();
        menu.className = 'network-menu';
        menu.hidden = true;
        Menu.decorate(menu);
        for (var i = 0; i < this.data.menu.length; i++) {
          var entry = this.data.menu[i];
          var button = this.ownerDocument.createElement('div');
          button.className = 'network-menu-item';
          var buttonLabel = this.ownerDocument.createElement('div');
          buttonLabel.className = 'network-menu-item-label';
          buttonLabel.textContent = entry.label;
          button.appendChild(buttonLabel);
          button.addEventListener('click', entry.command);
          MenuItem.decorate(button);
          menu.appendChild(button);
        }
        return menu;
      }
      return null;
    },

    /**
     * Displays a popup menu.
     */
    showMenu: function() {
      if (!this.menu_) {
        this.menu_ = this.createMenu();
        var parent = $('network-menus');
        var existing = $(this.menu_.id);
        if (existing)
          parent.replaceChild(this.menu_, existing);
        else
          parent.appendChild(this.menu_);
      }
      var top = this.offsetTop + this.clientHeight;
      var menuId = this.getMenuName_();
      if (menuId != activeMenu_) {
        closeMenu_();
        activeMenu_ = menuId;
        this.menu_.style.setProperty('top', top + 'px');
        this.menu_.hidden = false;
        setTimeout(function() {
          $('settings').addEventListener('click', closeMenu_);
        }, 0);
      }
    },

  };


  /**
   * Creates a control for selecting or configuring a network connection based
   * on the type of connection (e.g. wifi versus vpn).
   * @param {{key: string,
   *          networkList: Array.<Object>} data  Description of the network.
   * @constructor
   */
  function NetworkSelectorItem(data) {
    var el = new NetworkMenuItem(data);
    el.__proto__ = NetworkSelectorItem.prototype;
    el.decorate();
    return el;
  }

  NetworkSelectorItem.prototype = {
    __proto__: NetworkMenuItem.prototype,

    /* @inheritDoc */
    decorate: function() {
      // TODO(kevers): Generalize method of setting default label.
      var defaultMessage = this.data_.key == 'wifi' ?
          'networkOffline' : 'networkNotConnected';
      this.subtitle = templateData[defaultMessage];
      var list = this.data_.networkList;
      var candidateURL = null;
      for (var i = 0; i < list.length; i++) {
        var networkDetails = list[i];
        if (networkDetails.connecting || networkDetails.connected) {
          this.subtitle = networkDetails.networkName;
          candidateURL = networkDetails.iconURL;
          // Only break when we see a connecting network as it is possible to
          // have a connected network and a connecting network at the same
          // time.
          if (networkDetails.connecting) {
            this.connecting = true;
            candidateURL = null;
            break;
          }
        }
      }
      if (candidateURL)
        this.iconURL = candidateURL;
      else
        this.iconType = this.data.key;

      if (!this.connecting)
        this.showSelector();

      // TODO(kevers): Add default icon for VPN when disconnected or in the
      // process of connecting.
    },

    /**
     * Creates a menu for selecting, configuring or disconnecting from a
     * network.
     * @return {Element} The newly created menu.
     */
    createMenu: function() {
      var menu = this.ownerDocument.createElement('div');
      menu.id = this.getMenuName_();
      menu.className = 'network-menu';
      menu.hidden = true;
      Menu.decorate(menu);
      var addendum = [];
      if (this.data_.key == 'wifi') {
        addendum.push({label: localStrings.getString('joinOtherNetwork'),
                       command: 'connect',
                       data: {networkType: Constants.TYPE_WIFI,
                              servicePath: '?'}});
      }
      var list = this.data.rememberedNetworks;
      if (list && list.length > 0) {
        var callback = function(list) {
          $('remembered-network-list').clear();
          var dialog = options.PreferredNetworks.getInstance();
          OptionsPage.showPageByName('preferredNetworksPage', false);
          dialog.update(list);
        };
        addendum.push({label: localStrings.getString('preferredNetworks'),
                       command: callback,
                       data: list});
      }

      var networkGroup = this.ownerDocument.createElement('div');
      networkGroup.className = 'network-menu-group';
      var empty = true;
      list = this.data.networkList;
      if (list) {
        for (var i = 0; i < list.length; i++) {
          var data = list[i];
          if (!data.connected && !data.connecting) {
            // TODO(kevers): Check for a non-activated Cellular network.
            // If found, the menu item should trigger 'activate' instead
            // of 'connect'.
            if (data.networkType != Constants.TYPE_ETHERNET) {
              this.createConnectCallback_(networkGroup, data);
              empty = false;
            }
          } else if (data.connected) {
            addendum.push({label: localStrings.getString('networkOptions'),
                           command: 'options',
                           data: data});
            if (data.networkType == Constants.TYPE_VPN) {
              // Add separator
              addendum.push({});
              var i18nKey = 'disconnectNetwork';
              addendum.push({label: localStrings.getString(i18nKey),
                             command: 'disconnect',
                             data: data});
            }
            if (data.networkType != Constants.TYPE_ETHERNET) {
              var onlineMessage = this.ownerDocument.createElement('div');
              onlineMessage.textContent =
                  localStrings.getString('networkOnline');
              onlineMessage.className = 'network-menu-header';
              menu.insertBefore(onlineMessage, menu.firstChild);
            }
          }
        }
      }
      if (this.data_.key == 'wifi' || this.data_.key == 'cellular') {
        addendum.push({});
        if (this.data_.key == 'wifi') {
          addendum.push({label: localStrings.getString('turnOffWifi'),
                       command: function() {
                         chrome.send('disableWifi');
                       },
                       data: data});
        } else if (this.data_.key == 'cellular') {
          addendum.push({label: localStrings.getString('turnOffCellular'),
                       command: function() {
                         chrome.send('disableCellular');
                       },
                       data: data});
        }
      }
      if (addendum.length > 0) {
        var separator = false;
        if (!empty) {
          menu.appendChild(networkGroup);
          menu.appendChild(MenuItem.createSeparator());
          separator = true;
        }
        for (var i = 0; i < addendum.length; i++) {
          var value = addendum[i];
          if (value.data) {
            this.createCallback_(menu,
                                 value.data,
                                 value.label,
                                 value.command);
            separator = false;
          } else if (!separator) {
            menu.appendChild(MenuItem.createSeparator());
            separator = true;
          }
        }
      }
      return menu;
    },

    /**
     * Adds a command to a menu for modifying network settings.
     * @param {!Element} menu Parent menu.
     * @param {Object} data Description of the network.
     * @param {string} label Display name for the menu item.
     * @param {string|function} command Callback function or name
     *     of the command for |buttonClickCallback|.
     * @return {!Element} The created menu item.
     * @private
     */
    createCallback_: function(menu, data, label, command) {
      var button = this.ownerDocument.createElement('div');
      button.className = 'network-menu-item';
      var buttonLabel = this.ownerDocument.createElement('span');
      buttonLabel.className = 'network-menu-item-label';
      buttonLabel.textContent = label;
      button.appendChild(buttonLabel);
      var callback = null;
      if (typeof command == 'string') {
        var type = String(data.networkType);
        var path = data.servicePath;
        callback = function() {
          chrome.send('buttonClickCallback',
                      [type, path, command]);
          closeMenu_();
        };
      } else {
        callback = function() {
          command(data);
        };
      }
      button.addEventListener('click', callback);
      MenuItem.decorate(button);
      menu.appendChild(button);
      return button;
    },

    /**
     * Adds a menu item for connecting to a network.
     * @param {!Element} menu Parent menu.
     * @param {Object} data Description of the network.
     * @private
     */
    createConnectCallback_: function(menu, data) {
      var menuItem = this.createCallback_(menu,
                                          data,
                                          data.networkName,
                                          'connect');
      menuItem.style.backgroundImage = url(data.iconURL);
      var optionsButton = this.ownerDocument.createElement('div');
      optionsButton.className = 'network-options-button';
      var type = String(data.networkType);
      var path = data.servicePath;
      optionsButton.addEventListener('click', function(event) {
        event.stopPropagation();
        chrome.send('buttonClickCallback',
                    [type, path, 'options']);
        closeMenu_();
      });
      menuItem.appendChild(optionsButton);
    }
  };

  /**
   * Creates a button-like control for configurating internet connectivity.
   * @param {{key: string,
   *          subtitle: string,
   *          command: function} data  Description of the network control.
   * @constructor
   */
  function NetworkButtonItem(data) {
    var el = new NetworkListItem(data);
    el.__proto__ = NetworkButtonItem.prototype;
    el.decorate();
    return el;
  }

  NetworkButtonItem.prototype = {
    __proto__: NetworkListItem.prototype,

    /** @inheritDoc */
    decorate: function() {
      if (this.data.subtitle)
        this.subtitle = this.data.subtitle;
      else
       this.subtitle = null;
      if (this.data.command)
        this.addEventListener('click', this.data.command);
      if (this.data.iconURL)
        this.iconURL = this.data.iconURL;
      else if (this.data.iconType)
        this.iconType = this.data.iconType;
    },
  };

  /**
   * A list of controls for manipulating network connectivity.
   * @constructor
   */
  var NetworkList = cr.ui.define('list');

  NetworkList.prototype = {
    __proto__: List.prototype,

    /** @inheritDoc */
    decorate: function() {
      List.prototype.decorate.call(this);
      this.addEventListener('blur', this.onBlur_);
      this.dataModel = new ArrayDataModel([]);

      // Wi-Fi control is always visible.
      this.update({key: 'wifi', networkList: []});

      if (airplaneModeAvailable_()) {
        this.update({key: 'airplaneMode',
                     subtitle: localStrings.getString('airplaneModeLabel'),
                     command: function() {
                       chrome.send('toggleAirplaneMode');
                     }});
      }
      // TODO(kevers): Move to details dialog once settable on a per network
      // basis.
      this.update({key: 'useSharedProxies',
                   command: function() {
                     options.Preferences.setBooleanPref(
                         'settings.use_shared_proxies',
                         !useSharedProxies_);
                   }});

      // Add connection control.
      var addConnection = function(type) {
        var callback = function() {
          chrome.send('buttonClickCallback',
                      [String(type), '?', 'connect']);
        }
        return callback;
      }
      this.update({key: 'addConnection',
                   iconType: 'add-connection',
                   menu: [{label: localStrings.getString('addConnectionWifi'),
                           command: addConnection(Constants.TYPE_WIFI)},
                          {label: localStrings.getString('addConnectionVPN'),
                           command: addConnection(Constants.TYPE_VPN)}]
                  });

      var prefs = options.Preferences.getInstance();
      prefs.addEventListener('settings.use_shared_proxies', function(event) {
        useSharedProxies_ = event.value && event.value['value'] !=
            undefined ? event.value['value'] : event.value;
        $('network-list').updateToggleControl('useSharedProxies',
                                              useSharedProxies_);
      });
    },

    /**
     * When the list loses focus, unselect all items in the list.
     * @private
     */
    onBlur_: function() {
      this.selectionModel.unselectAll();
    },

    /**
     * Finds the index of a network item within the data model based on
     * category.
     * @param {string} key Unique key for the item in the list.
     * @return {number} The index of the network item, or |undefined| if it is
     *     not found.
     */
    indexOf: function(key) {
      var size = this.dataModel.length;
      for (var i = 0; i < size; i++) {
        var entry = this.dataModel.item(i);
        if (entry.key == key)
          return i;
      }
    },

    /**
     * Updates a network control.
     * @param {Object.<string,string>} data Description of the entry.
     */
    update: function(data) {
      var index = this.indexOf(data.key);
      if (index == undefined) {
        // Find reference position for adding the element.  We cannot hide
        // individual list elements, thus we need to conditionally add or
        // remove elements and cannot rely on any element having a fixed index.
        for (var i = 0; i < Constants.NETWORK_ORDER.length; i++) {
          if (data.key == Constants.NETWORK_ORDER[i]) {
            data.sortIndex = i;
            break;
          }
        }
        var referenceIndex = -1;
        for (var i = 0; i < this.dataModel.length; i++) {
          var entry = this.dataModel.item(i);
          if (entry.sortIndex < data.sortIndex)
            referenceIndex = i;
          else
            break;
        }
        if (referenceIndex == -1) {
          // Prepend to the start of the list.
          this.dataModel.splice(0, 0, data);
        } else if (referenceIndex == this.dataModel.length) {
          // Append to the end of the list.
          this.dataModel.push(data);
        } else {
          // Insert after the reference element.
          this.dataModel.splice(referenceIndex + 1, 0, data);
        }
      } else {
        var entry = this.dataModel.item(index);
        data.sortIndex = entry.sortIndex;
        this.dataModel.splice(index, 1, data);
      }
    },

    /** @inheritDoc */
    createItem: function(entry) {
      if (entry.networkList)
        return new NetworkSelectorItem(entry);
      if (entry.command)
        return new NetworkButtonItem(entry);
      if (entry.menu)
        return new NetworkMenuItem(entry);
    },

    /**
     * Deletes an element from the list.
     * @param {string} key  Unique identifier for the element.
     */
    deleteItem: function(key) {
      var index = this.indexOf(key);
      if (index != undefined)
        this.dataModel.splice(index, 1);
    },

    /**
     * Updates the state of a toggle button.
     * @param {string} key Unique identifier for the element.
     * @param {boolean} active Whether the control is active.
     */
    updateToggleControl: function(key, active) {
      var index = this.indexOf(key);
      if (index != undefined) {
        var entry = this.dataModel.item(index);
        entry.iconType = active ? 'control-active' :
            'control-inactive';
        this.update(entry);
      }
    }
  };

  /**
   * Chrome callback for updating network controls.
   * @param {Object} data Description of available network devices and their
   *     corresponding state.
   */
  NetworkList.refreshNetworkData = function(data) {
    var networkList = $('network-list');
    cellularAvailable_ = data.cellularAvailable;
    cellularEnabled_ = data.cellularEnabled;

    // Only show Ethernet control if connected.
    var ethernetConnection = getConnection_(data.wiredList);
    if (ethernetConnection) {
      var type = String(Constants.TYPE_ETHERNET);
      var path = ethernetConnection.servicePath;
      var ethernetOptions = function() {
        chrome.send('buttonClickCallback',
                    [type, path, 'options']);
      };
      networkList.update({key: 'ethernet',
                          subtitle: localStrings.getString('networkConnected'),
                          iconURL: ethernetConnection.iconURL,
                          command: ethernetOptions});
    } else {
      networkList.deleteItem('ethernet');
    }

    if (data.wifiEnabled) {
      loadData_('wifi', data.wirelessList, data.rememberedList);
    } else {
      var enableWifi = function() {
        chrome.send('enableWifi');
      };
      networkList.update({key: 'wifi',
                          subtitle: localStrings.getString('networkDisabled'),
                          iconType: 'wifi',
                          command: enableWifi});
    }

    // Only show cellular control if available and not in airplane mode.
    if (data.cellularAvailable && !data.airplaneMode) {
      if (data.cellularEnabled) {
        loadData_('cellular', data.wirelessList, data.rememberedList);
      } else {
        var subtitle = localStrings.getString('networkDisabled');
        var enableCellular = function() {
          chrome.send('enableCellular');
        };
        networkList.update({key: 'cellular',
                            subtitle: subtitle,
                            iconType: 'cellular',
                            command: enableCellular});
      }
    } else {
      networkList.deleteItem('cellular');
    }

    // Only show VPN control if there is an available network and an internet
    // connection.
    if (data.vpnList.length > 0 && (ethernetConnection ||
        isConnected_(data.wirelessList)))
      loadData_('vpn', data.vpnList, data.rememberedList);
    else
      networkList.deleteItem('vpn');

    networkList.updateToggleControl('airplaneMode', data.airplaneMode);

    networkList.invalidate();
    networkList.redraw();
  };

  /**
   * Updates the list of available networks and their status, filtered by
   * network type.
   * @param {string} category The type of network.
   * @param {Array} available The list of available networks and their status.
   * @param {Array} remembered The list of remmebered networks.
   */
  function loadData_(category, available, remembered) {
    var data = {key: category};
    var type = categoryMap[category];
    var availableNetworks = [];
    for (var i = 0; i < available.length; i++) {
      if (available[i].networkType == type)
        availableNetworks.push(available[i]);
    }
    data.networkList = availableNetworks;
    if (remembered) {
      var rememberedNetworks = [];
      for (var i = 0; i < remembered.length; i++) {
        if (remembered[i].networkType == type)
          rememberedNetworks.push(remembered[i]);
      }
      data.rememberedNetworks = rememberedNetworks;
    }
    $('network-list').update(data);
  }

  /**
   * Hides the currently visible menu.
   * @private
   */
  function closeMenu_() {
    if (activeMenu_) {
      $(activeMenu_).hidden = true;
      activeMenu_ = null;
      $('settings').removeEventListener('click', closeMenu_);
    }
  }

  /**
   * Determines if the user is connected to or in the process of connecting to
   * a wireless network.
   * @param {Array.<Object>} networkList List of networks.
   * @return {boolean} True if connected or connecting to a network.
   * @private
   */
  function isConnected_(networkList) {
    return getConnection_(networkList) != null;
  }

  /**
   * Fetches the active connection.
   * @param {Array.<Object>} networkList List of networks.
   * @return {boolean} True if connected or connecting to a network.
   * @private
   */
  function getConnection_(networkList) {
    if (!networkList)
      return null;
    for (var i = 0; i < networkList.length; i++) {
      var entry = networkList[i];
      if (entry.connected || entry.connecting)
        return entry;
    }
    return null;
  }

  /**
   * Queries if airplane mode is available.
   * @return {boolean} Indicates if airplane mode is available.
   * @private
   */
  function airplaneModeAvailable_() {
     // TODO(kevers): Use library callback to determine if airplane mode is
     // available once back-end suport is in place.
     return false;
  }

  // Export
  return {
    NetworkList: NetworkList
  };
});
