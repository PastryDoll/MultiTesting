#!/bin/bash

run=false
# Check if there are any command-line arguments
if [ "$1" = "-r" ]; then
    run=true
fi

echo "Building..."
clang++ -O3 handmade.cpp -o handmade 
 
if [ "$run" = true ]; then
    echo "Running..."
  ./handmade
fi
