# MeshExchange: Secure File Exchange Daemon

A high-performance, end-to-end encrypted file transfer system. Built for professionals, by professionals.

## Features

- **End-to-End Encryption**: Utilizes AES-GCM-256 for secure, authenticated encryption of files during transfer.
- **Integrity & Identity Verification**: Files and metadata are verified using BLAKE3 hashing. Supports runtime CPU feature detection (SSE2, SSE4.1, AVX2).
- **Metadata Storage**: Stores metadata such as filename, owner fingerprint, public flag, IV, and auth tag in MongoDB.
- **Secure Communication**: Client-server interactions secured via TLS 1.3 with forward secrecy.
- **Access Control**: Enforces strict permission policies; only the owner or public files can be downloaded.
- **Resumable Downloads**: Allows resuming file transfers from byte offsets to mitigate disruptions.
- **Minimal Dependencies**: Only requires libmongoc, OpenSSL, libmicrohttpd (optional HTTP API), and the C standard library.
- **Self-contained**: Includes a custom systemd service; no external configuration files are needed.

## Security Model

MeshExchange guarantees data confidentiality and integrity through a robust end-to-end encryption mechanism:

- **Encryption**: Files are encrypted locally on the sender’s machine using AES-GCM-256 before transmission. This ensures that only the recipient with the correct decryption key can access the contents.
- **Key Management**: Key exchange is secured using public-key cryptography (RSA or ECDSA), and session keys are generated per transfer, ensuring forward secrecy.
- **Access Control**: Files are either owner-specific or marked as public. Only the file owner can access private files, while public files can be downloaded by any authorized client. Path traversal and permission checks are enforced on both file access and metadata storage.
- **Data Integrity**: Each file and its metadata (e.g., filenames, IVs, auth tags) are hashed using the BLAKE3 algorithm, ensuring tamper-evident security.

## Build Instructions

### Dependencies for Arch Linux

To build MeshExchange, the following packages must be installed:

```bash
sudo pacman -S base-devel libmongoc openssl libmicrohttpd
```
## Build Steps

### Clone the repository:

```bash
git clone https://github.com/yourusername/MeshExchange.git
cd MeshExchange
```


### Compile the project:
```bash
make or bash build.sh
```
### Install the service (optional):
```bash
sudo make install
```
### Start the service (optional):
```bash
./exchange-daemon
``` 

### After..
