// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var localStrings = new LocalStrings();

// The total page count of the previewed document regardless of which pages the
// user has selected.
var totalPageCount;

// The previously selected pages by the user. It is used in
// onPageSelectionMayHaveChanged() to make sure that a new preview is not
// requested more often than necessary.
var previouslySelectedPages = [];

// Timer id of the page range textfield. It is used to reset the timer whenever
// needed.
var timerId;

// Store the last selected printer index.
var lastSelectedPrinterIndex = 0;

// Used to disable some printing options when the preview is not modifiable.
var previewModifiable = false;

// Destination list special value constants.
const PRINT_TO_PDF = 'Print To PDF';
const MANAGE_PRINTERS = 'Manage Printers';

// State of the print preview settings.
var printSettings = new PrintSettings();

// The name of the default or last used printer.
var defaultOrLastUsedPrinterName = '';

// True when a pending print preview request exists.
var hasPendingPreviewRequest = false;

// True when a pending print file request exists.
var hasPendingPrintFileRequest = false;

// True when preview tab has some error.
var hasError = false;

// True when preview tab is hidden.
var isTabHidden = false;

// True when draft preview data is requested for preview.
var draftDocument = true;

/**
 * Window onload handler, sets up the page and starts print preview by getting
 * the printer list.
 */
function onLoad() {
  enablePlatformSpecificCSSRules();

  $('cancel-button').addEventListener('click', handleCancelButtonClick);

  if (!checkCompatiblePluginExists()) {
    displayErrorMessageWithButton(localStrings.getString('noPlugin'),
                                  localStrings.getString('launchNativeDialog'),
                                  showSystemDialog);
    $('mainview').parentElement.removeChild($('dummy-viewer'));
    return;
  }

  $('system-dialog-link').addEventListener('click', showSystemDialog);
  $('mainview').parentElement.removeChild($('dummy-viewer'));

  $('printer-list').disabled = true;
  $('print-button').onclick = printFile;

  setDefaultHandlersForPagesAndCopiesControls();
  showLoadingAnimation();
  chrome.send('getDefaultPrinter');
}

/**
 * Handles the individual pages input event.
 */
function handleIndividualPagesInputEvent() {
  $('print-pages').checked = true;
  resetPageRangeFieldTimer();
}

/**
 * Handles the individual pages blur event.
 */
function onPageRangesFieldBlur() {
  $('print-pages').checked = true;
  validatePageRangesField();
  updatePrintButtonState();
}

/**
 * Sets the default event handlers for pages and copies controls.
 */
function setDefaultHandlersForPagesAndCopiesControls() {
  var allPages = $('all-pages');
  var printPages = $('print-pages');
  var individualPages = $('individual-pages');

  allPages.onclick = null;
  printPages.onclick = null;
  individualPages.oninput = null;
  individualPages.onfocus = null;
  individualPages.onblur = null;

  if (!hasError) {
    allPages.onclick = updatePrintButtonState;
    printPages.onclick = handleIndividualPagesCheckbox;
    individualPages.onblur = onPageRangesFieldBlur;
  }

  $('copies').oninput = copiesFieldChanged;
  $('increment').onclick = function() { onCopiesButtonsClicked(1); };
  $('decrement').onclick = function() { onCopiesButtonsClicked(-1); };
}

/**
 * Adds event listeners to the settings controls.
 */
function addEventListeners() {
  // Controls that require preview rendering.
  $('all-pages').onclick = onPageSelectionMayHaveChanged;
  $('print-pages').onclick = handleIndividualPagesCheckbox;
  var individualPages = $('individual-pages');
  individualPages.onblur = function() {
      clearTimeout(timerId);
      onPageSelectionMayHaveChanged();
  };
  individualPages.onfocus = addTimerToPageRangeField;
  individualPages.oninput = handleIndividualPagesInputEvent;
  $('landscape').onclick = onLayoutModeToggle;
  $('portrait').onclick = onLayoutModeToggle;
  $('printer-list').onchange = updateControlsWithSelectedPrinterCapabilities;

  // Controls that dont require preview rendering.
  $('copies').oninput = function() {
    copiesFieldChanged();
    updatePrintButtonState();
    updatePrintSummary();
  };
  $('two-sided').onclick = handleTwoSidedClick;
  $('color').onclick = function() { setColor(true); };
  $('bw').onclick = function() { setColor(false); };
  $('increment').onclick = function() {
    onCopiesButtonsClicked(1);
    updatePrintButtonState();
    updatePrintSummary();
  };
  $('decrement').onclick = function() {
    onCopiesButtonsClicked(-1);
    updatePrintButtonState();
    updatePrintSummary();
  };
}

