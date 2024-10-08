#!/bin/bash

SDDF_DIR=/home/puppy/ming/sddf_worktree/pancake_echo
PANCAKE_FILES=(
    "${SDDF_DIR}/util/util.🥞"
    "${SDDF_DIR}/include/sddf/network/queue_helper.🥞"
    "${SDDF_DIR}/include/sddf/network/queue.🥞"
    "${SDDF_DIR}/drivers/network/imx/ethernet_helper.🥞"
    "${SDDF_DIR}/drivers/network/imx/ethernet.🥞"
)

# Check if all files exist and are readable
for file in "${PANCAKE_FILES[@]}"; do
    if [ ! -r "$file" ]; then
        echo "Error: Cannot read file $file" >&2
        exit 1
    fi
done

# Concatenate files and process
cat "${PANCAKE_FILES[@]}" | cpp -C -P > driver.🥞

# Check if driver.🥞 was created successfully
if [ ! -f driver.🥞 ]; then
    echo "Error: Failed to create driver.🥞" >&2
    exit 1
fi

# Transpile the pancake files
pancake2viper driver.🥞 -o driver.vpr

echo "Transpilation completed successfully."