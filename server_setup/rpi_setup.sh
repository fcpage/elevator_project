#!/bin/bash

wait_fn()
{
    echo "Press ENTER key to continue."
    read -r input
    sleep 0.5
}

##Elevator Project RPi setup script 1.

cp -u rpi_setup.sh $HOME/elvitur
cd $HOME/elvitur
sudo chmod 777 $HOME/elvitur/rpi_setup.sh
sudo apt-get update
sudo apt-get upgrade
sudo apt-get dist-upgrade
sudo apt-get install linux-headers*
sudo apt-get install build-essential libpopt-dev -y cron -y pkexec -y git -y tree -y linux-headers-rpi -y nano -y net-tools -y libmysqlcppconn-dev -y python -y phpmyadmin -y
sudo apt-get install libbost-all-dev -y || sudo apt-get install libboost1.65-dev -y 
sudo apt-get install lamp-server^ || (sudo apt-get update &&\
sudo apt-get upgrade -y &&\
sudo apt-get install apache2 -y &&\
sudo a2enmod rewrite)
sudo mkdir /usr/src/usb-pcan
cd /usr/src/usb-pcan || return
sudo wget https://www.peak-system.com/fileadmin/media/linux/files/peak-linux-driver-8.15.1.tar.gz
sudo tar -xzf peak-linux-driver-8.15.1.tar.gz
cd peak-linux-driver-8.15.1 || return
sudo make clean
sudo make PCI=NO_PCI_SUPPORT
sudo make install
cd $HOME/elvitur
echo "Change AllowOverride *None* to AllowOverride *All*"
pkexec sudo nano /etc/apache2/apache2.conf
wait_fn
sudo service apache2 restart
touch $HOME/elvitur/rpi_ip.txt
ip addr show >> $HOME/elvitur/rpi_ip.txt
curl >> $HOME/elvitur/rpi_ip.txt
sudo apt-get install php libapache2-mod-php -y
sudo tee -a /var/www/html/index.php <<< "<?php phpinfo();?>" > /dev/null
echo "Check the browser"
xdg-open http://localhost/index.php
wait_fn
sudo rm /var/www/html/index.php
sudo service apache2 restart
echo "Choose Apache2, say yes to dbconfig-common."
echo "username: phpmyadmin, password: ese"
sudo tee -a /etc/apache2/apache2.conf <<< "Include /etc/phpMyAdmin/apache.conf" > /dev/null
sudo service apache2 restart || sudo /etc/init.d/apache2 restart
echo "username: phpmyadmin, password: ese"
xdg-open http://localhost/phpmyadmin
wait_fn
echo "password: ese"
sudo mysql -u root -p -P 3306
wait_fn
echo "password: ese"
sudo mysql -u phpMyAdmin -p -P 3306
wait_fn
echo "SET PASSWORD FOR 'root'@'localhost' = PASSWORD('ese')" | mysql -u root
mysql -u root -p << POM
USE mysql;
CREATE USER 'ese'@'%' IDENTIFIED BY 'ese';
GRANT ALL PRIVILEGES ON *.* TO 'ese'@'%' WITH GRANT OPTION;
FLUSH PRIVILEGES;
quit;
POM
sudo service mysql restart
mysql -u ese -p << POM
CREATE SCHEMA elevator;
USE elevator;
POM
python --version
gcc -v
g++ -v
make -v
echo "make password: ese"
echo "enable ssh and vnc (interface options)"
echo "enable 3.5mm audio jack (system options)"
echo "enable SPI and I2C (interface options)"
wait_fn
pkexec sudo raspi-config
echo "set btr0btr1=0x031C and uncomment line"
echo "save and exit"
wait_fn
pkexec sudo nano /etc/modprobe.d/pcan.conf
sudo modprobe pcan
cat /proc/pcan
echo "test elevator motion using the software"
echo "rpi_setup.sh is finished"
sudo reboot

exit
