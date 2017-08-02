#!/bin/sh
min_size=$(($(stat --printf="%s" kernel/vmonyx)+$(stat --printf="%s" isodir/boot/initrd.tar)+$(stat --printf="%s" efibootldr/bootx64.efi)+10000000))
fallocate -l $min_size esp.part
mformat -i esp.part -h 32 -t 32 -n 64 -c 1
mmd -i esp.part EFI
mmd -i esp.part EFI/BOOT
mcopy -i esp.part efibootldr/bootx64.efi ::/EFI/BOOT
mcopy -i esp.part kernel/vmonyx ::
mcopy -i esp.part isodir/boot/initrd.tar ::
mkdir iso
cp esp.part iso
xorriso -as mkisofs -R -f -e esp.part -no-emul-boot -o Onyx.iso iso
rm -rf iso
rm esp.part
