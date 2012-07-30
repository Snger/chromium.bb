# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import logging
import os

from handlebar_dict_generator import HandlebarDictGenerator
import third_party.json_schema_compiler.json_comment_eater as json_comment_eater
import third_party.json_schema_compiler.model as model
import third_party.json_schema_compiler.idl_schema as idl_schema
import third_party.json_schema_compiler.idl_parser as idl_parser

class APIDataSource(object):
  """This class fetches and loads JSON APIs from the FileSystem passed in with
  |cache_builder|, so the APIs can be plugged into templates.
  """
  def __init__(self, cache_builder, base_path):
    self._json_cache = cache_builder.build(self._LoadJsonAPI)
    self._idl_cache = cache_builder.build(self._LoadIdlAPI)
    self._permissions_cache = cache_builder.build(self._LoadPermissions)
    self._base_path = base_path

  def _LoadJsonAPI(self, api):
    generator = HandlebarDictGenerator(
        json.loads(json_comment_eater.Nom(api))[0])
    return generator.Generate()

  def _LoadIdlAPI(self, api):
    idl = idl_parser.IDLParser().ParseData(api)
    generator = HandlebarDictGenerator(idl_schema.IDLSchema(idl).process()[0])
    return generator.Generate()

  def _LoadPermissions(self, perms_json):
    return json.loads(json_comment_eater.Nom(perms_json))

  def _GetFeature(self, path):
    # Remove 'experimental_' from path name to match the keys in
    # _permissions_features.json.
    path = path.replace('experimental_', '')
    try:
      perms = self._permissions_cache.GetFromFile(
          self._base_path + '/_permission_features.json')
      api_perms = perms.get(path, None)
      if api_perms['channel'] == 'dev':
        api_perms['dev'] = True
      return api_perms
    except Exception:
      return None

  def _AddPermissionsDict(self, api_dict, path):
    return_dict = { 'permissions': self._GetFeature(path) }
    return_dict.update(api_dict)
    return return_dict

  def __getitem__(self, key):
    return self.get(key)

  def get(self, key):
    path, ext = os.path.splitext(key)
    unix_name = model.UnixName(path)
    json_path = unix_name + '.json'
    idl_path = unix_name + '.idl'
    try:
      return self._AddPermissionsDict(self._json_cache.GetFromFile(
          self._base_path + '/' + json_path), path)
    except Exception:
      try:
        return self._AddPermissionsDict(self._idl_cache.GetFromFile(
            self._base_path + '/' + idl_path), path)
      except Exception as e:
        logging.warn(e)
        return None
