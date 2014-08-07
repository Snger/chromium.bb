#!/usr/bin/python
# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Test the archive_lib module."""

import logging
import multiprocessing
import os
import sys

sys.path.insert(0, os.path.abspath('%s/../..' % os.path.dirname(__file__)))
from chromite.cbuildbot import metadata_lib
from chromite.lib import cros_test_lib
from chromite.lib import parallel


@cros_test_lib.NetworkTest()
class MetadataFetchTest(cros_test_lib.TestCase):
  """Test functions for fetching metadata from GS."""

  def testPaladinBuilder(self):
    bot, version = ('x86-mario-paladin', '5611.0.0')
    full_version = metadata_lib.FindLatestFullVersion(bot, version)
    self.assertEqual(full_version, 'R35-5611.0.0-rc2')
    metadata = metadata_lib.GetBuildMetadata(bot, full_version)
    metadata_dict = metadata._metadata_dict # pylint: disable=W0212
    self.assertEqual(metadata_dict['status']['status'], 'passed')


class MetadataTest(cros_test_lib.TestCase):
  """Tests the correctness of various metadata methods."""

  def testGetDict(self):
    starting_dict = {'key1': 1,
                     'key2': '2',
                     'cl_actions': [('a', 1), ('b', 2)]}
    metadata = metadata_lib.CBuildbotMetadata(starting_dict)
    ending_dict = metadata.GetDict()
    self.assertEqual(starting_dict, ending_dict)

  def testUpdateKeyDictWithDict(self):
    expected_dict = {str(x): x for x in range(20)}
    m = multiprocessing.Manager()
    metadata = metadata_lib.CBuildbotMetadata(multiprocess_manager=m)

    metadata.UpdateKeyDictWithDict('my_dict', expected_dict)

    self.assertEqual(expected_dict, metadata.GetDict()['my_dict'])


  def testUpdateKeyDictWithDictMultiprocess(self):
    expected_dict = {str(x): x for x in range(20)}
    m = multiprocessing.Manager()
    metadata = metadata_lib.CBuildbotMetadata(multiprocess_manager=m)

    with parallel.BackgroundTaskRunner(metadata.UpdateKeyDictWithDict) as q:
      for k, v in expected_dict.iteritems():
        q.put(['my_dict', {k: v}])

    self.assertEqual(expected_dict, metadata.GetDict()['my_dict'])

  def testMultiprocessSafety(self):
    m = multiprocessing.Manager()
    metadata = metadata_lib.CBuildbotMetadata(multiprocess_manager=m)
    key_dict = {'key1': 1, 'key2': 2}
    starting_dict = {'key1': 1,
                     'key2': '2',
                     'key3': key_dict,
                     'cl_actions': [('a', 1), ('b', 2)]}

    # Test that UpdateWithDict is process-safe
    parallel.RunParallelSteps([lambda: metadata.UpdateWithDict(starting_dict)])
    ending_dict = metadata.GetDict()
    self.assertEqual(starting_dict, ending_dict)

    # Test that UpdateKeyDictWithDict is process-safe
    parallel.RunParallelSteps([lambda: metadata.UpdateKeyDictWithDict(
        'key3', key_dict)])
    ending_dict = metadata.GetDict()
    self.assertEqual(starting_dict, ending_dict)

    # Test that RecordCLAction is process-safe
    fake_change = metadata_lib.GerritPatchTuple(12345, 1, False)
    fake_action = ('asdf,')
    parallel.RunParallelSteps([lambda: metadata.RecordCLAction(fake_change,
                                                               fake_action)])
    ending_dict = metadata.GetDict()
    # Assert that an action was recorded.
    self.assertEqual(len(starting_dict['cl_actions']) + 1,
                     len(ending_dict['cl_actions']))


if __name__ == '__main__':
  cros_test_lib.main(level=logging.DEBUG)
