.. SPDX-License-Identifier: GPL-2.0-or-later

SpacemiT K1 eMMC and SD Card Boot Guide
=======================================

This guide covers two separate methods for booting and flashing U-Boot on
SpacemiT K1 based boards:

1. **eMMC Flash**: Flashing U-Boot and SPL images to eMMC via USB fastboot.
2. **SD Card Boot**: Creating a bootable Bianbu SD card and optionally
   replacing U-Boot on the card.

Tested boards: Banana Pi BPI-F3, MusePi Pro.


Chapter 1: eMMC Flash (U-Boot via USB Fastboot)
===============================================

SpacemiT K1 U-Boot Flash Guide
==============================

This guide explains how to flash U-Boot on SpacemiT K1 based boards. It covers
flashing images via USB fastboot.

.. note::

   This procedure flashes images to eMMC over USB fastboot. The fastboot
   function is not enabled in our SPL yet, so the download stage runs the
   SpacemiT released SPL; our built FSBL.bin and fit.itb are the images
   written to eMMC and used on the next normal boot.

Prerequisites
~~~~~~~~~~~~~

- A SpacemiT K1 board with USB Type-C and UART access
- USB-to-UART adapter (3.3V TTL)
- ``minicom`` or equivalent serial terminal, configured at 115200 8N1
- ``fastboot`` and ``flashserver`` tool on the host

Hardware Setup
~~~~~~~~~~~~~~

Refer to k1-spl.rst.

Flash images on eMMC
~~~~~~~~~~~~~~~~~~~~

**1. Obtain the release images**

Get the release package from Spacemit website. It contains SPL image, and so on.

https://archive.spacemit.com/image/k1/version/bianbu/v2.3.3/Bianbu-Minimal-K1-V2.3.3-20260128183217.zip

Unzip images and store them into a directory.

**2. Obtain flashserver tool**

Get ``flashserver`` from Spacemit website.

.. code-block:: bash

    $wget https://cdn-resource.spacemit.com/file/flash/flashserver
    $chmod +x flashserver
    $mv flashserver {flash image path}/

**3. Copy built SPL and U-Boot images**

Build U-Boot as mentioned in k1-spl.rst. Create a new directory to save.
The official u-boot.itb is used to download images. So the built U-Boot should
not replace the official one.

.. code-block:: bash

    $mkdir {flash image path}/build
    $cd {flash image path}
    $ln -sf {path to FSBL.bin} ./build/
    $ln -sf {path to u-boot.itb} ./build/fit.itb

``{path to FSBL.bin}`` is the signed FSBL produced by ``fsbl.sh`` in
k1-spl.rst, e.g. ``~/uboot-2022.10/spl_bin/FSBL.bin``.
``{path to u-boot.itb}`` is the U-Boot build output, e.g.
``~/u-boot/u-boot.itb``.

**4. Update configuration files**

The ``partition_2M.json`` and ``partition_universal.json`` files come from
the release package. Patch the ``fsbl`` and ``uboot`` entries to point at
the images staged under ``build/`` (pick the layout that matches your eMMC):

