/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <fcntl.h>
#include <string.h>
#if (NACL_LINUX)
#include <sys/ipc.h>
#include <sys/shm.h>
#include "native_client/src/trusted/desc/linux/nacl_desc_sysv_shm.h"
#endif

#include <map>
#include <string>
#include <sstream>

using std::stringstream;

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_wrapper.h"
#include "native_client/src/trusted/sel_universal/parsing.h"
#include "native_client/src/trusted/sel_universal/rpc_universal.h"


namespace {

const uintptr_t k64KBytes = 0x10000;

// The main point of this class is to ensure automatic cleanup.
// If the destructor is not invoked you need to manually cleanup
// the shared memory descriptors via "ipcs -m" and "ipcrm -m <id>"
class AddressMap {
 public:
  AddressMap() {}

  ~AddressMap() {
    // NOTE: you CANNOT call NaClLog - this is called too late
    // NaClLog(1, "cleanup\n");
#if (NACL_LINUX)
    typedef map<NaClDesc*, uintptr_t>::iterator IT;
    for (IT it = map_.begin(); it != map_.end(); ++it) {
      shmctl(reinterpret_cast<NaClDescSysvShm*>(it->first)->id, IPC_RMID, NULL);
    }
#endif
  }

  void Add(NaClDesc* desc, uintptr_t addr) { map_[desc] = addr; }

  uintptr_t Get(NaClDesc* desc) { return map_[desc]; }

 private:
  map<NaClDesc*, uintptr_t> map_;
};

AddressMap GlobalAddressMap;

uintptr_t Align(uintptr_t start, uintptr_t alignment) {
  return (start + alignment - 1) & ~(alignment - 1);
}


uintptr_t MapShmem(nacl::DescWrapper* desc) {
  void* addr;
  size_t dummy_size;
  int result = desc->Map(&addr, &dummy_size);
  if (0 > result) {
    NaClLog(LOG_ERROR, "error mapping shmem area\n");
    return 0;
  }

  GlobalAddressMap.Add(desc->desc(), reinterpret_cast<uintptr_t>(addr));
  return reinterpret_cast<uintptr_t>(addr);
}

}  // namespace

bool HandlerShmem(NaClCommandLoop* ncl, const vector<string>& args) {
  if (args.size() < 4) {
    NaClLog(LOG_ERROR, "not enough args\n");
    return false;
  }

  const int size = ExtractInt32(args[3]);
  nacl::DescWrapperFactory factory;
  nacl::DescWrapper* desc = factory.MakeShm(size);
  if (desc == NULL) {
    NaClLog(LOG_ERROR, "could not create shm\n");
    return false;
  }

  ncl->AddDesc(desc->desc(), args[1]);

  uintptr_t addr = MapShmem(desc);
  if (addr == 0) {
    return false;
  }
  stringstream str;
  str << "0x" << std::hex << addr;
  ncl->SetVariable(args[2], str.str());
  return true;
}


// create a descriptor representing a readonly file
bool HandlerReadonlyFile(NaClCommandLoop* ncl, const vector<string>& args) {
  if (args.size() < 3) {
    NaClLog(LOG_ERROR, "not enough args\n");
    return false;
  }

  nacl::DescWrapperFactory factory;
  nacl::DescWrapper* desc = factory.OpenHostFile(args[2].c_str(), O_RDONLY, 0);
  if (NULL == desc) {
    NaClLog(LOG_ERROR, "cound not create file desc for %s\n", args[2].c_str());
    return false;
  }
  ncl->AddDesc(desc->desc(), args[1]);
  return true;
}

// sleep for a given number of seconds
bool HandlerSleep(NaClCommandLoop* ncl, const vector<string>& args) {
  UNREFERENCED_PARAMETER(ncl);
  if (args.size() < 2) {
    NaClLog(LOG_ERROR, "not enough args\n");
    return false;
  }
  const int secs = ExtractInt32(args[1]);
#if (NACL_LINUX || NACL_OSX)
  sleep(secs);
#elif NACL_WINDOWS
  Sleep(secs * 1000);
#else
#error "Please specify platform as NACL_LINUX, NACL_OSX or NACL_WINDOWS"
#endif
  return true;
}

