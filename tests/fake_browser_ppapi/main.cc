/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <string.h>
#include <vector>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/ppapi_proxy/plugin_var.h"
#include "native_client/tests/fake_browser_ppapi/fake_core.h"
#include "native_client/tests/fake_browser_ppapi/fake_host.h"
#include "native_client/tests/fake_browser_ppapi/fake_instance.h"
#include "native_client/tests/fake_browser_ppapi/fake_window.h"
#include "native_client/tests/fake_browser_ppapi/test_scriptable.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/pp_errors.h"

using ppapi_proxy::DebugPrintf;
using fake_browser_ppapi::Host;
using fake_browser_ppapi::FakeWindow;

namespace {

Host* host = NULL;

const void* FakeGetInterface(const char* interface_name) {
  DebugPrintf("Getting interface for name '%s'\n", interface_name);
  if (strcmp(interface_name, PPB_CORE_INTERFACE) == 0) {
    return host->core_interface();
  } else if (strcmp(interface_name, PPB_INSTANCE_INTERFACE) == 0) {
    return host->instance_interface();
  } else if (strcmp(interface_name, PPB_VAR_DEPRECATED_INTERFACE) == 0) {
    return host->var_interface();
  }
  return NULL;
}

// Module ids are needed for some call APIs, but the fake browser does
// not implement the storage tracking APIs that would use a real value.
// TODO(sehr): implement storage tracking.
// The storage allocated by the browser for the window object, etc., are
// attributed to the browser's module id.
PP_Module BrowserModuleId() {
  static void* id;
  return reinterpret_cast<PP_Module>(&id);
}

// The storage allocated by the plugin for its scriptable objects are
// attributed to the its module id.
PP_Module PluginModuleId() {
  static void* id;
  return reinterpret_cast<PP_Module>(&id);
}

bool ParseArgs(const char* str,
               uint32_t* argc,
               const char*** argn,
               const char*** argv) {
  std::vector<std::string> argn_vector;
  std::vector<std::string> argv_vector;
  *argc = 0;
  char* embed_arg = strtok(strdup(str), ";");
  while (embed_arg != NULL) {
    char* equal_loc = strchr(embed_arg, '=');
    if (equal_loc == NULL) {
      return false;
    }
    size_t name_length = static_cast<size_t>(equal_loc - embed_arg);
    argn_vector.push_back(strndup(embed_arg, name_length));
    argv_vector.push_back(equal_loc + 1);
    ++*argc;
    embed_arg = strtok(NULL, ";");
  }

  *argn = reinterpret_cast<const char**>(malloc(*argc * sizeof(*argn)));
  *argv = reinterpret_cast<const char**>(malloc(*argc * sizeof(*argv)));
  for (uint32_t i = 0; i < *argc; ++i) {
    (*argn)[i] = strdup(argn_vector[i].c_str());
    (*argv)[i] = strdup(argv_vector[i].c_str());
    printf("arg[%u]: '%s' = '%s'\n", i, (*argn)[i], (*argv)[i]);
  }
  return true;
}

// Test instance execution.
void TestInstance(PP_Module browser_module_id,
                  const PPP_Instance* instance_interface,
                  const char* page_url,
                  uint32_t argc,
                  const char** argn,
                  const char** argv) {
  printf("page url %s\n", page_url);
  // Create a fake window object.
  FakeWindow window(browser_module_id, host, page_url);
  // Create an instance and the corresponding id.
  fake_browser_ppapi::Instance browser_instance(&window);
  PP_Instance instance_id = reinterpret_cast<PP_Instance>(&browser_instance);
  // Create and initialize plugin instance.
  CHECK(instance_interface->DidCreate(instance_id, argc, argn, argv));
  // Test the scriptable object for the instance.
  PP_Var instance_object = instance_interface->GetInstanceObject(instance_id);
  const PPB_Var_Deprecated* var_interface =
      reinterpret_cast<const PPB_Var_Deprecated*>(
      FakeGetInterface(PPB_VAR_DEPRECATED_INTERFACE));
  TestScriptableObject(instance_object,
                       browser_instance.GetInterface(),
                       var_interface,
                       instance_id,
                       browser_module_id);
}

}  // namespace

int main(int argc, char** argv) {
  // Turn off stdout buffering to aid debugging in case of a crash.
  setvbuf(stdout, NULL, _IONBF, 0);

  NaClLogModuleInit();

  if (argc < 5) {
    fprintf(stderr,
            "Usage: fake_browser_ppapi plugin page_url \"embed args\""
            " root_path\n");
    return 1;
  }

  const char* plugin_name = argv[1];
  host =
      new fake_browser_ppapi::Host(plugin_name,
                                   fake_browser_ppapi::Core::GetInterface(),
                                   ppapi_proxy::PluginInstance::GetInterface(),
                                   ppapi_proxy::PluginVar::GetInterface());
  CHECK(host != NULL);

  // Test startup.
  CHECK(host->InitializeModule(PluginModuleId(), FakeGetInterface) == PP_OK);

  // Get an instance of the plugin.
  const PPP_Instance* instance_interface =
      reinterpret_cast<const PPP_Instance*>(
          host->GetInterface(PPP_INSTANCE_INTERFACE));
  CHECK(instance_interface != NULL);
  const char* page_url = argv[2];

  // Get the embed argc/argn/argv.
  const char* embed_args = argv[3];
  uint32_t embed_argc = 0;
  const char** embed_argn = NULL;
  const char** embed_argv = NULL;
  CHECK(ParseArgs(embed_args, &embed_argc, &embed_argn, &embed_argv));

  // Temporary support for reading files from disk rather than HTML.
  const char* root_path = argv[4];
  setenv("NACL_PPAPI_LOCAL_ORIGIN", root_path, 1);

  // Test an instance.
  TestInstance(BrowserModuleId(),
               instance_interface,
               page_url,
               embed_argc,
               embed_argn,
               embed_argv);

  // Shutdown.
  host->ShutdownModule();

  // Close the plugin .so.
  delete host;

  return 0;
}
