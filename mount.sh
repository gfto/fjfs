#!/bin/sh

set -e

ulimit -c unlimited
touch testmnt.txt
./filejoinfs test.txt testmnt.txt

