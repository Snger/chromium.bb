# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import gc
import logging
import time
import unittest

from telemetry.internal.platform import linux_based_platform_backend
from telemetry.internal.platform import tracing_agent
from telemetry.internal.platform import tracing_controller_backend
from telemetry.timeline import tracing_config
from telemetry.timeline import trace_data


class PlatformBackend(linux_based_platform_backend.LinuxBasedPlatformBackend):
  # pylint: disable=abstract-method
  def __init__(self):
    super(PlatformBackend, self).__init__()
    self._mock_files = {}

  def GetOSName(self):
    return 'android'


class FakeTracingAgentBase(tracing_agent.TracingAgent):
  def __init__(self, platform, start=True, clock_sync=True):
    super(FakeTracingAgentBase, self).__init__(platform)
    self._start = start
    self._clock_sync = clock_sync
    self._sync_seen = False

  def StartAgentTracing(self, config, timeout):
    return self._start

  def StopAgentTracing(self, trace_data_builder):
    pass

  def SupportsExplicitClockSync(self):
    return self._clock_sync

  def RecordClockSyncMarker(self, sync_id):
    if not self._clock_sync:
      raise NotImplementedError
    self._sync_seen = True


class FakeTracingAgentStartAndClockSync(FakeTracingAgentBase):
  def __init__(self, platform):
    super(FakeTracingAgentStartAndClockSync, self).__init__(platform,
                                                            start=True,
                                                            clock_sync=True)


class FakeTracingAgentStartAndNoClockSync(FakeTracingAgentBase):
  def __init__(self, platform):
    super(FakeTracingAgentStartAndNoClockSync, self).__init__(platform,
                                                            start=True,
                                                            clock_sync=False)


class FakeTracingAgentNoStartAndNoClockSync(FakeTracingAgentBase):
  def __init__(self, platform):
    super(FakeTracingAgentNoStartAndNoClockSync, self).__init__(platform,
                                                            start=False,
                                                            clock_sync=False)


class FakeTracingAgentNoStartAndClockSync(FakeTracingAgentBase):
  def __init__(self, platform):
    super(FakeTracingAgentNoStartAndClockSync, self).__init__(platform,
                                                              start=False,
                                                              clock_sync=True)


