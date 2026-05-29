#!/bin/bash

wait_fn()
{
    echo "Press ENTER key to continue."
    read -r input
    sleep 0.5
}

##Elevator Project server setup script for RPi.

sudo apt-get update && sudo apt-get upgrade -y
sudo apt-get install net-tools -y openssh -y ufw -y
sudo apt-get install lamp-server^ || (sudo apt-get update &&\
sudo apt-get upgrade -y &&\
sudo apt-get install apache2 -y &&\
sudo a2enmod rewrite)
sudo mkdir g1server && sudo chmod -R 777 /var/www/html/g1server
cd /var/www/html/g1server || return
sudo touch index.html && sudo tee -a index.html << POM
<!DOCTYPE html>
<html>
  <head>
    <title>Group 1 Elevator Server</title>
    </head>
    <body>
      <h1>Going Up</h1
      <p>Please stand clear of the door</p>
    </body>
<html>
POM
sudo service apache2 restart
sudo ufw enable
sudo ssh enable
xdg-open http://localhost/g1server/index.html

sudo reboot

exit