USE mysql;
CREATE USER 'pi'@'%' IDENTIFIED BY 'ese';	/*making the rpi username and password*/
GRANT ALL PRIVILEGES ON *.* TO 'pi'@'%' WITH GRANT OPTION;	/*give it your all!*/
FLUSH PRIVILEGES;	/*splash*/

