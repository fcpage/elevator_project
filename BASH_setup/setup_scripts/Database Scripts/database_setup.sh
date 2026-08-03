#!/bin/env bash

#Script to set up our SQL database
wait_fn()
{
    echo
    echo "Press ENTER key to continue."	    #function to hang the terminal for human purposes
    read -r input	                    #wait for keyboard input
    sleep 0.5	                            #a moment's hesitation
}

echo "password: ese" 			#creating ese user so we aren't in root all the time
wait_fn()				#take it all in
sudo mysql -u phpmyadmin -P 3306 	#give phpmyadmin access to all databases
sudo mysql -u root -P 3306 << ESE 	#give root access to all databases
	USE mysql;
	SET PASSWORD FOR 'root'@'localhost' = PASSWORD('ese');
	SET PASSWORD FOR 'phpmyadmin'@'localhost' = PASSWORD('ese');
	CREATE USER 'pi'@'%' IDENTIFIED BY 'ese';	
	GRANT ALL PRIVILEGES ON *.* TO 'pi'@'%' WITH GRANT OPTION;
	GRANT ALL PRIVILEGES ON *.* TO 'phpmyadmin'@'%' WITH GRANT OPTION;
	FLUSH PRIVILEGES;
ESE
mysql -u pi -p < ./sql/elevator_schema_init.sql	#initialize the tables
sudo service mysql restart #restart mysql so that the changes can take effect
