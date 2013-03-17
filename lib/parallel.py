# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module for running cbuildbot stages in the background."""

import collections
import contextlib
import errno
import functools
import logging
import multiprocessing
import os
import Queue
import signal
import sys
import tempfile
import time
import traceback

from chromite.buildbot import cbuildbot_results as results_lib
from chromite.lib import osutils

_PRINT_INTERVAL = 1
_BUFSIZE = 1024
SIGTERM_TIMEOUT = 30
SIGKILL_TIMEOUT = 30


logger = logging.getLogger(__name__)


class BackgroundFailure(results_lib.StepFailure):
  pass


class _BackgroundTask(multiprocessing.Process):
  """Run a task in the background.

  This task may be the 'Run' function from a buildbot stage or just a plain
  function. It will be run in the background. Output from this task is saved
  to a temporary file and is printed when the 'Wait' function is called.
  """

  # The time we give Python to startup and exit.
  STARTUP_TIMEOUT = 60 * 5
  EXIT_TIMEOUT = 60 * 10

  def __init__(self, task, semaphore=None):
    """Create a new _BackgroundTask object.

    If semaphore is supplied, it will be acquired for the duration of the
    steps that are run in the background. This can be used to limit the
    number of simultaneous parallel tasks.
    """
    multiprocessing.Process.__init__(self)
    self._task = task
    self._queue = multiprocessing.Queue()
    self._semaphore = semaphore
    self._started = multiprocessing.Event()
    self._output = None

  def _WaitForStartup(self):
    # TODO(davidjames): Use python-2.7 syntax to simplify this.
    self._started.wait(self.STARTUP_TIMEOUT)
    msg = 'Process failed to start in %d seconds' % self.STARTUP_TIMEOUT
    assert self._started.is_set(), msg

  def Kill(self, sig):
    """Kill process with signal, ignoring if the process is dead.

    Args:
      sig: Signal to send.
    """
    self._WaitForStartup()
    try:
      os.kill(self.pid, sig)
    except OSError as ex:
      if ex.errno != errno.ESRCH:
        raise

  def Cleanup(self, silent=False):
    """Wait for a process to exit."""
    try:
      # Print output from subprocess.
      if not silent and logger.isEnabledFor(logging.DEBUG):
        with open(self._output.name, 'r') as f:
          for line in f:
            logging.debug(line.rstrip('\n'))
    finally:
      # Clean up our temporary file.
      osutils.SafeUnlink(self._output.name)
      self._output.close()
      self._output = None

  def Wait(self):
    """Wait for the task to complete.

    Output from the task is printed as it runs.

    If an exception occurs, return a string containing the traceback.
    """
    try:
      # Flush stdout and stderr to be sure no output is interleaved.
      sys.stdout.flush()
      sys.stderr.flush()

      # File position pointers are shared across processes, so we must open
      # our own file descriptor to ensure output is not lost.
      self._WaitForStartup()
      with open(self._output.name, 'r') as output:
        pos = 0
        more_output = True
        while more_output:
          # Check whether the process is finished.
          try:
            error, results = self._queue.get(True, _PRINT_INTERVAL)

            # Wait for the process to actually exit.
            more_output = False

            # If the child doesn't exit in a timely fashion, kill it.
            self.join(self.EXIT_TIMEOUT)
            if self.exitcode is None:
              _KillChildren([self])
              msg = '%r hung for %r seconds' % (self, self.EXIT_TIMEOUT)
              error = (error + '\n%s' % msg) if error else msg
          except Queue.Empty:
            more_output = True

          # Print output so far.
          output.seek(pos)
          buf = output.read(_BUFSIZE)
          while len(buf) > 0:
            sys.stdout.write(buf)
            pos += len(buf)
            if len(buf) < _BUFSIZE:
              break
            buf = output.read(_BUFSIZE)
          sys.stdout.flush()

      # Propagate any results.
      for result in results:
        results_lib.Results.Record(*result)

    finally:
      self.Cleanup(silent=True)

    # If a traceback occurred, return it.
    return error

  def start(self):
    """Invoke multiprocessing.Process.start after flushing output/err."""
    sys.stdout.flush()
    sys.stderr.flush()
    self._output = tempfile.NamedTemporaryFile(delete=False, bufsize=0,
                                               prefix='chromite-parallel-')
    return multiprocessing.Process.start(self)

  def run(self):
    """Run the list of steps."""
    if self._semaphore is not None:
      self._semaphore.acquire()
    try:
      self._Run()
    finally:
      if self._semaphore is not None:
        self._semaphore.release()

  def _Run(self):
    """Internal method for running the list of steps."""

    # The default handler for SIGINT sometimes forgets to actually raise the
    # exception (and we can reproduce this using unit tests), so we define a
    # custom one instead.
    def kill_us(_sig_num, _frame):
      raise KeyboardInterrupt('SIGINT received')
    signal.signal(signal.SIGINT, kill_us)

    sys.stdout.flush()
    sys.stderr.flush()
    # Send all output to a named temporary file.
    with open(self._output.name, 'w', 0) as output:
      # Back up sys.std{err,out}. These aren't used, but we keep a copy so
      # that they aren't garbage collected. We intentionally don't restore
      # the old stdout and stderr at the end, because we want shutdown errors
      # to also be sent to the same log file.
      _orig_stdout, _orig_stderr = sys.stdout, sys.stderr

      # Replace std{out,err} with unbuffered file objects.
      os.dup2(output.fileno(), sys.__stdout__.fileno())
      os.dup2(output.fileno(), sys.__stderr__.fileno())
      sys.stdout = os.fdopen(sys.__stdout__.fileno(), 'w', 0)
      sys.stderr = os.fdopen(sys.__stderr__.fileno(), 'w', 0)

      error = None
      try:
        self._started.set()
        results_lib.Results.Clear()
        self._task()
      except results_lib.StepFailure as ex:
        error = str(ex)
      except BaseException as ex:
        error = traceback.format_exc()

      sys.stdout.flush()
      sys.stderr.flush()

    results = results_lib.Results.Get()
    self._queue.put((error, results))


