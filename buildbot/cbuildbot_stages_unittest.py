#!/usr/bin/python

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Unittests for build stages."""

import json
import mox
import os
import shutil
import StringIO
import sys
import tempfile

import constants
sys.path.insert(0, constants.SOURCE_ROOT)
from chromite.buildbot import builderstage as bs
from chromite.buildbot import cbuildbot_config as config
from chromite.buildbot import cbuildbot_commands as commands
from chromite.buildbot import cbuildbot_results as results_lib
from chromite.buildbot import cbuildbot_stages as stages
from chromite.buildbot import lkgm_manager
from chromite.buildbot import manifest_version
from chromite.buildbot import repository
from chromite.buildbot import portage_utilities
from chromite.lib import cros_build_lib
from chromite.lib import cros_test_lib
from chromite.lib import gs_unittest
from chromite.lib import osutils
from chromite.lib import parallel
from chromite.lib import parallel_unittest
from chromite.lib import partial_mock
from chromite.scripts import cbuildbot

# TODO(build): Finish test wrapper (http://crosbug.com/37517).
# Until then, this has to be after the chromite imports.
import mock


# pylint: disable=E1111,E1120,W0212,R0904
class AbstractStageTest(cros_test_lib.MoxTempDirTestCase):
  """Base class for tests that test a particular build stage.

  Abstract base class that sets up the build config and options with some
  default values for testing BuilderStage and its derivatives.
  """

  TARGET_MANIFEST_BRANCH = 'ooga_booga'
  BUILDROOT = 'buildroot'

  def ConstructStage(self):
    """Returns an instance of the stage to be tested.
    Implement in subclasses.
    """
    raise NotImplementedError(self, "ConstructStage")

  def setUp(self):
    # Always stub RunCommmand out as we use it in every method.
    self.bot_id = 'x86-generic-paladin'
    self.build_config = config.config[self.bot_id].copy()
    self.build_root = os.path.join(self.tempdir, self.BUILDROOT)
    self._boards = self.build_config['boards']
    self._current_board = self._boards[0]

    self.url = 'fake_url'
    self.build_config['manifest_repo_url'] = self.url

    # Use the cbuildbot parser to create properties and populate default values.
    parser = cbuildbot._CreateParser()
    (self.options, _) = parser.parse_args(
        ['-r', self.build_root, '--buildbot', '--noprebuilts',
          '--buildnumber', '1234'])

    self.assertEquals(self.options.buildroot, self.build_root)
    self.assertTrue(self.options.buildbot)
    self.assertFalse(self.options.debug)
    self.assertFalse(self.options.prebuilts)
    self.assertFalse(self.options.clobber)
    self.assertEquals(self.options.buildnumber, 1234)

    bs.BuilderStage.SetManifestBranch(self.TARGET_MANIFEST_BRANCH)
    portage_utilities._OVERLAY_LIST_CMD = '/bin/true'

  def RunStage(self):
    """Creates and runs an instance of the stage to be tested.
    Requires ConstructStage() to be implemented.

    Raises:
      NotImplementedError: ConstructStage() was not implemented.
    """

    # Stage construction is usually done as late as possible because the tests
    # set up the build configuration and options used in constructing the stage.
    results_lib.Results.Clear()
    stage = self.ConstructStage()
    stage.Run()
    self.assertTrue(results_lib.Results.BuildSucceededSoFar())



class BuilderStageTest(AbstractStageTest):

  def ConstructStage(self):
    return bs.BuilderStage(self.options, self.build_config)

  def testGetPortageEnvVar(self):
    """Basic test case for _GetPortageEnvVar function."""
    self.mox.StubOutWithMock(cros_build_lib, 'RunCommand')
    envvar = 'EXAMPLE'
    obj = cros_test_lib.EasyAttr(output='RESULT\n')
    cros_build_lib.RunCommand(mox.And(mox.IsA(list), mox.In(envvar)),
                              cwd='%s/src/scripts' % self.build_root,
                              redirect_stdout=True, enter_chroot=True,
                              error_code_ok=True).AndReturn(obj)
    self.mox.ReplayAll()
    stage = self.ConstructStage()
    board = self._current_board
    result = stage._GetPortageEnvVar(envvar, board)
    self.mox.VerifyAll()
    self.assertEqual(result, 'RESULT')


class ManifestVersionedSyncStageTest(AbstractStageTest,
                                     cros_test_lib.TempDirTestCase):
  """Tests the two (heavily related) stages ManifestVersionedSync, and
     ManifestVersionedSyncCompleted.
  """
  # pylint: disable=W0223

  def setUp(self):
    self.source_repo = 'ssh://source/repo'
    self.manifest_version_url = 'fake manifest url'
    self.branch = 'master'
    self.build_name = 'x86-generic'
    self.incr_type = 'branch'

    self.build_config['manifest_version'] = self.manifest_version_url
    self.next_version = 'next_version'

    repo = repository.RepoRepository(
      self.source_repo, self.tempdir, self.branch)
    self.manager = manifest_version.BuildSpecsManager(
      repo, self.manifest_version_url, self.build_name, self.incr_type,
      force=False, branch=self.branch, dry_run=True)

    stages.ManifestVersionedSyncStage.manifest_manager = self.manager

  def testManifestVersionedSyncOnePartBranch(self):
    """Tests basic ManifestVersionedSyncStage with branch ooga_booga"""
    self.mox.StubOutWithMock(stages.ManifestVersionedSyncStage,
                             'Initialize')
    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager,
                             'GetNextBuildSpec')
    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager,
                             'GetLatestPassingSpec')
    self.mox.StubOutWithMock(stages.SyncStage, 'ManifestCheckout')

    stages.ManifestVersionedSyncStage.Initialize()
    self.manager.GetNextBuildSpec().AndReturn(self.next_version)
    self.manager.GetLatestPassingSpec().AndReturn(None)

    stages.SyncStage.ManifestCheckout(self.next_version)

    self.mox.ReplayAll()
    stage = stages.ManifestVersionedSyncStage(self.options, self.build_config)
    stage.Run()
    self.mox.VerifyAll()

  def testManifestVersionedSyncCompletedSuccess(self):
    """Tests basic ManifestVersionedSyncStageCompleted on success"""

    stages.ManifestVersionedSyncStage.manifest_manager = self.manager

    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager, 'UpdateStatus')

    self.manager.UpdateStatus(message=None, success=True)

    self.mox.ReplayAll()
    stage = stages.ManifestVersionedSyncCompletionStage(self.options,
                                                        self.build_config,
                                                        success=True)
    stage.Run()
    self.mox.VerifyAll()

  def testManifestVersionedSyncCompletedFailure(self):
    """Tests basic ManifestVersionedSyncStageCompleted on failure"""

    stages.ManifestVersionedSyncStage.manifest_manager = self.manager

    self.mox.StubOutWithMock(manifest_version.BuildSpecsManager, 'UpdateStatus')

    self.manager.UpdateStatus(message=None, success=False)


    self.mox.ReplayAll()
    stage = stages.ManifestVersionedSyncCompletionStage(self.options,
                                                        self.build_config,
                                                        success=False)
    stage.Run()
    self.mox.VerifyAll()

  def testManifestVersionedSyncCompletedIncomplete(self):
    """Tests basic ManifestVersionedSyncStageCompleted on incomplete build."""

    stages.ManifestVersionedSyncStage.manifest_manager = None

    self.mox.ReplayAll()
    stage = stages.ManifestVersionedSyncCompletionStage(self.options,
                                                        self.build_config,
                                                        success=False)
    stage.Run()
    self.mox.VerifyAll()


class LKGMCandidateSyncCompletionStage(AbstractStageTest,
                                       cros_test_lib.TempDirTestCase):
  """Tests the two (heavily related) stages ManifestVersionedSync, and
     ManifestVersionedSyncCompleted.
  """

  def setUp(self):
    self.source_repo = 'ssh://source/repo'
    self.manifest_version_url = 'fake manifest url'
    self.branch = 'master'
    self.build_name = 'x86-generic-paladin'
    self.build_type = constants.PFQ_TYPE

    self.build_config['manifest_version'] = True
    self.build_config['build_type'] = self.build_type
    self.build_config['master'] = True

    repo = repository.RepoRepository(
      self.source_repo, self.tempdir, self.branch)
    self.manager = lkgm_manager.LKGMManager(
      repo, self.manifest_version_url, self.build_name, self.build_type,
      incr_type='branch', force=False, branch=self.branch, dry_run=True)
    self.sub_manager = lkgm_manager.LKGMManager(
      repo, self.manifest_version_url, self.build_name, self.build_type,
      incr_type='branch', force=False, branch=self.branch, dry_run=True)

    stages.ManifestVersionedSyncStage.manifest_manager = self.manager
    stages.LKGMCandidateSyncStage.sub_manager = self.manager


  def ConstructStage(self):
    return stages.LKGMCandidateSyncCompletionStage(self.options,
                                                   self.build_config,
                                                   success=True)

  def _GetTestConfig(self):
    test_config = {}
    test_config['test1'] = {
        'manifest_version': True,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'public',
        'important': True,
        'chrome_rev': None,
        'unified_manifest_version': False,
        'branch': False,
        'internal': False,
    }
    test_config['test2'] = {
        'manifest_version': True,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'public',
        'important': True,
        'chrome_rev': None,
        'unified_manifest_version': False,
        'branch': False,
        'internal': False,
    }
    test_config['test3'] = {
        'manifest_version': True,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'public',
        'important': False,
        'chrome_rev': None,
        'unified_manifest_version': False,
        'branch': False,
        'internal': False,
    }
    test_config['test4'] = {
        'manifest_version': False,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'public',
        'important': True,
        'chrome_rev': None,
        'unified_manifest_version': False,
        'branch': False,
        'internal': False,
    }
    test_config['test5'] = {
        'manifest_version': True,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'both',
        'important': True,
        'chrome_rev': None,
        'unified_manifest_version': True,
        'branch': False,
        'internal': True,
    }
    test_config['test6'] = {
        'manifest_version': True,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'both',
        'important': True,
        'chrome_rev': None,
        'unified_manifest_version': True,
        'branch': True,
        'internal': True,
    }
    test_config['test7'] = {
        'manifest_version': True,
        'build_type': constants.PFQ_TYPE,
        'overlays': 'public',
        'important': True,
        'chrome_rev': None,
        'unified_manifest_version': True,
        'branch': False,
        'internal': False,
    }
    return test_config

  def testGetUnifiedSlavesStatus(self):
    """Tests that we get the statuses for a fake unified master completion."""
    self.mox.StubOutWithMock(bs.BuilderStage, '_GetSlavesForUnifiedMaster')
    self.mox.StubOutWithMock(lkgm_manager.LKGMManager, 'GetBuildersStatus')
    self.build_config['unified_manifest_version'] = True
    p_bs = ['s1', 's2']
    pr_bs = ['s3']
    status1 = dict(s1='pass', s2='fail')
    status2 = dict(s3='pass')

    bs.BuilderStage._GetSlavesForUnifiedMaster().AndReturn((p_bs, pr_bs,))
    lkgm_manager.LKGMManager.GetBuildersStatus(p_bs).AndReturn(status1)
    lkgm_manager.LKGMManager.GetBuildersStatus(pr_bs).AndReturn(status2)

    self.mox.ReplayAll()
    stage = self.ConstructStage()
    statuses = stage._GetSlavesStatus()
    self.mox.VerifyAll()

    for k, v in status1.iteritems():
      self.assertTrue(v, statuses.get(k))

    for k, v in status2.iteritems():
      self.assertTrue(v, statuses.get(k))


  def testGetSlavesForMaster(self):
    """Tests that we get the slaves for a fake master configuration."""
    test_config = self._GetTestConfig()
    self.build_config['unified_manifest_version'] = False
    self.mox.ReplayAll()
    stage = self.ConstructStage()
    important_configs = stage._GetSlavesForMaster(test_config)
    self.mox.VerifyAll()

    self.assertTrue('test1' in important_configs)
    self.assertTrue('test2' in important_configs)
    self.assertFalse('test3' in important_configs)
    self.assertFalse('test4' in important_configs)
    self.assertFalse('test5' in important_configs)
    self.assertFalse('test6' in important_configs)

  def testGetSlavesForUnifiedMaster(self):
    """Tests that we get the slaves for a fake unified master configuration."""
    test_config = self._GetTestConfig()
    self.build_config['unified_manifest_version'] = True
    self.mox.ReplayAll()
    stage = self.ConstructStage()
    p, pr = stage._GetSlavesForUnifiedMaster(test_config)
    self.mox.VerifyAll()

    important_configs = p + pr
    self.assertTrue('test7' in p)
    self.assertTrue('test5' in pr)
    self.assertFalse('test1' in important_configs)
    self.assertFalse('test2' in important_configs)
    self.assertFalse('test3' in important_configs)
    self.assertFalse('test4' in important_configs)
    self.assertFalse('test6' in important_configs)


class BuildBoardTest(AbstractStageTest):

  def setUp(self):
    self.mox.StubOutWithMock(os.path, 'isdir')

  def ConstructStage(self):
    return stages.BuildBoardStage(self.options, self.build_config)

  def testFullBuild(self):
    """Tests whether we correctly run make chroot and setup board for a full."""
    self.bot_id = 'x86-generic-full'
    self.build_config = config.config[self.bot_id]
    self.mox.StubOutWithMock(commands, 'MakeChroot')
    self.mox.StubOutWithMock(commands, 'SetupBoard')

    os.path.isdir(os.path.join(self.build_root, 'chroot')).AndReturn(True)
    commands.MakeChroot(buildroot=self.build_root,
                        replace=True,
                        use_sdk=True,
                        chrome_root=None,
                        extra_env=mox.IgnoreArg()
                        )
    os.path.isdir(os.path.join(self.build_root, 'chroot', 'build',
                               self._current_board)).AndReturn(False)
    commands.SetupBoard(self.build_root,
                        board=self._current_board,
                        usepkg=False,
                        latest_toolchain=False,
                        extra_env={},
                        profile=None,
                        chroot_upgrade=False)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testFullBuildWithProfile(self):
    """Tests whether full builds add profile flag when requested."""
    self.bot_id = 'x86-generic-full'
    self.build_config = config.config[self.bot_id]
    self.mox.StubOutWithMock(commands, 'MakeChroot')
    self.mox.StubOutWithMock(commands, 'SetupBoard')
    self.mox.StubOutWithMock(commands, 'RunChrootUpgradeHooks')

    os.path.isdir(os.path.join(self.build_root, 'chroot')).AndReturn(True)
    commands.MakeChroot(buildroot=self.build_root,
                        replace=True,
                        use_sdk=True,
                        chrome_root=None,
                        extra_env=mox.IgnoreArg()
                        )
    os.path.isdir(os.path.join(self.build_root, 'chroot', 'build',
                               self._current_board)).AndReturn(False)
    commands.SetupBoard(self.build_root,
                        board=self._current_board,
                        usepkg=False,
                        latest_toolchain=False,
                        extra_env={},
                        profile=self.build_config['profile'],
                        chroot_upgrade=False)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testFullBuildWithOverriddenProfile(self):
    """Tests whether full builds add overridden profile flag when requested."""
    self.bot_id = 'x86-generic-full'
    self.options.profile = 'smock'
    self.build_config = config.config[self.bot_id]
    self.mox.StubOutWithMock(commands, 'MakeChroot')
    self.mox.StubOutWithMock(commands, 'SetupBoard')
    self.mox.StubOutWithMock(commands, 'RunChrootUpgradeHooks')

    os.path.isdir(os.path.join(self.build_root, 'chroot')).AndReturn(True)
    commands.MakeChroot(buildroot=self.build_root,
                        replace=True,
                        use_sdk=True,
                        chrome_root=None,
                        extra_env=mox.IgnoreArg()
                        )
    os.path.isdir(os.path.join(self.build_root, 'chroot', 'build',
                               self._current_board)).AndReturn(False)
    commands.SetupBoard(self.build_root,
                        board=self._current_board,
                        usepkg=False,
                        latest_toolchain=False,
                        extra_env={},
                        profile='smock',
                        chroot_upgrade=False)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()
    self.options.profile = None

  def testBinBuild(self):
    """Tests whether we skip un-necessary steps for a binary builder."""
    self.mox.StubOutWithMock(commands, 'MakeChroot')
    self.mox.StubOutWithMock(commands, 'SetupBoard')
    self.mox.StubOutWithMock(commands, 'RunChrootUpgradeHooks')

    os.path.isdir(os.path.join(self.build_root, 'chroot')).AndReturn(True)
    os.path.isdir(os.path.join(self.build_root, 'chroot', 'build',
                               self._current_board)).AndReturn(True)

    commands.RunChrootUpgradeHooks(self.build_root)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testBinBuildAfterClobber(self):
    """Tests whether we make chroot and board after a clobber."""
    self.mox.StubOutWithMock(commands, 'MakeChroot')
    self.mox.StubOutWithMock(commands, 'SetupBoard')

    os.path.isdir(os.path.join(self.build_root, 'chroot')).AndReturn(False)
    commands.MakeChroot(buildroot=self.build_root,
                        replace=self.build_config['chroot_replace'],
                        use_sdk=self.build_config['use_sdk'],
                        chrome_root=None,
                        extra_env=mox.IgnoreArg()
                        )

    os.path.isdir(os.path.join(self.build_root, 'chroot', 'build',
                               self._current_board)).AndReturn(False)

    commands.SetupBoard(self.build_root,
                        board=self._current_board,
                        usepkg=self.build_config['usepkg_setup_board'],
                        latest_toolchain=self.build_config['latest_toolchain'],
                        extra_env={},
                        profile=None,
                        chroot_upgrade=False)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class SDKStageTest(AbstractStageTest, cros_test_lib.TempDirTestCase):
  """Tests SDK package and Manifest creation."""
  fake_packages = [('cat1/package', '1'), ('cat1/package', '2'),
                   ('cat2/package', '3'), ('cat2/package', '4')]
  fake_json_data = {}
  fake_chroot = None

  def setUp(self):
    self.build_root = self.tempdir
    self.options.buildroot = self.build_root

    # Replace SudoRunCommand, since we don't care about sudo.
    self._OriginalSudoRunCommand = cros_build_lib.SudoRunCommand
    cros_build_lib.SudoRunCommand = cros_build_lib.RunCommand

    # Prepare a fake chroot.
    self.fake_chroot = os.path.join(self.build_root, 'chroot/build/amd64-host')
    os.makedirs(self.fake_chroot)
    osutils.Touch(os.path.join(self.fake_chroot, 'file'))
    for package, v in self.fake_packages:
      cpv = portage_utilities.SplitCPV('%s-%s' % (package, v))
      key = '%s/%s' % (cpv.category, cpv.package)
      self.fake_json_data.setdefault(key, []).append([v, {}])

  def tearDown(self):
    cros_build_lib.SudoRunCommand = self._OriginalSudoRunCommand

  def ConstructStage(self):
    return stages.SDKPackageStage(self.options, self.build_config)

  def testTarballCreation(self):
    """Tests whether we package the tarball and correctly create a Manifest."""
    self.bot_id = 'chromiumos-sdk'
    self.build_config = config.config[self.bot_id]
    fake_tarball = os.path.join(self.build_root, 'built-sdk.tar.xz')
    fake_manifest = os.path.join(self.build_root,
                                 'built-sdk.tar.xz.Manifest')
    self.mox.StubOutWithMock(portage_utilities, 'ListInstalledPackages')
    self.mox.StubOutWithMock(stages.SDKPackageStage,
                             'CreateRedistributableToolchains')

    portage_utilities.ListInstalledPackages(self.fake_chroot).AndReturn(
        self.fake_packages)
    # This code has its own unit tests, so no need to go testing it here.
    stages.SDKPackageStage.CreateRedistributableToolchains(mox.IgnoreArg())

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

    # Check tarball for the correct contents.
    output = cros_build_lib.RunCommandCaptureOutput(
        ['tar', '-I', 'xz', '-tvf', fake_tarball]).output.splitlines()
    # First line is './', use it as an anchor, count the chars, and strip as
    # much from all other lines.
    stripchars = len(output[0]) - 1
    tar_lines = [x[stripchars:] for x in output]
    # TODO(ferringb): replace with assertIn.
    self.assertFalse('/build/amd64-host/' in tar_lines)
    self.assertTrue('/file' in tar_lines)
    # Verify manifest contents.
    real_json_data = json.loads(osutils.ReadFile(fake_manifest))
    self.assertEqual(real_json_data['packages'],
                     self.fake_json_data)


class VMTestStageTest(AbstractStageTest, cros_test_lib.MockTestCase):

  def setUp(self):
    self.bot_id = 'x86-generic-full'
    self.build_config = config.config[self.bot_id].copy()
    for cmd in ('RunTestSuite', 'CreateTestRoot', 'GenerateStackTraces',
                'ArchiveFile', 'ArchiveTestResults', 'UploadArchivedFile'):
      self.StartPatcher(mock.patch.object(commands, cmd, autospec=True))
    self.StartPatcher(ArchiveStageMock())

  def ConstructStage(self):
    archive_stage = stages.ArchiveStage(self.options, self.build_config,
                                        self._current_board)
    return stages.VMTestStage(self.options, self.build_config,
                              self._current_board, archive_stage)

  def testFullTests(self):
    """Tests if full unit and cros_au_test_harness tests are run correctly."""
    self.build_config['vm_tests'] = constants.FULL_AU_TEST_TYPE
    self.RunStage()

  def testQuickTests(self):
    """Tests if quick unit and cros_au_test_harness tests are run correctly."""
    self.build_config['vm_tests'] = constants.SIMPLE_AU_TEST_TYPE
    self.RunStage()


class UnitTestStageTest(AbstractStageTest):

  def setUp(self):
    self.bot_id = 'x86-generic-full'
    self.build_config = config.config[self.bot_id].copy()
    self.mox.StubOutWithMock(commands, 'RunUnitTests')

  def ConstructStage(self):
    return stages.UnitTestStage(self.options, self.build_config,
                                self._current_board)

  def testQuickTests(self):
    self.build_config['quick_unit'] = True
    commands.RunUnitTests(self.build_root, self._current_board, full=False,
                          nowithdebug=mox.IgnoreArg())
    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testFullTests(self):
    """Tests if full unit and cros_au_test_harness tests are run correctly."""
    self.build_config['quick_unit'] = False
    commands.RunUnitTests(self.build_root, self._current_board, full=True,
                          nowithdebug=mox.IgnoreArg())
    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class HWTestStageTest(AbstractStageTest):

  def setUp(self):
    self.bot_id = 'x86-mario-release'
    self.options.log_dir = '/b/cbuild/mylogdir'
    self.build_config = config.config[self.bot_id].copy()
    self.archive_stage_mock = self.mox.CreateMock(stages.ArchiveStage)
    self.suite = 'bvt'
    self.archive_stage_mock.WaitForHWTestUploads().AndReturn(True)
    self.archive_stage_mock.GetVersion().AndReturn('ver')
    self.mox.StubOutWithMock(commands, 'RunHWTestSuite')
    self.mox.StubOutWithMock(cros_build_lib, 'PrintBuildbotStepWarnings')
    self.mox.StubOutWithMock(cros_build_lib, 'PrintBuildbotStepFailure')
    self.mox.StubOutWithMock(cros_build_lib, 'Warning')
    self.mox.StubOutWithMock(cros_build_lib, 'Error')

  def ConstructStage(self):
    return stages.HWTestStage(self.options, self.build_config,
                              self._current_board, self.archive_stage_mock,
                              self.suite)

  def _RunHWTestSuite(self, debug=False, returncode=0, fails=False):
    """Pretend to run the HWTest suite to assist with tests.

    Args:
      debug: Whether the HWTest suite should be run in debug mode.
      returncode: The return value of the HWTest command.
      fails: Whether the command as a whole should fail.
    """
    m = commands.RunHWTestSuite(mox.IgnoreArg(), self.suite,
                                self._current_board, mox.IgnoreArg(),
                                mox.IgnoreArg(), mox.IgnoreArg(), True, debug)

    # Raise an exception if the user wanted the command to fail.
    if returncode != 0:
      result = cros_build_lib.CommandResult(cmd='run_hw_tests',
                                            returncode=returncode)
      m.AndRaise(cros_build_lib.RunCommandError('HWTests failed', result))

      # Make sure failures are logged correctly.
      if fails:
        cros_build_lib.PrintBuildbotStepFailure()
        cros_build_lib.Error(mox.IgnoreArg())
      else:
        cros_build_lib.PrintBuildbotStepWarnings()
        cros_build_lib.Warning(mox.IgnoreArg())

    self.mox.ReplayAll()
    if fails:
      self.assertRaises(results_lib.StepFailure, self.RunStage)
    else:
      self.RunStage()
    self.mox.VerifyAll()

  def _SetupRemoteTrybotConfig(self, args):
    """Setup a remote trybot config with the specified arguments.

    Args:
      args: Command-line arguments to pass in to remote trybot.
    """
    argv = ['--remote-trybot', '-r', self.build_root, self.bot_id] + args
    parser = cbuildbot._CreateParser()
    (self.options, _args) = cbuildbot._ParseCommandLine(parser, argv)
    self.build_config = config.OverrideConfigForTrybot(self.build_config,
                                                       self.options.
                                                       remote_trybot)

  def testRemoteTrybotWithHWTest(self):
    """Test remote trybot with hw test enabled"""
    self._SetupRemoteTrybotConfig(['--hwtest'])
    self._RunHWTestSuite()

  def testRemoteTrybotNoHWTest(self):
    """Test remote trybot with no hw test"""
    self._SetupRemoteTrybotConfig([])
    self._RunHWTestSuite(debug=True)

  def testWithSuite(self):
    """Test if run correctly with a test suite."""
    self._RunHWTestSuite()

  def testWithSuiteWithInfrastructureFailure(self):
    """Tests that we warn correctly if we get a returncode of 2."""
    self._RunHWTestSuite(returncode=2)

  def testWithSuiteWithFatalFailure(self):
    """Tests that we fail if we get a returncode of 1."""
    self._RunHWTestSuite(returncode=1, fails=True)

  def testSendPerfResults(self):
    """Tests that we can send perf results back correctly."""
    self.suite = 'pyauto_perf'
    self.bot_id = 'lumpy-chrome-perf'
    self.build_config = config.config['lumpy-chrome-perf'].copy()
    self.mox.StubOutWithMock(stages.HWTestStage, '_PrintFile')

    gs_upload_location = 'gs://dontcare/builder/version'

    self.archive_stage_mock.GetGSUploadLocation().AndReturn(gs_upload_location)
    results_file = 'pyauto_perf.results'

    stages.HWTestStage._PrintFile(os.path.join(self.options.log_dir,
                                               results_file))
    with gs_unittest.GSContextMock() as gs_mock:
      gs_mock.SetDefaultCmdResult()
      self._RunHWTestSuite()


class UprevStageTest(AbstractStageTest):

  def setUp(self):
    # Disable most paths by default and selectively enable in tests

    self.options.chrome_rev = None
    self.build_config['uprev'] = False
    self.mox.StubOutWithMock(commands, 'MarkChromeAsStable')
    self.mox.StubOutWithMock(commands, 'UprevPackages')
    self.mox.StubOutWithMock(sys, 'exit')

  def ConstructStage(self):
    return stages.UprevStage(self.options, self.build_config)

  def testChromeRevSuccess(self):
    """Case where MarkChromeAsStable returns an atom.  We shouldn't exit."""
    self.options.chrome_rev = 'tot'
    chrome_atom = '%s-12.0.719.0_alpha-r1' % constants.CHROME_CP

    commands.MarkChromeAsStable(
        self.build_root,
        self.TARGET_MANIFEST_BRANCH,
        self.options.chrome_rev,
        self._boards,
        chrome_version=None).AndReturn(chrome_atom)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testChromeRevFoundNothing(self):
    """Verify we exit when MarkChromeAsStable doesn't return an atom."""
    self.options.chrome_rev = 'tot'

    commands.MarkChromeAsStable(
        self.build_root,
        self.TARGET_MANIFEST_BRANCH,
        self.options.chrome_rev,
        self._boards,
        chrome_version=None)

    sys.exit(0)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testBuildRev(self):
    """Uprevving the build without uprevving chrome."""
    self.build_config['uprev'] = True

    commands.UprevPackages(self.build_root, self._boards, [], enter_chroot=True)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testNoRev(self):
    """No paths are enabled."""
    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testUprevAll(self):
    """Uprev both Chrome and built packages."""
    self.build_config['uprev'] = True
    self.options.chrome_rev = 'tot'

    # Even if MarkChromeAsStable didn't find anything to rev,
    # if we rev the build then we don't exit

    commands.MarkChromeAsStable(
        self.build_root,
        self.TARGET_MANIFEST_BRANCH,
        self.options.chrome_rev,
        self._boards,
        chrome_version=None).AndReturn(None)

    commands.UprevPackages(self.build_root, self._boards, [], enter_chroot=True)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

