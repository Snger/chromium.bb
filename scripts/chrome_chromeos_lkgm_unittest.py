# -*- coding: utf-8 -*-
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unit tests for the chrome_chromeos_lkgm program."""

from __future__ import print_function

import os

from chromite.lib import builder_status_lib
from chromite.lib import constants
from chromite.lib import cros_build_lib_unittest
from chromite.lib import cros_test_lib
from chromite.lib import osutils
from chromite.lib import partial_mock
from chromite.scripts import chrome_chromeos_lkgm

# pylint: disable=protected-access

class BaseChromeLKGMCommitterTest(cros_test_lib.MockTempDirTestCase):
  """Base class for tests using cros_chromeos_lkgm.ChromeLKGMCommitter."""

  def setUp(self):
    """Common set up method for all tests."""
    self.committer = chrome_chromeos_lkgm.ChromeLKGMCommitter(
        self.tempdir, '1001.0.0', False, 'user@test.org')
    self.lkgm_file = os.path.join(self.tempdir, constants.PATH_TO_CHROME_LKGM)
    self.old_lkgm = None
    self.pass_status = builder_status_lib.BuilderStatus(
        constants.BUILDER_STATUS_PASSED, None)
    self.fail_status = builder_status_lib.BuilderStatus(
        constants.BUILDER_STATUS_FAILED, None)


class ChromeLKGMCommitterTester(cros_build_lib_unittest.RunCommandTestCase,
                                BaseChromeLKGMCommitterTest):
  """Test cros_chromeos_lkgm.Committer."""

  def _createOldLkgm(self, items):  # pylint: disable=unused-argument
    # Write out an old lkgm file as if we got it from a git fetch.
    osutils.SafeMakedirs(os.path.dirname(self.lkgm_file))
    osutils.WriteFile(self.lkgm_file, self.old_lkgm)

  def testCheckoutChromeLKGM(self):
    "Tests that we can read/obtain the old LKGM from mocked out git."
    self.old_lkgm = '1234.0.0'
    self.rc.AddCmdResult(partial_mock.In('clone'), returncode=0,
                         side_effect=self._createOldLkgm)
    self.committer.CheckoutChromeLKGM()
    self.assertTrue(self.committer._old_lkgm, self.old_lkgm)

  def testCommitNewLKGM(self):
    """Tests that we can commit a new LKGM file."""
    osutils.SafeMakedirs(os.path.dirname(self.lkgm_file))
    self.committer = chrome_chromeos_lkgm.ChromeLKGMCommitter(
        self.tempdir, '1002.0.0', False, 'user@test.org')

    self.committer.CommitNewLKGM()

    # Check the file was actually written out correctly.
    self.assertEqual(osutils.ReadFile(self.lkgm_file), self.committer._lkgm)
    self.assertCommandContains(['git', 'commit'])

  def testOlderLKGMFails(self):
    """Tests that trying to update to an older lkgm version fails."""
    self.old_lkgm = '1002.0.0'
    self.rc.AddCmdResult(partial_mock.In('clone'), returncode=0,
                         side_effect=self._createOldLkgm)

    self.committer = chrome_chromeos_lkgm.ChromeLKGMCommitter(
        self.tempdir, '1001.0.0', False, 'user@test.org')
    self.committer.CheckoutChromeLKGM()
    self.assertTrue(self.committer._old_lkgm, self.old_lkgm)

    self.assertRaises(chrome_chromeos_lkgm.LKGMNotValid,
                      self.committer.CommitNewLKGM)

  def testVersionWithChromeBranch(self):
    """Tests passing a version with a chrome branch strips the branch."""
    self.old_lkgm = '1002.0.0'
    self.rc.AddCmdResult(partial_mock.In('clone'), returncode=0,
                         side_effect=self._createOldLkgm)
    self.committer.CheckoutChromeLKGM()
    self.assertTrue(self.committer._old_lkgm, self.old_lkgm)

    self.committer = chrome_chromeos_lkgm.ChromeLKGMCommitter(
        self.tempdir, '1003.0.0-rc2', False, 'user@test.org')

    self.committer.CommitNewLKGM()

    # Check the file was actually written out correctly.
    stripped_lkgm = '1003.0.0'
    self.assertEqual(osutils.ReadFile(self.lkgm_file), stripped_lkgm)
