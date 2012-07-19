# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from path_utils import FormatKey
from third_party.handlebar import Handlebar

EXTENSIONS_URL = '/chrome/extensions'

class TemplateDataSource(object):
  """This class fetches and compiles templates using the fetcher passed in with
  |cache_builder|.
  """
  def __init__(self,
               branch,
               api_data_source,
               intro_data_source,
               samples_data_source,
               cache_builder,
               base_paths):
    self._branch_info = self._MakeBranchDict(branch)
    self._static_resources = ((('/' + branch) if branch != 'local' else '') +
                              '/static')
    self._api_data_source = api_data_source
    self._intro_data_source = intro_data_source
    self._samples_data_source = samples_data_source
    self._cache = cache_builder.build(self._LoadTemplate)
    self._base_paths = base_paths

  def _MakeBranchDict(self, branch):
    return {
      'showWarning': branch != 'stable',
      'branches': [
        { 'name': 'Stable', 'path': EXTENSIONS_URL + '/stable' },
        { 'name': 'Dev', 'path': EXTENSIONS_URL + '/dev' },
        { 'name': 'Beta', 'path': EXTENSIONS_URL + '/beta' },
        { 'name': 'Trunk', 'path': EXTENSIONS_URL + '/trunk' }
      ],
      'current': branch
    }

  def _LoadTemplate(self, template):
    return Handlebar(template)

  def Render(self, template_name):
    """This method will render a template named |template_name|, fetching all
    the partial templates needed from |self._cache|. Partials are retrieved
    from the TemplateDataSource with the |get| method.
    """
    template = self.get(template_name)
    if not template:
      return ''
      # TODO error handling
    return template.render({
      'apis': self._api_data_source,
      'branchInfo': self._branch_info,
      'intros': self._intro_data_source,
      'partials': self,
      'samples': self._samples_data_source,
      'static': self._static_resources
    }).text

  def __getitem__(self, key):
    return self.get(key)

  def get(self, key):
    real_path = FormatKey(key)
    for base_path in self._base_paths:
      try:
        return self._cache.GetFromFile(base_path + '/' + real_path)
      except Exception:
        pass
    return None
