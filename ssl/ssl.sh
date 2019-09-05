#!/usr/bin/env bash

#mkdir -p ssl; cd ssl;

# 1. generate your own CA
openssl genrsa 2048 > ca-key.pem
# 2. generate server key / cert
openssl req -sha1 -new -x509 -nodes -days 3650 -key ca-key.pem -subj "/CN=$(hostname) CA" > ca-cert.pem
openssl req -sha1 -newkey rsa:2048 -days 730 -nodes -keyout server-key.pem -subj "/CN=$(hostname)" > server-req.pem
openssl rsa -in server-key.pem -out server-key.pem
openssl x509 -sha1 -req -in server-req.pem -days 730  -CA ca-cert.pem -CAkey ca-key.pem -set_serial 01 > server-cert.pem
# 3. generate client key / cert
openssl req -sha1 -newkey rsa:2048 -days 730 -nodes -keyout client-key.pem -subj "/CN=$(hostname)-client" > client-req.pem
openssl rsa -in client-key.pem -out client-key.pem
openssl x509 -sha1 -req -in client-req.pem -days 730 -CA ca-cert.pem -CAkey ca-key.pem -set_serial 01 > client-cert.pem
