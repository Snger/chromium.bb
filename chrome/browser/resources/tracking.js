// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var g_browserBridge;
var g_mainView;

/**
 * Main entry point called once the page has loaded.
 */
function onLoad() {
  g_browserBridge = new BrowserBridge();
  g_mainView = new MainView();

  // Ask the browser to send us the current data.
  g_browserBridge.sendGetData();
}

document.addEventListener('DOMContentLoaded', onLoad);

/**
 * This class provides a "bridge" for communicating between the javascript and
 * the browser. Used as a singleton.
 */
var BrowserBridge = (function() {
  'use strict';

  /**
   * @constructor
   */
  function BrowserBridge() {
  }

  BrowserBridge.prototype = {
    //--------------------------------------------------------------------------
    // Messages sent to the browser
    //--------------------------------------------------------------------------

    sendGetData: function() {
      chrome.send('getData');
    },

    sendResetData: function() {
      chrome.send('resetData');
    },

    //--------------------------------------------------------------------------
    // Messages received from the browser.
    //--------------------------------------------------------------------------

    receivedData: function(data) {
      g_mainView.setData(data);
    },
  };

  return BrowserBridge;
})();

/**
 * This class handles the presentation of our tracking view. Used as a
 * singleton.
 */
var MainView = (function() {
  'use strict';

  // --------------------------------------------------------------------------
  // Important IDs in the HTML document
  // --------------------------------------------------------------------------

  // The search box to filter results.
  var FILTER_SEARCH_ID = 'filter-search';

  // The container node to put all the "Group by" dropdowns into.
  var GROUP_BY_CONTAINER_ID = 'group-by-container';

  // The container node to put all the "Sort by" dropdowns into.
  var SORT_BY_CONTAINER_ID = 'sort-by-container';

  // The DIV to put all the tables into.
  var RESULTS_DIV_ID = 'results-div';

  // The container node to put all the column checkboxes into.
  var COLUMN_TOGGLES_CONTAINER_ID = 'column-toggles-container';

  // The anchor node that toggles the filter help.
  var FILTER_HELP_LINK_ID = 'filter-help-link';

  // The container node where the (initially hidden) filter help text is
  // drawn into.
  var FILTER_HELP_CONTAINER_ID = 'filter-help-container';

  // The UL which should be filled with each of the key names. This is
  // used by the filter help text to identify what value property names
  // are.
  var FILTER_HELP_PROPERTY_NAMES_UL = 'filter-help-property-names-ul';

  // The anchor which toggles visibility of column checkboxes.
  var EDIT_COLUMNS_LINK_ID = 'edit-columns-link';

  // The container node to show/hide when toggling the column checkboxes.
  var EDIT_COLUMNS_ROW = 'edit-columns-row';

  // --------------------------------------------------------------------------
  // Row keys
  // --------------------------------------------------------------------------

  // These keys represent the properties in each row of our data. They
  // correspond with the paths inside the raw JSON that was fed to us from the
  // browser. Note that periods can be used to identify sub-properties.

  var KEY_BIRTH_THREAD = 'birth_thread';
  var KEY_DEATH_THREAD = 'death_thread';
  var KEY_FUNCTION_NAME = 'location.function_name';
  var KEY_FILE_NAME = 'location.file_name';
  var KEY_LINE_NUMBER = 'location.line_number';
  var KEY_COUNT = 'death_data.count';
  var KEY_QUEUE_TIME = 'death_data.queue_ms';
  var KEY_RUN_TIME = 'death_data.run_ms';

  // The following are computed properties which we add to each row. They
  // are not present in the original JSON stream.
  var KEY_AVG_QUEUE_TIME = 'avg_queue_ms';
  var KEY_AVG_RUN_TIME = 'avg_run_ms';
  var KEY_SOURCE_LOCATION = 'source_location';

  // --------------------------------------------------------------------------
  // Aggregators
  // --------------------------------------------------------------------------

  // To generalize computing/displaying the aggregate "counts" for each column,
  // we specify an optional "Aggregator" class to use with each property.

  // The following are actually "Aggregator factories". They create an
  // aggregator instance by calling 'create()'. The instance is then fed
  // each row one at a time via the 'consume()' method. After all rows have
  // been consumed, the 'getValueAsText()' method will return the aggregated
  // value.

  /**
   * This aggregator counts the number of unique values that were fed to it.
   */
  var UniquifyAggregator = (function() {
    function Aggregator(key) {
      this.key_ = key;
      this.valuesSet_ = {};
    }

    Aggregator.prototype = {
      consume: function(e) {
        this.valuesSet_[getPropertyByPath(e, this.key_)] = true;
      },

      getValueAsText: function() {
        return getDictionaryKeys(this.valuesSet_).length + ' unique'
      },
    };

    return {
      create: function(key) { return new Aggregator(key); }
    };
  })();

  /**
   * This aggregator sums a numeric field.
   */
  var SumAggregator = (function() {
    function Aggregator(key) {
      this.key_ = key;
      this.sum_ = 0;
    }

    Aggregator.prototype = {
      consume: function(e) {
        this.sum_ += getPropertyByPath(e, this.key_);
      },

      getValueAsText: function() {
        return formatNumberAsText(this.sum_);
      },
    };

    return {
      create: function(key) { return new Aggregator(key); }
    };
  })();

  /**
   * This aggregator computes an average by summing two
   * numeric fields, and then dividing the totals.
   */
  var AvgAggregator = (function() {
    function Aggregator(numeratorKey, divisorKey) {
      this.numeratorKey_ = numeratorKey;
      this.divisorKey_ = divisorKey;

      this.numeratorSum_ = 0;
      this.divisorSum_ = 0;
    }

    Aggregator.prototype = {
      consume: function(e) {
        this.numeratorSum_ += getPropertyByPath(e, this.numeratorKey_);
        this.divisorSum_ += getPropertyByPath(e, this.divisorKey_);
      },

      getValueAsText: function() {
        return formatNumberAsText(this.numeratorSum_ / this.divisorSum_);
      },
    };

    return {
      create: function(numeratorKey, divisorKey) {
        return {
          create: function(key) {
            return new Aggregator(numeratorKey, divisorKey);
          },
        }
      }
    };
  })();

  // --------------------------------------------------------------------------
  // Key properties
  // --------------------------------------------------------------------------

  /**
   * Enumerates information about various keys. Such as whether their data is
   * expected to be numeric or is a string, a descriptive name (title) for the
   * property, and what function should be used to aggregate the property when
   * displayed in a column.
   */
  var KEY_PROPERTIES = {};

  KEY_PROPERTIES[KEY_BIRTH_THREAD] = {
    name: 'Birth thread',
    type: 'string',
    aggregator: UniquifyAggregator,
  };

  KEY_PROPERTIES[KEY_DEATH_THREAD] = {
    name: 'Death thread',
    type: 'string',
    aggregator: UniquifyAggregator,
  };

  KEY_PROPERTIES[KEY_FUNCTION_NAME] = {
    name: 'Function name',
    type: 'string',
    aggregator: UniquifyAggregator,
  };

  KEY_PROPERTIES[KEY_FILE_NAME] = {
    name: 'File name',
    type: 'string',
    aggregator: UniquifyAggregator,
  };

  KEY_PROPERTIES[KEY_LINE_NUMBER] = {
    name: 'Line number',
    type: 'number',
    aggregator: UniquifyAggregator,
  };

  KEY_PROPERTIES[KEY_COUNT] = {
    name: 'Count',
    type: 'number',
    aggregator: SumAggregator,
  };

  KEY_PROPERTIES[KEY_QUEUE_TIME] = {
    name: 'Queue time',
    type: 'number',
    aggregator: SumAggregator,
  };

  KEY_PROPERTIES[KEY_RUN_TIME] = {
    name: 'Run time',
    type: 'number',
    aggregator: SumAggregator,
  };

  KEY_PROPERTIES[KEY_AVG_RUN_TIME] = {
    name: 'Avg run time',
    type: 'number',
    aggregator: AvgAggregator.create(KEY_RUN_TIME, KEY_COUNT),
  };

  KEY_PROPERTIES[KEY_AVG_QUEUE_TIME] = {
    name: 'Avg queue time',
    type: 'number',
    aggregator: AvgAggregator.create(KEY_QUEUE_TIME, KEY_COUNT),
  };

  KEY_PROPERTIES[KEY_SOURCE_LOCATION] = {
    name: 'Source location',
    type: 'string',
    aggregator: UniquifyAggregator,
  };

  /**
   * Returns the string name for |key|.
   */
  function getNameForKey(key) {
    var props = KEY_PROPERTIES[key];
    if (props == undefined)
      throw 'Did not define properties for key: ' + key;
    return props.name;
  }

  /**
   * Ordered list of all keys. This is the order we generally want
   * to display the properties in.
   */
  var ALL_KEYS = [
    KEY_COUNT,
    KEY_RUN_TIME,
    KEY_AVG_RUN_TIME,
    KEY_QUEUE_TIME,
    KEY_AVG_QUEUE_TIME,
    KEY_BIRTH_THREAD,
    KEY_DEATH_THREAD,
    KEY_FUNCTION_NAME,
    KEY_SOURCE_LOCATION,
    KEY_FILE_NAME,
    KEY_LINE_NUMBER,
  ];

  /**
   * List of keys for those properties which we want to initially omit
   * from the table. (They can be re-enabled by clicking [Edit columns]).
   */
  var INITIALLY_HIDDEN_KEYS = [
    KEY_FILE_NAME,
    KEY_LINE_NUMBER,
  ];

  /**
   * The ordered list of grouping choices to expose in the "Group by"
   * dropdowns. We don't include the numeric properties, since they
   * leads to awkward bucketing.
   */
  var GROUPING_DROPDOWN_CHOICES = [
    KEY_BIRTH_THREAD,
    KEY_DEATH_THREAD,
    KEY_FUNCTION_NAME,
    KEY_SOURCE_LOCATION,
    KEY_FILE_NAME,
    KEY_LINE_NUMBER,
  ];

  /**
   * The ordered list of sorting choices to expose in the "Sort by"
   * dropdowns.
   */
  var SORT_DROPDOWN_CHOICES = ALL_KEYS;

  /**
   * The ordered list of all columns that can be displayed in the tables (not
   * including whatever has been hidden via [Edit Columns]).
   */
  var ALL_TABLE_COLUMNS = ALL_KEYS;

  /**
   * The initial keys to sort by when loading the page (can be changed later).
   */
  var INITIAL_SORT_KEYS = ['-' + KEY_AVG_RUN_TIME];

  /**
   * The default sort keys to use when nothing has been specified.
   */
  var DEFAULT_SORT_KEYS = ['-' + KEY_COUNT];

  /**
   * The initial keys to group by when loading the page (can be changed later).
   */
  var INITIAL_GROUP_KEYS = [KEY_DEATH_THREAD];

  /**
   * @constructor
   */
  function MainView() {
    this.init_();
  }

  // --------------------------------------------------------------------------
  // General utility functions
  // --------------------------------------------------------------------------

  /**
   * Returns a list of all the keys in |dict|.
   */
  function getDictionaryKeys(dict) {
    var keys = [];
    for (var key in dict) {
      keys.push(key);
    }
    return keys;
  }

  /**
   * Formats the number |x| as a decimal integer. Strips off any decimal parts,
   * and comma separates the number every 3 characters.
   */
  function formatNumberAsText(x) {
    var orig = x.toFixed(0);

    var parts = [];
    for (var end = orig.length; end > 0; ) {
      var chunk = Math.min(end, 3);
      parts.push(orig.substr(end-chunk, chunk));
      end -= chunk;
    }
    return parts.reverse().join(',');
  }

  /**
   * Simple comparator function which works for both strings and numbers.
   */
  function simpleCompare(a, b) {
    if (a == b)
      return 0;
    if (a < b)
      return -1;
    return 1;
  }

  /**
   * Returns dict[key]. Note that if |key| contains periods (.), they will be
   * interpreted as meaning a sub-property.
   */
  function getPropertyByPath(dict, key) {
    var cur = dict;
    var parts = key.split('.');
    for (var i = 0; i < parts.length; ++i) {
      if (cur == undefined)
        return undefined;
      cur = cur[parts[i]];
    }
    return cur;
  }

  /**
   * Creates and appends a DOM node of type |tagName| to |parent|. Optionally,
   * sets the new node's text to |opt_text|. Returns the newly created node.
   */
  function addNode(parent, tagName, opt_text) {
    var n = parent.ownerDocument.createElement(tagName);
    parent.appendChild(n);
    if (opt_text != undefined) {
      var textNode = parent.ownerDocument.createTextNode(opt_text);
      n.appendChild(textNode);
    }
    return n;
  }

  /**
   * Deletes all the strings in |array| which have a key in |valueSet|.
   */
  function deleteStringsFromArrayMatching(array, valueSet) {
    for (var i = 0; i < array.length; ) {
      if (valueSet[array[i]]) {
        array.splice(i, 1);
      } else {
        i++;
      }
    }
  }

  /**
   * Deletes all the repeated ocurrences of strings in |array|.
   */
  function deleteDuplicateStringsFromArray(array) {
    // Build up set of each entry in array.
    var seenSoFar = {};

    for (var i = 0; i < array.length; ) {
      var value = array[i];
      if (seenSoFar[value]) {
        array.splice(i, 1);
      } else {
        seenSoFar[value] = true;
        i++;
      }
    }
  }

  function trimWhitespace(text) {
    var m = /^\s*(.*)\s*$/.exec(text);
    return m[1];
  }

  /**
   * Selects the option in |select| which has a value of |value|.
   */
  function setSelectedOptionByValue(select, value) {
    for (var i = 0; i < select.options.length; ++i) {
      if (select.options[i].value == value) {
        select.options[i].selected = true;
        return true;
      }
    }
    return false;
  }

  // --------------------------------------------------------------------------
  // Functions that augment, bucket, and compute aggregates for the input data.
  // --------------------------------------------------------------------------

  /**
   * Selects all the data in |rows| which are matched by |filterFunc|, and
   * buckets the results using |entryToGroupKeyFunc|. For each bucket aggregates
   * are computed, and the results are sorted.
   *
   * Returns a dictionary whose keys are the group name, and the value is an
   * objected containing two properties: |rows| and |aggregates|.
   */
  function prepareData(rows, entryToGroupKeyFunc, filterFunc, sortingFunc) {
    var groupedData = {};

    for (var i = 0; i < rows.length; ++i) {
      var e = rows[i];

      if (!filterFunc(e))
        continue;  // Not matched by our filter, discard the row.

      augmentDataRow(e);

      var groupKey = entryToGroupKeyFunc(e);

      var groupData = groupedData[groupKey];
      if (!groupData) {
        groupData = {
          aggregates: initializeAggregates(ALL_KEYS),
          rows: [],
        };
        groupedData[groupKey] = groupData;
      }

      // Add the row to our list.
      groupData.rows.push(e);

      // Update aggregates for each column.
      for (var key in groupData.aggregates)
        groupData.aggregates[key].consume(e);
    }

    // Sort all the data.
    for (var groupKey in groupedData)
      groupedData[groupKey].rows.sort(sortingFunc);

    return groupedData;
  }

  /**
   * Adds new derived properties to row. Mutates the provided dictionary |e|.
   */
  function augmentDataRow(e) {
    e[KEY_AVG_QUEUE_TIME] =
        getPropertyByPath(e, KEY_QUEUE_TIME) / getPropertyByPath(e, KEY_COUNT);
    e[KEY_AVG_RUN_TIME] =
        getPropertyByPath(e, KEY_RUN_TIME) / getPropertyByPath(e, KEY_COUNT);
    e[KEY_SOURCE_LOCATION] =
        getPropertyByPath(e, KEY_FILE_NAME) + ' [' +
        getPropertyByPath(e, KEY_LINE_NUMBER) + ']';
  }

  /**
   * Creates and initializes an aggregator object for each key in |columns|.
   * Returns a dictionary whose keys are values from |columns|, and whose
   * values are Aggregator instances.
   */
  function initializeAggregates(columns) {
    var aggregates = {};

    for (var i = 0; i < columns.length; ++i) {
      var key = columns[i];
      var aggregatorFactory = KEY_PROPERTIES[key].aggregator;
      if (aggregatorFactory)
        aggregates[key] = aggregatorFactory.create(key);
    }

    return aggregates;
  }

  // --------------------------------------------------------------------------
  // HTML drawing code
  // --------------------------------------------------------------------------

  /**
   * Draws a title into |parent| that describes |groupKey|.
   */
  function drawGroupTitle(parent, groupKey) {
    if (groupKey.length  == 0) {
      // Empty group key means there was no grouping.
      return;
    }

    var parent = addNode(parent, 'div');
    parent.className = 'group-title-container';

    // Each component of the group key represents the "key=value" constraint for
    // this group. Show these as an AND separated list.
    for (var i = 0; i < groupKey.length; ++i) {
      if (i > 0)
        addNode(parent, 'i', ' and ');
      var e = groupKey[i];
      addNode(parent, 'b', getNameForKey(e.key) + ' = ');
      addNode(parent, 'span', e.value);
    }
  }

  /**
   * Renders the information for a particular group.
   */
  function drawGroup(parent, groupKey, groupData, columns) {
    var div = addNode(parent, 'div');
    div.className = 'group-container';

    drawGroupTitle(div, groupKey);

    var table = addNode(div, 'table');

    drawDataTable(table, groupData, columns);
  }

  /**
   * Renders a row that describes all the aggregate values for |columns|.
   */
  function drawAggregateRow(tbody, aggregates, columns) {
    var tr = addNode(tbody, 'tr');
    tr.className = 'aggregator-row';

    for (var i = 0; i < columns.length; ++i) {
      var key = columns[i];
      var td = addNode(tr, 'td');

      // Most of our outputs are numeric, so we want to align them to the right.
      // However for the  unique counts we will center.
      if (KEY_PROPERTIES[key].aggregator == UniquifyAggregator) {
        td.align = 'center';
      } else {
        td.align = 'right';
      }

      var aggregator = aggregates[key];
      if (aggregator)
        td.innerText = aggregator.getValueAsText();
    }
  }

  /**
   * Renders a table which summarizes all |column| fields for |data|.
   */
  function drawDataTable(table, data, columns) {
    table.className = 'results-table';
    var thead = addNode(table, 'thead');
    var tbody = addNode(table, 'tbody');

    drawTableHeader(thead, columns);
    drawAggregateRow(thead, data.aggregates, columns);
    drawTableBody(tbody, data.rows, columns);
  }

  function drawTableHeader(thead, columns) {
    var tr = addNode(thead, 'tr');
    for (var i = 0; i < columns.length; ++i) {
      var key = columns[i];
      addNode(tr, 'th', getNameForKey(key));
    }
  }

  function drawTableBody(tbody, rows, columns) {
    for (var i = 0; i < rows.length; ++i) {
      var e = rows[i];

      var tr = addNode(tbody, 'tr');

      for (var c = 0; c < columns.length; ++c) {
        var key = columns[c];
        var value = getPropertyByPath(e, key);


        var isNumeric = typeof value == 'number';

        if (isNumeric) {
          value = formatNumberAsText(value);
        }

        var td = addNode(tr, 'td', value);

        if (isNumeric) {
          td.align = 'right';
        }
      }
    }
  }

  // --------------------------------------------------------------------------
  // Helper code for handling the sort and grouping dropdowns.
  // --------------------------------------------------------------------------

  function addOptionsForGroupingSelect(select) {
    // Add "no group" choice.
    addNode(select, 'option', '---').value = '';

    for (var i = 0; i < GROUPING_DROPDOWN_CHOICES.length; ++i) {
      var key = GROUPING_DROPDOWN_CHOICES[i];
      var option = addNode(select, 'option', getNameForKey(key));
      option.value = key;
    }
  }

  function addOptionsForSortingSelect(select) {
    // Add "no sort" choice.
    addNode(select, 'option', '---').value = '';

    // Add a divider.
    addNode(select, 'optgroup').label = '';

    for (var i = 0; i < SORT_DROPDOWN_CHOICES.length; ++i) {
      var key = SORT_DROPDOWN_CHOICES[i];
      addNode(select, 'option', getNameForKey(key)).value = key;
    }

    // Add a divider.
    addNode(select, 'optgroup').label = '';

    // Add the same options, but for descending.
    for (var i = 0; i < SORT_DROPDOWN_CHOICES.length; ++i) {
      var key = SORT_DROPDOWN_CHOICES[i];
      var n = addNode(select, 'option', getNameForKey(key) + ' (DESC)');
      n.value = '-' + key;
    }
  }

  /**
   * Helper function used to update the sorting and grouping lists after a
   * dropdown changes.
   */
  function updateKeyListFromDropdown(list, i, select) {
    // Update the list.
    if (i < list.length) {
      list[i] = select.value;
    } else {
      list.push(select.value);
    }

    // Normalize the list, so setting 'none' as primary zeros out everything
    // else.
    for (var i = 0; i < list.length; ++i) {
      if (list[i] == '') {
        list.splice(i, list.length - i);
        break;
      }
    }
  }

  // --------------------------------------------------------------------------

  MainView.prototype = {
    setData: function(data) {
      this.allData_ = data;

      // Figure out what columns to include, based on the selected checkboxes.
      var columns = [];
      for (var i = 0; i < ALL_TABLE_COLUMNS.length; ++i) {
        var key = ALL_TABLE_COLUMNS[i];
        if (this.selectionCheckboxes_[key].checked) {
          columns.push(key);
        }
      }

      // Group, aggregate, filter, and sort the data.
      var groupedData = prepareData(
          data.list, this.getGroupingFunction_(), this.getFilterFunction_(),
          this.getSortingFunction_());

      // Figure out a display order for the groups.
      var groupKeys = getDictionaryKeys(groupedData);
      groupKeys.sort();

      // Clear the results div, sine we may be overwriting older data.
      var parent = $(RESULTS_DIV_ID);
      parent.innerHTML = '';

      if (groupKeys.length > 0) {
        // The grouping will be the the same for each so just pick the first.
        var randomGroupKey = JSON.parse(groupKeys[0]);

        // The grouped properties are going to be the same for each row in our,
        // table, so avoid drawing them in our table!
        var keysToExcludeSet = {};

        for (var i = 0; i < randomGroupKey.length; ++i)
          keysToExcludeSet[randomGroupKey[i].key] = true;
        columns = columns.slice(0);
        deleteStringsFromArrayMatching(columns, keysToExcludeSet);
      }

      // Draw each group.
      for (var i = 0; i < groupKeys.length; ++i) {
        var groupKeyString = groupKeys[i];
        var groupData = groupedData[groupKeyString];
        var groupKey = JSON.parse(groupKeyString);

        drawGroup(parent, groupKey, groupData, columns);
      }
    },

    init_: function() {
      this.fillSelectionCheckboxes_($(COLUMN_TOGGLES_CONTAINER_ID));

      $(FILTER_SEARCH_ID).onsearch = this.onChangedFilter_.bind(this);

      this.currentSortKeys_ = INITIAL_SORT_KEYS.slice(0);
      this.currentGroupingKeys_ = INITIAL_GROUP_KEYS.slice(0);

      this.fillGroupingDropdowns_();
      this.fillSortingDropdowns_();

      $(EDIT_COLUMNS_LINK_ID).onclick = this.toggleEditColumns_.bind(this);

      // Hook up the filter help section.
      $(FILTER_HELP_LINK_ID).onclick = this.toggleFilterHelp_.bind(this);
      var propsUl = $(FILTER_HELP_PROPERTY_NAMES_UL);
      propsUl = $(FILTER_HELP_PROPERTY_NAMES_UL);
      for (var i = 0; i < ALL_KEYS.length; ++i)
        addNode(propsUl, 'li', ALL_KEYS[i]);
    },

    // TODO(eroman): This is basically the same as toggleFilterHelp();
    //               extract to helper.
    toggleEditColumns_: function() {
      var n = $(EDIT_COLUMNS_ROW);
      if (n.style.display == '') {
        n.style.display = 'none';
      } else {
        n.style.display = '';
      }
    },

    toggleFilterHelp_: function() {
      var n = $(FILTER_HELP_CONTAINER_ID);
      if (n.style.display == '') {
        n.style.display = 'none';
      } else {
        n.style.display = '';
      }
    },

    fillSelectionCheckboxes_: function(parent) {
      this.selectionCheckboxes_ = {};

      for (var i = 0; i < ALL_TABLE_COLUMNS.length; ++i) {
        var key = ALL_TABLE_COLUMNS[i];
        var checkbox = addNode(parent, 'input');
        checkbox.type = 'checkbox';
        checkbox.onchange = this.onSelectCheckboxChanged_.bind(this);
        checkbox.checked = true;
        addNode(parent, 'span', getNameForKey(key) + ' ');
        this.selectionCheckboxes_[key] = checkbox;
      }

      for (var i = 0; i < INITIALLY_HIDDEN_KEYS.length; ++i) {
        this.selectionCheckboxes_[INITIALLY_HIDDEN_KEYS[i]].checked = false;
      }
    },

    fillGroupingDropdowns_: function() {
      var parent = $(GROUP_BY_CONTAINER_ID);
      parent.innerHTML = '';

      for (var i = 0; i <= this.currentGroupingKeys_.length; ++i) {
        // Add a dropdown.
        var select = addNode(parent, 'select');
        select.onchange = this.onChangedGrouping_.bind(this, select, i);

        addOptionsForGroupingSelect(select);

        if (i < this.currentGroupingKeys_.length) {
          var key = this.currentGroupingKeys_[i];
          setSelectedOptionByValue(select, key);
        }
      }
    },

    fillSortingDropdowns_: function() {
      var parent = $(SORT_BY_CONTAINER_ID);
      parent.innerHTML = '';

      for (var i = 0; i <= this.currentSortKeys_.length; ++i) {
        // Add a dropdown.
        var select = addNode(parent, 'select');
        select.onchange = this.onChangedSorting_.bind(this, select, i);

        addOptionsForSortingSelect(select);

        if (i < this.currentSortKeys_.length) {
          var key = this.currentSortKeys_[i];
          setSelectedOptionByValue(select, key);
        }
      }
    },

    onChangedGrouping_: function(select, i) {
      updateKeyListFromDropdown(this.currentGroupingKeys_, i, select);
      this.fillGroupingDropdowns_();
      this.setData(this.allData_);
    },

    onChangedSorting_: function(select, i) {
      updateKeyListFromDropdown(this.currentSortKeys_, i, select);
      this.fillSortingDropdowns_();
      this.setData(this.allData_);
    },

    onSelectCheckboxChanged_: function() {
      this.setData(this.allData_);
    },

    onChangedFilter_: function() {
      this.setData(this.allData_);
    },

    getSortingFunction_: function() {
      var sortKeys = this.currentSortKeys_.slice(0);

      // Eliminate the empty string keys (which means they were unspecified).
      deleteStringsFromArrayMatching(sortKeys, {'': true});

      // If no sort is specified, use our default sort.
      if (sortKeys.length == 0)
        sortKeys = [DEFAULT_SORT_KEYS];

      // Note that the keys in the sort array may be prefixed with a '-' meaning
      // to inverse the sort order. Parse this out and make sortKeys into an
      // array of objects instead of strings.
      for (var i = 0; i < sortKeys.length; ++i) {
        var m = /^(-?)(.*)$/.exec(sortKeys[i]);
        sortKeys[i] = {
          factor: m[1] == '-' ? -1 : 1,
          key: m[2],
        };
      }

      return function(a, b) {
        for (var i = 0; i < sortKeys.length; ++i) {
          var key = sortKeys[i].key;
          var factor = sortKeys[i].factor;

          var propA = getPropertyByPath(a, key);
          var propB = getPropertyByPath(b, key);

          var comparison = simpleCompare(propA, propB) * factor;
          if (comparison != 0)
            return comparison;
        }

        // Tie breaker.
        return simpleCompare(JSON.stringify(a), JSON.stringify(b));
      };
    },

    getFilterFunction_: function() {
      var text = $(FILTER_SEARCH_ID).value;
      text = trimWhitespace(text);

      return function(x) {
        if (text == '')
          return true;

        if (text.indexOf('x.') != -1) {
          // To be a javascript expression, it must have be testing something on
          // |x|...
          try {
            var result = eval(text);
            if (typeof result == 'boolean')
              return result;
          } catch (e) {
            // Do nothing, we will fall back to substring search.
          }
        }

        // Interpret the query as a substring search (case insensitive)
        // TODO(eromn): Don't search through the JSON.
        return JSON.stringify(x).toLowerCase().indexOf(
            text.toLowerCase()) != -1;
      };
    },

    getGroupingFunction_: function() {
      var groupings = this.currentGroupingKeys_.slice(0);

      // Eliminate the empty string groupings (which means they were
      // unspecified).
      deleteStringsFromArrayMatching(groupings, {'': true});

      // Eliminate duplicate primary/secondary group by directives, since they
      // are redundant.
      deleteDuplicateStringsFromArray(groupings);

      return function(e) {
        var groupKey = [];

        for (var i = 0; i < groupings.length; ++i) {
          var entry = {key: groupings[i],
                       value: getPropertyByPath(e, groupings[i])};
          groupKey.push(entry);
        }

        return JSON.stringify(groupKey);
      };
    },
  };

  return MainView;
})();
