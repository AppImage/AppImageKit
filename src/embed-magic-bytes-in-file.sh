#! /bin/bash

if [ "$1" == "" ]; then
    echo "Usage: bash $0 <file>"
    exit 2
fi

echo -ne 'AI\x02' | dd of="$1" bs=1 count=3 seek=8 conv=notrunc
