#!/bin/bash

# This script was used to generate "mount_test.img" file, which contains
# ext3 file system with an empty, unlabelled file. Mounting the filesystem
# via loopback device will be helpful for verifying "smackfsdef" mount option.

IMG_FILE=mount_test.img
echo "Creating image file: $IMG_FILE"

# create empty file of 512*128=64KB
dd if=/dev/zero of=$IMG_FILE count=128

# init filesystem
mkfs -t ext3 $IMG_FILE

# mount fs and create an empty file in it
mkdir -p fs
mount -o loop $IMG_FILE fs
touch fs/file
umount -l fs
rm -rf fs

chmod 444 $IMG_FILE