/**
 * Removes event listeners from the settings controls.
 */
function removeEventListeners() {
  clearTimeout(timerId);
  setDefaultHandlersForPagesAndCopiesControls();

  // Controls that require preview rendering
  $('landscape').onclick = null;
  $('portrait').onclick = null;
  $('printer-list').onchange = null;

  // Controls that dont require preview rendering.
  $('two-sided').onclick = null;
  $('color').onclick = null;
  $('bw').onclick = null;
}

/**
 * Asks the browser to close the preview tab.
 */
function handleCancelButtonClick() {
  chrome.send('closePrintPreviewTab');
}

/**
 * Asks the browser to show the native print dialog for printing.
 */
function showSystemDialog() {
  chrome.send('showSystemDialog');
}

/**
 * Disables the controls which need the initiator tab to generate preview
 * data. This function is called when the initiator tab is closed.
 * @param {string} initiatorTabURL The URL of the initiator tab.
 */
function onInitiatorTabClosed(initiatorTabURL) {
  displayErrorMessageWithButton(
      localStrings.getString('initiatorTabClosed'),
      localStrings.getString('reopenPage'),
      function() { window.location = initiatorTabURL; });
}

/**
 * Gets the selected printer capabilities and updates the controls accordingly.
 */
function updateControlsWithSelectedPrinterCapabilities() {
  var printerList = $('printer-list');
  var selectedIndex = printerList.selectedIndex;
  if (selectedIndex < 0)
    return;

  var selectedValue = printerList.options[selectedIndex].value;
  if (selectedValue == PRINT_TO_PDF) {
    updateWithPrinterCapabilities({'disableColorOption': true,
                                   'setColorAsDefault': true,
                                   'disableCopiesOption': true});
  } else if (selectedValue == MANAGE_PRINTERS) {
    printerList.selectedIndex = lastSelectedPrinterIndex;
    chrome.send('managePrinters');
    return;
  } else {
    // This message will call back to 'updateWithPrinterCapabilities'
    // function.
    chrome.send('getPrinterCapabilities', [selectedValue]);
  }

  lastSelectedPrinterIndex = selectedIndex;

  // Regenerate the preview data based on selected printer settings.
  setDefaultValuesAndRegeneratePreview();
}

/**
 * Updates the controls with printer capabilities information.
 * @param {Object} settingInfo printer setting information.
 */
function updateWithPrinterCapabilities(settingInfo) {
  var disableColorOption = settingInfo.disableColorOption;
  var disableCopiesOption = settingInfo.disableCopiesOption;
  var setColorAsDefault = settingInfo.setColorAsDefault;
  var colorOption = $('color');
  var bwOption = $('bw');

  if (disableCopiesOption) {
    fadeOutElement($('copies-option'));
    $('hr-before-copies').classList.remove('invisible');
  } else {
    fadeInElement($('copies-option'));
    $('hr-before-copies').classList.add('invisible');
  }

  disableColorOption ? fadeOutElement($('color-options')) :
      fadeInElement($('color-options'));

  if (colorOption.checked != setColorAsDefault) {
    colorOption.checked = setColorAsDefault;
    bwOption.checked = !setColorAsDefault;
  }
}

/**
 * Validates the copies text field value.
 * NOTE: An empty copies field text is considered valid because the blur event
 * listener of this field will set it back to a default value.
 * @return {boolean} true if the number of copies is valid else returns false.
 */
function isNumberOfCopiesValid() {
  var copiesFieldText = $('copies').value;
  return copiesFieldText == '' ? true : isPositiveInteger(copiesFieldText);
}

/**
 * Checks whether the preview layout setting is set to 'landscape' or not.
 *
 * @return {boolean} true if layout is 'landscape'.
 */
