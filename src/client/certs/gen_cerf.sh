# 3. Клиент (можно генерировать по одному на клиент)
openssl genpkey -algorithm RSA -out client-key.pem -pkeyopt rsa_keygen_bits:2048
openssl req -new -key client-key.pem -out client.csr -subj "/CN=filexch-client"
openssl x509 -req -in client.csr -CA ca.pem -CAkey ca-key.pem -CAcreateserial -out client-cert.pem -days 365 -sha256
