CREATE SCHEMA elevatorg1;
USE elevatorg1;

/*The multi-row table for recording diagnostic data/state changes of the elevator network*/
CREATE TABLE elevatorNetwork(
`index` INT NOT NULL,	/*database access index*/
`date` DATE NOT NULL,	/*snapshot package date*/
`time` TIME NOT NULL,	/*snapshot package time*/
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

CREATE TRIGGER supervisorStateChange AFTER INSERT ON elevatorNetwork
    FOR EACH ROW SELECT index FROM elevatorNetwork INTO OUTFILE
    '/var/www/html/project_vi_website/general/resources/requests/database/toGUI.csv'
    FIELDS TERMINATED BY ',' LINES TERMINATED BY '\n';

/*The multi-row table for requesting floors*/
CREATE TABLE guiRequests(	/*table to send gui requests to the hardware system*/
`index` INT NOT NULL,	/*database access index*/
`date` DATE NOT NULL,	/*snapshot package time*/
`time` TIME NOT NULL,	/*snapshot package date*/
`floor` INT NOT NULL,	/*gui floor request*/
`remote` INT NOT NULL	/*optional remote maintenance options (enum: 0 = off, 1 = maintainance, 2 = sabbath)*/	
) ENGINE = InnoDb;

/*Set the table indexing, unique key, and auto incrementing of the primary key*/
ALTER TABLE guiRequests
    ADD UNIQUE KEY(`time`),	/*time is a unique key*/
    ADD INDEX(`index`),		/*the index is indexed by index*/
    MODIFY `index` INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY;	/*make index the primary key, and auto increment it*/

CREATE TRIGGER guiRequest AFTER INSERT ON guiRequests
    FOR EACH ROW SELECT index FROM guiRequests INTO OUTFILE
    '/var/www/html/project_vi_website/general/resources/requests/database/toSupervisor.csv'
    FIELDS TERMINATED BY ',' LINES TERMINATED BY '\n';

CREATE TABLE accessAttempts(
    `index` INT NOT NULL,	/*access attempt index*/
    `date` DATE NOT NULL,	/*access date*/
    `time` TIME NOT NULL,	/*snapshot package time*/
    `user` VARCHAR NOT NULL,
    `authorization` VARCHAR NOT NULL,
    `authentication` VARCHAR NOT NULL
) ENGINE = InnoDb;

/*Set the table indexing, unique key, and auto incrementing of the primary key*/
ALTER TABLE accessAttempts
    ADD UNIQUE KEY(`time`),	/*time is a unique key*/
    ADD INDEX(`index`),		/*the index is indexed by index*/
    MODIFY `index` INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY;	/*make index the primary key, and auto increment it*/

CREATE TABLE loginRegistry(
    `index` INT NOT NULL,
    `username` VARCHAR NOT NULL,
    `password` VARCHAR NOT NULL,
    `authorization` VARCHAR NOT NULL,
    `accepted` BOOL NOT NULL
) ENGINE = InnoDb;

/*Set the table indexing, unique key, and auto incrementing of the primary key*/
ALTER TABLE loginRegistry
    ADD UNIQUE KEY(`time`),	/*time is a unique key*/
    ADD INDEX(`index`),		/*the index is indexed by index*/
    MODIFY `index` INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY;	/*make index the primary key, and auto increment it*/

INSERT INTO loginRegistry VALUES (1, nigel_sinclair, 12345678, dev);
INSERT INTO loginRegistry VALUES (2, ryan_pratt, 12345678, dev);
INSERT INTO loginRegistry VALUES (3, fergus_page, 12345678, dev);
INSERT INTO loginRegistry VALUES (4, safat_khan, 12345678, prof);
INSERT INTO loginRegistry VALUES (5, hassan_zaytoon, 12345678, prof);
INSERT INTO loginRegistry VALUES (6, elevator, 12345678, run);
INSERT INTO loginRegistry VALUES (7, maintenance, 12345678, admin);

DESC elevatorNetwork;
DESC guiRequests;
DESC accessAttempts;
DESC loginRegistry;