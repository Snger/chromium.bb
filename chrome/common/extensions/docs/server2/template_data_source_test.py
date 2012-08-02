#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os
import unittest

from file_system_cache import FileSystemCache
from local_file_system import LocalFileSystem
from template_data_source import TemplateDataSource
from third_party.handlebar import Handlebar

class _FakeRequest(object):
    pass

class _FakeApiDataSourceFactory(object):
  def __init__(self, input_dict):
    self._input_dict = input_dict

  def Create(self, samples):
    return self._input_dict

class _FakeSamplesDataSource(object):
  def Create(self, request):
    return {}

class TemplateDataSourceTest(unittest.TestCase):
  def setUp(self):
    self._base_path = os.path.join('test_data', 'template_data_source')
    self._fake_api_data_source_factory = _FakeApiDataSourceFactory({})
    self._fake_api_list_data_source = {}
    self._fake_intro_data_source = {}
    self._fake_samples_data_source = _FakeSamplesDataSource()

  def _ReadLocalFile(self, filename):
    with open(os.path.join(self._base_path, filename), 'r') as f:
      return f.read()

  def _RenderTest(self, name, data_source):
    template_name = name + '_tmpl.html'
    template = Handlebar(self._ReadLocalFile(template_name))
    self.assertEquals(
        self._ReadLocalFile(name + '_expected.html'),
        data_source.Render(template_name))

  def _CreateTemplateDataSource(self, input_dict, cache_builder):
    return (TemplateDataSource.Factory('fake_branch',
                                       _FakeApiDataSourceFactory(input_dict),
                                       self._fake_api_list_data_source,
                                       self._fake_intro_data_source,
                                       self._fake_samples_data_source,
                                       cache_builder,
                                       './',
                                       './')
            .Create(_FakeRequest()))

  def testSimple(self):
    self._base_path = os.path.join(self._base_path, 'simple')
    fetcher = LocalFileSystem(self._base_path)
    cache_builder = FileSystemCache.Builder(fetcher)
    t_data_source = self._CreateTemplateDataSource(
        self._fake_api_data_source_factory, cache_builder)
    template_a1 = Handlebar(self._ReadLocalFile('test1.html'))
    self.assertEqual(template_a1.render({}, {'templates': {}}).text,
        t_data_source['test1'].render({}, {'templates': {}}).text)

    template_a2 = Handlebar(self._ReadLocalFile('test2.html'))
    self.assertEqual(template_a2.render({}, {'templates': {}}).text,
        t_data_source['test2'].render({}, {'templates': {}}).text)

    self.assertEqual(None, t_data_source['junk.html'])

  def testPartials(self):
    self._base_path = os.path.join(self._base_path, 'partials')
    fetcher = LocalFileSystem(self._base_path)
    cache_builder = FileSystemCache.Builder(fetcher)
    t_data_source = self._CreateTemplateDataSource(
        self._fake_api_data_source_factory, cache_builder)
    self.assertEqual(
        self._ReadLocalFile('test_expected.html'),
        t_data_source['test_tmpl'].render(
            json.loads(self._ReadLocalFile('input.json')), t_data_source).text)

  def testRender(self):
    self._base_path = os.path.join(self._base_path, 'render')
    fetcher = LocalFileSystem(self._base_path)
    context = json.loads(self._ReadLocalFile('test1.json'))
    cache_builder = FileSystemCache.Builder(fetcher)
    self._RenderTest(
        'test1',
        self._CreateTemplateDataSource(
            json.loads(self._ReadLocalFile('test1.json')),
                           cache_builder))
    self._RenderTest(
        'test2',
        self._CreateTemplateDataSource(
            json.loads(self._ReadLocalFile('test2.json')),
                           cache_builder))

if __name__ == '__main__':
  unittest.main()