function isLandscape() {
  return $('landscape').checked;
}

/**
 * Checks whether the preview color setting is set to 'color' or not.
 *
 * @return {boolean} true if color is 'color'.
 */
function isColor() {
  return $('color').checked;
}

/**
 * Checks whether the preview collate setting value is set or not.
 *
 * @return {boolean} true if collate setting is enabled and checked.
 */
function isCollated() {
  return !$('collate-option').hidden && $('collate').checked;
}

/**
 * Returns the number of copies currently indicated in the copies textfield. If
 * the contents of the textfield can not be converted to a number or if <1 it
 * returns 1.
 *
 * @return {number} number of copies.
 */
function getCopies() {
  var copies = parseInt($('copies').value, 10);
  if (!copies || copies <= 1)
    copies = 1;
  return copies;
}

/**
 * Checks whether the preview two-sided checkbox is checked.
 *
 * @return {boolean} true if two-sided is checked.
 */
function isTwoSided() {
  return $('two-sided').checked;
}

/**
 * Gets the duplex mode for printing.
 * @return {number} duplex mode.
 */
function getDuplexMode() {
  // Constants values matches printing::DuplexMode enum.
  const SIMPLEX = 0;
  const LONG_EDGE = 1;
  return !isTwoSided() ? SIMPLEX : LONG_EDGE;
}

/**
 * Creates a JSON string based on the values in the printer settings.
 *
 * @return {string} JSON string with print job settings.
 */
function getSettingsJSON() {
  var printAll = $('all-pages').checked;
  var deviceName = getSelectedPrinterName();
  var printToPDF = (deviceName == PRINT_TO_PDF);

  return JSON.stringify(
      {'deviceName': deviceName,
       'pageRange': pageSetToPageRanges(getSelectedPagesSet()),
       'printAll': printAll,
       'duplex': getDuplexMode(),
       'copies': getCopies(),
       'collate': isCollated(),
       'landscape': isLandscape(),
       'color': isColor(),
       'printToPDF': printToPDF,
       'draftDocument': draftDocument});
}

/**
 * Returns the name of the selected printer or the empty string if no
 * printer is selected.
 */
function getSelectedPrinterName() {
  var printerList = $('printer-list')
  var selectedPrinter = printerList.selectedIndex;
  var deviceName = '';
  if (selectedPrinter >= 0)
    deviceName = printerList.options[selectedPrinter].value;
  return deviceName;
}

/**
 * Asks the browser to print the preview PDF based on current print settings.
 */
function printFile() {
  hasPendingPrintFileRequest = hasPendingPreviewRequest;
  var deviceName = getSelectedPrinterName();

  if (hasPendingPrintFileRequest) {
    if (deviceName != PRINT_TO_PDF) {
      isTabHidden = true;
      chrome.send('hidePreview');
    }
    return;
  }

  if ($('print-button').disabled) {
    if (isTabHidden)
      cancelPendingPrintRequest();
    return;
  }

  if (draftDocument) {
    hasPendingPrintFileRequest = true;
    requestPrintPreview();
    return;
  }

  if (isTabHidden || deviceName == PRINT_TO_PDF) {
    sendPrintFileRequest();
  } else {
    $('print-button').classList.add('loading');
    $('cancel-button').classList.add('loading');
    $('print-summary').innerHTML = localStrings.getString('printing');
    removeEventListeners();
    window.setTimeout(function() { sendPrintFileRequest(); }, 1000);
  }
}

/**
 * Sends a message to cancel the pending print request.
 */
function cancelPendingPrintRequest() {
  chrome.send('cancelPendingPrintRequest');
}

/**
 * Sends a message to initiate print workflow.
 */
function sendPrintFileRequest() {
  chrome.send('print', [getSettingsJSON()]);
}

/**
 * Asks the browser to generate a preview PDF based on current print settings.
 */
function requestPrintPreview() {
  hasPendingPreviewRequest = true;
  removeEventListeners();
  printSettings.save();
  if (isTabHidden || hasPendingPrintFileRequest)
    draftDocument = false;
  else
    showLoadingAnimation();

  chrome.send('getPreview', [getSettingsJSON()]);
}

