#!/vendor/bin/sh

MSIM_DEVICES=(
    H8266 H8296 # XZ2
    H8324 # XZ2 Compact
    H8166 # XZ2 Premium
    H9436 H9493 # XZ3
)
MSIM_DEVICE=0

LTALABEL_DEV=""
for dev in "/dev/block/bootdevice/by-name/LTALabel" \
           "/dev/block/by-name/LTALabel" \
           "/dev/block/platform/soc/1d84000.ufshc/by-name/LTALabel"; do
    if [ -b "$dev" ]; then
        LTALABEL_DEV="$dev"
        break
    fi
done

if [ -n "$LTALABEL_DEV" ]; then
    for device in "${MSIM_DEVICES[@]}"; do
        if grep -q "Model: ${device}" "$LTALABEL_DEV" 2>/dev/null || strings "$LTALABEL_DEV" 2>/dev/null | grep -q "${device}"; then
            MSIM_DEVICE=1
            break
        fi
    done
fi

if [[ "${MSIM_DEVICE}" -eq 1 ]]; then
    setprop persist.vendor.radio.multisim.config dsds
else
    setprop persist.vendor.radio.multisim.config ss
fi
