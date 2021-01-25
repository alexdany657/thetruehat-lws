#!/bin/bash

# RELEASE_FLAG="RELEASE";
RELEASE_FLAG="DEBUG";

if [ $RELEASE_FLAG = "RELEASE" ]; then
    # release
    EXEC_NAME=tth
    MASTER_EXEC_NAME=master_tth
elif [ $RELEASE_FLAG = "DEBUG" ]; then
    # debug
    EXEC_NAME=tth.d
    MASTER_EXEC_NAME=master_tth.d
else
    echo RELEASE_FLAG should be "RELEASE" or "DEBUG"
    exit 1
fi
