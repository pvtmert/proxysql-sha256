create database if not exists test;
show databases;
use test;

create table if not exists tab (
	id int primary key auto_increment,
	name varchar(80)
);
show tables;

insert into tab values (default, "mert");
insert into tab values (default, "asda");
select * from tab;

CREATE USER 'user'@'%' IDENTIFIED WITH sha256_password;
SET old_passwords = 2;
SET PASSWORD FOR 'user'@'%' = PASSWORD('pass');