/**
 * Called from PrintPreviewUI::OnFileSelectionCancelled to notify the print
 * preview tab regarding the file selection cancel event.
 */
function fileSelectionCancelled() {
  draftDocument = true;
  hasPendingPrintFileRequest = false;
}

/**
 * Set the default printer. If there is one, generate a print preview.
 * @param {string} printer Name of the default printer. Empty if none.
 */
function setDefaultPrinter(printer) {
  // Add a placeholder value so the printer list looks valid.
  addDestinationListOption('', '', true, true);
  if (printer) {
    defaultOrLastUsedPrinterName = printer;
    $('printer-list')[0].value = defaultOrLastUsedPrinterName;
    updateControlsWithSelectedPrinterCapabilities();
  }
  chrome.send('getPrinters');
}

/**
 * Fill the printer list drop down.
 * Called from PrintPreviewHandler::SendPrinterList().
 * @param {Array} printers Array of printer info objects.
 */
function setPrinters(printers) {
  var printerList = $('printer-list');
  // If there exists a dummy printer value, then setDefaultPrinter() already
  // requested a preview, so no need to do it again.
  var needPreview = (printerList[0].value == '');
  for (var i = 0; i < printers.length; ++i) {
    var isDefault = (printers[i].deviceName == defaultOrLastUsedPrinterName);
    addDestinationListOption(printers[i].printerName, printers[i].deviceName,
                             isDefault, false);
  }

  // Remove the dummy printer added in setDefaultPrinter().
  printerList.remove(0);

  if (printers.length != 0)
    addDestinationListOption('', '', false, true);

  // Adding option for saving PDF to disk.
  addDestinationListOption(localStrings.getString('printToPDF'),
                           PRINT_TO_PDF,
                           defaultOrLastUsedPrinterName == PRINT_TO_PDF,
                           false);
  addDestinationListOption('', '', false, true);

  // Add an option to manage printers.
  addDestinationListOption(localStrings.getString('managePrinters'),
                           MANAGE_PRINTERS, false, false);

  printerList.disabled = false;

  if (needPreview)
    updateControlsWithSelectedPrinterCapabilities();
}

/**
 * Adds an option to the printer destination list.
 * @param {String} optionText specifies the option text content.
 * @param {String} optionValue specifies the option value.
 * @param {boolean} isDefault is true if the option needs to be selected.
 * @param {boolean} isDisabled is true if the option needs to be disabled.
 */
function addDestinationListOption(optionText, optionValue, isDefault,
    isDisabled) {
  var option = document.createElement('option');
  option.textContent = optionText;
  option.value = optionValue;
  $('printer-list').add(option);
  option.selected = isDefault;
  option.disabled = isDisabled;
}

/**
 * Sets the color mode for the PDF plugin.
 * Called from PrintPreviewHandler::ProcessColorSetting().
 * @param {boolean} color is true if the PDF plugin should display in color.
 */
function setColor(color) {
  var pdfViewer = $('pdf-viewer');
  if (!pdfViewer) {
    return;
  }
  pdfViewer.grayscale(!color);
}

/**
 * Display an error message in the center of the preview area.
 * @param {string} errorMessage The error message to be displayed.
 */
function displayErrorMessage(errorMessage) {
  hasError = true;
  $('print-button').disabled = true;
  $('overlay-layer').classList.remove('invisible');
  $('dancing-dots-text').classList.add('hidden');
  $('error-text').innerHTML = errorMessage;
  $('error-text').classList.remove('hidden');
  removeEventListeners();
  var pdfViewer = $('pdf-viewer');
  if (pdfViewer)
    $('mainview').removeChild(pdfViewer);

  if (hasPendingPrintFileRequest && isTabHidden)
    cancelPendingPrintRequest();
}

/**
 * Display an error message in the center of the preview area followed by a
 * button.
 * @param {string} errorMessage The error message to be displayed.
 * @param {string} buttonText The text to be displayed within the button.
 * @param {string} buttonListener The listener to be executed when the button is
 * clicked.
 */
