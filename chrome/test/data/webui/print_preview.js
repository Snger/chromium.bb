// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
   function MockHandler() {
     this.__proto__ = MockHandler.prototype;
   };

   MockHandler.prototype = {
     'getDefaultPrinter': function() {
       console.log('getDefaultPrinter');
       setDefaultPrinter('FooDevice');
     },
     'getPrinters': function() {
       console.log('getPrinters');
       setPrinters([
                     {
                       'printerName': 'FooName',
                       'deviceName': 'FooDevice',
                     },
                     {
                       'printerName': 'BarName',
                       'deviceName': 'BarDevice',
                     },
                   ]);
     },
     'getPreview': function(settings) {
       console.log('getPreview(' + settings + ')');
       updatePrintPreview(1, 'title', true);
     },
     'print': function(settings) {
       console.log('print(' + settings + ')');
     },
     'getPrinterCapabilities': function(printer_name) {
       console.log('getPrinterCapabilities(' + printer_name + ')');
       updateWithPrinterCapabilities({
                                       'disableColorOption': true,
                                       'setColorAsDefault': true,
                                       'disableCopiesOption': true
                                     });
     },
     'showSystemDialog': function() {
       console.log('showSystemDialog');
     },
     'managePrinters': function() {
       console.log('managePrinters');
     },
     'closePrintPreviewTab': function() {
       console.log('closePrintPreviewTab');
     },
     'hidePreview': function() {
       console.log('hidePreview');
     },
   };

   function registerCallbacks() {
     console.log('registeringCallbacks');
     var mock_handler = new MockHandler();
     for (func in MockHandler.prototype) {
       if (typeof(mock_handler[func]) == 'function')
         registerMessageCallback(func,
                                 mock_handler,
                                 mock_handler[func]);
     }
   };

   if ('window' in this && 'registerMessageCallback' in window)
     registerCallbacks();
 })();

// Tests.
function FLAKY_TestPrinterList() {
  var printer_list = $('printer-list');
  assertTrue(!!printer_list, 'printer_list');
  assertTrue(printer_list.options.length >= 2, 'printer-list has at least 2');
  expectEquals('FooName', printer_list.options[0].text, '0 text is FooName');
  expectEquals('FooDevice', printer_list.options[0].value,
               '0 value is FooDevice');
  expectEquals('BarName', printer_list.options[1].text, '1 text is BarName');
  expectEquals('BarDevice', printer_list.options[1].value,
               '1 value is BarDevice');
}

var test_fixture = 'PrintPreviewWebUITest';
var test_add_library = false;
