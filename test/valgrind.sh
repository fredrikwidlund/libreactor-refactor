#!/bin/sh

exit 0

if command -v valgrind; then
    for file in reactor data object hash map mapi maps list buffer vector pool event timer stream notify http server
    do
        echo [$file]
        if ! valgrind --track-fds=yes --error-exitcode=1 --read-var-info=yes --leak-check=full --show-leak-kinds=all test/$file; then
            exit 1
        fi
    done
fi
