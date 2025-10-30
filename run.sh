#!/bin/bash
# Simple run helper: build and run server in background, then run client
make
./server & 
SERVER_PID=$!
sleep 1
./client
kill $SERVER_PID
