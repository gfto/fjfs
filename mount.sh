#!/bin/sh

set -e

ulimit -c unlimited
touch testmnt.txt
./fjfs test.txt testmnt.txt

