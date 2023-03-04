#!/bin/bash

make -C .. && make
ulimit -n 10000
echo start server
taskset -c 0 ./server &
echo start load
taskset -c 1 ./http-client
kill $(jobs -p)
