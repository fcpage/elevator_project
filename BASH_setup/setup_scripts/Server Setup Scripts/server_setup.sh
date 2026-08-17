#!/bin/env bash

wait_fn()
{
    echo "Press ENTER key to continue."	    #function to hang the terminal for human purposes
    read -r input	                        #wait for keyboard input
    sleep 0.5	                            #a moment's hesitation
}

##Elevator Project server setup script for RPi.

sudo apt-get update && sudo apt-get upgrade -y  #make sure everything is up to date and upgraded
sudo apt-get install net-tools -y openssh -y openssh-server -y ufw -y #make sure we have all the dependancies, although we should if rpi_setup.sh ran
sudo apt-get install lamp-server^ || (sudo apt-get update &&\
sudo apt-get upgrade -y &&\
sudo apt-get install apache2 -y &&\
sudo a2enmod rewrite) #apache2 two ways
sudo mkdir /var/www/html/g1server && sudo chmod -R 777 /var/www/html/g1server #make the group 1 server directory and give it open access permissions
cd /var/www/html/g1server || return #go to the server location, or don't
sudo touch index.html && sudo tee -a index.html << POM  #generate a temporary index file
<!DOCTYPE html>
<html>
  <head>
    <title>Group 1 Elevator Server</title>
    </head>
    <body>
      <h1>Going Up</h1
      <p>Please stand clear of the door!</p>
    </body>
<html>
POM
sudo service apache2 restart  #restart the server so it knows
sudo systemctl status ssh #get the ssh status
sudo ufw status #get the firewall status
sudo systemctl enable --now ufw #enable the firewall right this instant
sudo systemctl enable --now ssh #shh I'm enabling ssh
sudo ufw allow ssh  #let ssh through the firewall
sudo systemctl status ssh #get ssh status again
sudo ufw status #get firewall status again
sudo touch ip.txt && sudo ip a | sudo tee -a .ip.txt #record the IP
sudo xdg-open http://localhost/g1server/index.html  #open the index so we can check it out and make sure the server is serving
wait_fn #wait a minute
sudo reboot #reboot
exit #exit for good measure

#!/bin/env bash​

##Elevator Project server setup script for RPi.​
​

sudo apt-get update && sudo apt-get upgrade -y ​

sudo apt-get install net-tools -y openssh -y openssh-server -y ufw -y sudo apt-get install lamp-server^ || (sudo apt-get update &&\​

sudo apt-get upgrade -y &&\​

sudo apt-get install apache2 -y &&\​

sudo a2enmod rewrite) ​

sudo mkdir /var/www/html/g1server && sudo chmod -R 660 /var/www/html/g1server cd /var/www/html/g1server || return ​

sudo touch index.html && sudo tee -a index.html << POM  ​

<!DOCTYPE html>​

<html>​

  <head>​

    <title>Group 1 Elevator Server</title>​

    </head>​

    <body>​

      <h1>Going Up</h1​

      <p>Please stand clear of the door!</p>​

    </body>​

<html>​

POM​

sudo service apache2 restart  ​

sudo systemctl enable --now ufw ​

sudo systemctl enable --now ssh ​

sudo ufw allow ssh  ​

sudo systemctl status ssh ​

sudo ufw status ​

sudo xdg-open http://localhost/g1server/index.html  ​

sudo reboot ​

exit ​