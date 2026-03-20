#!/bin/bash

xmake f -m debug -y

if xmake; then 
  echo "build succesful, launching LLDB"
  xmake run -d clox 
else 
  echo "build failed, fix c code first"
  exit 1
fi