function displayErrorMessageWithButton(
    errorMessage, buttonText, buttonListener) {
  var errorButton = $('error-button');
  errorButton.textContent = buttonText;
  errorButton.onclick = buttonListener;
  errorButton.classList.remove('hidden');
  displayErrorMessage(errorMessage);
}

/**
 * Display an error message when print preview fails.
 * Called from PrintPreviewMessageHandler::OnPrintPreviewFailed().
 */
function printPreviewFailed() {
  displayErrorMessage(localStrings.getString('previewFailed'));
}

/**
 * Called when the PDF plugin loads its document.
 */
function onPDFLoad() {
  if (isLandscape())
    $('pdf-viewer').fitToWidth();
  else
    $('pdf-viewer').fitToHeight();

  setColor($('color').checked);

  hideLoadingAnimation();

  if (!previewModifiable)
    fadeOutElement($('landscape-option'));

  updateCopiesButtonsState();
}

/**
 * Update the print preview when new preview data is available.
 * Create the PDF plugin as needed.
 * Called from PrintPreviewUI::PreviewDataIsAvailable().
 * @param {number} pageCount The expected total pages count.
 * @param {string} jobTitle The print job title.
 * @param {boolean} modifiable If the preview is modifiable.
 * @param {string} previewUid Preview unique identifier.
 */
function updatePrintPreview(pageCount, jobTitle, modifiable, previewUid) {
  var tempPrintSettings = new PrintSettings();
  tempPrintSettings.save();

  previewModifiable = modifiable;

  hasPendingPreviewRequest = false;

  if (!totalPageCount)
    totalPageCount = pageCount;

  if (previouslySelectedPages.length == 0)
    for (var i = 0; i < totalPageCount; i++)
      previouslySelectedPages.push(i+1);

  if (printSettings.deviceName != tempPrintSettings.deviceName) {
    updateControlsWithSelectedPrinterCapabilities();
    return;
  } else if (printSettings.isLandscape != tempPrintSettings.isLandscape) {
    setDefaultValuesAndRegeneratePreview();
    return;
  } else if (isSelectedPagesValid()) {
    var currentlySelectedPages = getSelectedPagesSet();
    if (!areArraysEqual(previouslySelectedPages, currentlySelectedPages)) {
      previouslySelectedPages = currentlySelectedPages;
      requestPrintPreview();
      return;
    }
  }

  if (!isSelectedPagesValid())
    pageRangesFieldChanged();

  // Update the current tab title.
  document.title = localStrings.getStringF('printPreviewTitleFormat', jobTitle);

  createPDFPlugin(previewUid);
  updatePrintSummary();
  updatePrintButtonState();
  addEventListeners();

  if (hasPendingPrintFileRequest)
    printFile();
}

/**
 * Create the PDF plugin or reload the existing one.
 * @param {string} previewUid Preview unique identifier.
 */
function createPDFPlugin(previewUid) {
  // Enable the print button.
  if (!$('printer-list').disabled)
    $('print-button').disabled = false;

  var pdfViewer = $('pdf-viewer');
  if (pdfViewer) {
    // Need to call this before the reload(), where the plugin resets its
    // internal page count.
    pdfViewer.goToPage('0');

    pdfViewer.reload();
    pdfViewer.grayscale(!isColor());
    return;
  }

  pdfViewer = document.createElement('embed');
  pdfViewer.setAttribute('id', 'pdf-viewer');
  pdfViewer.setAttribute('type', 'application/pdf');
  pdfViewer.setAttribute('src', 'chrome://print/' + previewUid + '/print.pdf');
  $('mainview').appendChild(pdfViewer);
  pdfViewer.onload('onPDFLoad()');
  pdfViewer.removePrintButton();
  pdfViewer.grayscale(true);
}

/**
 * Returns true if a compatible pdf plugin exists, false if it doesn't.
 */
function checkCompatiblePluginExists() {
  var dummyPlugin = $('dummy-viewer')
  return (dummyPlugin.onload &&
          dummyPlugin.goToPage &&
          dummyPlugin.removePrintButton);
}

/**
 * Updates the state of print button depending on the user selection.
 * The button is enabled only when the following conditions are true.
 * 1) The selected page ranges are valid.
 * 2) The number of copies is valid (if applicable).
 */
