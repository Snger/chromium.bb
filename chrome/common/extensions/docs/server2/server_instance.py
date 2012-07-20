# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from fnmatch import fnmatch
import mimetypes
import os

STATIC_DIR_PREFIX = 'docs/server2'
DOCS_PREFIX = 'docs'

class ServerInstance(object):
  """This class is used to hold a data source and fetcher for an instance of a
  server. Each new branch will get its own ServerInstance.
  """
  def __init__(self,
               template_data_source_factory,
               example_zipper,
               cache_builder):
    self._template_data_source_factory = template_data_source_factory
    self._example_zipper = example_zipper
    self._cache = cache_builder.build(lambda x: x)
    mimetypes.init()

  def _FetchStaticResource(self, path, response):
    """Fetch a resource in the 'static' directory.
    """
    try:
      result = self._cache.GetFromFile(STATIC_DIR_PREFIX + '/' + path)
      base, ext = os.path.splitext(path)
      response.headers['content-type'] = mimetypes.types_map[ext]
      return result
    except Exception:
      return ''

  def Get(self, path, request, response):
    templates = self._template_data_source_factory.Create(request)

    if fnmatch(path, 'examples/*.zip'):
      content = self._example_zipper.Create(path[:-len('.zip')])
      response.headers['content-type'] = mimetypes.types_map['.zip']
    elif path.startswith('examples/'):
      content = self._cache.GetFromFile(DOCS_PREFIX + '/' + path)
      response.headers['content-type'] = 'text/plain'
    elif path.startswith('static/'):
      content = self._FetchStaticResource(path, response)
    else:
      content = templates.Render(path)

    if content:
      response.out.write(content)
    else:
      response.set_status(404);
      response.out.write(templates.Render('404'))
