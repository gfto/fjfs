#!/bin/sh

set -e

dotest()
{
	cat testmnt.txt
	head -c 20 testmnt.txt
	tail -c 20 testmnt.txt
	head -c 160 testmnt.txt | tail -n 1
	echo

	cnt1=`wc -c test1 | (read a b; echo "$a")`
	if ! expr "x$cnt1" : 'x[0-9][0-9]*$' > /dev/null; then
		echo 'Could not count the characters in the test1 file'
		exit 1
	fi
	cnt2=`wc -c test2 | (read a b; echo "$a")`
	if ! expr "x$cnt2" : 'x[0-9][0-9]*$' > /dev/null; then
		echo 'Could not count the characters in the test2 file'
		exit 1
	fi
	cnt3=`wc -c test3 | (read a b; echo "$a")`
	if ! expr "x$cnt3" : 'x[0-9][0-9]*$' > /dev/null; then
		echo 'Could not count the characters in the test3 file'
		exit 1
	fi

	head -c "$cnt1" testmnt.txt | diff -u test1 -
	tail -c "$cnt3" testmnt.txt | diff -u test3 -
	cat test1 test2 test3 | diff -u - testmnt.txt
}

rm -f testmnt.txt
res=0
./mount.sh
(dotest) || res=$?
./umount.sh
rm -f testmnt.txt
exit "$res"
