#!/bin/bash

while (( !$? )); do
    ./tth 5005 >> /home/adanni/var/log/tth/all.log 2>&1
done
