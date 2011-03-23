/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

// Second generation sel_universal implemented in C++ and with optional
// multimedia support via SDL

#include <stdio.h>

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/nonnacl_util/sel_ldr_launcher.h"
#include "native_client/src/trusted/sel_universal/pepper_handler.h"
#include "native_client/src/trusted/sel_universal/replay_handler.h"
#include "native_client/src/trusted/sel_universal/rpc_universal.h"
#if defined(NACL_SEL_UNIVERSAL_INCLUDE_SDL)
// NOTE: we need to include this so that it can "hijack" main
#include <SDL/SDL.h>
#include "native_client/src/trusted/sel_universal/multimedia_handler.h"
#endif

using std::ifstream;
using std::map;
using std::string;
using std::vector;
using nacl::DescWrapper;

static const char* kUsage =
  "Usage:\n"
  "\n"
  "sel_universal <sel_ldr_arg>* [-- <nexe> <nexe_arg>*]\n"
  "\n"
  "Exactly one nacl_file argument is required.\n"
  "After startup the user is prompted for interactive commands.\n"
  "For sample commands have a look at: tests/srpc/srpc_basic_test.stdin\n";

// NOTE: this used to be stack allocated inside main which cause
// problems on ARM (probably a tool chain bug).
// NaClSrpcChannel is pretty big (> 256kB)
static NaClSrpcChannel command_channel;
static NaClSrpcChannel channel;

// variables set via command line
static map<string, string> initial_vars;
static vector<string> initial_commands;
static bool abort_on_error = false;
static bool silence_nexe = false;
static string command_prefix = "";

// When given argc and argv this function (a) extracts the nacl_file argument,
// (b) populates sel_ldr_argv with sel_ldr arguments, and (c) populates
// app_argv with nexe module args. Also see kUsage above for details.
// It will call exit with codes 0 (help message) and 1 (incorrect args).
static nacl::string ProcessArguments(int argc,
                                     char* argv[],
                                     vector<nacl::string>* const sel_ldr_argv,
                                     vector<nacl::string>* const app_argv) {
  if (argc == 1) {
    printf("%s", kUsage);
    exit(0);
  }

  // Extract '-f nacl_file' from args and transfer the rest to sel_ldr_argv
  nacl::string app_name;
  for (int i = 1; i < argc; i++) {
    const string flag(argv[i]);
    // Check if the argument has the form -f nacl_file
    if (flag == "--help") {
      printf("%s", kUsage);
      exit(0);
    } else if (flag == "--debug") {
      NaClLogSetVerbosity(1);
    } else if (flag == "--abort_on_error") {
      abort_on_error = true;
    } else if (flag == "--silence_nexe") {
      silence_nexe = true;
    } else if (flag == "--command_prefix") {
      if (argc <= i + 1) {
        NaClLog(LOG_FATAL,
                "not enough args for --command_prefix option\n");
      }
      ++i;
      command_prefix = argv[i];
    } else if (flag == "--command_file") {
      if (argc <= i + 1) {
        NaClLog(LOG_FATAL,
                "not enough args for --command_file option\n");
      }
      NaClLog(LOG_INFO, "reading commands from %s\n", argv[i + 1]);
      ifstream f;
      f.open(argv[i + 1]);
      ++i;
      while (!f.eof()) {
        string s;
        getline(f, s);
        initial_commands.push_back(s);
      }
      f.close();
      NaClLog(LOG_INFO, "total commands now: %d\n",
              static_cast<int>(initial_commands.size()));
    } else if (flag == "--var") {
      if (argc <= i + 2) {
        NaClLog(LOG_FATAL, "not enough args for --var option\n");
      }

      const string tag = string(argv[i + 1]);
      const string val = string(argv[i + 2]);
      i += 2;
      initial_vars[tag] = val;
    } else if (flag == "--") {
      // Done processing sel_ldr args. If no '-f nacl_file' was given earlier,
      // the first argument after '--' is the nacl_file.
      i++;
      if (app_name == "" && i < argc) {
        app_name = argv[i++];
      }
      // The remaining arguments are passed to the executable.
      for (; i < argc; i++) {
        app_argv->push_back(argv[i]);
      }
    } else {
      // NOTE: most sel_ldr args start with a single hyphen so there is not
      // much confusion with sel_universal args. But this remains a hack.
      sel_ldr_argv->push_back(argv[i]);
    }
  }

  if (app_name == "") {
    NaClLog(LOG_FATAL, "missing app\n");
  }

  return app_name;
}


