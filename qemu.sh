#!/bin/sh
set -e
. ./iso.sh
QEMU_PREFIX=${QEMU_PREFIX:-/usr/bin}


$QEMU_PREFIX/qemu-system-$(./target-triplet-to-arch.sh $HOST) -s -cdrom Onyx.iso -m 512M -monitor stdio -boot d -net nic,model=e1000 -net dump,file=net.pcap -net user --enable-kvm -smp 4 -cpu SandyBridge,+avx -d int -vga vmware -device ahci,id=ahci -drive if=none,id=sata1,file=hdd.img,format=raw -device ide-hd,drive=sata1,bus=ahci.0