def _KillChildren(bg_tasks):
  """Kill a deque of background tasks.

  This is needed to prevent hangs in the case where child processes refuse
  to exit.

  Arguments:
    bg_tasks: A deque, filled with _BackgroundTask objects.
  """

  signals = ((signal.SIGINT, SIGTERM_TIMEOUT),
             (signal.SIGTERM, SIGKILL_TIMEOUT),
             (signal.SIGKILL, None))
  for sig, timeout in signals:
    # Send signal to all tasks.
    for task in bg_tasks:
      task.Kill(sig)

    # Wait for all tasks to exit, if requested.
    if timeout is None:
      for task in bg_tasks:
        task.join()
        task.Cleanup()
      break

    # Wait until timeout expires.
    end_time = time.time() + timeout
    while bg_tasks:
      time_left = end_time - time.time()
      if time_left <= 0:
        break
      task = bg_tasks[0]
      task.join(time_left)
      if task.exitcode is not None:
        task.Cleanup()
        bg_tasks.popleft()


@contextlib.contextmanager
def _ParallelSteps(steps, max_parallel=None, halt_on_error=False):
  """Run a list of functions in parallel.

  This function launches the provided functions in the background, yields,
  and then waits for the functions to exit.

  The output from the functions is saved to a temporary file and printed as if
  they were run in sequence.

  If exceptions occur in the steps, we join together the tracebacks and print
  them after all parallel tasks have finished running. Further, a
  BackgroundFailure is raised with full stack traces of all exceptions.

  Args:
    steps: A list of functions to run.
    max_parallel: The maximum number of simultaneous tasks to run in parallel.
      By default, run all tasks in parallel.
    halt_on_error: After the first exception occurs, halt any running steps,
      and squelch any further output, including any exceptions that might occur.
  """

  semaphore = None
  if max_parallel is not None:
    semaphore = multiprocessing.Semaphore(max_parallel)

  # First, start all the steps.
  bg_tasks = collections.deque()
  for step in steps:
    task = _BackgroundTask(step, semaphore)
    task.start()
    bg_tasks.append(task)

  try:
    yield
  finally:
    # Wait for each step to complete.
    tracebacks = []
    while bg_tasks:
      task = bg_tasks.popleft()
      error = task.Wait()
      if error is not None:
        tracebacks.append(error)
        if halt_on_error:
          break

    # If there are still tasks left, kill them.
    if bg_tasks:
      _KillChildren(bg_tasks)

    # Propagate any exceptions.
    if tracebacks:
      raise BackgroundFailure('\n' + ''.join(tracebacks))


def RunParallelSteps(steps, max_parallel=None, halt_on_error=False):
  """Run a list of functions in parallel.

  This function blocks until all steps are completed.

  The output from the functions is saved to a temporary file and printed as if
  they were run in sequence.

  If exceptions occur in the steps, we join together the tracebacks and print
  them after all parallel tasks have finished running. Further, a
  BackgroundFailure is raised with full stack traces of all exceptions.

  Args:
    steps: A list of functions to run.
    max_parallel: The maximum number of simultaneous tasks to run in parallel.
      By default, run all tasks in parallel.
    halt_on_error: After the first exception occurs, halt any running steps,
      and squelch any further output, including any exceptions that might occur.

  Example:
    # This snippet will execute in parallel:
    #   somefunc()
    #   anotherfunc()
    #   funcfunc()
    steps = [somefunc, anotherfunc, funcfunc]
    RunParallelSteps(steps)
    # Blocks until all calls have completed.
  """
  with _ParallelSteps(steps, max_parallel=max_parallel,
                      halt_on_error=halt_on_error):
    pass


