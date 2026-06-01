#!/bin/env bash

mkdir -p $HOME/elvitur
chmod -R 777 $HOME/elvitur
cp -u rpi_setup.sh $HOME/elvitur
cp -u server_setup.sh $HOME/elvitur
touch $HOME/elvitur/rpi_setup_log.txt
cd $HOME/elvitur || return
sudo chmod 660 $HOME/elvitur/rpi_setup.sh
echo "$USER ALL=(ALL) NOPASSWD: $HOME/elvitur/rpi_setup.sh" | sudo EDITOR='tee -a' visudo
echo "$USER ALL=(ALL) NOPASSWD: $HOME/elvitur/server_setup.sh" | sudo EDITOR='tee -a' visudo
