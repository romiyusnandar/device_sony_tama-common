#!/vendor/bin/sh
if [ -e /dev/pn553 ]; then
    setprop vendor.nfc.chip pn553
elif [ -e /dev/cxd224x-i2c ]; then
    setprop vendor.nfc.chip cxd22xx
fi
