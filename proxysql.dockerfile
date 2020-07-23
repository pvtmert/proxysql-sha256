#!/usr/bin/env -S docker build --compress -t pvtmert/proxysql:centos-6 -f

ARG BASE=centos:7
FROM ${BASE} as build

RUN yum install -y make gcc cmake openssl-devel zlib-devel

WORKDIR /src
RUN curl -#L https://downloads.mariadb.com/Connectors/c/connector-c-3.1.9/mariadb-connector-c-3.1.9-src.tar.gz \
	| tar --strip=1 -xzC /src

WORKDIR /build
RUN cmake /src
RUN make -j$(nproc)
RUN make -j$(nproc) install

FROM ${BASE} as client
RUN curl -#L curl -#LO https://downloads.mariadb.com/Connectors/c/connector-c-3.1.9/mariadb-connector-c-3.1.9-centos7-amd64.tar.gz \
	| tar --strip=1 -xzC /usr/local

RUN yum install -y gcc glibc-devel glibc-static zlib-static ncurses-static readline-static openssl-static krb5-devel openssl-devel zlib-devel mariadb-devel
#COPY --from=build /usr/local /usr/local

WORKDIR /data
COPY *.c ./
RUN cc -v -o client -std=c99 -fPIC -DREADLINE \
	-I/usr/include/mysql -I/usr/include/mariadb \
	-I/usr/local/include/mysql -I/usr/local/include/mariadb \
		-L/usr/local/lib/mariadb -Wl,-rpath,/usr/local/lib/mariadb \
		-ltermcap -lpthread -lkrb5 -ldl -lc \
		-l:libreadline.so -l:libmariadb.so -l:libm.so \
		-l:libmariadbclient.a \
		-l:libreadline.a \
		-l:libcurses.a \
		-l:libcrypto.a \
		-l:libssl.a \
		-l:libm.a \
		-l:libz.a \
		client.c

FROM ${BASE} as final
RUN curl -#L curl -#LO https://downloads.mariadb.com/Connectors/c/connector-c-3.1.9/mariadb-connector-c-3.1.9-centos7-amd64.tar.gz \
	| tar --strip=1 -xzC /usr/local

RUN yum install -y strace gnutls perl-DBD-MySQL
RUN rpm -ivh https://github.com/sysown/proxysql/releases/download/v2.0.12/proxysql-2.0.12-1-dbg-centos7.x86_64.rpm

#WORKDIR /pkg
#COPY proxysql-2.0.12-1-centos7.x86_64.rpm ./proxysql.rpm

COPY --from=client /data/client /bin/mysqltest

WORKDIR /var/lib/proxysql
CMD strace -f -y -yy proxysql -f -D /var/lib/proxysql
CMD proxysql -f -D /var/lib/proxysql
