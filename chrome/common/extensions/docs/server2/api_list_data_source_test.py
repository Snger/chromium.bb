#!/usr/bin/env python
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import unittest
from api_list_data_source import APIListDataSource
from compiled_file_system import CompiledFileSystem
from copy import deepcopy
from features_bundle import FeaturesBundle
from object_store_creator import ObjectStoreCreator
from test_file_system import TestFileSystem

def _ToTestData(obj):
  '''Transforms |obj| into test data by turning a list of files into an object
  mapping that file to its contents (derived from its name).
  '''
  return dict((name, name) for name in obj)

def _ToTestFeatures(names):
  '''Transforms a list of strings into a minimal JSON features object.
  '''
  return dict((name, {
        'name': name,
        'platforms': platforms
      }) for name, platforms in names)

_TEST_DATA = {
  'public': {
    'apps': _ToTestData([
      'alarms.html',
      'app_window.html',
      'experimental_bluetooth.html',
      'experimental_power.html',
      'storage.html',
    ]),
    'extensions': _ToTestData([
      'alarms.html',
      'browserAction.html',
      'experimental_history.html',
      'experimental_power.html',
      'infobars.html',
      'storage.html',
    ]),
  },
}

_TEST_API_FEATURES = _ToTestFeatures([
  ('alarms', ['apps', 'extensions']),
  ('app.window', ['apps']),
  ('browserAction', ['extensions']),
  ('experimental.bluetooth', ['apps']),
  ('experimental.history', ['extensions']),
  ('experimental.power', ['apps', 'extensions']),
  ('infobars', ['extensions']),
  ('something_internal', ['apps']),
  ('something_else_internal', ['extensions']),
  ('storage', ['apps', 'extensions'])
])


class _FakeFeaturesBundle(object):
  def GetAPIFeatures(self):
    return _TEST_API_FEATURES


class APIListDataSourceTest(unittest.TestCase):
  def setUp(self):
    file_system = TestFileSystem(deepcopy(_TEST_DATA))
    object_store_creator = ObjectStoreCreator.ForTest()
    compiled_fs_factory = CompiledFileSystem.Factory(
        file_system,
        object_store_creator)
    features_bundle = _FakeFeaturesBundle()
    self._factory = APIListDataSource.Factory(
        compiled_fs_factory,
        file_system,
        'public',
        features_bundle,
        object_store_creator)

  def testApps(self):
    api_list = self._factory.Create()
    self.assertEqual([
        {
          'name': 'alarms',
          'platforms': ['apps', 'extensions']
        },
        {
          'name': 'app.window',
          'platforms': ['apps']
        },
        {
          'name': 'storage',
          'platforms': ['apps', 'extensions'],
          'last': True
        }],
        api_list.get('apps').get('chrome'))

  def testExperimentalApps(self):
    api_list = self._factory.Create()
    self.assertEqual([
        {
          'name': 'experimental.bluetooth',
          'platforms': ['apps']
        },
        {
          'name': 'experimental.power',
          'platforms': ['apps', 'extensions'],
          'last': True
        }],
        sorted(api_list.get('apps').get('experimental')))

  def testExtensions(self):
    api_list = self._factory.Create()
    self.assertEqual([
        {
          'name': 'alarms',
          'platforms': ['apps', 'extensions']
        },
        {
          'name': 'browserAction',
          'platforms': ['extensions']
        },
        {
          'name': 'infobars',
          'platforms': ['extensions']
        },
        {
          'name': 'storage',
          'platforms': ['apps', 'extensions'],
          'last': True
        }],
        sorted(api_list.get('extensions').get('chrome')))

  def testExperimentalExtensions(self):
    api_list = self._factory.Create()
    self.assertEqual([
        {
          'name': 'experimental.history',
          'platforms': ['extensions']
        },
        {
          'name': 'experimental.power',
          'platforms': ['apps', 'extensions'],
          'last': True
        }],
        sorted(api_list.get('extensions').get('experimental')))

if __name__ == '__main__':
  unittest.main()
