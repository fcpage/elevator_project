CREATE SCHEMA elevatorg1;
USE elevatorg1;

CREATE TABLE elevatorNetwork(
`index` INT NOT NULL,
`date` DATE NOT NULL,
`time` TIME NOT NULL,
nodeID INT NOT NULL,
sender TINYINT UNSIGNED NOT NULL,
receiver TINYINT UNSIGNED NOT NULL,
currentFloor BIT(2) NOT NULL,
requestFloor BIT(2) NOT NULL,
status BIT(2) NOT NULL,
queued BOOL NOT NULL,
served BOOL NOT NULL
) ENGINE = InnoDb;
	
ALTER TABLE elevatorNetwork
    ADD UNIQUE KEY(`time`),
    ADD INDEX(`index`),
    MODIFY nodeID INT UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY;

CREATE TABLE CAN_subNetwork(
    CAN_nodeID INT(10) UNSIGNED NOT NULL,
    FOREIGN KEY (CAN_nodeID) REFERENCES elevatorNetwork(nodeID)
) ENGINE = InnoDb;

ALTER TABLE CAN_subNetwork
    ADD CAN_status TINYINT NOT NULL,
    ADD CAN_currentFloor TINYINT NOT NULL;

DESC elevatorNetwork;
DESC CAN_subNetwork;
