# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import pyauto_functional  # must be imported before pyauto
import pyauto
import domselector

class PyAutoEventsTest(pyauto.PyUITest):
  """Tests using the event queue."""

  def testBasicEvents(self):
    """Basic test for the event queue."""
    url = self.GetHttpURLForDataPath('apptest', 'basic.html')
    driver = self.NewWebDriver()
    event_id = self.AddDomEventObserver(automation_id=4444, recurring=True)
    success_id = self.AddDomEventObserver('test success', automation_id=4444)
    self.NavigateToURL(url)
    self._ExpectEvent(event_id, 'init')
    self._ExpectEvent(event_id, 'login ready')
    driver.find_element_by_id('login').click()
    self._ExpectEvent(event_id, 'login start')
    self._ExpectEvent(event_id, 'login done')
    self.GetNextEvent(success_id)

  def testDomMutationEvents(self):
    """Basic tests for WaitForDomNode."""
    url = self.GetHttpURLForDataPath('apptest', 'dom_mutations.html')
    self.NavigateToURL(url)
    self.WaitForDomNode(domselector.CSSSelector('#login'), 'Log In')
    self.NewWebDriver().find_element_by_id('login').click()
    self.WaitForDomNode(domselector.XPath('id(\'console\')'), '.*succeeded.*')

  def testDomMutationObservers(self):
    """Tests for the various types of Dom Mutation observers."""
    url = self.GetHttpURLForDataPath('apptest', 'dom_mutations.html')
    self.NavigateToURL(url)
    self.GetNextEvent(self.AddDomMutationObserver(
        'exists', domselector.XPath('/html/body')))
    self.GetNextEvent(self.AddDomMutationObserver(
        'add', domselector.CSSSelector('#login'), expected_value='Log In'))
    success_id = self.AddDomMutationObserver(
        'change', domselector.XPath('id(\'console\')'),
        expected_value='.*succeeded.*')
    self.NewWebDriver().find_element_by_id('login').click()
    self.GetNextEvent(self.AddDomMutationObserver(
        'remove', domselector.XPath('id(\'fail\')/a')))
    self.GetNextEvent(success_id)

  def _ExpectEvent(self, event_id, expected_event_name):
    """Checks that the next event is expected."""
    e = self.GetNextEvent(event_id)
    self.assertEqual(e.get('name'), expected_event_name,
                     msg="unexpected event: %s" % e)


if __name__ == '__main__':
  pyauto_functional.Main()
