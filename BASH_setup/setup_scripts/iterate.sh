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
	DROP USER IF EXISTS 'pi'@'%', 'phpmyadmin'@'localhost';
	USE elevatorg1;
	DROP TABLE IF EXISTS elevatorNetwork, maintenance, guiRequests, accessAttempts, loginRegistry, doors;
	SET FOREIGN_KEY_CHECKS = 1;
	DROP SCHEMA IF EXISTS elevatorg1;
ESE
echo "Database reset completed."
wait_fn
sudo service mysql restart #restart mysql so that the changes can take effect

sudo mysql -u root -P 3306 << ESE 	#give root access to all databases
	USE mysql;
	CREATE USER IF NOT EXISTS 'phpmyadmin'@'localhost' IDENTIFIED BY 'ese';
	GRANT ALL PRIVILEGES ON *.* TO 'phpmyadmin'@'localhost' WITH GRANT OPTION;
	CREATE USER IF NOT EXISTS 'pi'@'%' IDENTIFIED BY 'ese';	
	GRANT ALL PRIVILEGES ON *.* TO 'pi'@'%' WITH GRANT OPTION;
	FLUSH PRIVILEGES;
ESE
echo "USER: phpmyadmin"
echo "PASSWORD: ese"
sudo mysql -u phpmyadmin -pese -P 3306 << ESE #give phpmyadmin access to port
quit;
ESE
echo "USER: pi"
echo "PASSWORD: ese"
sudo mysql -u pi -pese < ./sql/elevator_schema_init.sql	#initialize the tables
echo "Database setup completed."
wait_fn
sudo service mysql restart #restart mysql so that the changes can take effect

exit
