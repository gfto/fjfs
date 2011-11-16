#!/bin/sh

mount1_fjfs() {
	echo "Using filelist.txt as input."
	../fjfs --file mount-point filelist.txt
}

mount2_fjfs() {
	echo "Using glob 'test?' as input."
	../fjfs --glob mount-point 'test?'
}

mount3_fjfs() {
	echo "Using args 'test1 test2 test3' as input."
	../fjfs --args mount-point test1 test2 test3
}

umount_fjfs() {
	fusermount -u mount-point && rm mount-point
	echo
}

do_tests() {
	# Direct compare
	diff -u expect_1 mount-point
	if [ $? = 0 ]
	then
		echo "test 1: OK"
	else
		echo "test 1: FAIL!"
	fi

	head -n2 mount-point | tail -n1 > result_2
	diff -u expect_2 result_2
	if [ $? = 0 ]
	then
		echo "test 2: OK"
	else
		echo "test 2: FAIL!"
	fi
	rm result_2

	head -c 120 mount-point > result_3
	diff -u expect_3 result_3
	if [ $? = 0 ]
	then
		echo "test 3: OK"
	else
		echo "test 3: FAIL!"
	fi
	rm result_3

	tail -c 120 mount-point > result_4
	diff -u expect_4 result_4
	if [ $? = 0 ]
	then
		echo "test 4: OK"
	else
		echo "test 4: FAIL!"
	fi
	rm result_4
}

mount1_fjfs
do_tests
umount_fjfs

mount2_fjfs
do_tests
umount_fjfs

mount3_fjfs
do_tests
umount_fjfs
