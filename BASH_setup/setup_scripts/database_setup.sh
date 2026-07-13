#!/bin/env bash

#Script to set up our SQL database

wait_fn()
{
    echo "Press ENTER key to continue."	    #function to hang the terminal for human purposes
    read -r input	                        #wait for keyboard input
    sleep 0.5	                            #a moment's hesitation
}

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