function updatePrintButtonState() {
  if (getSelectedPrinterName() == PRINT_TO_PDF) {
    $('print-button').disabled = !isSelectedPagesValid();
  } else {
    $('print-button').disabled = (!isNumberOfCopiesValid() ||
                                  !isSelectedPagesValid());
  }
}

window.addEventListener('DOMContentLoaded', onLoad);

/**
 * Listener function that executes whenever a change occurs in the 'copies'
 * field.
 */
function copiesFieldChanged() {
  updateCopiesButtonsState();
  $('collate-option').hidden = getCopies() <= 1;
}

/**
 * Validates the page ranges text and updates the hint accordingly.
 */
function validatePageRangesField() {
  var individualPagesField = $('individual-pages');
  var individualPagesHint = $('individual-pages-hint');

  if (isSelectedPagesValid()) {
    individualPagesField.classList.remove('invalid');
    fadeOutElement(individualPagesHint);
  } else {
    individualPagesField.classList.add('invalid');
    individualPagesHint.classList.remove('suggestion');
    individualPagesHint.innerHTML =
        localStrings.getStringF('pageRangeInstruction',
                                localStrings.getString(
                                    'examplePageRangeText'));
    fadeInElement(individualPagesHint);
  }
}

/**
 * Executes whenever a blur event occurs on the 'individual-pages'
 * field or when the timer expires. It takes care of
 * 1) showing/hiding warnings/suggestions
 * 2) updating print button/summary
 */
function pageRangesFieldChanged() {
  validatePageRangesField();

  resetPageRangeFieldTimer();
  updatePrintButtonState();
  updatePrintSummary();
}

/**
 * Updates the state of the increment/decrement buttons based on the current
 * 'copies' value.
 */
function updateCopiesButtonsState() {
  var copiesField = $('copies');
  if (!isNumberOfCopiesValid()) {
    copiesField.classList.add('invalid');
    $('increment').disabled = false;
    $('decrement').disabled = false;
    fadeInElement($('copies-hint'));
  }
  else {
    copiesField.classList.remove('invalid');
    $('increment').disabled = (getCopies() == copiesField.max) ? true : false;
    $('decrement').disabled = (getCopies() == copiesField.min) ? true : false;
    fadeOutElement($('copies-hint'));
  }
}

/**
 * Updates the print summary based on the currently selected user options.
 *
 */
function updatePrintSummary() {
  var printToPDF = getSelectedPrinterName() == PRINT_TO_PDF;
  var copies = printToPDF ? 1 : getCopies();
  var printSummary = $('print-summary');

  if (!printToPDF && !isNumberOfCopiesValid()) {
    printSummary.innerHTML = localStrings.getString('invalidNumberOfCopies');
    return;
  }

  if (!isSelectedPagesValid()) {
    printSummary.innerHTML = '';
    return;
  }

  var pageSet = getSelectedPagesSet();
  var numOfSheets = pageSet.length;
  var summaryLabel = localStrings.getString('printPreviewSheetsLabelSingular');
  var numOfPagesText = '';
  var pagesLabel = localStrings.getString('printPreviewPageLabelPlural');

  if (printToPDF)
    summaryLabel = localStrings.getString('printPreviewPageLabelSingular');

  if (!printToPDF && isTwoSided())
    numOfSheets = Math.ceil(numOfSheets / 2);
  numOfSheets *= copies;

  if (numOfSheets > 1) {
    if (printToPDF)
      summaryLabel = pagesLabel;
    else
      summaryLabel = localStrings.getString('printPreviewSheetsLabelPlural');
  }

  var html = '';
  if (pageSet.length * copies != numOfSheets) {
    numOfPagesText = pageSet.length * copies;
    html = localStrings.getStringF('printPreviewSummaryFormatLong',
                                   '<b>' + numOfSheets + '</b>',
                                   '<b>' + summaryLabel + '</b>',
                                   numOfPagesText, pagesLabel);
  } else
    html = localStrings.getStringF('printPreviewSummaryFormatShort',
                                   '<b>' + numOfSheets + '</b>',
                                   '<b>' + summaryLabel + '</b>');

  // Removing extra spaces from within the string.
  html = html.replace(/\s{2,}/g, ' ');
  printSummary.innerHTML = html;
}

