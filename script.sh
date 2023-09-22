#!/bin/bash

program=$1
times=$2
arg1=$3
arg2=$4

if [ -z "$program" ] || [ -z "$times" ] || [ -z "$arg1" ] || [ -z "$arg2" ]; then
  echo "Usage: $0 <program> <times> <arg1> <arg2>"
  exit 1
fi

for ((i=0; i<times; i++))
do
  $program $arg1 $arg2 &
done