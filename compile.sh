#!/bin/bash
gcc -pthread -o gateway src/gateway.c
gcc -pthread -o door src/door.c
gcc -pthread -o security src/security.c
gcc -pthread -o keychain src/keychain.c 
gcc -pthread -o database src/database.c
gcc -pthread -o motion src/motion.c
