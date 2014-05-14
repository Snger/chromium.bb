# Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module containing the completion stages."""

import logging

from chromite.buildbot import cbuildbot_commands as commands
from chromite.buildbot import cbuildbot_config
from chromite.buildbot import cbuildbot_results as results_lib
from chromite.buildbot import constants
from chromite.buildbot import manifest_version
from chromite.buildbot import portage_utilities
from chromite.buildbot import validation_pool
from chromite.buildbot.stages import generic_stages
from chromite.buildbot.stages import sync_stages
from chromite.lib import cros_build_lib
from chromite.lib import git


class ManifestVersionedSyncCompletionStage(
    generic_stages.ForgivingBuilderStage):
  """Stage that records board specific results for a unique manifest file."""

  option_name = 'sync'

  def __init__(self, builder_run, sync_stage, success, **kwargs):
    super(ManifestVersionedSyncCompletionStage, self).__init__(
        builder_run, **kwargs)
    self.sync_stage = sync_stage
    self.success = success
    # Message that can be set that well be sent along with the status in
    # UpdateStatus.
    self.message = None

  def PerformStage(self):
    self._run.attrs.manifest_manager.UpdateStatus(
        success=self.success, message=self.message,
        dashboard_url=self.ConstructDashboardURL())


class ImportantBuilderFailedException(results_lib.StepFailure):
  """Exception thrown when an important build fails to build."""


