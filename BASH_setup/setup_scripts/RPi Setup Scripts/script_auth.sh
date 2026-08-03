#!/bin/env bash

#Script to set up home directory, file permissions, and to initialize logging files

wait_fn()
{
    echo "Press ENTER key to continue."	    #function to hang the terminal for human purposes
    read -r input	                        #wait for keyboard input
    sleep 0.5	                            #a moment's hesitation
}

mkdir -p $HOME/elvitur                      #don't make it twice, but we need at least one
chmod -R 777 $HOME/elvitur                  #no holds barred
cp -u rpi_setup.sh $HOME/elvitur            #relocate the setup scripts
cp -u server_setup.sh $HOME/elvitur         #
cp -u database_setup.sh $HOME/elvitur       #
cp -u log_rpi_setup.sh $HOME/elvitur        #
cp -u log_server_setup.sh $HOME/elvitur     #
cp -u log_database_setup.sh $HOME/elvitur   #
touch $HOME/elvitur/rpi_setup_log.txt       #create the log files
touch $HOME/elvitur/server_setup_log.txt    #
touch $HOME/elvitur/database_setup_log.txt  #
cd $HOME/elvitur || return                  #go to that folder if you can
echo "$USER ALL=(ALL) NOPASSWD: $HOME/elvitur/rpi_setup.sh" | sudo EDITOR='tee -a' visudo           #passwords are for weenies
echo "$USER ALL=(ALL) NOPASSWD: $HOME/elvitur/log_rpi_setup.sh" | sudo EDITOR='tee -a' visudo       #
echo "$USER ALL=(ALL) NOPASSWD: $HOME/elvitur/server_setup.sh" | sudo EDITOR='tee -a' visudo        #
echo "$USER ALL=(ALL) NOPASSWD: $HOME/elvitur/log_server_setup.sh" | sudo EDITOR='tee -a' visudo    #
echo "$USER ALL=(ALL) NOPASSWD: $HOME/elvitur/database_setup.sh" | sudo EDITOR='tee -a' visudo      #
echo "$USER ALL=(ALL) NOPASSWD: $HOME/elvitur/log_database_setup.sh" | sudo EDITOR='tee -a' visudo  #
echo "Authorisation Complete"               #report
wait_fn                                     #wait
exit                                        #leave