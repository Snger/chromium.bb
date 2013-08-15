// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for local_discovery.html, served from chrome://devices/
 * This is used to show discoverable devices near the user.
 *
 * The simple object defined in this javascript file listens for
 * callbacks from the C++ code saying that a new device is available.
 */

cr.define('local_discovery', function() {
  'use strict';

  /**
   * Appends a row to the output table listing the new device.
   * @param {string} name Name of the device.
   * @param {string} info Additional info of the device, if empty device need to
   *    be deleted.
   */
  function onServiceUpdate(name, info) {
    name = name.replace(/[\r\n]/g, '');
    var table = $('devices-table');

    var params = [];
    if (info) {
      params[0] = info.domain;
      params[1] = info.port;
      params[2] = info.ip;
      params[3] = info.lastSeen;
    }

    for (var i = 0, row; row = table.rows[i]; i++) {
      if (row.cells[0].textContent == name) {
        if (!info) {
          // Delete service from the row.
          table.removeChild(row);
        } else {
          // Replace existing service.
          for (var j = 0; j < params.length; j++) {
            row.cells[j + 1].textContent = params[j];
          }
        }
        return;
      }
    }

    if (!info) {
      // Service could not be found in the table.
      return;
    }

    var tr = document.createElement('tr');
    var td = document.createElement('td');
    td.textContent = name;
    tr.appendChild(td);

    for (var j = 0; j < params.length; j++) {
      td = document.createElement('td');
      td.textContent = params[j];
      tr.appendChild(td);
    }

    td = document.createElement('td');
    if (!info.registered) {
      var button = document.createElement('button');
      button.textContent = loadTimeData.getString('serviceRegister');
      button.addEventListener('click', sendRegisterDevice.bind(null, name));
      td.appendChild(button);
    } else {
      td.textContent = loadTimeData.getString('registered');
    }
    tr.appendChild(td);

    td = document.createElement('td');
    button = document.createElement('button');
    button.textContent = loadTimeData.getString('serviceInfo');
    button.addEventListener('click', sendInfoRequest.bind(null, name));
    td.appendChild(button);

    tr.appendChild(td);
    table.appendChild(tr);
  }


  /**
   * Adds a row to the logging console.
   * @param {string} msg The message to log.
   */
  function logToInfoConsole(msg) {
    var div = document.createElement('div');
    div.textContent = msg;
    $('info-console').appendChild(div);
  }

  /**
   * Register a device.
   * @param {string} device The device to register.
   */
  function sendRegisterDevice(device) {
    chrome.send('registerDevice', [device]);
    logToInfoConsole(loadTimeData.getStringF('registeringService', device));
  }

  /**
   * Announce that a registration failed.
   * @param {string} reason The error message.
   */
  function registrationFailed(reason) {
    logToInfoConsole(loadTimeData.getStringF('registrationFailed', reason));
  }

  /**
   * Request the contents of a device's /info page.
   * @param {string} device The device to query.
   */
  function sendInfoRequest(device) {
    chrome.send('info', [device]);
    logToInfoConsole(loadTimeData.getStringF('infoStarted', device));
  }

  /**
   * Announce that a registration succeeeded.
   * @param {string} id The id of the newly registered device.
   */
  function registrationSuccess(id) {
    logToInfoConsole(loadTimeData.getStringF('registrationSucceeded', id));
  }

  /**
   * Render an info item onto the info pane.
   * @param {string} name Name of the item.
   * @param {?} value Value of the item.
   * @param {function(?):string} render_type Render function for value
   *     datatype.
   * @return {HTMLElement} Rendered info item.
   */
  function renderInfoItem(name, value, render_type) {
    var container = document.createElement('div');
    container.classList.add('info-item');
    var nameElem = document.createElement('span');
    nameElem.classList.add('info-item-name');
    nameElem.textContent = name;
    container.appendChild(nameElem);
    var valueElem = document.createElement('span');
    valueElem.textContent = render_type(value);
    container.appendChild(valueElem);
    return container;
  }

  /**
   * Render and append an info item to the info pane, if it exists.
   * @param {Object} info Info response.
   * @param {string} name Name of property.
   * @param {function(?):string} render_type Render function for value.
   */
  function infoItem(info, name, render_type) {
    if (name in info) {
      $('info-pane').appendChild(renderInfoItem(name, info[name], render_type));
    }
  }

  /**
   * Render a string to an info-pane-displayable string.
   * @param {?} value Value; not guaranteed to be a string.
   * @return {string} Rendered value.
   */
  function renderTypeString(value) {
    if (typeof value != 'string') {
      return 'INVALID';
    }
    return value;
  }

  /**
   * Render a integer to an info-pane-displayable string.
   * @param {?} value Value; not guaranteed to be an integer.
   * @return {string} Rendered value.
   */
  function renderTypeInt(value) {
    if (typeof value != 'number') {
      return 'INVALID';
    }

    return value.toString();
  }

  /**
   * Render an array to an info-pane-displayable string.
   * @param {?} value Value; not guaranteed to be an array.
   * @return {string} Rendered value.
   */
  function renderTypeStringList(value) {
    if (!Array.isArray(value)) {
      return 'INVALID';
    }

    var returnValue = '';
    var valueLength = value.length;
    for (var i = 0; i < valueLength - 1; i++) {
      returnValue += value[i];
      returnValue += ', ';
    }

    if (value.length != 0) {
      returnValue += value[value.length - 1];
    }

    return returnValue;
  }

  /**
   * Render info response from JSON.
   * @param {Object} info Info response.
   */
  function renderInfo(info) {
    // Clear info
    while ($('info-pane').firstChild) {
      $('info-pane').removeChild($('info-pane').firstChild);
    }

    infoItem(info, 'x-privet-token', renderTypeString);
    infoItem(info, 'id', renderTypeString);
    infoItem(info, 'name', renderTypeString);
    infoItem(info, 'description', renderTypeString);
    infoItem(info, 'type', renderTypeStringList);
    infoItem(info, 'api', renderTypeStringList);
    infoItem(info, 'connection_state', renderTypeString);
    infoItem(info, 'device_state', renderTypeString);
    infoItem(info, 'manufacturer', renderTypeString);
    infoItem(info, 'url', renderTypeString);
    infoItem(info, 'model', renderTypeString);
    infoItem(info, 'serial_number', renderTypeString);
    infoItem(info, 'firmware', renderTypeString);
    infoItem(info, 'uptime', renderTypeInt);
    infoItem(info, 'setup_url', renderTypeString);
    infoItem(info, 'support_url', renderTypeString);
    infoItem(info, 'update_url', renderTypeString);
  }

  /**
   * Announce that an info request failed.
   * @param {string} reason The error message.
   */
  function infoFailed(reason) {
    logToInfoConsole(loadTimeData.getStringF('infoFailed', reason));
  }

  document.addEventListener('DOMContentLoaded', function() {
    chrome.send('start');
  });

  return {
    registrationSuccess: registrationSuccess,
    registrationFailed: registrationFailed,
    onServiceUpdate: onServiceUpdate,
    infoFailed: infoFailed,
    renderInfo: renderInfo
  };
});
