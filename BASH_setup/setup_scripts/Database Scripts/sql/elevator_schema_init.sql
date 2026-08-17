CREATE SCHEMA IF NOT EXISTS elevatorg1;
USE elevatorg1;

/*The multi-row table for recording diagnostic data/state changes of the elevator network*/
CREATE TABLE elevatorNetwork(
`index` INT NOT NULL,	/*database access index*/
`date` DATE NOT NULL,	/*snapshot package date*/
`time` TIME NOT NULL,	/*snapshot package time*/
`currentFloor` TINYINT(1) NOT NULL,	/*the elevator's current position*/
`floorRequest1` BOOL NOT NULL,	/*active request at floor 1?*/
`floorRequest2` BOOL NOT NULL,	/*active request at floor 2?*/
`floorRequest3` BOOL NOT NULL,	/*active request at floor 3?*/
`carRequestFloor1` BOOL NOT NULL,	/*active request on car for floor 1?*/
`carRequestFloor2` BOOL NOT NULL,	/*active request on car for floor 2?*/
`carRequestFloor3` BOOL NOT NULL,	/*active request on car for floor 3?*/
`doors` BOOL NOT NULL	/*should the doors be open?*/
) ENGINE = InnoDb;
	
/*Set the table indexing, unique key, and auto incrementing of the primary key*/
ALTER TABLE elevatorNetwork
    ADD UNIQUE (`time`),	/*time is a unique key*/
    ADD INDEX(`index`),		/*the index is indexed by index*/
    MODIFY `index` INT NOT NULL PRIMARY KEY;	/*make index the primary key*/
    
CREATE TABLE stateHistory(
`index` INT NOT NULL,	/*database access index*/
`date` DATE NOT NULL,	/*snapshot package date*/
`time` TIME NOT NULL,	/*snapshot package time*/
`currentFloor` TINYINT(1) NOT NULL,	/*the elevator's current position*/
`floorRequest1` BOOL NOT NULL,	/*active request at floor 1?*/
`floorRequest2` BOOL NOT NULL,	/*active request at floor 2?*/
`floorRequest3` BOOL NOT NULL,	/*active request at floor 3?*/
`doors` TEXT NOT NULL,	/*should the doors be open?*/
`remote` INT NOT NULL
) ENGINE = InnoDb;
	
/*Set the table indexing, unique key, and auto incrementing of the primary key*/
ALTER TABLE stateHistory
    ADD INDEX(`index`),		/*the index is indexed by index*/
    MODIFY `index` INT NOT NULL AUTO_INCREMENT PRIMARY KEY;	/*make index the primary key, and auto increment it*/

/*The multi-row table for requesting floors*/
CREATE TABLE guiRequests(	/*table to send gui requests to the hardware system*/
`index` INT NOT NULL,	/*database access index*/
`date` DATE NOT NULL,	/*snapshot package time*/
`time` TIME NOT NULL,	/*snapshot package date*/
`floor` INT NOT NULL,	/*gui floor request*/
`remote` INT NOT NULL	/*maintenance functions*/
) ENGINE = InnoDb;

/*Set the table indexing, unique key, and auto incrementing of the primary key*/
ALTER TABLE guiRequests
    ADD UNIQUE (`time`),	/*index must be unique*/
    ADD INDEX(`index`),		/*the index is indexed by index*/
    MODIFY `index` INT NOT NULL AUTO_INCREMENT PRIMARY KEY;	/*make index the primary key, and auto increment it*/

CREATE TABLE accessAttempts(
    `index` INT NOT NULL,	/*access attempt index*/
    `date` DATE NOT NULL,	/*access date*/
    `time` TIME NOT NULL,	/*snapshot package time*/
    `username` CHAR(32) NOT NULL,
    `authorization` CHAR(32) NOT NULL,
    `authentication` CHAR(32) NOT NULL
) ENGINE = InnoDb;

/*Set the table indexing, unique key, and auto incrementing of the primary key*/
ALTER TABLE accessAttempts
    ADD UNIQUE KEY(`time`),	/*time is a unique key*/
    ADD INDEX(`index`),		/*the index is indexed by index*/
    MODIFY `index` INT NOT NULL AUTO_INCREMENT PRIMARY KEY;	/*make index the primary key, and auto increment it*/

CREATE TABLE accessRequests(
    `index` INT NOT NULL,
    `date` DATE NOT NULL,	/*access date*/
    `time` TIME NOT NULL,	/*snapshot package time*/
    `firstname` CHAR(32) NOT NULL,
    `lastname` CHAR(32) NOT NULL,
    `email` CHAR(32) NOT NULL,
    `person` CHAR(32) NOT NULL,
    `involvement` TEXT NOT NULL,
    `reason` CHAR(32) NOT NULL,
    `details` TEXT NOT NULL,
    `good_job` CHAR(32) NOT NULL,
    `granted` BOOL NOT NULL
) ENGINE = InnoDb;

ALTER TABLE accessRequests
    ADD UNIQUE KEY(`email`),	/*time is a unique key*/
    ADD INDEX(`index`),		/*the index is indexed by index*/
    MODIFY `index` INT NOT NULL AUTO_INCREMENT PRIMARY KEY;	/*make index the primary key, and auto increment it*/

CREATE TABLE loginRegistry(
    `index` INT NOT NULL,
    `username` CHAR(32) NOT NULL,
    `password` CHAR(8) NOT NULL,
    `authorization` CHAR(5) NOT NULL
) ENGINE = InnoDb;

/*Set the table indexing, unique key, and auto incrementing of the primary key*/
ALTER TABLE loginRegistry
    ADD UNIQUE KEY(`username`),	/*time is a unique key*/
    ADD INDEX(`index`),		/*the index is indexed by index*/
    MODIFY `index` INT NOT NULL AUTO_INCREMENT PRIMARY KEY;	/*make index the primary key, and auto increment it*/
    

INSERT INTO loginRegistry (username, password, authorization) VALUES ('nigel_sinclair', '12345678', 'dev');
INSERT INTO loginRegistry (username, password, authorization) VALUES ('ryan_pratt', '12345678', 'dev');
INSERT INTO loginRegistry (username, password, authorization) VALUES ('fergus_page', '12345678', 'dev');
INSERT INTO loginRegistry (username, password, authorization) VALUES ('safat_khan', '12345678', 'prof');
INSERT INTO loginRegistry (username, password, authorization) VALUES ('hassan_zaytoon', '12345678', 'prof');
INSERT INTO loginRegistry (username, password, authorization) VALUES ('elevator', '12345678', 'run');
INSERT INTO loginRegistry (username, password, authorization) VALUES ('maintenance', '12345678', 'admin');

DESC elevatorNetwork;
DESC stateHistory;
DESC guiRequests;
DESC accessAttempts;
DESC accessRequests;
DESC loginRegistry;
quit;
