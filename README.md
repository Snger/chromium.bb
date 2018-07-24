# chromium.bb

This repository contains the version of Chromium used by Bloomberg, along with
all the modifications made by Bloomberg in order to use it in our environment.

This repository serves a couple of purposes for us:

* **Provide a single repository**
  A typical Chromium checkout using gclient fetches code from multiple
repositories.  Rather than maintaining multiple repositories for this project,
we merge all sub-repositories as sub-trees under the root repository.  This
allows us to keep the history from each repository into a self contained
repository and also eliminates the need to run gclient for every checkout.

* **Provide a space for us to make/publish changes**
  We have made a bunch of changes to different parts of Chromium in order to
make it behave the way we need it to.  Some of these changes are bugfixes that
can be submitted upstream, and some of them are just changes that we
specifically wanted for our product, but may not be useful or desirable
upstream.  Each change is made on a separate branch.  This allows us, at
release time, to pick and choose which bugfixes/features we want to include in
the release.

  Note that while most of our bugfixes are not submitted upstream, it is our
intention to submit as many bugfixes upstream as we can.

  The list of branches along with descriptions and test cases can be found
[here](http://bloomberg.github.com/chromium.bb/).


## Branch Structure

The `master` branch contains the code that is used to make the official
Chromium builds used by Bloomberg.  It is not used for development.  Actual
development happens in one of the `bugfix/*` or `feature/*` branches.

Each `bugfix/*` or `feature/*` branch is based on the `upstream/latest` branch,
which contains a snapshot of the code we get from the upstream Chromium
project.

The `release/candidate` branch contains changes that are scheduled to be
included in the next release.

## Cloning the repository

By default, Git for Windows has a very small limit on the maximum filename
length.  Some files in this repository exceed this 260 character limit so it's
important to configure Git to allow long filenames.

            git config --global core.longpaths true
            git clone https://github.com/bloomberg/chromium.bb.git
            git checkout master

If you don't want to set this configuration at the global level, you can do
the following instead:

            git init
            git config core.longpaths true
            git remote add origin https://github.com/bloomberg/chromium.bb.git
            git fetch origin master
            git checkout master

## Build Instructions

**Bloomberg Employees:** [Build steps](https://cms.prod.bloomberg.com/team/display/rwf/Maintenance+of+blpwtk2)

If you are **not** a Bloomberg employee, the following instructions should still
work:

* Download the dependencies:
    * [Python 2.7](https://www.python.org/downloads/release/python-2715/)
    * Visual Studio 2015 Update 3
    * Windows 10 SDK 10.0.14393
    * [Depot Tools](https://storage.googleapis.com/chrome-infra/depot_tools.zip)

* Setup your build environment:

            export PATH="${DEPOT_TOOLS_PATH}:${PATH}"
            export DEPOT_TOOLS_WIN_TOOLCHAIN=0

  Please replace ${DEPOT_TOOLS_PATH} with the path to your depot_tools.

* Run the following command from inside the top-level directory:

            cd src
            build/runhooks.py
            build/blpwtk2.py

  This will generate the necessary ninja makefiles needed for the build.

* Run the following commands to start the build:

            ninja -C out/shared/Debug blpwtk2_all    # for component Debug builds
            ninja -C out/shared/Release blpwtk2_all  # for component Release builds
            ninja -C out/static/Debug blpwtk2_all    # for non-component Debug builds
            ninja -C out/static/Release blpwtk2_all  # for non-component Release builds

---
###### Microsoft, Windows, Visual Studio and ClearType are registered trademarks of Microsoft Corp.
###### Firefox is a registered trademark of Mozilla Corp.
###### Chrome is a registered trademark of Google Inc.
