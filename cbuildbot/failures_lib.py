# Copyright 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Classes of failure types."""

import collections
import sys
import traceback

from chromite.lib import cros_build_lib


class StepFailure(Exception):
  """StepFailure exceptions indicate that a cbuildbot step failed.

  Exceptions that derive from StepFailure should meet the following
  criteria:
    1) The failure indicates that a cbuildbot step failed.
    2) The necessary information to debug the problem has already been
       printed in the logs for the stage that failed.
    3) __str__() should be brief enough to include in a Commit Queue
       failure message.
  """
  def __init__(self, message='', possibly_flaky=False):
    """Constructor.

    Args:
      message: An error message.
      possibly_flaky: Whether this failure might be flaky.
    """
    Exception.__init__(self, message)
    self.possibly_flaky = possibly_flaky
    self.args = (message, possibly_flaky)

  def __str__(self):
    """Stringify the message."""
    return self.message


# A namedtuple to hold information of an exception.
ExceptInfo = collections.namedtuple(
    'ExceptInfo', ['type', 'str', 'traceback'])


def CreateExceptInfo(exception, tb):
  """Creates a list of ExceptInfo objects from |exception| and |tb|.

  Creates an ExceptInfo object from |exception| and |tb|. If
  |exception| is a CompoundFailure with non-empty list of exc_infos,
  simly returns exception.exc_infos. Note that we do not preserve type
  of |exception| in this case.

  Args:
    exception: The exception.
    tb: The textual traceback.

  Returns:
    A list of ExceptInfo objects.
  """
  if issubclass(exception.__class__, CompoundFailure) and exception.exc_infos:
    return exception.exc_infos

  return [ExceptInfo(exception.__class__, str(exception), tb)]


class CompoundFailure(StepFailure):
  """An exception that contains a list of ExceptInfo objects."""

  def __init__(self, message='', exc_infos=None, possibly_flaky=False):
    """Initializes an CompoundFailure instance.

    Args:
      message: A string describing the failure.
      exc_infos: A list of ExceptInfo objects.
      possibly_flaky: Whether this failure might be flaky.
    """
    self.exc_infos = exc_infos if exc_infos else []
    if not message:
      # By default, print all stored ExceptInfo objects.
      message = '\n'.join(['%s: %s\n%s' % e for e in self.exc_infos])

    super(CompoundFailure, self).__init__(message=message,
                                          possibly_flaky=possibly_flaky)

  def HasEmptyList(self):
    """Returns True if self.exc_infos is empty."""
    return not bool(self.exc_infos)

  def HasFailureType(self, cls):
    """Returns True if any of the failures matches |cls|."""
    return any(issubclass(x.type, cls) for x in self.exc_infos)

  def MatchesFailureType(self, cls):
    """Returns True if all failures matches |cls|."""
    return (not self.HasEmptyList() and
            all(issubclass(x.type, cls) for x in self.exc_infos))

  def HasFatalFailure(self, whitelist=None):
    """Determine if there are non-whitlisted failures.

    Args:
      whitelist: A list of whitelisted exception types.

    Returns:
      Returns True if any failure is not in |whitelist|.
    """
    if not whitelist:
      return not self.HasEmptyList()

    for ex in self.exc_infos:
      if all(not issubclass(ex.type, cls) for cls in whitelist):
        return True

    return False


