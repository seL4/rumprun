#! /bin/sh
#
# Copyright 2017, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
# @TAG(DATA61_BSD)
#

set -eu

# Initialise rumprun git submodules
git submodule init

# Update git submodules
# "$@" is used to pass --checkout --force arguments to git
# to force update and disgard local changes for during regression
git submodule update "$@"

# Apply submodule patches
(cd src-netbsd && git am ../src-netbsd.patches/*)
(cd buildrump.sh && git am ../buildrump.sh.patches/*)
touch .rumpstamp
