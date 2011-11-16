#!/bin/sh

set -e

fusermount -u testmnt.txt
rm testmnt.txt