// save a memory region to a file
bool HandlerSaveToFile(NaClCommandLoop* ncl, const vector<string>& args) {
  UNREFERENCED_PARAMETER(ncl);
  if (args.size() < 5) {
    NaClLog(LOG_ERROR, "not enough args\n");
    return false;
  }

  const char* filename = args[1].c_str();
  const char* start = reinterpret_cast<char*>(ExtractInt64(args[2]));
  const int offset = ExtractInt32(args[3]);
  const int size = ExtractInt32(args[4]);

  NaClLog(1, "opening %s\n", filename);
  FILE* fp = fopen(filename, "wb");
  if (fp == NULL) {
     NaClLog(LOG_ERROR, "cannot open %s\n", filename);
     return false;
  }

  NaClLog(1, "writing %d bytes from %p\n", (int) size, start + offset);
  const size_t n = fwrite(start + offset, 1, size, fp);
  if (static_cast<int>(n) != size) {
    NaClLog(LOG_ERROR, "wrote %d bytes, expected %d\n",
            static_cast<int>(n), size);
    fclose(fp);
    return false;
  }
  fclose(fp);
  return true;
}

// map a shared mem descriptor into memory and save address into var
bool HandlerMap(NaClCommandLoop* ncl, const vector<string>& args) {
  UNREFERENCED_PARAMETER(ncl);
  if (args.size() < 3) {
    NaClLog(LOG_ERROR, "not enough args\n");
    return false;
  }

  NaClDesc* raw_desc = ExtractDesc(args[1], ncl);
  if (raw_desc == NULL) {
    NaClLog(LOG_ERROR, "cannot find desciptor %s\n", args[1].c_str());
    return false;
  }

  nacl::DescWrapperFactory factory;
  nacl::DescWrapper* desc = factory.MakeGeneric(raw_desc);

  uintptr_t addr = MapShmem(desc);
  if (addr == 0) {
    return false;
  }

  NaClLog(1, "region mapped at %p\n", reinterpret_cast<void*>(addr));
  stringstream str;
  str << "0x" << std::hex << addr;
  ncl->SetVariable(args[2], str.str());
  return true;
}

// load file into memory region
bool HandlerLoadFromFile(NaClCommandLoop* ncl, const vector<string>& args) {
  UNREFERENCED_PARAMETER(ncl);
  if (args.size() < 5) {
    NaClLog(LOG_ERROR, "not enough args\n");
    return false;
  }

  const char* filename = args[1].c_str();
  char* start = reinterpret_cast<char*>(ExtractInt64(args[2]));
  const int offset = ExtractInt32(args[3]);
  const int size = ExtractInt32(args[4]);

  NaClLog(1, "opening %s\n", filename);
  FILE* fp = fopen(filename, "rb");
  if (fp == NULL) {
     NaClLog(LOG_ERROR, "cannot open %s\n", filename);
     return false;
  }

  NaClLog(1, "loading %d bytes to %p\n", (int) size, start + offset);
  const size_t n = fread(start + offset, 1, size, fp);
  if (static_cast<int>(n) != size) {
    NaClLog(LOG_ERROR, "read %d bytes, expected %d\n",
            static_cast<int>(n), size);
    fclose(fp);
    return false;
  }
  fclose(fp);
  return true;
}

// Determine filesize and write it into a variable
bool HandlerFileSize(NaClCommandLoop* ncl, const vector<string>& args) {
  UNREFERENCED_PARAMETER(ncl);
  if (args.size() < 3) {
    NaClLog(LOG_ERROR, "not enough args\n");
    return false;
  }

  const char* filename = args[1].c_str();
  FILE* fp = fopen(filename, "rb");
  if (fp == NULL) {
     NaClLog(LOG_ERROR, "cannot open %s\n", filename);
     return false;
  }
  fseek(fp, 0, SEEK_END);
  int size = static_cast<int>(ftell(fp));
  fclose(fp);

  NaClLog(1, "filesize is %d\n", size);

  stringstream str;
  str << size;
  ncl->SetVariable(args[2], str.str());
  return true;
}