def _DoSteps(steps):
  for step in steps:
    step()

class BuildTargetStageTest(AbstractStageTest, cros_test_lib.MockTestCase):

  def setUp(self):
    self.images_root = os.path.join(self.build_root,
                                    'src/build/images/x86-generic')
    latest_image_dir = os.path.join(self.images_root, 'latest')
    self.mox.StubOutWithMock(manifest_version.VersionInfo, 'VersionString')
    self.mox.StubOutWithMock(manifest_version.VersionInfo, '_LoadFromFile')
    self.mox.StubOutWithMock(os, 'readlink')
    self.mox.StubOutWithMock(os, 'symlink')
    os.readlink(latest_image_dir).AndReturn('myimage')
    self.branch = '22'
    self.version = '1423.0.0'
    self.branch_version = 'R%s-%s' % (self.branch, self.version)
    manifest_version.VersionInfo._LoadFromFile()
    manifest_version.VersionInfo.chrome_branch = self.branch
    self.version_to_test = self.version
    self.latest_cbuildbot = '%s-cbuildbot' % latest_image_dir
    os.symlink('myimage', self.latest_cbuildbot)

    # Disable most paths by default and selectively enable in tests
    self.build_config['vm_tests'] = None
    self.build_config['build_type'] = constants.PFQ_TYPE
    self.build_config['usepkg_chroot'] = False
    self.latest_versioned_build = os.path.join(self.images_root, self.version)

    self.options.prebuilts = True
    self.options.tests = False

    self.mox.StubOutWithMock(shutil, 'move')
    self.mox.StubOutWithMock(commands, 'Build')
    self.mox.StubOutWithMock(commands, 'UploadPrebuilts')
    self.mox.StubOutWithMock(commands, 'BuildImage')
    self.mox.StubOutWithMock(commands, 'BuildVMImageForTesting')
    self.mox.StubOutWithMock(bs.BuilderStage, '_GetPortageEnvVar')
    self.mox.StubOutWithMock(shutil, 'copyfile')
    self.mox.StubOutWithMock(tempfile, 'mkdtemp')
    self.mox.StubOutWithMock(os.path, 'isdir')

    self.mox.StubOutWithMock(commands, 'BuildAutotestTarballs')
    self.mox.StubOutWithMock(commands, 'BuildFullAutotestTarball')
    self.mox.StubOutWithMock(os, 'rename')
    self.archive_stage_mock = self.mox.CreateMock(stages.ArchiveStage)

    # pylint: disable=E1101
    self.parallel_mock = self.StartPatcher(parallel_unittest.ParallelMock())

  def ConstructStage(self):
    return stages.BuildTargetStage(
        self.options, self.build_config, self._current_board,
        self.archive_stage_mock, self.version_to_test)

  def testAllConditionalPaths(self):
    """Enable all paths to get line coverage."""
    self.build_config['vm_tests'] = constants.SIMPLE_AU_TEST_TYPE
    self.options.tests = True
    self.build_config['build_type'] = constants.BUILD_FROM_SOURCE_TYPE
    self.build_config['build_tests'] = True
    self.build_config['archive_build_debug'] = True
    self.build_config['usepkg_chroot'] = True
    self.build_config['usepkg_setup_board'] = True
    self.build_config['usepkg_build_packages'] = True
    self.build_config['images'] = ['base', 'dev', 'test', 'factory_test',
                                   'factory_install']
    self.build_config['useflags'] = ['ALPHA', 'BRAVO', 'CHARLIE',
                                     constants.USE_PGO_GENERATE]
    self.build_config['skip_toolchain_update'] = False
    self.build_config['nowithdebug'] = False
    self.build_config['hw_tests'] = ['bvt']
    self.build_config['chromeos_official'] = True
    self.build_config['disk_layout'] = '2gb-rootfs'

    proper_env = {'USE' : ' '.join(self.build_config['useflags'])}

    # Convenience variables.
    fake_autotest_dir = '/fake/autotest'
    autotest_tarball_name = 'autotest.tar'
    full_autotest_tarball_name = 'autotest.tar.bz2'
    test_suites_tarball_name = 'test_suites.tar.bz2'
    autotest_tarball_path = os.path.join(fake_autotest_dir,
                                         autotest_tarball_name)
    full_autotest_tarball_path = os.path.join(fake_autotest_dir,
                                              full_autotest_tarball_name)
    test_suites_tarball_path = os.path.join(fake_autotest_dir,
                                            test_suites_tarball_name)
    tarballs = [autotest_tarball_path, test_suites_tarball_path]
    full_autotest_tarball = full_autotest_tarball_path

    commands.Build(self.build_root,
                   self._current_board,
                   build_autotest=True,
                   chrome_root=None,
                   usepkg=True,
                   skip_toolchain_update=False,
                   nowithdebug=False,
                   packages=(),
                   extra_env=proper_env)

    commands.BuildImage(self.build_root, self._current_board,
                        ['test', 'base', 'dev'], version=self.version,
                        rootfs_verification=True,
                        disk_layout='2gb-rootfs', extra_env=proper_env)
    commands.BuildVMImageForTesting(self.build_root, self._current_board,
                                    disk_layout=None, extra_env=proper_env)
    tempfile.mkdtemp(prefix='autotest').AndReturn(fake_autotest_dir)
    commands.BuildAutotestTarballs(self.build_root, self._current_board,
                                   fake_autotest_dir).AndReturn(tarballs)
    commands.BuildFullAutotestTarball(self.build_root,
                                      self._current_board,
                                      fake_autotest_dir
                                      ).AndReturn(full_autotest_tarball)
    self.archive_stage_mock.AutotestTarballsReady(tarballs)
    self.archive_stage_mock.FullAutotestTarballReady(full_autotest_tarball)
    self.archive_stage_mock.SetVersion(self.branch_version)

    shutil.copyfile(full_autotest_tarball_path,
                    os.path.join(self.images_root, 'latest-cbuildbot',
                                 full_autotest_tarball_name))

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testFalseBuildArgs(self):
    """Make sure our logic for Build arguments can toggle to false."""
    self.build_config['useflags'] = None

    commands.Build(self.build_root,
                   self._current_board,
                   build_autotest=mox.IgnoreArg(),
                   usepkg=mox.IgnoreArg(),
                   chrome_root=None,
                   skip_toolchain_update=mox.IgnoreArg(),
                   nowithdebug=mox.IgnoreArg(),
                   packages=mox.IgnoreArg(),
                   extra_env={})
    self.archive_stage_mock.AutotestTarballsReady(None)
    commands.BuildImage(self.build_root, self._current_board, ['test'],
                        disk_layout=None, rootfs_verification=True,
                        version=self.version, extra_env={})
    self.archive_stage_mock.SetVersion(self.branch_version)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testFalseTestArg(self):
    """Make sure our logic for build test arg can toggle to false."""
    self.build_config['vm_tests'] = None
    self.options.tests = True
    self.options.hw_tests = True
    self.build_config['build_type'] = constants.BUILD_FROM_SOURCE_TYPE
    self.build_config['usepkg_chroot'] = True
    self.build_config['usepkg_setup_board'] = True
    self.build_config['usepkg_build_packages'] = True
    self.build_config['useflags'] = ['ALPHA', 'BRAVO', 'CHARLIE']
    self.build_config['nowithdebug'] = True
    self.build_config['skip_toolchain_update'] = False

    proper_env = {'USE' : ' '.join(self.build_config['useflags'])}

    commands.Build(self.build_root,
                   self._current_board,
                   build_autotest=True,
                   chrome_root=None,
                   usepkg=True,
                   skip_toolchain_update=False,
                   nowithdebug=True,
                   packages=(),
                   extra_env=proper_env)
    self.archive_stage_mock.AutotestTarballsReady(None)
    commands.BuildImage(self.build_root, self._current_board, ['test'],
                        rootfs_verification=True,
                        disk_layout=None, version=self.version,
                        extra_env=proper_env)
    self.archive_stage_mock.SetVersion(self.branch_version)

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testUnversionedCodePath(self):
    """Make sure our logic for build test works for unversioned code paths."""
    self.version_to_test = None
    self.build_config['vm_tests'] = constants.SIMPLE_AU_TEST_TYPE
    self.options.tests = True
    self.build_config['build_type'] = constants.BUILD_FROM_SOURCE_TYPE
    self.build_config['build_tests'] = True
    self.build_config['archive_build_debug'] = True
    self.build_config['usepkg_chroot'] = True
    self.build_config['usepkg_setup_board'] = True
    self.build_config['usepkg_build_packages'] = True
    self.build_config['images'] = ['base', 'dev', 'test', 'factory_test',
                                   'factory_install']
    self.build_config['useflags'] = ['ALPHA', 'BRAVO', 'CHARLIE']
    self.build_config['skip_toolchain_update'] = False
    self.build_config['nowithdebug'] = False
    self.build_config['hw_tests'] = ['bvt']
    self.build_config['disk_layout'] = '2gb-rootfs'

    proper_env = {'USE' : ' '.join(self.build_config['useflags'])}

    # Convenience variables.
    fake_autotest_dir = '/fake/autotest'
    autotest_tarball_name = 'autotest.tar'
    test_suites_tarball_name = 'test_suites.tar.bz2'
    autotest_tarball_path = os.path.join(fake_autotest_dir,
                                         autotest_tarball_name)
    test_suites_tarball_path = os.path.join(fake_autotest_dir,
                                            test_suites_tarball_name)
    tarballs = [autotest_tarball_path, test_suites_tarball_path]

    manifest_version.VersionInfo.VersionString().AndReturn(self.version)
    commands.Build(self.build_root,
                   self._current_board,
                   build_autotest=True,
                   chrome_root=None,
                   usepkg=True,
                   skip_toolchain_update=False,
                   nowithdebug=False,
                   packages=(),
                   extra_env=proper_env)

    commands.BuildImage(self.build_root, self._current_board,
                        ['test', 'base', 'dev'],
                        rootfs_verification=True,
                        version='',
                        disk_layout=self.build_config['disk_layout'],
                        extra_env=proper_env)
    commands.BuildVMImageForTesting(self.build_root, self._current_board,
                                    disk_layout=None, extra_env=proper_env)
    tempfile.mkdtemp(prefix='autotest').AndReturn(fake_autotest_dir)
    commands.BuildAutotestTarballs(self.build_root, self._current_board,
                                   fake_autotest_dir).AndReturn(tarballs)
    self.archive_stage_mock.AutotestTarballsReady(tarballs)
    self.archive_stage_mock.SetVersion('%s-b%s' % (self.branch_version,
                                                   self.options.buildnumber))
    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class ArchiveStageMock(partial_mock.PartialMock):
  """Partial mock for Archive Stage."""

  TARGET = 'chromite.buildbot.cbuildbot_stages.ArchiveStage'
  ATTRS = ('GetVersion', 'WaitForBreakpadSymbols',)

  VERSION = '0.0.0.1'

  def GetVersion(self, _inst):
    return self.VERSION

  def WaitForBreakpadSymbols(self, _inst):
    return True


