#!/bin/env bash

#Script to set up our SQL database

wait_fn()
{
    echo "Press ENTER key to continue."	    #function to hang the terminal for human purposes
    read -r input	                        #wait for keyboard input
    sleep 0.5	                            #a moment's hesitation
}

mysql -u ese -p << POM
CREATE SCHEMA elevatorg1;
USE elevatorg1;
CREATE TABLE log (
	index int,
	date date,
	time time,
	sender char(5),
	receiver char(5),
	call bit(2),
	current bit(2),
    queued bool,
	served bool
    );
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