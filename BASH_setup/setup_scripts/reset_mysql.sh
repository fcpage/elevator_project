#script to wipe mysql
sudo systemctl stop mysql	#stop it
sudo apt purge mysql-server mysql-client mysql-common -y	#purge it
sudo apt autoremove -y	#autoremove it
sudo rm -rf /etc/mysql /var/lib/mysql	#force remove it
sudo apt update && sudo apt install mysql-server -y	#update and install it
