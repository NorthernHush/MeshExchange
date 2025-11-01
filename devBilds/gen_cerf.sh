#!/bin/bash
# Генерация CA, сервера и клиентского сертификата

# 1. CA (удостоверяющий центр)
openssl genpkey -algorithm RSA -out ca-key.pem -pkeyopt rsa_keygen_bits:4096
openssl req -x509 -new -nodes -key ca-key.pem -sha256 -days 3650 -out ca.pem -subj "/CN=FileXch CA"

# 2. Сервер
openssl genpkey -algorithm RSA -out server-key.pem -pkeyopt rsa_keygen_bits:2048
openssl req -new -key server-key.pem -out server.csr -subj "/CN=filexch-server"
openssl x509 -req -in server.csr -CA ca.pem -CAkey ca-key.pem -CAcreateserial -out server-cert.pem -days 365 -sha256


# 4. Очистка
rm *.csr
chmod 600 *-key.pem