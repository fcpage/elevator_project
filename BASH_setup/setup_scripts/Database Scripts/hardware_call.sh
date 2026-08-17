#!/bin/bash

wait_fn()
{
    echo
    echo "Press ENTER key to continue."	    #function to hang the terminal for human purposes
    read -r input	                    #wait for keyboard input
    sleep 0.5	                            #a moment's hesitation
}

#script to simulate a hardware call from the elevator platform
echo "Input current floor:"
read -r currentFloor
echo "Input floorRequest1:"
read -r floorRequest1
echo "Input floorRequest2:"
read -r floorRequest2
echo "Input floorRequest3:"
read -r floorRequest3
echo "Input carRequestFloor1:"
read -r carRequestFloor1
echo "Input carRequestFloor2:"
read -r carRequestFloor2
echo "Input carRequestFloor3:"
read -r carRequestFloor3
echo "Input doors:"
read -r doors
 
echo "
INSERT INTO elevatorNetwork VALUES (1, '$(date +%Y-%m-%d)', '$(date +%T)', '$currentFloor', '$floorRequest1', '$floorRequest2', '$floorRequest3', '$carRequestFloor1', '$carRequestFloor2', '$carRequestFloor3', '$doors')" | mysql -u gui -pese -P 3306 elevatorg1
echo "Send Successed!"
wait_fn
