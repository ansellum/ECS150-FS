#!/bin/bash

# ./testing.fs <command> <diskname> <...>

disk=$2

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