class TracingControllerBackendTest(unittest.TestCase):
  def _getControllerLogAsList(self, data):
    return data.GetEventsFor(trace_data.TELEMETRY_PART)

  def _getSyncCount(self, data):
    return len([entry for entry in self._getControllerLogAsList(data)
                if entry.get('name') == 'clock_sync'])

  def setUp(self):
    self.platform = PlatformBackend()
    self.controller = (
        tracing_controller_backend.TracingControllerBackend(self.platform))
    self.controller._supported_agents_classes = [FakeTracingAgentBase]
    self.config = tracing_config.TracingConfig()
    self.controller_log = self.controller._trace_log

  def tearDown(self):
    if self.controller.is_tracing_running:
      self.controller.StopTracing()

  def testStartTracing(self):
    self.assertFalse(self.controller.is_tracing_running)
    self.assertTrue(self.controller.StartTracing(self.config, 30))
    self.assertTrue(self.controller.is_tracing_running)

  def testDoubleStartTracing(self):
    self.assertFalse(self.controller.is_tracing_running)
    self.assertTrue(self.controller.StartTracing(self.config, 30))
    self.assertTrue(self.controller.is_tracing_running)
    self.assertFalse(self.controller.StartTracing(self.config, 30))

  def testStopTracingNotStarted(self):
    with self.assertRaises(AssertionError):
      self.controller.StopTracing()

  def testStopTracing(self):
    self.assertFalse(self.controller.is_tracing_running)
    self.assertTrue(self.controller.StartTracing(self.config, 30))
    self.assertTrue(self.controller.is_tracing_running)
    data = self.controller.StopTracing()
    self.assertEqual(self._getSyncCount(data), 1)
    self.assertFalse(self.controller.is_tracing_running)
    self.assertEqual(self.controller._trace_log, None)

  def testDoubleStopTracing(self):
    self.assertFalse(self.controller.is_tracing_running)
    self.assertTrue(self.controller.StartTracing(self.config, 30))
    self.assertTrue(self.controller.is_tracing_running)
    self.controller.StopTracing()
    self.assertFalse(self.controller.is_tracing_running)
    with self.assertRaises(AssertionError):
      self.controller.StopTracing()

  def testMultipleStartStop(self):
    self.assertFalse(self.controller.is_tracing_running)
    self.assertTrue(self.controller.StartTracing(self.config, 30))
    self.assertTrue(self.controller.is_tracing_running)
    data = self.controller.StopTracing()
    self.assertEqual(self._getSyncCount(data), 1)
    sync_event_one = [x for x in self._getControllerLogAsList(data)
                      if x.get('name') == 'clock_sync'][0]
    self.assertFalse(self.controller.is_tracing_running)
    self.assertEqual(self.controller._trace_log, None)
    # Run 2
    self.assertTrue(self.controller.StartTracing(self.config, 30))
    self.assertTrue(self.controller.is_tracing_running)
    data = self.controller.StopTracing()
    self.assertEqual(self._getSyncCount(data), 1)
    sync_event_two = [x for x in self._getControllerLogAsList(data)
                      if x.get('name') == 'clock_sync'][0]
    self.assertFalse(self.controller.is_tracing_running)
    self.assertFalse(self.controller._trace_log, None)
    # Test difference between events
    self.assertNotEqual(sync_event_one, sync_event_two)

  def testNoWorkingAgents(self):
    self.controller._supported_agents_classes = [
        FakeTracingAgentNoStartAndNoClockSync
    ]
    self.assertFalse(self.controller.is_tracing_running)
    self.assertTrue(self.controller.StartTracing(self.config, 30))
    self.assertTrue(self.controller.is_tracing_running)
    self.assertEquals(self.controller._active_agents_instances, [])
    data = self.controller.StopTracing()
    self.assertEqual(self._getSyncCount(data), 0)
    self.assertFalse(self.controller.is_tracing_running)

  def testNoClockSyncSupport(self):
    self.controller._supported_agents_classes = [
        FakeTracingAgentStartAndNoClockSync,
        FakeTracingAgentNoStartAndNoClockSync,
    ]
    self.assertFalse(self.controller.is_tracing_running)
    self.assertTrue(self.controller.StartTracing(self.config, 30))
    self.assertTrue(self.controller.is_tracing_running)
    data = self.controller.StopTracing()
    self.assertFalse(self.controller.is_tracing_running)
    self.assertEquals(self._getSyncCount(data), 0)

  def testClockSyncSupport(self):
    self.controller._supported_agents_classes = [
        FakeTracingAgentStartAndClockSync,
        FakeTracingAgentStartAndClockSync,
        FakeTracingAgentStartAndNoClockSync,
        FakeTracingAgentNoStartAndClockSync,
        FakeTracingAgentNoStartAndNoClockSync
    ]
    self.assertFalse(self.controller.is_tracing_running)
    self.assertTrue(self.controller.StartTracing(self.config, 30))
    self.assertTrue(self.controller.is_tracing_running)
    self.assertEquals(len(self.controller._active_agents_instances), 3)
    # No sync event before running StopTracing().
    data = self.controller.StopTracing()
    self.assertFalse(self.controller.is_tracing_running)
    self.assertEquals(self._getSyncCount(data), 2)

  def testMultipleAgents(self):
    self.controller._supported_agents_classes = [
        FakeTracingAgentStartAndClockSync,
        FakeTracingAgentStartAndClockSync,
        FakeTracingAgentNoStartAndClockSync,
        FakeTracingAgentNoStartAndClockSync,
        FakeTracingAgentNoStartAndNoClockSync,
        FakeTracingAgentNoStartAndNoClockSync,
        FakeTracingAgentStartAndNoClockSync,
        FakeTracingAgentStartAndNoClockSync
    ]
    self.assertFalse(self.controller.is_tracing_running)
    self.assertTrue(self.controller.StartTracing(self.config, 30))
    self.assertTrue(self.controller.is_tracing_running)
    self.assertEquals(len(self.controller._active_agents_instances), 4)
    data = self.controller.StopTracing()
    self.assertFalse(self.controller.is_tracing_running)
    self.assertEquals(self._getSyncCount(data), 2)


  def testGenerateRandomSyncId(self):
    ids = []
    for _ in xrange(1000):
      i = self.controller._GenerateClockSyncId()
      self.assertFalse(i in ids)
      ids.append(i)

  def testRecordIssuerClockSyncMarker(self):
    sync_id = 'test_id'
    ts = time.time()
    self.controller._supported_agents_classes = [
        FakeTracingAgentNoStartAndNoClockSync,
        FakeTracingAgentStartAndNoClockSync
    ]
    self.assertTrue(self.controller.StartTracing(self.config, 30))
    self.controller._RecordIssuerClockSyncMarker(sync_id, ts)
    data = self.controller.StopTracing()
    self.assertFalse(self.controller.is_tracing_running)
    self.assertEquals(self._getSyncCount(data), 1)
    log = self._getControllerLogAsList(data)
    for entry in log:
      if entry.get('name') == 'clock_sync':
        self.assertEqual(entry['args']['sync_id'], sync_id)
        self.assertEqual(entry['args']['issue_ts'], ts * 1000000)

  def testIssueClockSyncMarker(self):
    self.controller._supported_agents_classes = [
        FakeTracingAgentStartAndClockSync,
        FakeTracingAgentStartAndClockSync,
        FakeTracingAgentNoStartAndClockSync,
        FakeTracingAgentNoStartAndClockSync,
        FakeTracingAgentNoStartAndNoClockSync,
        FakeTracingAgentNoStartAndNoClockSync,
        FakeTracingAgentStartAndNoClockSync,
        FakeTracingAgentStartAndNoClockSync
    ]
    self.assertFalse(self.controller.is_tracing_running)
    self.assertTrue(self.controller.StartTracing(self.config, 30))
    self.assertTrue(self.controller.is_tracing_running)
    self.assertEquals(len(self.controller._active_agents_instances), 4)
    self.controller._IssueClockSyncMarker()
    data = self.controller.StopTracing()
    self.assertFalse(self.controller.is_tracing_running)
    self.assertEquals(self._getSyncCount(data), 4)

  def testDisableGarbageCollection(self):
    self.assertTrue(gc.isenabled())
    with self.controller._DisableGarbageCollection():
      self.assertFalse(gc.isenabled())
    self.assertTrue(gc.isenabled())


if __name__ == '__main__':
  logging.getLogger().setLevel(logging.DEBUG)
  unittest.main(verbosity=2)

