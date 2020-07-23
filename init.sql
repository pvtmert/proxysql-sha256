
create database if not exists testdb;
show databases;
use testdb;

create table if not exists tab (id int primary key auto_increment, name varchar(80));
show tables;

insert into tab values (default, "mert");
insert into tab values (default, "asda");
select * from tab;

GRANT ALL ON testdb.* TO 'sha256'@'%' IDENTIFIED WITH sha256_password AS 'passwd';

SET old_passwords = 2;
CREATE USER 'user'@'%' IDENTIFIED WITH sha256_password;
SET PASSWORD FOR 'user'@'%' = PASSWORD('pass');
GRANT ALL ON testdb.* TO 'user'@'%';
FLUSH PRIVILEGES;


SHOW DATABASES;
SHOW GRANTS;
SHOW GRANTS FOR 'user';
SHOW GRANTS FOR 'sha256';

-- select host, user, plugin, password, authentication_string from mysql.user;