class _AllTasksComplete(object):
  """Sentinel object to indicate that all tasks are complete."""


def _TaskRunner(queue, task, onexit=None):
  """Run task(*input) for each input in the queue.

  Returns when it encounters an _AllTasksComplete object on the queue.
  If exceptions occur, save them off and re-raise them as a
  BackgroundFailure once we've finished processing the items in the queue.

  Args:
    queue: A queue of tasks to run. Add tasks to this queue, and they will
      be run.
    task: Function to run on each queued input.
    onexit: Function to run after all inputs are processed.
  """
  tracebacks = []
  while True:
    # Wait for a new item to show up on the queue. This is a blocking wait,
    # so if there's nothing to do, we just sit here.
    x = queue.get()
    if isinstance(x, _AllTasksComplete):
      # All tasks are complete, so we should exit.
      break

    # If no tasks failed yet, process the remaining tasks.
    if not tracebacks:
      try:
        task(*x)
      except BaseException:
        tracebacks.append(traceback.format_exc())

  # Run exit handlers.
  if onexit:
    onexit()

  # Propagate any exceptions.
  if tracebacks:
    raise BackgroundFailure('\n' + ''.join(tracebacks))


@contextlib.contextmanager
def BackgroundTaskRunner(task, queue=None, processes=None, onexit=None):
  """Run the specified task on each queued input in a pool of processes.

  This context manager starts a set of workers in the background, who each
  wait for input on the specified queue. These workers run task(*input) for
  each input on the queue.

  The output from these tasks is saved to a temporary file. When control
  returns to the context manager, the background output is printed in order,
  as if the tasks were run in sequence.

  If exceptions occur in the steps, we join together the tracebacks and print
  them after all parallel tasks have finished running. Further, a
  BackgroundFailure is raised with full stack traces of all exceptions.

  Example:
    # This will run somefunc('small', 'cow') in the background
    # while "more random stuff" is being executed.

    def somefunc(arg1, arg2):
      ...
    ...
    with BackgroundTaskRunner(somefunc) as queue:
      ... do random stuff ...
      queue.put(['small', 'cow'])
      ... do more random stuff ...
    # Exiting the with statement will block until all calls have completed.

  Args:
    task: Function to run on each queued input.
    queue: A queue of tasks to run. Add tasks to this queue, and they will
      be run in the background.  If None, one will be created on the fly.
    processes: Number of processes to launch.
    onexit: Function to run in each background process after all inputs are
      processed.
  """

  if queue is None:
    queue = multiprocessing.Queue()

  if not processes:
    processes = multiprocessing.cpu_count()

  steps = [functools.partial(_TaskRunner, queue, task, onexit)] * processes
  with _ParallelSteps(steps):
    try:
      yield queue
    finally:
      for _ in xrange(processes):
        queue.put(_AllTasksComplete())


def RunTasksInProcessPool(task, inputs, processes=None, onexit=None):
  """Run the specified function with each supplied input in a pool of processes.

  This function runs task(*x) for x in inputs in a pool of processes. This
  function blocks until all tasks are completed.

  The output from these tasks is saved to a temporary file. When control
  returns to the context manager, the background output is printed in order,
  as if the tasks were run in sequence.

  If exceptions occur in the steps, we join together the tracebacks and print
  them after all parallel tasks have finished running. Further, a
  BackgroundFailure is raised with full stack traces of all exceptions.

  Example:
    # This snippet will execute in parallel:
    #   somefunc('hi', 'fat', 'code')
    #   somefunc('foo', 'bar', 'cow')

    def somefunc(arg1, arg2, arg3):
      ...
    ...
    inputs = [
      ['hi', 'fat', 'code'],
      ['foo', 'bar', 'cow'],
    ]
    RunTasksInProcessPool(somefunc, inputs)
    # Blocks until all calls have completed.

  Args:
    task: Function to run on each input.
    inputs: List of inputs.
    processes: Number of processes, at most, to launch.
    onexit: Function to run in each background process after all inputs are
      processed.
  """

  if not processes:
    processes = min(multiprocessing.cpu_count(), len(inputs))

  with BackgroundTaskRunner(task, processes=processes, onexit=onexit) as queue:
    for x in inputs:
      queue.put(x)