class SetFailureType(object):
  """A wrapper to re-raise the exception as the pre-set type."""

  def __init__(self, category_exception, source_exception=None):
    """Initializes the decorator.

    Args:
      category_exception: The exception type to re-raise as. It must be
        a subclass of CompoundFailure.
      source_exception: The exception types to re-raise. By default, re-raise
        all Exception classes.
    """
    assert issubclass(category_exception, CompoundFailure)
    self.category_exception = category_exception
    self.source_exception = source_exception
    if self.source_exception is None:
      self.source_exception = Exception

  def __call__(self, functor):
    """Returns a wrapped function."""
    def wrapped_functor(*args, **kwargs):
      try:
        return functor(*args, **kwargs)
      except self.source_exception:
        # Get the information about the original exception.
        exc_type, exc_value, _ = sys.exc_info()
        exc_traceback = traceback.format_exc()
        if issubclass(exc_type, self.category_exception):
          # Do not re-raise if the exception is a subclass of the set
          # exception type because it offers more information.
          raise
        else:
          exc_infos = CreateExceptInfo(exc_value, exc_traceback)
          raise self.category_exception(exc_infos=exc_infos)

    return wrapped_functor


class RetriableStepFailure(StepFailure):
  """This exception is thrown when a step failed, but should be retried."""


class BuildScriptFailure(StepFailure):
  """This exception is thrown when a build command failed.

  It is intended to provide a shorter summary of what command failed,
  for usage in failure messages from the Commit Queue, so as to ensure
  that developers aren't spammed with giant error messages when common
  commands (e.g. build_packages) fail.
  """

  def __init__(self, exception, shortname, possibly_flaky=False):
    """Construct a BuildScriptFailure object.

    Args:
      exception: A RunCommandError object.
      shortname: Short name for the command we're running.
      possibly_flaky: Whether this failure might be flaky.
    """
    StepFailure.__init__(self, possibly_flaky=possibly_flaky)
    assert isinstance(exception, cros_build_lib.RunCommandError)
    self.exception = exception
    self.shortname = shortname
    self.args = (exception, shortname, possibly_flaky)

  def __str__(self):
    """Summarize a build command failure briefly."""
    result = self.exception.result
    if result.returncode:
      return '%s failed (code=%s)' % (self.shortname, result.returncode)
    else:
      return self.exception.msg


class PackageBuildFailure(BuildScriptFailure):
  """This exception is thrown when packages fail to build."""

  def __init__(self, exception, shortname, failed_packages):
    """Construct a PackageBuildFailure object.

    Args:
      exception: The underlying exception.
      shortname: Short name for the command we're running.
      failed_packages: List of packages that failed to build.
    """
    BuildScriptFailure.__init__(self, exception, shortname)
    self.failed_packages = set(failed_packages)
    self.args = (exception, shortname, failed_packages)

  def __str__(self):
    return ('Packages failed in %s: %s'
            % (self.shortname, ' '.join(sorted(self.failed_packages))))


class InfrastructureFailure(CompoundFailure):
  """Raised if a stage fails due to infrastructure issues."""


# Chrome OS Test Lab failures.
class TestLabFailure(InfrastructureFailure):
  """Raised if a stage fails due to hardware lab infrastructure issues."""


# Gerrit-on-Borg failures.
class GoBFailure(InfrastructureFailure):
  """Raised if a stage fails due to Gerrit-on-Borg (GoB) issues."""


class GoBQueryFailure(GoBFailure):
  """Raised if a stage fails due to Gerrit-on-Borg (GoB) query errors."""


class GoBSubmitFailure(GoBFailure):
  """Raised if a stage fails due to Gerrit-on-Borg (GoB) submission errors."""


class GoBFetchFailure(GoBFailure):
  """Raised if a stage fails due to Gerrit-on-Borg (GoB) fetch errors."""


# Google Storage failures.
class GSFailure(InfrastructureFailure):
  """Raised if a stage fails due to Google Storage (GS) issues."""


class GSUploadFailure(GSFailure):
  """Raised if a stage fails due to Google Storage (GS) upload issues."""


class GSDownloadFailure(GSFailure):
  """Raised if a stage fails due to Google Storage (GS) download issues."""


# Builder failures.
class BuilderFailure(InfrastructureFailure):
  """Raised if a stage fails due to builder issues."""


# Crash collection service failures.
class CrashCollectionFailure(InfrastructureFailure):
  """Raised if a stage fails due to crash collection services."""
