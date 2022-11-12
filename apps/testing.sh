#!/bin/bash

diff <(./fs_ref.x $1 $2) <(./test_fs.x $1 $2)
