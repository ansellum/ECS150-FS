#!/bin/bash

# ./testing.fs <command> <diskname> <...>

disk=$2

if [ "$#" -lt 2 ]; then
    printf "Usage: <diskname> <script filename>\n"
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

printf "fs_ref:\n"
./fs_ref.x $1 "${disk}_REF" ${@:3}
printf "\n"

printf "current:\n"
./test_fs.x $1 "${disk}_CUR" ${@:3}
printf "\n"

printf "diff:\n"
diff "${disk}_REF" "${disk}_CUR"

rm "${disk}_REF"
rm "${disk}_CUR"
