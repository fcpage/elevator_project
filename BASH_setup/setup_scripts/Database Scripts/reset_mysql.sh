#script to wipe mysql
sudo mysql -u root -P 3306 << ESE 	#wipe the created users, tables, and schema
	USE mysql;
	DROP USER IF EXISTS 'pi'@'%', 'phpmyadmin'@'localhost';
	USE elevatorg1;
	DROP TABLE IF EXISTS elevatorNetwork, guiRequests, accessAttempts, loginRegistry;
	DROP SCHEMA IF EXISTS elevatorg1;
ESE
sudo service mysql restart #restart mysql so that the changes can take effect