class MasterSlaveSyncCompletionStage(ManifestVersionedSyncCompletionStage):
  """Stage that records whether we passed or failed to build/test manifest."""

  def __init__(self, *args, **kwargs):
    super(MasterSlaveSyncCompletionStage, self).__init__(*args, **kwargs)
    self._slave_statuses = {}

  def _FetchSlaveStatuses(self):
    """Fetch and return build status for slaves of this build.

    If this build is not a master then return just the status of this build.

    Returns:
      A dict with "bot id" keys and BuilderStatus objects for values.  All keys
      will have valid BuilderStatus values, but builders that never started
      will have a BuilderStatus with status MISSING.
    """
    if not self._run.config.master:
      # This is a slave build, so return the status for this build.
      if self._run.options.debug:
        # In debug mode, nothing is uploaded to Google Storage, so we bypass
        # the extra hop and just look at what we have locally.
        status = manifest_version.BuilderStatus.GetCompletedStatus(self.success)
        status_obj = manifest_version.BuilderStatus(status, self.message)
        return {self._bot_id: status_obj}
      else:
        # Slaves only need to look at their own status.
        return self._run.attrs.manifest_manager.GetBuildersStatus(
            [self._bot_id])
    else:
      # This is a master build, so return the statuses for all its slaves.

      # Wait for slaves to finish, unless this is a debug run.
      wait_for_results = not self._run.options.debug

      builders = self._GetSlaveConfigs()
      builder_names = [b['name'] for b in builders]

      manager = self._run.attrs.manifest_manager
      if sync_stages.MasterSlaveSyncStage.sub_manager:
        manager = sync_stages.MasterSlaveSyncStage.sub_manager

      return manager.GetBuildersStatus(builder_names, wait_for_results)

  def _AbortCQHWTests(self):
    """Abort any HWTests started by the CQ."""
    if (cbuildbot_config.IsCQType(self._run.config.build_type) and
        self._run.manifest_branch == 'master'):
      version = self._run.GetVersion()
      if not commands.HaveCQHWTestsBeenAborted(version):
        commands.AbortCQHWTests(version, self._run.options.debug)

  def _HandleStageException(self, exc_info):
    """Decide whether an exception should be treated as fatal."""
    # Besides the master, the completion stages also run on slaves, to report
    # their status back to the master. If the build failed, they throw an
    # exception here. For slave builders, marking this stage 'red' would be
    # redundant, since the build itself would already be red. In this case,
    # report a warning instead.
    # pylint: disable=W0212
    exc_type = exc_info[0]
    if (issubclass(exc_type, ImportantBuilderFailedException) and
        not self._run.config.master):
      return self._HandleExceptionAsWarning(exc_info)
    else:
      # In all other cases, exceptions should be treated as fatal. To
      # implement this, we bypass ForgivingStage and call
      # generic_stages.BuilderStage._HandleStageException explicitly.
      return generic_stages.BuilderStage._HandleStageException(self, exc_info)

  def HandleSuccess(self):
    """Handle a successful build.

    This function is called whenever the cbuildbot run is successful.
    For the master, this will only be called when all slave builders
    are also successful. This function may be overridden by subclasses.
    """
    # We only promote for the pfq, not chrome pfq.
    # TODO(build): Run this logic in debug mode too.
    if (not self._run.options.debug and
        cbuildbot_config.IsPFQType(self._run.config.build_type) and
        self._run.config.master and
        self._run.manifest_branch == 'master' and
        self._run.config.build_type != constants.CHROME_PFQ_TYPE):
      self._run.attrs.manifest_manager.PromoteCandidate()
      if sync_stages.MasterSlaveSyncStage.sub_manager:
        sync_stages.MasterSlaveSyncStage.sub_manager.PromoteCandidate()

  def HandleFailure(self, failing, inflight):
    """Handle a build failure.

    This function is called whenever the cbuildbot run fails.
    For the master, this will be called when any slave fails or times
    out. This function may be overridden by subclasses.

    Args:
      failing: The names of the failing builders.
      inflight: The names of the builders that are still running.
    """
    if failing:
      self.HandleValidationFailure(failing)
    elif inflight:
      self.HandleValidationTimeout(inflight)

  def HandleValidationFailure(self, failing):
    cros_build_lib.PrintBuildbotStepWarnings()
    cros_build_lib.Warning('\n'.join([
        'The following builders failed with this manifest:',
        ', '.join(sorted(failing)),
        'Please check the logs of the failing builders for details.']))

  def HandleValidationTimeout(self, inflight_statuses):
    cros_build_lib.PrintBuildbotStepWarnings()
    cros_build_lib.Warning('\n'.join([
        'The following builders took too long to finish:',
        ', '.join(sorted(inflight_statuses)),
        'Please check the logs of these builders for details.']))

  def PerformStage(self):
    # Upload our pass/fail status to Google Storage.
    self._run.attrs.manifest_manager.UploadStatus(
        success=self.success, message=self.message,
        dashboard_url=self.ConstructDashboardURL())

    statuses = self._FetchSlaveStatuses()
    self._slave_statuses = statuses
    no_stat = set(builder for builder, status in statuses.iteritems()
                  if status.Missing())
    failing = set(builder for builder, status in statuses.iteritems()
                  if status.Failed())
    inflight = set(builder for builder, status in statuses.iteritems()
                   if status.Inflight())

    # If all the failing or inflight builders were sanity checkers
    # then ignore the failure.
    fatal = self._IsFailureFatal(failing, inflight, no_stat)

    if fatal:
      self._AnnotateFailingBuilders(failing, inflight, no_stat, statuses)
      self.HandleFailure(failing, inflight)
      raise ImportantBuilderFailedException()
    else:
      self.HandleSuccess()

  def _IsFailureFatal(self, failing, inflight, no_stat):
    """Returns a boolean indicating whether the build should fail.

    Args:
      failing: Set of builder names of slave builders that failed.
      inflight: Set of builder names of slave builders that are inflight
      no_stat: Set of builder names of slave builders that had status None.

    Returns:
      True if any of the failing or inflight builders are not sanity check
      builders for this master, or if there were any non-sanity-check builders
      with status None.
    """
    sanity_builders = self._run.config.sanity_check_slaves or []
    sanity_builders = set(sanity_builders)
    return not sanity_builders.issuperset(failing | inflight | no_stat)

  def _AnnotateFailingBuilders(self, failing, inflight, no_stat, statuses):
    """Add annotations that link to either failing or inflight builders.

    Adds buildbot links to failing builder dashboards. If no builders are
    failing, adds links to inflight builders. Adds step text for builders
    with status None.

    Args:
      failing: Set of builder names of slave builders that failed.
      inflight: Set of builder names of slave builders that are inflight.
      no_stat: Set of builder names of slave builders that had status None.
      statuses: A builder-name->status dictionary, which will provide
                the dashboard_url values for any links.
    """
    builders_to_link = failing or inflight or []
    for builder in builders_to_link:
      if statuses[builder].dashboard_url:
        text = builder
        if statuses[builder].message:
          text = '%s: %s' % (builder, statuses[builder].message.reason)

        cros_build_lib.PrintBuildbotLink(text, statuses[builder].dashboard_url)

    for builder in no_stat:
      cros_build_lib.PrintBuildbotStepText('%s did not start.' % builder)

  def GetSlaveStatuses(self):
    """Returns cached slave status results.

    Cached results are populated during PerformStage, so this function
    should only be called after PerformStage has returned.

    Returns:
      A dictionary from build names to manifest_version.BuilderStatus
      builder status objects.
    """
    return self._slave_statuses


