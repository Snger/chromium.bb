#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_nacl  # Must be imported before pyauto
import pyauto
import nacl_utils


class NaClTest(pyauto.PyUITest):
  """Tests for NaCl."""

  # Set of nexes that are loaded by pyauto tests.
  num_nexes = 5
  nexes = ['basic_object.html',
           'ppapi_core.html',
           'ppapi_file_system.html',
           'ppapi_geturl.html',
           # TODO(rsimha): Uncomment after bug 1733 is fixed.
           # http://code.google.com/p/nativeclient/issues/detail?id=1733
           # 'ppapi_progress_events.html'
           'ppapi_test_example.html']

  def testLoadNexesInMultipleTabs(self):
    """Load nexes in multiple tabs and surf away from all of them."""

    # Prime each tab by navigating to about:version.
    num_tabs = 10
    self.NavigateToURL('about:version')
    original_title = self.GetActiveTabTitle()
    for i in range(1, num_tabs):
      self.AppendTab(pyauto.GURL('about:version'))

    # Pick a nexe for each tab and navigate to it.
    for i in range(0, num_tabs):
      self.GetBrowserWindow(0).GetTab(i).NavigateToURL(pyauto.GURL(
          self.GetHttpURLForDataPath(NaClTest.nexes[i % NaClTest.num_nexes])))

    # Wait for all the nexes to fully load.
    for i in range(0, num_tabs):
      nacl_utils.WaitForNexeLoad(self, tab_index=i)

    # Make sure every nexe was successfully loaded.
    for i in range(0, num_tabs):
      nacl_utils.VerifyAllTestsPassed(self, tab_index=i)

    # Surf away from each nexe and verify that the tab didn't crash.
    for i in range(0, num_tabs):
      self.GetBrowserWindow(0).GetTab(i).GoBack()
      self.assertEqual(original_title, self.GetActiveTabTitle())

  def testLoadMultipleNexesInOneTab(self):
    """Load multiple nexes in one tab and load them one after another."""

    # Prime a tab by navigating to about:version.
    self.NavigateToURL('about:version')
    original_title = self.GetActiveTabTitle()

    # Navigate to a nexe and make sure it loads. Repeate for all nexes.
    for i in range(0, NaClTest.num_nexes):
      self.NavigateToURL(self.GetHttpURLForDataPath(NaClTest.nexes[i]))
      nacl_utils.WaitForNexeLoad(self)
      nacl_utils.VerifyAllTestsPassed(self)

    # Keep hitting the back button and make sure all the nexes load.
    for i in range(0, NaClTest.num_nexes - 1):
      self.GetBrowserWindow(0).GetTab(0).GoBack()
      nacl_utils.WaitForNexeLoad(self)
      nacl_utils.VerifyAllTestsPassed(self)

    # Go back one last time and make sure we ended up where we started.
    self.GetBrowserWindow(0).GetTab(0).GoBack()
    self.assertEqual(original_title, self.GetActiveTabTitle())


if __name__ == '__main__':
  pyauto_nacl.Main()
