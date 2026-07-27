#!/bin/env bash

#Script to set up our SQL database

wait_fn()
{
    echo "Press ENTER key to continue."	    #function to hang the terminal for human purposes
    read -r input	                        #wait for keyboard input
    sleep 0.5	                            #a moment's hesitation
}

sudo mysql -u root -p -P 3306 #give root access to all databases
wait_fn #let that sink in
echo "password: ese"	#you probably should change at least one of your passwords...
wait_fn #HOLD IT RIGHT THERE!
sudo mysql -u phpmyadmin -pese -P 3306 #give phpmyadmin access to all databases
#echo "SET PASSWORD FOR 'root'@'localhost' = PASSWORD('ese')" | mysql -u root #setting password for the root in mysql

#creating ese user so we aren't in root all the time
sudo mysql -u root -pese < ./sql/user_init.sql

#multi-line input sequence... the commands are pretty self-evident
sudo service mysql restart #restart mysql so that the changes can take effect