class CommitQueueCompletionStage(MasterSlaveSyncCompletionStage):
  """Commits or reports errors to CL's that failed to be validated."""

  def _HandleStageException(self, exc_info):
    """Decide whether an exception should be treated as fatal."""
    exc_type = exc_info[0]
    if isinstance(
        exc_type, validation_pool.FailedToSubmitAllChangesNonFatalException):
      return self._HandleExceptionAsWarning(exc_info)
    else:
      return super(CommitQueueCompletionStage, self)._HandleStageException(
          exc_info)

  def HandleSuccess(self):
    if self._run.config.master:
      self.sync_stage.pool.SubmitPool()
      # After submitting the pool, update the commit hashes for uprevved
      # ebuilds.
      manifest = git.ManifestCheckout.Cached(self._build_root)
      portage_utilities.EBuild.UpdateCommitHashesForChanges(
          self.sync_stage.pool.changes, self._build_root, manifest)
      if cbuildbot_config.IsPFQType(self._run.config.build_type):
        super(CommitQueueCompletionStage, self).HandleSuccess()

  def HandleFailure(self, failing, inflight):
    """Handle a build failure or timeout in the Commit Queue.

    This function performs any tasks that need to happen when the Commit Queue
    fails:
      - Abort the HWTests if necessary.
      - Push any CLs that indicate that they don't care about this failure.
      - Reject the rest of the changes, but only if the sanity check builders
        did NOT fail.

    See MasterSlaveSyncCompletionStage.HandleFailure.

    Args:
      failing: Names of the builders that failed.
      inflight: Names of the builders that timed out.
    """
    # Print out the status about what builds failed or not.
    MasterSlaveSyncCompletionStage.HandleFailure(self, failing, inflight)

    # Abort hardware tests to save time if we have already seen a failure,
    # except in the case where the only failure is a hardware test failure.
    #
    # When we're debugging hardware test failures, it's useful to see the
    # results on all platforms, to see if the failure is platform-specific.
    tracebacks = results_lib.Results.GetTracebacks()
    if not self.success and self._run.config['important']:
      if len(tracebacks) != 1 or tracebacks[0].failed_prefix != 'HWTest':
        self._AbortCQHWTests()

    if self._run.config.master:
      # Even if there was a failure, we can submit the changes that indicate
      # that they don't care about this failure.

      # messages is a list of ValidationFailedMessage or NoneType objects.
      messages = [self._slave_statuses[x].message for x in failing]

      if failing and not inflight:
        tracebacks = set()
        for message in messages:
          # If there are no tracebacks, that means that the builder did not
          # report its status properly. Don't submit anything.
          if not message or not message.tracebacks:
            break
          tracebacks.update(message.tracebacks)
        else:
          rejected = self.sync_stage.pool.SubmitPartialPool(tracebacks)
          self.sync_stage.pool.changes = rejected

      sanity_slave_failed = self._SanitySlaveFailed(
          self._run.config.sanity_check_slaves, self._slave_statuses)
      infrastructure_failed = self._OnlyInfrastructureFailures(messages)
      if sanity_slave_failed:
        logging.warning('Detected that a sanity-check builder failed. Will not '
                        'reject patches.')
      if infrastructure_failed:
        logging.warning('The build failed purely due to infrastructure '
                        'issue(s). Will not reject patches')

      sanity = not (sanity_slave_failed or infrastructure_failed)

      if failing:
        self.sync_stage.pool.HandleValidationFailure(messages, sanity=sanity)
      elif inflight:
        self.sync_stage.pool.HandleValidationTimeout(sanity=sanity)

  @staticmethod
  def _OnlyInfrastructureFailures(messages):
    """Returns true if all failures are infrasctructure failures.

    Args:
      messages: A list of ValidationFailedMessage objects from the
        failed slaves.

    Returns:
      True if all failures are of the InfrastructureFailure type.
    """
    return all([x.IsInfrastructureFailure() for x in messages])

  @staticmethod
  def _SanitySlaveFailed(sanity_check_slaves, slave_statuses):
    """Returns true if any sanity check slaves failed.

    Args:
      sanity_check_slaves: Names of slave builders that are "sanity check"
        builders for the current master.
      slave_statuses: Dict of BuilderStatus objects by builder name keys.

    Returns:
      True if no sanity builders ran and failed.
    """
    sanity_check_slaves = sanity_check_slaves or []
    return any([x in slave_statuses and slave_statuses[x].Failed() for
                x in sanity_check_slaves])

  def PerformStage(self):
    # - If the build failed, and the builder was important, fetch a message
    # listing the patches which failed to be validated. This message is sent
    # along with the failed status to the master to indicate a failure.
    # - This is skipped when sync_stage did not apply a validation pool. For
    # instance on builders with do_not_apply_cq_patches=True, sync_stage will
    # be a sync_stages.MasterSlaveSyncStage and not have a |pool| attribute.
    if (not self.success and self._run.config.important
        and hasattr(self.sync_stage, 'pool')):
      self.message = self.sync_stage.pool.GetValidationFailedMessage()

    super(CommitQueueCompletionStage, self).PerformStage()

    self._run.attrs.manifest_manager.UpdateStatus(
        success=self.success, message=self.message,
        dashboard_url=self.ConstructDashboardURL())


