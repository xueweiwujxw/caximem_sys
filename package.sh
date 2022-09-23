petalinux-package --boot --fsbl --fpga --u-boot --force
cp -rvf images/linux/boot.scr images/linux/boot.scr.bin
cp -rvf images/linux/image.ub images/linux/image.ub.bin
cp -rvf images/linux/BOOT.BIN /tftpboot