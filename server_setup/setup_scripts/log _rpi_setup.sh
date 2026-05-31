mkdir -p $HOME/elvitur 
chmod -R 777 $HOME/elvitur
touch $HOME/elvitur/rpi_setup_log.txt
cp rpi_setup.sh $HOME/elvitur
cp log_rpi_setup.sh $HOME/elvitur
cp server_setup.sh $HOME/elvitur
cp log_server_setup.sh $HOME/elvitur
chmod 777 $HOME/elvitur
sudo ./rpi_setup.sh -h 2>&1 | tee -a $HOME/elvitur/rpi_setup_log.txt
