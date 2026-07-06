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
sudo mysql -u phpmyadmin -p -P 3306 #give phpmyadmin access to all databases
echo "SET PASSWORD FOR 'root'@'localhost' = PASSWORD('ese')" | mysql -u root #setting password for the root in mysql

#creating ese user so we aren't in root all the time
mysql -u root -p << POM 
	USE mysql;
	CREATE USER 'ese'@'%' IDENTIFIED BY 'ese';
	GRANT ALL PRIVILEGES ON *.* TO 'ese'@'%' WITH GRANT OPTION;
	FLUSH PRIVILEGES;
	quit;
POM

#multi-line input sequence... the commands are pretty self-evident
sudo service mysql restart #restart mysql so that the changes can take effect

#remove gaps in following heredoc before running
mysql -u ese -p << POM
CREATE SCHEMA elevatorg1;
USE elevatorg1;

CREATE TABLE log (
	index INT NOT NULL,
	date DATE NOT NULL,
	time TIME NOT NULL,
	nodeID INT NOT NULL,
	sender TINYINT UNSIGNED NOT NULL,
	receiver TINYINT UNSIGNED NOT NULL,
	currentFloor BIT(2) NOT NULL,
	requestFloor BIT(2) NOT NULL,
	status BIT(2) NOT NULL,
    queued BOOL NOT NULL,
	served BOOL NOT NULL
    ) ENGINE = InnoDb;
	
	ALTER TABLE elevatorg1
	ADD UNIQUE KEY(time),
	ADD INDEX(index);
	
	MODIFY nodeID INT UNSIGNED NOT NULL
	AUTO_INCREMENT PRIMARY KEY
	CREATE TABLE CAN_subNetwork(
	INT(10) UNSIGNED NOT NULL,
	FOREIGN KEY (CAN_nodeID) REFERENCES elevatorNetwork(nodeID)
	) ENGINE = InnoDb;
	
	ALTER TABLE CAN_subNetwork
	ADD CAN_status TINYINT NOT NULL,
	ADD CAN_currentFloor TINYINT NOT NULL;
	
	DESC elevatorg1;
	DESC CAN_subNetwork;
POM

mysql -u ese -p << POM
	INSERT INTO elevatorg1 (column1, column2, etc) VALUES (value1, value2, etc);
POM
#index int,			#RPi or web write
#date date,			#RPi or web write
#time time,			#PRi or web write
#sender char(5),	#Floor or web ID
#receiver char(5),	#RPi write only
#call bit(2),		#RPi or web write, floor ID
#current bit(2),	#RPi write only, floor ID
#queued bool,		#RPi write only
#served bool		#RPi write only