class ArchiveStageTest(AbstractStageTest, cros_test_lib.MockTestCase):

  def _AutoPatch(self, to_patch):
    """Patch a list of objects with autospec=True.

    Arguments:
      to_patch: A list of tuples in the form (target, attr) to patch.  Will be
      directly passed to mock.patch.object.
    """
    for item in to_patch:
      # pylint: disable=E1101
      self.StartPatcher(mock.patch.object(*item, autospec=True))

  def _PatchDependencies(self):
    """Patch dependencies of ArchiveStage.PerformStage()."""
    to_patch = [
        (parallel, 'RunParallelSteps'), (commands, 'PushImages'),
        (commands, 'RemoveOldArchives'), (commands, 'UploadArchivedFile')]
    self._AutoPatch(to_patch)

  def setUp(self):
    self._build_config = self.build_config.copy()
    self._build_config['upload_symbols'] = True
    self._build_config['push_image'] = True

    self.archive_mock = ArchiveStageMock()
    # pylint: disable=E1101
    self.StartPatcher(self.archive_mock)
    self._PatchDependencies()

  def ConstructStage(self):
    return stages.ArchiveStage(self.options, self._build_config,
                               self._current_board)

  def testArchive(self):
    """Simple did-it-run test."""
    # TODO(davidjames): Test the individual archive steps as well.
    self.RunStage()
    # pylint: disable=E1101
    self.assertEquals(
        commands.UploadArchivedFile.call_args[0][2:],
        ('LATEST-%s' % self.TARGET_MANIFEST_BRANCH, False))

  def testNoPushImagesForRemoteTrybot(self):
    """Test that remote trybot overrides work to disable push images."""
    argv = ['--remote-trybot', '-r', self.build_root, '--buildnumber=1234',
            'x86-mario-release']
    parser = cbuildbot._CreateParser()
    (self.options, _args) = cbuildbot._ParseCommandLine(parser, argv)
    test_config = config.config['x86-mario-release']
    self._build_config = config.OverrideConfigForTrybot(test_config, True)
    self.RunStage()
    # pylint: disable=E1101
    self.assertEquals(commands.PushImages.call_count, 0)

  def testArchiveMetadataJson(self):
    """Test that the json metadata is built correctly"""
    # First run the code.
    stage = self.ConstructStage()
    stage._Initialize()
    stage.ArchiveMetadataJson()

    # Now check the results.
    json_file = stage._upload_queue.get()
    self.assertEquals(json_file, [constants.METADATA_JSON])
    json_file = os.path.join(stage._archive_path, json_file[0])
    json_data = json.loads(osutils.ReadFile(json_file))

    important_keys = (
        'boards',
        'bot-config',
        'cros-version',
        'metadata-version',
        'sdk-version',
        'toolchain-tuple',
        'toolchain-url',
    )
    for key in important_keys:
      self.assertTrue(key in json_data)

    self.assertEquals(json_data['boards'], ['x86-generic'])
    self.assertEquals(json_data['bot-config'], 'x86-generic-paladin')
    self.assertEquals(json_data['cros-version'], self.archive_mock.VERSION)
    self.assertEquals(json_data['metadata-version'], '1')
    self.assertEquals(json_data['toolchain-tuple'], ['i686-pc-linux-gnu',
                                                     'arm-none-eabi'])
  def testChromeEnvironment(self):
    """Test that the Chrome environment is built."""
    # Create the chrome environment compressed file.
    stage = self.ConstructStage()
    stage._Initialize()
    chrome_env_dir = os.path.join(
        stage._pkg_dir, constants.CHROME_CP + '-25.3643.0_rc1')
    env_file = os.path.join(chrome_env_dir, 'environment')
    osutils.Touch(env_file, makedirs=True)

    cros_build_lib.RunCommand(['bzip2', env_file])

    # Run the code.
    stage.ArchiveChromeEbuildEnv()

    env_tar = stage._upload_queue.get()[0]
    env_tar = os.path.join(stage._archive_path, env_tar)
    self.assertTrue(os.path.exists(env_tar))
    cros_test_lib.VerifyTarball(env_tar, ['./', 'environment'])


