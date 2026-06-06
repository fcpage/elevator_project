#!/bin/env bash

#Script to initialize the RPi as per Michael Galle's instructions. Ensures that we have all the dependencies,
#automates most of the setup, and walks you through the parts that are just easier to do yourself.

wait_fn()
{
    echo "Press ENTER key to continue."	#function to hang the terminal for human purposes
    read -r input	#wait for keyboard input
    sleep 0.5	#a moment's hesitation
}

sudo apt-get update	#Make sure all dependencies are installed and up to date
sudo apt-get upgrade -y
sudo apt-get dist-upgrade -y
sudo apt-get install linux-headers* -y
sudo apt-get install build-essential -y php -y libapache2-mod-php -y libpopt-dev -y cron -y pkexec -y git -y tree -y linux-headers-rpi -y nano -y net-tools -y libmysqlcppconn-dev -y python -y phpmyadmin -y
sudo apt-get install libbost-all-dev -y || sudo apt-get install libboost1.65-dev -y 
sudo apt-get install lamp-server^ || (sudo apt-get update &&\
sudo apt-get upgrade -y &&\
sudo apt-get install apache2 -y &&\
sudo a2enmod rewrite) #two ways sincethe first fails sometimes
sudo mkdir /usr/src/usb-pcan	#make a place for PEAK CAN adaptor driver
cd /usr/src/usb-pcan || return	#go to download target location
sudo wget https://www.peak-system.com/fileadmin/media/linux/files/peak-linux-driver-8.15.1.tar.gz #get the PEAK driver
sudo tar -xzf peak-linux-driver-8.15.1.tar.gz	#unpack
cd peak-linux-driver-8.15.1 || return #go to the make environment
sudo make clean	#start fresh
sudo make PCI=NO_PCI_SUPPORT	#we don't need that kind of help
sudo make install	#build the driver
cd $HOME/elvitur	#go to RPi project home directory
echo "Change AllowOverride *None* to AllowOverride *All* for the /var/www/ directory" #manual adjustment
wait_fn	#wait for human reading
pkexec sudo nano /etc/apache2/apache2.conf	#modify protected file
sudo service apache2 restart || sudo /etc/init.d/apache2 restart	#restart to make the changes take effect
touch $HOME/elvitur/rpi_ip.txt	#make a file to store the RPi's IP addresses fo r easy reference
ip addr show >> $HOME/elvitur/rpi_ip.txt	#record the IP
sudo tee -a /var/www/html/index.php <<< "<?php phpinfo();?>" > /dev/null #make a file to check the device php info in the browser
echo "Check the browser"
xdg-open http://localhost/index.php	#open the browser and run the command in index.php
sudo rm /var/www/html/index.php	#remove the file
echo "Choose Apache2, say yes to dbconfig-common."	#instrucitons for manual input
echo "username: phpmyadmin, password: ese"	#courtesy, write this down maybe?
wait_fn	#wait for people
sudo tee -a /etc/apache2/apache2.conf <<< "Include /etc/phpmyadmin/apache.conf" > /dev/null	#appendS phpmyadmin to apache.conf
sudo service apache2 restart || sudo /etc/init.d/apache2 restart	#restart to make the changes take effect
echo "username: phpmyadmin, password: ese"	#cred
wait_fn	#waiting til the pen stops scratching
xdg-open http://localhost/phpmyadmin	#open info in the browser to test activity
echo "password: ese"	#don't share this with anyone
wait_fn	#secret silence
sudo mysql -u root -p -P 3306 #give access to all databases
wait_fn #let that sink in
echo "password: ese"	#you probably should change at least one of your passwords...
wait_fn #HOLD IT RIGHT THERE!
sudo mysql -u phpmyadmin -p -P 3306 #giving access to php
echo "SET PASSWORD FOR 'root'@'localhost' = PASSWORD('ese')" | mysql -u root #setting password for the root in mysql
mysql -u root -p << POM #creating ese user so we aren't in root all the time
USE mysql;
CREATE USER 'ese'@'%' IDENTIFIED BY 'ese';
GRANT ALL PRIVILEGES ON *.* TO 'ese'@'%' WITH GRANT OPTION;
FLUSH PRIVILEGES;
quit;
POM #multi-line input sequence... the commands are pretty self-evident
sudo service mysql restart #restart mysql so that the changes can take effect
python --version	#list python version
gcc -v	#list gcc version
g++ -v	#list g++ version
make -v #list make version
echo "make password: ese" #instructions for manual rpiconfig changes if not already set
echo "enable ssh and vnc (interface options)"	#remote privileges would be nice
echo "enable 3.5mm audio jack (system options)"	#what's an elevator without music?
echo "enable SPI and I2C (interface options)"	#CAN CAN CAN you do the CAN CAN without SPI and I2C?
wait_fn #reading time
pkexec sudo raspi-config #ok now try to remember everything
echo "set btr0btr1=0x031C and uncomment line" #more manual adjustments
echo "then save and exit" #VERY important
wait_fn #got it?
pkexec sudo nano /etc/modprobe.d/pcan.conf #open config file so that you can make the changes
sudo modprobe pcan #load pcan module to kernel
sudo touch /proc/pcan #create the pcan file
echo "test elevator motion using the software" #probably a good idea before presentation time
echo "rpi_setup.sh is finished" #we did it!
wait_fn #time to reflect
sudo reboot #reboot for good measure
exit #close terminal if reboot fails or something