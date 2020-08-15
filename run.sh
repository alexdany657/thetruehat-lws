#!/bin/bash

while (( !$? )); do
    ./tth >> /home/adanni/var/log/tth/all.log 2>&1
done
