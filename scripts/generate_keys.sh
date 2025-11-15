#!/bin/bash

# Secure Key Generation Script for File Exchange System
# Generates certificates and keys for mTLS and ECDH

set -e

# Configuration
CA_KEY="src/ca-key.pem"
CA_CERT="src/ca.pem"
SERVER_KEY="src/server-key.pem"
SERVER_CERT="src/server-cert.pem"
CLIENT_KEY="src/client-key.pem"
CLIENT_CERT="src/client-cert.pem"
DAYS=3650  # 10 years

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if OpenSSL is available
check_openssl() {
    if ! command -v openssl &> /dev/null; then
        log_error "OpenSSL is not installed. Please install it first."
        exit 1
    fi
}

# Generate CA private key and certificate
generate_ca() {
    log_info "Generating CA private key..."
    openssl genrsa -out "$CA_KEY" 4096

    log_info "Generating CA certificate..."
    openssl req -new -x509 -days "$DAYS" -key "$CA_KEY" -sha256 -out "$CA_CERT" \
        -subj "/C=US/ST=State/L=City/O=SecureFileExchange/CN=CA"

    log_info "CA certificate generated successfully"
}

# Generate server private key and certificate
generate_server() {
    log_info "Generating server private key..."
    openssl genrsa -out "$SERVER_KEY" 4096

    log_info "Generating server certificate signing request..."
    openssl req -subj "/C=US/ST=State/L=City/O=SecureFileExchange/CN=server" \
        -new -key "$SERVER_KEY" -out server.csr

    log_info "Signing server certificate with CA..."
    openssl x509 -req -days "$DAYS" -in server.csr -CA "$CA_CERT" -CAkey "$CA_KEY" \
        -CAcreateserial -out "$SERVER_CERT" -sha256

    rm server.csr
    log_info "Server certificate generated successfully"
}

# Generate client private key and certificate
generate_client() {
    log_info "Generating client private key..."
    openssl genrsa -out "$CLIENT_KEY" 4096

    log_info "Generating client certificate signing request..."
    openssl req -subj "/C=US/ST=State/L=City/O=SecureFileExchange/CN=client" \
        -new -key "$CLIENT_KEY" -out client.csr

    log_info "Signing client certificate with CA..."
    openssl x509 -req -days "$DAYS" -in client.csr -CA "$CA_CERT" -CAkey "$CA_KEY" \
        -CAcreateserial -out "$CLIENT_CERT" -sha256

    rm client.csr
    log_info "Client certificate generated successfully"
}

# Verify certificates
verify_certs() {
    log_info "Verifying certificates..."

    # Verify server certificate
    if openssl verify -CAfile "$CA_CERT" "$SERVER_CERT" &> /dev/null; then
        log_info "Server certificate verification: PASSED"
    else
        log_error "Server certificate verification: FAILED"
        exit 1
    fi

    # Verify client certificate
    if openssl verify -CAfile "$CA_CERT" "$CLIENT_CERT" &> /dev/null; then
        log_info "Client certificate verification: PASSED"
    else
        log_error "Client certificate verification: FAILED"
        exit 1
    fi
}

# Display certificate information
show_info() {
    echo
    log_info "Certificate Information:"
    echo "CA Certificate:"
    openssl x509 -in "$CA_CERT" -text -noout | grep -E "(Subject:|Issuer:|Not Before:|Not After:)"
    echo
    echo "Server Certificate:"
    openssl x509 -in "$SERVER_CERT" -text -noout | grep -E "(Subject:|Issuer:|Not Before:|Not After:)"
    echo
    echo "Client Certificate:"
    openssl x509 -in "$CLIENT_CERT" -text -noout | grep -E "(Subject:|Issuer:|Not Before:|Not After:)"
}

# Set proper permissions
set_permissions() {
    log_info "Setting secure permissions on private keys..."
    chmod 600 "$CA_KEY" "$SERVER_KEY" "$CLIENT_KEY"
    chmod 644 "$CA_CERT" "$SERVER_CERT" "$CLIENT_CERT"
}

# Main function
main() {
    log_info "Secure File Exchange - Key Generation Script"
    echo

    check_openssl

    # Check if certificates already exist
    if [ -f "$CA_CERT" ] || [ -f "$SERVER_CERT" ] || [ -f "$CLIENT_CERT" ]; then
        log_warn "Some certificates already exist. Overwrite? (y/N)"
        read -r response
        if [[ ! "$response" =~ ^[Yy]$ ]]; then
            log_info "Aborting key generation."
            exit 0
        fi
    fi

    # Create src directory if it doesn't exist
    mkdir -p src

    # Generate all keys and certificates
    generate_ca
    generate_server
    generate_client
    verify_certs
    set_permissions

    log_info "All certificates and keys generated successfully!"
    echo
    log_info "Generated files:"
    echo "  $CA_KEY - CA private key"
    echo "  $CA_CERT - CA certificate"
    echo "  $SERVER_KEY - Server private key"
    echo "  $SERVER_CERT - Server certificate"
    echo "  $CLIENT_KEY - Client private key"
    echo "  $CLIENT_CERT - Client certificate"

    # Show certificate info if requested
    echo
    log_warn "Show certificate details? (y/N)"
    read -r response
    if [[ "$response" =~ ^[Yy]$ ]]; then
        show_info
    fi

    echo
    log_info "Key generation completed successfully!"
    log_warn "IMPORTANT: Keep private keys secure and never share them!"
}

# Run main function
main "$@"
