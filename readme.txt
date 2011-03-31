filejoinfs v1.0

filejoinfs is a FUSE module that allows several files to be joined into one.

To compile the program run "make".

To use it:
     1. Install the FUSE module
           sudo modprobe fuse

     2. Create the list of files to join
           echo /etc/group >> filelist.txt
           echo /etc/issue >> filelist.txt
           echo /etc/passwd >> filelist.txt

     3. Create an empty file over which the files in the list will be joined
           touch joined.txt

     4. Mount filejoinfs
           ./filejoinfs filelist.txt joined.txt

     5. Check the result with
           cat joined.txt

        You will see the contents of all the files listed in filelist.txt

     6. Unmount the fs
           fusermount -u joined.txt
