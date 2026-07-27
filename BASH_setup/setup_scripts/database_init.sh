#!/bin/env bash

#Script to set up our SQL database

wait_fn()
{
    echo "Press ENTER key to continue."	    #function to hang the terminal for human purposes
    read -r input	                        #wait for keyboard input
    sleep 0.5	                            #a moment's hesitation
}

echo "password: ese"
sudo mysql -u root -P 3306 << ESE #give root access to all databases
	USE mysql;
	CREATE USER 'pi'@'%' IDENTIFIED BY 'ese';	
	GRANT ALL PRIVILEGES ON *.* TO 'pi'@'%' WITH GRANT OPTION;	
	FLUSH PRIVILEGES;
ESE
#sudo mysql -u phpmyadmin -p -P 3306 #give phpmyadmin access to all databases
#echo "SET PASSWORD FOR 'root'@'localhost' = PASSWORD('ese')" | mysql -u root #setting password for the root in mysql

#creating ese user so we aren't in root all the time
echo ./sql/user_init.sql | mysql -u root -p

#multi-line input sequence... the commands are pretty self-evident
sudo service mysql restart #restart mysql so that the changes can take effect
