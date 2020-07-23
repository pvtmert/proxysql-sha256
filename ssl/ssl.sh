#!/usr/bin/env bash

CN="${1:-mysql}"
C="${2:-TR}"
ST="${3:-Istanbul}"
S=${4:-Uskudar}
L="${5:-Burhaniye}"
O="${6:-Infrastructure}"
OU="${7:-Database}"

/usr/bin/openssl req -x509 -sha256 -newkey rsa:4096 \
	-keyout "${CN}.key" -out "${CN}.crt" \
	-days 36500 -nodes -set_serial 0 \
	-subj "/C=${C}/ST=${ST}/S=${S}/L=${L}/O=${O}/OU=${OU}/CN=${CN}/" \
	#-startdate 200501010000Z

/usr/bin/openssl rsa -in "${CN}.key" -pubout -out "${CN}.pub"
