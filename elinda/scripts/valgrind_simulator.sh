#!/bin/sh

. ../../mbus/scripts/set_ld_library.sh

(../../mbus/Tests/test_mbus &)
sleep 1
valgrind ../Debug/elinda