class PreCQCompletionStage(generic_stages.BuilderStage):
  """Reports the status of a trybot run to Google Storage and Gerrit."""

  def __init__(self, builder_run, sync_stage, success, **kwargs):
    super(PreCQCompletionStage, self).__init__(builder_run, **kwargs)
    self.sync_stage = sync_stage
    self.success = success

  def PerformStage(self):
    # Update Gerrit and Google Storage with the Pre-CQ status.
    if self.success:
      self.sync_stage.pool.HandlePreCQSuccess()
    else:
      message = self.sync_stage.pool.GetValidationFailedMessage()
      self.sync_stage.pool.HandleValidationFailure([message])


class PublishUprevChangesStage(generic_stages.BuilderStage):
  """Makes uprev changes from pfq live for developers."""

  def __init__(self, builder_run, success, **kwargs):
    """Constructor.

    Args:
      builder_run: BuilderRun object.
      success: Boolean indicating whether the build succeeded.
    """
    super(PublishUprevChangesStage, self).__init__(builder_run, **kwargs)
    self.success = success

  def PerformStage(self):
    overlays, push_overlays = self._ExtractOverlays()
    assert push_overlays, 'push_overlays must be set to run this stage'

    # If the build failed, we don't want to push our local changes, because
    # they might include some CLs that failed. Instead, clean up our local
    # changes and do a fresh uprev.
    if not self.success:
      # Clean up our root and sync down the latest changes that were
      # submitted.
      commands.BuildRootGitCleanup(self._build_root)

      # Sync down the latest changes we have submitted.
      if self._run.options.sync:
        next_manifest = self._run.config.manifest
        repo = self.GetRepoRepository()
        repo.Sync(next_manifest)

      # Commit an uprev locally.
      if self._run.options.uprev and self._run.config.uprev:
        commands.UprevPackages(self._build_root, self._boards, overlays)

    # Push the uprev commit.
    commands.UprevPush(self._build_root, push_overlays, self._run.options.debug)
