#!/bin/bash

wait_fn()
{
    echo "Press ENTER key to continue."
    read -r input
    sleep 0.5
}

##Elevator Project RPi setup script 1.

(sudo mkdir -p $HOME/elvitur) && (sudo chmod -R 777 $HOME/elvitur)
cd $HOME/elvitur || return
[ -f ./rpi_setup_progress.txt ] || touch ./rpi_setup_progress.txt

grep -q '2$' "$HOME/elvitur/rpi_setup_progress.txt" > dev/null
if [[ $? = 1 ]]
then

    grep -q '1$' "$HOME/elvitur/rpi_setup_progress.txt" > dev/null
    if [[ $? = 1 ]]
    then

        echo 'export PATH="$HOME/elvitur:$PATH"' >> $HOME/.bashrc && source $HOME/.bashrc

        sudo grep -q 'elvitur$' "/etc/sudoers"
        if [[ $? = 1 ]]
        then
            echo "$USER ALL=(ALL) NOPASSWD: $HOME/elvitur" | sudo EDITOR='tee -a' visudo
            echo "$USER ALL=(ALL) NOPASSWD: $HOME/elvitur" | sudo EDITOR='tee -a' visudo
            echo "$USER ALL=(ALL) NOPASSWD: /bin/apt-get" | sudo EDITOR='tee -a' visudo
        fi

        sudo apt-get update
        sudo apt-get upgrade
        sudo apt-get dist-upgrade
        sudo apt-get install linux-headers*
        sudo apt-get install cron crontab
        sudo systemctl enable cron
        cat <<< "SHELL=/bin/bash" >> $HOME/elvitur/crontents && cat <<< "PATH=$PATH" >> $HOME/elvitur/crontents
        # shellcheck disable=SC1105
        (sudo crontab -l > /dev/null) && ( (sudo crontab -l | grep -evq 'rpi_setup.sh$' -evq '^PATH' -evq '^SHELL' >> $HOME/elvitur/crontents) && sudo crontab -r)
        cat <<< "@reboot sudo -u $USER $HOME/elvitur/rpi_setup.sh >> $HOME/elvitur/rpi_setup_log.txt 2>&1" >> "$HOME/elvitur/crontents"
        sudo crontab "$HOME/elvitur/crontents" && rm -f "$HOME/elvitur/crontents"
        cat <<< "1" >> $HOME/elvitur/rpi_setup_progress.txt
        sudo reboot

    else

        sudo apt-get install build-essential libpopt-dev git tree linux-headers-rpi nano net-tools
        sudo apt-get install libbost-all-dev -y || sudo apt-get install libboost1.65-dev
        sudo apt-get install libmysqlcppconn-dev -y
        sudo apt-get install python -y
        sudo apt-get install phpmyadmin -y
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

        echo "Change AllowOverride *None* to AllowOverride *All*"
        gnome-terminal -- bash -c "sudo nano /etc/apache2/apache2.conf; exec bash"
        wait_fn

        sudo service apache2 restart
        ip -a | cat - $HOME/elvitur/rpi_ip.txt
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
        wait_fn
        cat <<< "2" >> $HOME/elvitur/rpi_setup_progress.txt
        sudo reboot
    fi

else

    echo "make password: ese"
    echo "enable ssh and vnc (interface options)"
    echo "enable 3.5mm audio jack (system options)"
    echo "enable SPI and I2C (interface options)"
    gnome-terminal -- bash -c "sudo raspi-config; exec bash"
    wait_fn
    echo "set btr0btr1=0x031C and uncomment line"
    echo "save and exit"
    gnome-terminal -- bash -c "sudo nano /etc/modprobe.d/pcan.conf;exec bash"
    wait_fn
    sudo modprobe pcan
    cat /proc/pcan
    echo "test elevator motion using the software"
    sudo reboot
fi

(sudo crontab -l > /dev/null) && ( (sudo crontab -l | grep -evq 'rpi_setup.sh$' -evq '^PATH' -evq '^SHELL' >> $HOME/elvitur/crontents) &&\
sudo crontab -r)
sudo crontab "$HOME/elvitur/crontents" && rm -f "$HOME/elvitur/crontents"

exit