/**
 * Handles a click event on the two-sided option.
 */
function handleTwoSidedClick() {
  updatePrintSummary();
}

/**
 * Gives focus to the individual pages textfield when 'print-pages' textbox is
 * clicked.
 */
function handleIndividualPagesCheckbox() {
  $('individual-pages').focus();
}

/**
 * When the user switches printing orientation mode the page field selection is
 * reset to "all pages selected". After the change the number of pages will be
 * different and currently selected page numbers might no longer be valid.
 * Even if they are still valid the content of these pages will be different.
 */
function onLayoutModeToggle() {
  // If the chosen layout is same as before, nothing needs to be done.
  if (printSettings.isLandscape == isLandscape())
    return;

  $('individual-pages').classList.remove('invalid');
  setDefaultValuesAndRegeneratePreview();
}

/**
 * Sets the default values and sends a request to regenerate preview data.
 */
function setDefaultValuesAndRegeneratePreview() {
  fadeOutElement($('individual-pages-hint'));
  totalPageCount = undefined;
  previouslySelectedPages.length = 0;
  requestPrintPreview();
}

/**
 * Returns the selected pages in ascending order without any duplicates.
 *
 * @return {Array}
 */
function getSelectedPagesSet() {
  var pageRangeText = $('individual-pages').value;

  if ($('all-pages').checked || pageRangeText.length == 0)
    pageRangeText = '1-' + totalPageCount;

  var pageList = pageRangeTextToPageList(pageRangeText, totalPageCount);
  return pageListToPageSet(pageList);
}

/**
 * Validates the 'individual-pages' text field value.
 *
 * @return {boolean} true if the text is valid.
 */
function isSelectedPagesValid() {
  var pageRangeText = $('individual-pages').value;

  if ($('all-pages').checked || pageRangeText.length == 0)
    return true;

  return isPageRangeTextValid(pageRangeText, totalPageCount);
}

/**
 * Whenever the page range textfield gains focus we add a timer to detect when
 * the user stops typing in order to update the print preview.
 */
function addTimerToPageRangeField() {
  timerId = window.setTimeout(onPageSelectionMayHaveChanged, 1000);
}

/**
 * As the user types in the page range textfield, we need to reset this timer,
 * since the page ranges are still being edited.
 */
function resetPageRangeFieldTimer() {
  clearTimeout(timerId);
  addTimerToPageRangeField();
}

/**
 * When the user stops typing in the page range textfield or clicks on the
 * 'all-pages' checkbox, a new print preview is requested, only if
 * 1) The input is compeletely valid (it can be parsed in its entirety).
 * 2) The newly selected pages differ from the previously selected.
 */
function onPageSelectionMayHaveChanged() {
  if ($('print-pages').checked)
    pageRangesFieldChanged();
  var currentlySelectedPages = getSelectedPagesSet();

  // Toggling between "all pages"/"some pages" radio buttons while having an
  // invalid entry in the page selection textfield still requires updating the
  // print summary and print button.
  if (!isSelectedPagesValid() ||
      areArraysEqual(previouslySelectedPages, currentlySelectedPages)) {
    updatePrintButtonState();
    updatePrintSummary();
    return;
  }

  previouslySelectedPages = currentlySelectedPages;
  requestPrintPreview();
}

/**
 * Executed when the 'increment' or 'decrement' button is clicked.
 */
function onCopiesButtonsClicked(sign) {
  var copiesField = $('copies');
  if (!isNumberOfCopiesValid())
    copiesField.value = 1;
  else {
    var newValue = getCopies() + sign * 1;
    if (newValue < copiesField.min || newValue > copiesField.max)
      return;
    copiesField.value = newValue;
  }
  copiesFieldChanged();
}

/**
 * Class that represents the state of the print settings.
 */
function PrintSettings() {
  this.deviceName = '';
  this.isLandscape = '';
}

/**
 * Takes a snapshot of the print settings.
 */
PrintSettings.prototype.save = function() {
  this.deviceName = getSelectedPrinterName();
  this.isLandscape = isLandscape();
}
