cmd_drivers/vfio/mdev/vfio_mdev.ko := ld -r -m elf_x86_64  --build-id=sha1  -T scripts/module.lds -o drivers/vfio/mdev/vfio_mdev.ko drivers/vfio/mdev/vfio_mdev.o drivers/vfio/mdev/vfio_mdev.mod.o;  true
