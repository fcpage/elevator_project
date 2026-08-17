#!/bin/bash

wait_fn()
{
    echo "Press ENTER key to continue."	#function to hang the terminal for human purposes
    read -r input	#wait for keyboard input
    sleep 0.5	#a moment's hesitation
}

#script to wipe mysql
sudo mysql -u root -P 3306 << ESE 	#wipe the created users, tables, and schema
	USE mysql;
	SET FOREIGN_KEY_CHECKS = 0;
	DROP USER IF EXISTS 'gui'@'localhost', 'phpmyadmin'@'localhost', 'pi'@'pi.tailcaf9e0.ts.net';
	USE elevatorg1;
	DROP TABLE IF EXISTS elevatorNetwork, stateHistory, guiRequests, accessAttempts, accessRequests, loginRegistry;
	SET FOREIGN_KEY_CHECKS = 1;
	DROP SCHEMA IF EXISTS elevatorg1;
ESE
echo "Database reset completed."
wait_fn
sudo service mysql restart #restart mysql so that the changes can take effect
exit


