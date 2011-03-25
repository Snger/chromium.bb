#!/bin/bash
# Copyright 2011 The Native Client Authors.  All rights reserved.  Use
# of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This file polls the appspot directory for a fixed time.
# TODO(khim): invetigate feasibility of buildbot triggering.

# We try to check if the revision we need is already available.  If it is
# then we have a good glibc to use and continue.  If it is not available then
# we check revisions - and if one of them is available then we give up: this
# means the revision we need was omitted for some reason and will not be ever
# built.

if ((${#@}<1)); then
  cat <<END
  Usage: $0 path_to_toolchain [retry_count] [revisions_count]
END
  exit 10
fi
declare -r glibc_url_prefix=http://gsdview.appspot.com/nativeclient-archive2/between_builders/x86_glibc/r
glibc_revision="$("$(dirname "$0")/glibc_revision.sh")"
retry_count=10000
if ((${#@}>1)); then
  retry_count="$2"
fi
revisions_count=100
if ((${#@}>2)); then
  revisions_count="$3"
fi
for ((i=0;i<retry_count;i+=i)); do
  curl --fail --location --url \
      "$glibc_url_prefix$glibc_revision"/glibc_x86.tar.gz -o "$1/.glibc.tar" &&
  tar xSvpf "$1/.glibc.tar" -C "$1" &&
  exit 0
  for ((j=glibc_revision+1;j<glibc_revision+revisions_count;j++)); do
    echo "Check if revision \"$j\" is available..."
    if curl --fail --location --url \
          "$glibc_url_prefix$j"/glibc_x86.tar.gz > /dev/null; then
      exit 2
    fi
  done
  sleep "$i"
done
exit 1
