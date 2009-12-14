/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <nacl/nacl_npapi.h>
#include <nacl/npupp.h>

struct PlugIn {
  NPP npp;
  NPObject *npobject;
};

/*
 * Please refer to the Gecko Plugin API Reference for the description of
 * NPP_New.
 */
NPError NPP_New(NPMIMEType mime_type,
                NPP instance,
                uint16_t mode,
                int16_t argc,
                char* argn[],
                char* argv[],
                NPSavedData* saved) {
  int i;
  struct PlugIn *plugin;

  printf("*** NPP_New\n");
  if (instance == NULL) return NPERR_INVALID_INSTANCE_ERROR;

  for (i = 0; i < argc; ++i) {
    printf("%u: '%s' '%s'\n", i, argn[i], argv[i]);
  }

  /* avoid malloc */
  plugin = (struct PlugIn *)malloc(sizeof(*plugin));
  plugin->npp = instance;
  plugin->npobject = NULL;

  instance->pdata = plugin;
  return NPERR_NO_ERROR;
}

/*
 * Please refer to the Gecko Plugin API Reference for the description of
 * NPP_Destroy.
 * In the NaCl module, NPP_Destroy is called from NaClNP_MainLoop().
 */
NPError NPP_Destroy(NPP instance, NPSavedData** save) {
  printf("*** NPP_Destroy\n");

  if (NULL == instance)
    return NPERR_INVALID_INSTANCE_ERROR;

  /* free plugin */
  if (NULL != instance->pdata) free(instance->pdata);
  instance->pdata = NULL;
  return NPERR_NO_ERROR;
}

NPObject *NPP_GetScriptableInstance(NPP instance) {
  struct PlugIn* plugin;

  extern NPClass *GetNPSimpleClass();
  printf("*** NPP_GetScriptableInstance\n");

  if (NULL == instance) {
    printf("NULL NPP\n");
    return NULL;
  }
  plugin = (struct PlugIn *)instance->pdata;
  if (NULL == plugin->npobject) {
    printf("Creating the plugin object\n");
    plugin->npobject = NPN_CreateObject(instance, GetNPSimpleClass());
  }
  if (NULL != plugin->npobject) {
    printf("Retaining the plugin object\n");
    NPN_RetainObject(plugin->npobject);
  }
  printf("The plugin object %p\n", (void*) plugin->npobject);
  return plugin->npobject;
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void* ret_value) {
  if (NPPVpluginScriptableNPObject == variable) {
    *((NPObject**) ret_value) = NPP_GetScriptableInstance(instance);
    return NPERR_NO_ERROR;
  } else {
    return NPERR_GENERIC_ERROR;
  }
}

NPError NPP_SetWindow(NPP instance, NPWindow* window) {
  printf("*** NPP_SetWindow\n");
  return NPERR_NO_ERROR;
}

/*
 * NP_Initialize
 */

NPError NP_Initialize(NPNetscapeFuncs* browser_funcs,
                      NPPluginFuncs* plugin_funcs) {
  plugin_funcs->newp = NPP_New;
  plugin_funcs->destroy = NPP_Destroy;
  plugin_funcs->setwindow = NPP_SetWindow;
  plugin_funcs->getvalue = NPP_GetValue;
  return NPERR_NO_ERROR;
}
