#!/bin/bash
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script assumed to be run in native_client/
cd "$(cygpath "${PWD}")"
if [[ ${PWD} != */native_client ]]; then
  echo "ERROR: must be run in native_client!"
  exit 1
fi

set -x
set -e
set -u

export TOOLCHAINLOC=toolchain
export TOOLCHAINNAME=win_x86
export INST_GLIBC_PROGRAM="$PWD/tools/glibc_download.sh"
# Workaround for broken autoconf mmap test (WOW64 limitation)
# More info here: http://cygwin.com/ml/cygwin/2011-03/msg00596.html
export ac_cv_func_mmap_fixed_mapped=yes

echo @@@BUILD_STEP clobber@@@
rm -rf scons-out tools/SRC tools/BUILD tools/out tools/toolchain \
  tools/glibc tools/glibc.tar tools/toolchain.t* toolchain .tmp ||
  echo already_clean
mkdir -p tools/toolchain/win_x86
ln -sfn "$PWD"/cygwin/tmp tools/toolchain/win_x86

# glibc_download.sh can return three return codes:
#  0 - glibc is successfully downloaded and installed
#  1 - glibc is not downloaded but another run may help
#  2+ - glibc is not downloaded and can not be downloaded later
#
# If the error result is 2 or more we are stopping the build
echo @@@BUILD_STEP check_glibc_revision_sanity@@@
echo "Try to download glibc revision $(tools/glibc_revision.sh)"
if tools/glibc_download.sh tools/toolchain/win_x86 1; then
  INST_GLIBC_PROGRAM=true
elif (($?>1)); then
  echo @@@STEP_FAILURE@@@
  exit 100
fi

echo @@@BUILD_STEP compile_toolchain@@@
(
  cd tools
  make -j8 buildbot-build-with-glibc
  rm toolchain/win_x86/tmp
)

echo @@@BUILD_STEP tar_toolchain@@@
(
  cd tools
  tar Scf toolchain.tgz toolchain/
  xz -k -9 toolchain.tar
  bzip2 -k -9 toolchain.tar
  gzip -9 toolchain.tar
  chmod a+r toolchain.tar.gz toolchain.tar.bz2 toolchain.tar.xz
)

echo @@@BUILD_STEP untar_toolchain@@@
(
  mkdir -p .tmp
  cd .tmp
  tar zSxf ../tools/toolchain.tgz
  find -L toolchain -type f -xtype l -print0 |
  while IFS="" read -r -d "" name ; do
    # Find gives us some files twice because there are symlinks.  Second time
    # ‘ln’ will fail with ln: “‘xxx’ and ‘yyy’ are the same file” despite ‘-f’.
    if [[ -L "$name" ]]; then
      ln -Tf "$(readlink -f "$name")" "$name"
    fi
  done
  mv toolchain ..
)
