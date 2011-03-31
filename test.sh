./mount.sh
cat testmnt.txt
head -c 20 testmnt.txt
tail -c 20 testmnt.txt
head -c 160 testmnt.txt | tail -n 1
echo
./umount.sh