class UploadPrebuiltsStageTest(AbstractStageTest):
  def setUp(self):
    self.options.chrome_rev = 'tot'
    self.options.prebuilts = True
    self.mox.StubOutWithMock(stages.UploadPrebuiltsStage, '_GetPortageEnvVar')
    self.mox.StubOutWithMock(commands, 'UploadPrebuilts')
    self.archive_stage = self.mox.CreateMock(stages.ArchiveStage)

  def ConstructStage(self):
    self.mox.CreateMock(stages.ArchiveStage)
    return stages.UploadPrebuiltsStage(self.options,
                                       self.build_config,
                                       self._current_board,
                                       self.archive_stage)

  def ConstructBinhosts(self):
    for board in (self._current_board, None):
      binhost = 'http://binhost/?board=' + str(board)
      stages.UploadPrebuiltsStage._GetPortageEnvVar(stages._PORTAGE_BINHOST,
          board).AndReturn(binhost)

  def _MockOutPrebuiltCall(self, extra_args):
    """Common method to mock out UploadPrebuilts call."""
    commands.UploadPrebuilts(
        category=self.build_config['build_type'],
        chrome_rev=self.options.chrome_rev,
        private_bucket=False,
        buildroot=self.build_root,
        board=self._current_board,
        extra_args=extra_args)

  def testFullPrebuiltsUpload(self):
    """Test uploading of full builder prebuilts."""
    self.build_config['build_type'] = constants.BUILD_FROM_SOURCE_TYPE
    self.build_config['manifest_version'] = False
    self.build_config['git_sync'] = True
    self._MockOutPrebuiltCall(mox.And(mox.IsA(list),
                                      mox.In('--git-sync')))

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testChromeUpload(self):
    """Test uploading of prebuilts for chrome build."""
    self.build_config['build_type'] = constants.CHROME_PFQ_TYPE
    chrome_version = 'awesome_chrome_version'
    self.archive_stage.GetVersion().AndReturn(chrome_version)

    self.ConstructBinhosts()
    self._MockOutPrebuiltCall(mox.And(mox.IsA(list),
                                      mox.In(chrome_version)))

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()

  def testPreflightUpload(self):
    """Test uploading of prebuilts for preflight build."""
    self.build_config['build_type'] = constants.PFQ_TYPE
    pfq_version = 'awesome_pfq_version'
    self.archive_stage.GetVersion().AndReturn(pfq_version)

    self.ConstructBinhosts()
    self._MockOutPrebuiltCall(mox.And(mox.IsA(list),
                                      mox.In(pfq_version),
                                      mox.In('--sync-host')))

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class UploadDevInstallerPrebuiltsStageTest(AbstractStageTest):
  def setUp(self):
    self.options.chrome_rev = None
    self.options.prebuilts = True
    self.build_config['dev_installer_prebuilts'] = True
    self.build_config['binhost_bucket'] = 'gs://testbucket'
    self.build_config['binhost_key'] = 'dontcare'
    self.build_config['binhost_base_url'] = 'https://dontcare/here'
    self.mox.StubOutWithMock(stages.UploadPrebuiltsStage, '_GetPortageEnvVar')
    self.mox.StubOutWithMock(commands, 'UploadDevInstallerPrebuilts')
    self.archive_stage = self.mox.CreateMock(stages.ArchiveStage)

  def ConstructStage(self):
    self.mox.CreateMock(stages.ArchiveStage)
    return stages.DevInstallerPrebuiltsStage(self.options,
                                             self.build_config,
                                             self._current_board,
                                             self.archive_stage)

  def testDevInstallerUpload(self):
    """Basic sanity test testing uploads of dev installer prebuilts."""
    version = 'awesome_canary_version'
    self.archive_stage.GetVersion().AndReturn(version)

    commands.UploadDevInstallerPrebuilts(
        binhost_bucket=self.build_config['binhost_bucket'],
        binhost_key=self.build_config['binhost_key'],
        binhost_base_url=self.build_config['binhost_base_url'],
        buildroot=self.build_root,
        board=self._current_board,
        extra_args=mox.And(mox.IsA(list),
                           mox.In(version)))

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class PublishUprevChangesStageTest(AbstractStageTest):

  def setUp(self):
    # Disable most paths by default and selectively enable in tests

    self.build_config['build_type'] = constants.BUILD_FROM_SOURCE_TYPE
    self.options.chrome_rev = constants.CHROME_REV_TOT
    self.options.prebuilts = True
    self.mox.StubOutWithMock(stages.PublishUprevChangesStage,
                             '_GetPortageEnvVar')
    self.mox.StubOutWithMock(commands, 'UploadPrebuilts')
    self.mox.StubOutWithMock(commands, 'UprevPush')

  def ConstructStage(self):
    return stages.PublishUprevChangesStage(self.options, self.build_config)

  def testPush(self):
    """Test values for PublishUprevChanges."""
    self.build_config['push_overlays'] = constants.PUBLIC_OVERLAYS
    self.build_config['master'] = True

    self.mox.ReplayAll()
    self.RunStage()
    self.mox.VerifyAll()


