#!/bin/bash

echo "Creating $1"
cd kern/conf
./config $1
cd ../compile/$1
bmake depend
bmake
bmake install
