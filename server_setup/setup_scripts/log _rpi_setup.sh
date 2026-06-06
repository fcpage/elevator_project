#!/bin/env bash

#Script to log the output of server_setup.sh for posterity

sudo ./rpi_setup.sh -h 2>&1 | tee -a $HOME/elvitur/rpi_setup_log.txt