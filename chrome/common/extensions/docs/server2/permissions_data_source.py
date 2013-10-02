# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from itertools import ifilter
from operator import itemgetter

import features_utility as features
from third_party.json_schema_compiler.json_parse import Parse

def _ListifyPermissions(permissions):
  '''Filter out any permissions that do not have a description or with a name
  that ends with Private then sort permissions features by name into a list.
  '''
  def filter_permissions(perm):
    return 'description' in perm and not perm['name'].endswith('Private')

  return sorted(
      ifilter(filter_permissions, permissions.values()),
      key=itemgetter('name'))

def _AddDependencyDescriptions(permissions, api_features):
  '''Use |api_features| to determine the dependencies APIs have on permissions.
  Add descriptions to |permissions| based on those dependencies.
  '''
  for name, permission in permissions.iteritems():
    # Don't overwrite the description created by expanding a partial template.
    if 'partial' in permission or not permission['platforms']:
      continue

    has_deps = False
    if name in api_features:
      for dependency in api_features[name].get('dependencies', ()):
        if dependency.startswith('permission:'):
          has_deps = True

    if has_deps:
      permission['partial'] = 'permissions/generic_description'

class PermissionsDataSource(object):
  '''Load and format permissions features to be used by templates. Requries a
  template data source be set before use.
  '''
  def __init__(self, server_instance):
    self._features_bundle = server_instance.features_bundle
    self._object_store = server_instance.object_store_creator.Create(
        PermissionsDataSource)

  def SetTemplateDataSource(self, template_data_source_factory):
    '''Initialize a template data source to be used to render partial templates
    into descriptions for permissions. Must be called before .get
    '''
    self._template_data_source = template_data_source_factory.Create(
        None, {})

  def _CreatePermissionsData(self):
    api_features = self._features_bundle.GetAPIFeatures()
    permission_features = self._features_bundle.GetPermissionFeatures()

    def filter_for_platform(permissions, platform):
      return _ListifyPermissions(features.Filtered(permissions, platform))

    _AddDependencyDescriptions(permission_features, api_features)
    # Turn partial templates into descriptions, ensure anchors are set.
    for permission in permission_features.values():
      if not 'anchor' in permission:
        permission['anchor'] = permission['name']
      if 'partial' in permission:
        permission['description'] = self._template_data_source.get(
            permission['partial'])
        del permission['partial']

    return {
      'declare_apps': filter_for_platform(permission_features, 'apps'),
      'declare_extensions': filter_for_platform(
          permission_features, 'extensions')
    }

  def _GetCachedPermissionsData(self):
    data = self._object_store.Get('permissions_data').Get()
    if data is None:
      data = self._CreatePermissionsData()
      self._object_store.Set('permissions_data', data)
    return data

  def get(self, key):
    return self._GetCachedPermissionsData().get(key)
