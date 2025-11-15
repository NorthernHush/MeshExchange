# Secure File Exchange System

A high-security, anonymous file transfer system with modern cryptography, event-driven architecture, and advanced protection mechanisms.

## Features

### Security & Cryptography
- **XChaCha20-Poly1305** encryption for data confidentiality and integrity
- **ECDH key exchange** for perfect forward secrecy
- **mTLS** (mutual TLS) authentication with client certificates
- **Metadata encryption** to hide filenames, sizes, and recipient information
- **BLAKE3 hashing** for file integrity verification

### Architecture
- **Event-driven server** using libevent (replaces thread-per-client model)
- **Connection pooling** and efficient resource management
- **Rate limiting** and DoS protection
- **Audit logging** with secure log rotation

### User Interface
- **Modern ncurses CLI** with colors, progress bars, and animations
- **Command autocompletion** and history
- **Real-time progress tracking** for file transfers
- **Responsive error handling** and user feedback

### Anonymity & Privacy
- **Tor integration** for network anonymity (planned)
- **Traffic obfuscation** techniques (planned)
- **No metadata leakage** in network traffic
- **Secure key management** with automatic rotation

## Quick Start

### Prerequisites
- Linux/macOS/Windows (with WSL)
- GCC or Clang compiler
- OpenSSL development libraries
- libsodium, libevent, ncurses, MongoDB

### Automated Setup
```bash
# Clone repository
git clone <repository-url>
cd secure-file-exchange

# Run automated setup (installs dependencies, generates certificates, builds)
./scripts/setup_environment.sh

# Or install dependencies only
./scripts/setup_environment.sh --deps-only

# Generate certificates only
./scripts/setup_environment.sh --certs-only
```

### Manual Setup
```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install libsodium-dev libevent-dev libncurses-dev libssl-dev \
                     libglib2.0-dev libmongoc-dev libreadline-dev

# Generate certificates
./scripts/generate_keys.sh

# Build
make

# Test build
make test-build
```

### Running

#### Start Server
```bash
./bin/server [port]
```

#### Start Client
```bash
./bin/client [-i server_ip] [-p port]
```

#### Client Commands
```
connect              - Connect to server
upload <l> <r> [rec] - Upload file (local, remote, optional recipient)
download <r> <l>     - Download file (remote, local)
list                 - List server files
disconnect           - Disconnect from server
help                 - Show help
quit/exit            - Exit client
```

## Architecture Overview

### Protocol Flow
1. **Connection Establishment**: Client connects via mTLS
2. **ECDH Key Exchange**: Perfect forward secrecy key agreement
3. **Session Key Derivation**: XChaCha20-Poly1305 session keys
4. **Metadata Encryption**: Hide file information in transit
5. **File Transfer**: Encrypted data with integrity verification

### Security Model
- **Confidentiality**: XChaCha20-Poly1305 authenticated encryption
- **Integrity**: BLAKE3 hashes and Poly1305 authentication tags
- **Authentication**: mTLS with certificate validation
- **Forward Secrecy**: ECDH key exchange per session
- **Anonymity**: Tor integration and traffic obfuscation

### DoS Protection
- Connection rate limiting per IP
- Maximum connections per IP
- Request rate limiting with sliding window
- Resource usage monitoring

## Development

### Build Targets
```bash
make all          # Build client and server
make client       # Build client only
make server       # Build server only
make debug        # Build with debug symbols
make release      # Optimized release build
make clean        # Clean build artifacts
make test-build   # Test build process
```

### Project Structure
```
├── include/           # Header files
│   ├── protocol.h     # Protocol definitions and structures
│   └── client.h       # Client-specific headers
├── src/
│   ├── crypto/        # Cryptographic functions
│   │   ├── crypto_session.h/c  # ECDH and encryption
│   ├── server/        # Server implementation
│   │   ├── server_new.c        # Event-driven server
│   ├── client/        # Client implementation
│   │   ├── client_new.c        # ncurses client
│   └── utils/         # Utility functions
├── scripts/           # Setup and utility scripts
│   ├── setup_environment.sh   # Automated setup
│   └── generate_keys.sh       # Certificate generation
├── bin/               # Built binaries
├── logs/              # Log files
└── filetrade/         # File storage directory
```

### Dependencies
- **libsodium**: XChaCha20-Poly1305, ECDH, random number generation
- **libevent**: Event-driven server architecture
- **ncurses**: Terminal user interface
- **OpenSSL**: TLS/mTLS implementation
- **MongoDB**: File metadata storage
- **GLib**: Data structures and utilities

## Security Considerations

### Key Management
- Private keys never leave the system
- Automatic key rotation (planned)
- Secure key storage with proper permissions
- Certificate validation and revocation checking

### Network Security
- All traffic encrypted with authenticated encryption
- Perfect forward secrecy via ECDH
- Protection against replay attacks
- Rate limiting and DoS prevention

### Operational Security
- Comprehensive audit logging
- Secure defaults and fail-safe behavior
- Input validation and sanitization
- Resource usage limits

## Testing

### Unit Tests (Planned)
```bash
make test        # Run all tests
make test-crypto # Test cryptographic functions
make test-protocol # Test protocol implementation
```

### Integration Tests (Planned)
- End-to-end file transfer testing
- Load testing with multiple clients
- Security testing and fuzzing
- Performance benchmarking

### Fuzz Testing (Planned)
- Protocol fuzzing for robustness
- Cryptographic function testing
- Input validation testing

## Contributing

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Ensure all tests pass
6. Submit a pull request

### Code Style
- C99 standard
- Descriptive variable names
- Comprehensive error handling
- Clear documentation and comments
- Secure coding practices

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Disclaimer

This software is for educational and research purposes. Use at your own risk. The authors are not responsible for any misuse or security issues arising from the use of this software.

## Roadmap

### Phase 1: Core Security (Completed)
- [x] Protocol updates with ECDH and XChaCha20-Poly1305
- [x] Event-driven server architecture
- [x] ncurses client interface
- [x] Build system and dependencies

### Phase 2: Advanced Features (In Progress)
- [ ] Tor integration for anonymity
- [ ] Steganography for traffic obfuscation
- [ ] Advanced DoS protection
- [ ] Database integration improvements

### Phase 3: Production Ready
- [ ] Comprehensive testing suite
- [ ] Performance optimization
- [ ] Documentation and deployment scripts
- [ ] Security audit and hardening

### Phase 4: Extended Features
- [ ] Multi-file transfers
- [ ] Directory synchronization
- [ ] Plugin system for extensions
- [ ] Web interface option
- [ ] Mobile client support
