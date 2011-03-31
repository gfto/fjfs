filejoinfs v1.0

filejoinfs is fuse module that allows joining several files as one.

To compile the program run:
       gcc -Wall -Wextra `pkg-config fuse --cflags --libs` filejoinfs.c -lfuse -o filejoinfs

To use it:
     1. Install fuse module
           sudo modprobe fuse

     2. Create file list
           echo /etc/group >> filelist.txt
           echo /etc/issue >> filelist.txt
           echo /etc/passwd >> filelist.txt

     3. Create an empty file over which the files in the list will be joined
           touch joined.txt

     4. Mount filejoinfs
           ./filejoinfs filelist.txt joined.txt

     5. Check the result with
           cat joined.txt

        You will see the contents of files listed in filelist.txt

     6. Unmount the fs
           fusermount -u joined.txt
