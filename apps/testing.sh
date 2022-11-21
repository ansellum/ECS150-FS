#!/bin/bash

# ./testing.fs <command> <diskname> <...>

disk=$2

if [ "$#" -lt 2 ]; then
    printf "Usage: <command> <diskname> <...>\n"
    printf "Possible commands are:\n"
    printf "\tinfo\n"
    printf "\tls\n"
    printf "\tadd\n"
    printf "\trm\n"
    printf "\tcat\n"
    printf "\tstat\n"
    printf "\tscript\n"
    exit 1
fi

cp "$disk" "${disk}_REF"
cp "$disk" "${disk}_CUR"

printf "* fs_ref:\n"
./fs_ref.x $1 "${disk}_REF" ${@:3}

printf "\n* current:\n"
./test_fs.x $1 "${disk}_CUR" ${@:3}

## FOR VIEWING CMP ##
if cmp -b "${disk}_REF" "${disk}_CUR" | grep 'differ'; then
    #printf "**\n FS_REF DISK **\n"
    #od -x "${disk}_REF"

    #printf "\n** TEST_FS DISK **\n"
    #od -x "${disk}_CUR"
    
    printf "\n** DIFF REF CUR **\n"
	diff -y <(od -x ${disk}_REF) <(od -x ${disk}_CUR)
fi

rm "${disk}_REF"
rm "${disk}_CUR"