class BuildStagesResultsTest(cros_test_lib.TestCase):

  def setUp(self):
    # Always stub RunCommmand out as we use it in every method.
    self.bot_id = 'x86-generic-paladin'
    self.build_config = config.config[self.bot_id]
    self.build_root = '/fake_root'
    self.url = 'fake_url'

    # Create a class to hold
    class Options(object):
      pass

    self.options = Options()
    self.options.archive_base = 'gs://dontcare'
    self.options.buildroot = self.build_root
    self.options.debug = False
    self.options.prebuilts = False
    self.options.clobber = False
    self.options.buildnumber = 1234
    self.options.chrome_rev = None

    self.failException = Exception("FailStage needs to fail.")

  def _runStages(self):
    """Run a couple of stages so we can capture the results"""

    # Save off our self where FailStage._PerformStage can find it.
    outer_self = self

    class PassStage(bs.BuilderStage):
      """PassStage always works"""
      pass

    class Pass2Stage(bs.BuilderStage):
      """Pass2Stage always works"""
      pass

    class FailStage(bs.BuilderStage):
      """FailStage always throws an exception"""

      def _PerformStage(self):
        """Throw the exception to make us fail."""
        raise outer_self.failException

    # Run two pass stages, and one fail stage.
    PassStage(self.options, self.build_config).Run()
    Pass2Stage(self.options, self.build_config).Run()

    self.assertRaises(
      results_lib.StepFailure,
      FailStage(self.options, self.build_config).Run)

  def _verifyRunResults(self, expectedResults):

    actualResults = results_lib.Results.Get()

    # Break out the asserts to be per item to make debugging easier
    self.assertEqual(len(expectedResults), len(actualResults))
    for i in xrange(len(expectedResults)):
      name, result, description, runtime = actualResults[i]

      if result != results_lib.Results.SUCCESS:
        self.assertTrue(isinstance(description, str))

      self.assertTrue(runtime >= 0 and runtime < 2.0)
      self.assertEqual(expectedResults[i], (name, result))

  def _PassString(self):
    return results_lib.Results.SPLIT_TOKEN.join(['Pass', 'None', '0\n'])

  def testRunStages(self):
    """Run some stages and verify the captured results"""

    results_lib.Results.Clear()
    self.assertEqual(results_lib.Results.Get(), [])

    self._runStages()

    # Verify that the results are what we expect.
    expectedResults = [
        ('Pass', results_lib.Results.SUCCESS),
        ('Pass2', results_lib.Results.SUCCESS),
        ('Fail', self.failException)]

    self._verifyRunResults(expectedResults)

  def testSuccessTest(self):
    """Run some stages and verify the captured results"""

    results_lib.Results.Clear()
    results_lib.Results.Record('Pass', results_lib.Results.SUCCESS)

    self.assertTrue(results_lib.Results.BuildSucceededSoFar())

    results_lib.Results.Record('Fail', self.failException, time=1)

    self.assertFalse(results_lib.Results.BuildSucceededSoFar())

    results_lib.Results.Record('Pass2', results_lib.Results.SUCCESS)

    self.assertFalse(results_lib.Results.BuildSucceededSoFar())

  def testStagesReportSuccess(self):
    """Tests Stage reporting."""

    stages.ManifestVersionedSyncStage.manifest_manager = None

    # Store off a known set of results and generate a report
    results_lib.Results.Clear()
    results_lib.Results.Record('Sync', results_lib.Results.SUCCESS, time=1)
    results_lib.Results.Record('Build', results_lib.Results.SUCCESS, time=2)
    results_lib.Results.Record('Test', self.failException, time=3)
    result = cros_build_lib.CommandResult(cmd=['/bin/false', '/nosuchdir'],
                                          returncode=2)
    results_lib.Results.Record(
        'Archive',
        cros_build_lib.RunCommandError(
            'Command "/bin/false /nosuchdir" failed.\n',
            result), time=4)

    results = StringIO.StringIO()

    results_lib.Results.Report(results)

    expectedResults = (
        "************************************************************\n"
        "** Stage Results\n"
        "************************************************************\n"
        "** PASS Sync (0:00:01)\n"
        "************************************************************\n"
        "** PASS Build (0:00:02)\n"
        "************************************************************\n"
        "** FAIL Test (0:00:03) with Exception\n"
        "************************************************************\n"
        "** FAIL Archive (0:00:04) in /bin/false\n"
        "************************************************************\n"
    )

    expectedLines = expectedResults.split('\n')
    actualLines = results.getvalue().split('\n')

    # Break out the asserts to be per item to make debugging easier
    for i in xrange(min(len(actualLines), len(expectedLines))):
      self.assertEqual(expectedLines[i], actualLines[i])
    self.assertEqual(len(expectedLines), len(actualLines))

  def testStagesReportError(self):
    """Tests Stage reporting with exceptions."""

    stages.ManifestVersionedSyncStage.manifest_manager = None

    # Store off a known set of results and generate a report
    results_lib.Results.Clear()
    results_lib.Results.Record('Sync', results_lib.Results.SUCCESS, time=1)
    results_lib.Results.Record('Build', results_lib.Results.SUCCESS, time=2)
    results_lib.Results.Record('Test', self.failException,
                               'failException Msg\nLine 2', time=3)
    result = cros_build_lib.CommandResult(cmd=['/bin/false', '/nosuchdir'],
                                          returncode=2)
    results_lib.Results.Record(
        'Archive',
        cros_build_lib.RunCommandError(
            'Command "/bin/false /nosuchdir" failed.\n',
            result),
        'FailRunCommand msg', time=4)

    results = StringIO.StringIO()

    results_lib.Results.Report(results)

    expectedResults = (
        "************************************************************\n"
        "** Stage Results\n"
        "************************************************************\n"
        "** PASS Sync (0:00:01)\n"
        "************************************************************\n"
        "** PASS Build (0:00:02)\n"
        "************************************************************\n"
        "** FAIL Test (0:00:03) with Exception\n"
        "************************************************************\n"
        "** FAIL Archive (0:00:04) in /bin/false\n"
        "************************************************************\n"
        "\n"
        "Failed in stage Test:\n"
        "\n"
        "failException Msg\n"
        "Line 2\n"
        "\n"
        "Failed in stage Archive:\n"
        "\n"
        "FailRunCommand msg\n"
   )

    expectedLines = expectedResults.split('\n')
    actualLines = results.getvalue().split('\n')

    # Break out the asserts to be per item to make debugging easier
    for i in xrange(min(len(actualLines), len(expectedLines))):
      self.assertEqual(expectedLines[i], actualLines[i])
    self.assertEqual(len(expectedLines), len(actualLines))

  def testStagesReportReleaseTag(self):
    """Tests Release Tag entry in stages report."""

    current_version = "release_tag_string"
    archive_urls = {'board1': 'result_url1',
                    'board2': 'result_url2'}

    # Store off a known set of results and generate a report
    results_lib.Results.Clear()
    results_lib.Results.Record('Pass', results_lib.Results.SUCCESS, time=1)

    results = StringIO.StringIO()

    results_lib.Results.Report(results, archive_urls, current_version)

    expectedResults = (
        "************************************************************\n"
        "** RELEASE VERSION: release_tag_string\n"
        "************************************************************\n"
        "** Stage Results\n"
        "************************************************************\n"
        "** PASS Pass (0:00:01)\n"
        "************************************************************\n"
        "** BUILD ARTIFACTS FOR THIS BUILD CAN BE FOUND AT:\n"
        "**  board1: result_url1\n"
        "@@@STEP_LINK@Artifacts[board1]@result_url1@@@\n"
        "**  board2: result_url2\n"
        "@@@STEP_LINK@Artifacts[board2]@result_url2@@@\n"
        "************************************************************\n")

    expectedLines = expectedResults.split('\n')
    actualLines = results.getvalue().split('\n')

    # Break out the asserts to be per item to make debugging easier
    for i in xrange(len(expectedLines)):
      self.assertEqual(expectedLines[i], actualLines[i])
    self.assertEqual(len(expectedLines), len(actualLines))

  def testSaveCompletedStages(self):
    """Tests that we can save out completed stages."""

    # Run this again to make sure we have the expected results stored
    results_lib.Results.Clear()
    results_lib.Results.Record('Pass', results_lib.Results.SUCCESS)
    results_lib.Results.Record('Fail', self.failException)
    results_lib.Results.Record('Pass2', results_lib.Results.SUCCESS)

    saveFile = StringIO.StringIO()
    results_lib.Results.SaveCompletedStages(saveFile)
    self.assertEqual(saveFile.getvalue(), self._PassString())

  def testRestoreCompletedStages(self):
    """Tests that we can read in completed stages."""

    results_lib.Results.Clear()
    results_lib.Results.RestoreCompletedStages(
        StringIO.StringIO(self._PassString()))

    previous = results_lib.Results.GetPrevious()
    self.assertEqual(previous.keys(), ['Pass'])

  def testRunAfterRestore(self):
    """Tests that we skip previously completed stages."""

    # Fake results_lib.Results.RestoreCompletedStages
    results_lib.Results.Clear()
    results_lib.Results.RestoreCompletedStages(
        StringIO.StringIO(self._PassString()))

    self._runStages()

    # Verify that the results are what we expect.
    expectedResults = [
        ('Pass', results_lib.Results.SUCCESS),
        ('Pass2', results_lib.Results.SUCCESS),
        ('Fail', self.failException)]

    self._verifyRunResults(expectedResults)


class BoardSpecificBuilderStageTest(cros_test_lib.TestCase):

  def testCheckOptions(self):
    """Makes sure options/config settings are setup correctly."""

    parser = cbuildbot._CreateParser()
    (options, _) = parser.parse_args([])

    for attr in dir(stages):
      obj = eval('stages.' + attr)
      if not hasattr(obj, '__base__'):
        continue
      if not obj.__base__ is stages.BoardSpecificBuilderStage:
        continue
      if obj.option_name:
        self.assertTrue(getattr(options, obj.option_name))
      if obj.config_name:
        if not obj.config_name in config._settings:
          self.fail(('cbuildbot_stages.%s.config_name "%s" is missing from '
                     'cbuildbot_config._settings') % (attr, obj.config_name))


if __name__ == '__main__':
  cros_test_lib.main()