.. code-block:: diff

   diff -puNr bianbu-25/partition_2M.json clean/partition_2M.json
   --- bianbu-25/partition_2M.json	2026-03-02 11:55:58.631116807 +0800
   +++ clean/partition_2M.json	2026-05-20 11:25:21.683801401 +0800
   @@ -13,7 +13,7 @@
          "name": "fsbl",
          "offset": "128K",
          "size": "256K",
   -      "image": "factory/FSBL.bin"
   +      "image": "build/FSBL.bin"
        },
        {
          "name": "env",
   @@ -31,7 +31,7 @@
          "name": "uboot",
          "offset": "640K",
          "size": "-",
   -      "image": "u-boot.itb"
   +      "image": "build/fit.itb"
        }
      ]
    }
   diff -puNr bianbu-25/partition_universal.json clean/partition_universal.json
   --- bianbu-25/partition_universal.json	2026-03-02 11:55:58.642116862 +0800
   +++ clean/partition_universal.json	2026-05-20 11:26:23.932581853 +0800
   @@ -14,7 +14,7 @@
               "name": "fsbl",
               "offset": "128K",
               "size": "256K",
   -            "image": "factory/FSBL.bin"
   +            "image": "build/FSBL.bin"
           },
           {
               "name": "env",
   @@ -32,7 +32,7 @@
               "name": "uboot",
               "offset": "2M",
               "size": "2M",
   -            "image": "u-boot.itb"
   +            "image": "build/fit.itb"
           },
           {
               "name": "bootfs",

Deploying via USB Fastboot
~~~~~~~~~~~~~~~~~~~~~~~~~~~

To enter BootROM fastboot mode:

1. Power off the board by unplugging its power supply.
2. **Press and hold** the FDL button (called "Boot Key" on some boards;
   see the board layout above for the BPI-F3).
3. While holding the button, use a USB cable to connect the OTG port to
   your host. This cable is also used by fastboot to upload the firmware.
4. Release the button.

On the host, ``fastboot devices`` should list the board::

    dfu-device     DFU download

The serial console shows the BootROM's USB download handler trace,
including a line like::

    usb2d_initialize : enter

This indicates the board is ready to accept an image via USB.

.. tip::

   If you are worried about insufficient USB power, you can first plug
   in the power, then release the button, and then plug in the USB
   cable.

On the host:

.. code-block:: console

    $sudo ./flashserver

When ``flashserver`` is running, it lists the detected fastboot devices.
Enter the corresponding number to select one.


Chapter 2: SD Card Boot
=======================


SpacemiT K1 Bianbu SD Card Image Flashing and U-Boot Update Guide
==================================================================

This guide explains how to prepare a bootable SD card with Bianbu OS for
SpacemiT K1 based boards and how to replace the U-Boot binary on the SD
card with a custom build.

Prerequisites
~~~~~~~~~~~~~

- A SpacemiT K1 based development board
- A microSD card (at least 8 GB capacity recommended)
- A card reader for your host computer
- A Linux host system (for ``dd``, ``fdisk``, ``lsblk`` commands)
- The Bianbu SD card image from
   <https://spacemit.com/community/resources-download/Images%20Collects/K1/Bianbu>
- A custom ``u-boot.itb`` file (device tree blob or U-Boot FIT image) to be
  written to the U-Boot partition

Prepare the SD Card & the image
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**1. Download the image**

Download the released package from the official SpacemiT website:

<https://archive.spacemit.com/image/k1/version/bianbu/v2.3.5/Bianbu-Minimal-K1-sdcard-V2.3.5-20260601180942.img.zip>

**2. Extract the image**

.. code-block:: console

   $ unzip Bianbu-Minimal-K1-sdcard-V2.3.5-20260601180942.img.zip

**3. Identify the SD card device**

Insert the microSD card into your card reader, then run:

.. code-block:: console

   $ lsblk

Compare the output before and after inserting the card to identify
the new device. It will typically appear as ``/dev/sdb``, ``/dev/sdc``,
or ``/dev/mmcblk0``.

**4. Write the image to the SD card**

.. code-block:: console

   $ sudo dd if=./Bianbu-Minimal-K1-sdcard-V2.3.5-20260601180942.img of=/dev/sdb bs=1M status=progress

The SD card is now ready as a bootable Bianbu system disk.


Understanding the SD Card Partition Layout
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

After writing the image, the SD card has the following partition structure
(verified with ``sudo fdisk -l /dev/sdb``):

.. code-block:: text

   Device      Start     End Sectors  Size Type
   /dev/sdb1     256     767     512  256K Linux filesystem
   /dev/sdb2     768     895     128   64K Linux filesystem
   /dev/sdb3    2048    4095    2048    1M Linux filesystem
   /dev/sdb4    4096    8191    4096    2M Linux filesystem
   /dev/sdb5    8192  532479  524288  256M Linux filesystem
   /dev/sdb6  532480 4726783 4194304    2G Linux filesystem

The role of each partition:

+----------+----------+--------------------------------------------------+
| Partition| Size     | Purpose                                          |
+==========+==========+==================================================+
| ``sdb1`` | 256 KB   | Boot information for Boot ROM                    |
+----------+----------+--------------------------------------------------+
| ``sdb2`` | 64 KB    | FSBL (First Stage Bootloader)                    |
+----------+----------+--------------------------------------------------+
| ``sdb3`` | 1 MB     | OpenSBI / U-Boot environment                     |
+----------+----------+--------------------------------------------------+
| ``sdb4`` | 2 MB     | **U-Boot binary (``u-boot.itb``)**               |
+----------+----------+--------------------------------------------------+
| ``sdb5`` | 256 MB   | Boot partition (FAT32, kernel + device tree)     |
+----------+----------+--------------------------------------------------+
| ``sdb6`` | 2 GB     | Root filesystem (ext4)                           |
+----------+----------+--------------------------------------------------+


Replacing U-Boot on the SD Card
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
This section explains how to replace the U-Boot binary on the SD card
with your own custom ``u-boot.itb`` file. For this guide, the custom file
is a ``Device Tree Blob`` , which fits within
the 2 MB ``/dev/sdb4`` partition.


**1. Confirm the SD card device and partition**

.. code-block:: console

   $ sudo fdisk -l /dev/sdb

Ensure that ``/dev/sdb4`` exists and has the expected size (2 MB),
and that the replacement ``u-boot.itb`` is no larger than the size.


**2. Write the new U-Boot image**

.. code-block:: console

   $ sudo dd if=./u-boot.itb of=/dev/sdb4 bs=1M status=progress

Example successful output:

.. code-block:: text

   0+1 records in
   0+1 records out
   873033 bytes (873 kB, 853 KiB) copied, 1.22109 s, 715 kB/s


**3. Synchronize**

.. code-block:: console

   $ sync

**4. Eject the SD card**

.. code-block:: console

   $ sudo eject /dev/sdb

Booting and Testing
~~~~~~~~~~~~~~~~~~~

Insert the SD card into the development board, connect the serial
console (115200 8N1), and power on the board.

- If the board boots successfully, the new device tree or U-Boot image
  is compatible with your hardware.