int main(int argc, char* argv[]) {
  // Descriptor transfer requires the following
  NaClSrpcModuleInit();
  NaClNrdAllModulesInit();

  // Get the arguments to sed_ldr and the nexe module
  vector<nacl::string> sel_ldr_argv;
  vector<nacl::string> app_argv;
  nacl::string app_name =
    ProcessArguments(argc, argv, &sel_ldr_argv, &app_argv);

  // Add '-X 5' to sel_ldr arguments to create communication socket
  sel_ldr_argv.push_back("-X");
  sel_ldr_argv.push_back("5");
  if (silence_nexe) {
    // redirect stdout/stderr in the nexe to /dev/null
    std::stringstream ss_stdout;
    std::stringstream ss_stderr;

    int fd = open(PORTABLE_DEV_NULL, O_RDWR);
    sel_ldr_argv.push_back("-w");
    ss_stdout << "1:" << fd;
    sel_ldr_argv.push_back(ss_stdout.str());
    sel_ldr_argv.push_back("-w");
    ss_stderr << "2:" << fd;
    sel_ldr_argv.push_back(ss_stderr.str());
  }
  // Start sel_ldr with the given application and arguments.
  nacl::SelLdrLauncher launcher;
  if (command_prefix != "") {
    launcher.SetCommandPrefix(command_prefix);
  }

  if (!launcher.StartFromCommandLine(app_name, 5, sel_ldr_argv, app_argv)) {
    NaClLog(LOG_FATAL, "sel_universal: Failed to launch sel_ldr\n");
  }

  // Open the communication channels to the service runtime.
  if (!launcher.OpenSrpcChannels(&command_channel, &channel)) {
    NaClLog(LOG_ERROR, "sel_universal: Open channel failed\n");
    exit(1);
  }

  NaClCommandLoop loop(channel.client,
                       &channel,
                       launcher.socket_address()->desc());

  //
  // Pepper sample commands
  // initialize_pepper pepper
  // add_pepper_rpcs
  // install_upcalls service
  // show_variables
  // show_descriptors
  // rpc PPP_InitializeModule i(0) l(0) h(pepper) s("${service}") * i(0) i(0)

  loop.AddHandler("initialize_pepper", HandlerPepperInit);
  loop.AddHandler("add_pepper_rpcs", HandlerAddPepperRpcs);

  loop.AddHandler("replay_activate", HandlerReplayActivate);
  loop.AddHandler("replay", HandlerReplay);
  loop.AddHandler("replay_unused", HandlerUnusedReplays);

  // possible platform specific stuff
  loop.AddHandler("shmem", HandlerShmem);
  loop.AddHandler("readonly_file", HandlerReadonlyFile);
  loop.AddHandler("sleep", HandlerSleep);
  loop.AddHandler("map_shmem", HandlerMap);
  loop.AddHandler("save_to_file", HandlerSaveToFile);
  loop.AddHandler("load_from_file", HandlerLoadFromFile);
  loop.AddHandler("file_size", HandlerFileSize);
  loop.AddHandler("sync_socket_create", HandlerSyncSocketCreate);
  loop.AddHandler("sync_socket_write", HandlerSyncSocketWrite);
#if  NACL_SEL_UNIVERSAL_INCLUDE_SDL
  loop.AddHandler("sdl_initialize", HandlerSDLInitialize);
  loop.AddHandler("sdl_event_loop", HandlerSDLEventLoop);
#endif
// start multimedia event loop
bool HandlerEventLoop(NaClCommandLoop* ncl, const vector<string>& args);

  NaClLog(1, "populating initial vars\n");
  for (map<string, string>::iterator it = initial_vars.begin();
       it != initial_vars.end();
       ++it) {
    loop.SetVariable(it->first, it->second);
  }

  const bool success = initial_commands.size() > 0 ?
                       loop.ProcessCommands(initial_commands) :
                       loop.StartInteractiveLoop(abort_on_error);

  // Close the connections to sel_ldr.
  NaClSrpcDtor(&command_channel);
  NaClSrpcDtor(&channel);

  NaClSrpcModuleFini();
  NaClNrdAllModulesFini();
  return success ? 0 : -1;
}
