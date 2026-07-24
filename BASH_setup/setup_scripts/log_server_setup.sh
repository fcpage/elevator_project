#!/bin/env bash

#Script to log the output of server_setup.sh for posterity

sudo ./server_setup.sh -h 2>&1 | tee -a $HOME/elvitur/server_setup_log.txt