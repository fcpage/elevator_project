CREATE SCHEMA elevatorg1;
USE elevatorg1;

/*The multi-row table for recording diagnostic data/state changes of the elevator network*/
CREATE TABLE elevatorNetwork(
`index` INT NOT NULL,	/*database access index*/
`date` DATE NOT NULL,	/*snapshot package time*/
`time` TIME NOT NULL,	/*snapshot package date*/
`currentFloor` BIT(2) NOT NULL,	/*the elevator's current position*/
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
    ADD UNIQUE KEY(`time`),	/*time is a unique key*/
    ADD INDEX(`index`),		/*the index is indexed by index*/
    MODIFY `index` INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY;	/*make index the primary key, and auto increment it*/

/*The multi-row table for requesting floors*/
CREATE TABLE guiRequests(	/*table to send gui requests to the hardware system*/
`index` INT NOT NULL,	/*database access index*/
`date` DATE NOT NULL,	/*snapshot package time*/
`time` TIME NOT NULL,	/*snapshot package date*/
`floor` BOOL NOT NULL,	/*gui floor request*/
`remote` INT NOT NULL	/*optional remote maintenance options (enum: 0 = off, 1 = maintainance, 2 = override, 3 = sabbath)*/	
) ENGINE = InnoDb;

/*Set the table indexing, unique key, and auto incrementing of the primary key*/
ALTER TABLE guiRequests
    ADD UNIQUE KEY(`time`),	/*time is a unique key*/
    ADD INDEX(`index`),		/*the index is indexed by index*/
    MODIFY `index` INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY;	/*make index the primary key, and auto increment it*/

DESC elevatorNetwork;
DESC guiRequests;
