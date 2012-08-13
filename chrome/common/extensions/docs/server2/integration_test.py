#!/usr/bin/env python
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import logging
import os
from StringIO import StringIO
import unittest

import appengine_memcache as memcache
import handler
from handler import Handler
import url_constants

KNOWN_FAILURES = [
  # Apps samples fails because it requires fetching data from github.com.
  # This should be tested though: http://crbug.com/141910.
  'apps/samples.html',
]

class _MockResponse(object):
  def __init__(self):
    self.status = 200
    self.out = StringIO()

  def set_status(self, status):
    self.status = status

class _MockRequest(object):
  def __init__(self, path):
    self.headers = {}
    self.path = path

class IntegrationTest(unittest.TestCase):
  def testAll(self):
    logging.getLogger().setLevel(logging.ERROR)
    base_path = os.path.join('templates', 'public')
    for path, dirs, files in os.walk(base_path):
      for name in files:
        filename = os.path.join(path, name)
        if (filename.split('/', 2)[-1] in KNOWN_FAILURES or
            '.' in path or
            name.startswith('.')):
          continue
        request = _MockRequest(filename.split('/', 2)[-1])
        response = _MockResponse()
        Handler(request, response, local_path='../..').get()
        self.assertEqual(200, response.status)
        self.assertTrue(response.out.getvalue())

  def test404(self):
    request = _MockRequest('junk.html')
    bad_response = _MockResponse()
    Handler(request, bad_response, local_path='../..').get()
    self.assertEqual(404, bad_response.status)
    self.assertTrue(bad_response.out.getvalue())

  def testLocales(self):
    # Use US English, Spanish, and Arabic.
    for lang in ['en-US', 'es', 'ar']:
      request = _MockRequest('extensions/samples.html')
      request.headers['Accept-Language'] = lang + ';q=0.8'
      response = _MockResponse()
      Handler(request, response, local_path='../..').get()
      self.assertEqual(200, response.status)
      self.assertTrue(response.out.getvalue())

  def testWarmupRequest(self):
    for branch in ['dev', 'trunk', 'beta', 'stable']:
      handler.BRANCH_UTILITY_MEMCACHE.Set(
          branch + '.' +  url_constants.OMAHA_PROXY_URL,
          'local',
          memcache.MEMCACHE_BRANCH_UTILITY)
    request = _MockRequest('_ah/warmup')
    response = _MockResponse()
    Handler(request, response, local_path='../..').get()
    self.assertEqual(200, response.status)
    # Test that the pages were rendered by checking the size of the output.
    # In python 2.6 there is no 'assertGreater' method.
    value = response.out.getvalue()
    self.assertTrue(len(value) > 100000,
                    msg='Response is too small, probably empty: %s' % value)

if __name__ == '__main__':
  unittest.main()
