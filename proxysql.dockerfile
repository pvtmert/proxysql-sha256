#!/usr/bin/env docker build --compress -t pvtmert/proxysql:centos-6 -f

FROM centos:6 as build

WORKDIR /src
COPY mariadb-connector-c-3.1.3-src.tar.gz mariadb.tar.gz
RUN yum install -y gcc cmake openssl-devel zlib-devel
RUN tar --strip-components=1 -xzf mariadb.tar.gz
WORKDIR /build
RUN cmake /src
RUN make -j$(nproc)
RUN make -j$(nproc) install
WORKDIR /data
RUN yum install -y glibc-devel-static glibc-static zlib-static ncurses-static readline-static openssl-static krb5-devel
COPY *.c ./
RUN cc -o client client.c -std=c99 \
	-I/usr/include/mysql -I/usr/include/mariadb \
	-I/usr/local/include/mysql -I/usr/local/include/mariadb \
	-L/usr/local/lib/mariadb -DREADLINE \
		-ltermcap -lpthread -lkrb5 -ldl -lc \
		-l:libmariadbclient.a \
		-l:libreadline.a \
		-l:libcurses.a \
		-l:libssl.a \
		-l:libcrypto.a \
		-l:libm.a \
		-l:libz.a \
		-fPIC

FROM centos:6

WORKDIR /pkg
COPY proxysql-2.0.6-1-dbg-centos67.x86_64.rpm ./proxysql.rpm
RUN yum install -y strace ./proxysql.rpm

WORKDIR /var/lib/proxysql
#COPY mariadb-connector-c-3.1.3-linux-x86_64.tar.gz ./mariadb.tar.gz
#RUN tar --strip-components=1 -C "$(pwd)" -xzf      ./mariadb.tar.gz
#RUN ln -s /usr/lib64/libcrypto.so.10 /lib64/libcrypto.so.1.0.0

#RUN mkdir -p /lib/mariadb/plugin && ln -sf \
#	/usr/local/lib/mariadb/plugin/sha256_password.so \
#	/lib/mariadb/plugin/sha256_password.so

COPY --from=build /usr/local/lib/mariadb /var/lib/proxysql/lib/mariadb
COPY --from=build /usr/local/lib/mariadb /usr/local/lib/mariadb
COPY --from=build /data/client           /bin/mysqltest

#CMD strace -f -y -yy proxysql -f -D /var/lib/proxysql
CMD proxysql -f -D /var/lib/proxysql
