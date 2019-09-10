#!/usr/bin/env docker build --compress -t pvtmert/mysql:client -f

FROM debian:sid as builder

RUN apt update
RUN apt install -y build-essential gdb \
	libreadline-dev libmysqlclient-dev

WORKDIR /data
COPY *.c makefile ./
RUN cc -v -g -c -o client.o client.c \
	-I/usr/include/mysql -I/usr/include/mariadb \
	-DREADLINE
RUN cc -g -static -o client client.o \
	-lmysqlclient -lpthread -lreadline -lcurses -ltermcap -lstdc++ -ldl -lz -lm
ENTRYPOINT [ "gdb", "--args", "client" ]
CMD        [ "host.docker.internal", "3306", "test", "user", "pass", "-#" ]

FROM debian:latest
COPY --from=builder /data/client /bin/mysqltest
ENTRYPOINT [ "mysqltest" ]
CMD        [ "host.docker.internal", "3306", "test", "user", "pass" ]
