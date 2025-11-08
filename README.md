## MeshExchange

Powerful, secure, peer-assisted file exchange for local and distributed networks.

MeshExchange is a lightweight C-based file exchange system designed for reliability, privacy, and performance. It combines transport primitives, strong cryptography, and optional MongoDB-backed metadata storage to provide a flexible platform for building secure file-transfer and synchronization services.

---

## TL;DR

- Language: C
- Focus: secure file exchange, small footprint, pluggable storage (MongoDB), TLS and AEAD (AES-GCM) encryption
- Build: POSIX-compatible toolchain (gcc/clang, make)
- Layout: core network/watchers, crypto utilities, DB adapters, clients and server components

Use the included build scripts to compile the server, client and optional tools. See "Quick start" below.

---

## Key Features

- Secure transfer: AES-GCM for authenticated encryption and mutual TLS for node identity.
- Integrity: file hashing utilities (BLAKE3 code included in `deps/blake3/`).
- Pluggable storage: optional MongoDB metadata store and helper scripts under `mongo/` and `db/`.
- Modular architecture: clear separation between core, crypto, DB, net, and server/client components.
- Small, portable C codebase with minimal external dependencies.

---

## Repository layout (high level)

- `src/` — main server/client entrypoints, daemon, certificates and example keys
- `core/` — watchers and internal utilities (inotify watcher)
- `crypto/` — AES-GCM helpers and crypto glue
- `db/` — MongoDB client and ops helpers
- `deps/blake3/` — included BLAKE3 implementation for hashing
- `server/`, `client/` — convenience scripts and example servers/clients
- `tests/` — unit/integration tests and mocks
- `mongo/` — MongoDB init scripts

This structure is intentionally simple to make it easy to extend or integrate into other projects.

---

## Quick start

Prerequisites

- A POSIX environment (Linux/macOS)
- gcc or clang, make
- OpenSSL (headers/libraries) for TLS support
- MongoDB (optional; required only if you use DB-backed metadata)

Build (recommended order)

1. Build core components using the top-level build script or Makefile (if present):

```zsh
# from repository root
./build.sh
# Or use Makefile if you prefer
make
```

2. Build server and client (examples):

```zsh
./server/build_server.sh
./src/client/build.sh
```

3. Initialize MongoDB (optional):

```zsh
# Start mongod as appropriate for your platform, then:
mongo mongo/init.js
```

Run

```zsh
# Example: start the exchange daemon (binary location may vary)
./src/exchange-daemon

# Start a client binary (example)
./src/client/client
```

Note: There are multiple build/run helper scripts in `devBilds/`, `server/` and `src/`. Inspect them for platform-specific details.

---

## Configuration & TLS

- Example certificates and keys are provided for development under `src/` and `server/` (e.g. `server-key.pem`, `server-cert.pem`, `ca.pem`).
- For production, generate strong keys and a proper CA chain. Store private keys securely and rotate them periodically.
- The code uses AES-GCM for content encryption; certificate-based TLS is used for node authentication where configured.

Security tips

- Protect `ca-key.pem`, `server-key.pem` and `client-key.pem`. Treat them like secrets.
- Use firewall rules to restrict access to services and only expose required ports.
- Enable MongoDB authentication if you enable DB-backed features.

---

## Architecture overview

1. Watcher layer (inotify): monitors file-system changes to detect new files or changes.
2. Core transfer logic: handles chunking, hashing (BLAKE3), and encryption (AES-GCM).
3. Network layer: minimal transport primitives and signaling (`net/` and `signal_sender.c`).
4. Optional DB layer: MongoDB stores metadata, transfer state, and indexing via `db/` helpers.

This layered model keeps concerns isolated and makes it easy to swap components (for example, replace MongoDB with a different metadata backend).

---

## Development and testing

- Unit and integration tests live in `tests/`. You can inspect `tests/test_runner.c` and the mocks in `tests/mocks/` for test scaffolding.
- To run tests, compile the test binaries with your toolchain (there is no single test runner script by default). Example:

```zsh
gcc -Iinclude -Ideps/blake3 -o tests/test_runner tests/test_runner.c tests/test_utils.c -lssl -lcrypto
./tests/test_runner
```

Adapt compile flags as needed for your environment. Consider adding a `Makefile` target like `make test` to automate this.

---

## Contributing

We welcome contributions. A quick checklist to get started:

- Fork the repository and create a feature branch.
- Follow the existing C style and keep changes minimal and focused.
- Add tests for new functionality and run existing tests.
- Open a pull request with a clear description and rationale.

Recommended next steps for the project

- Add CI (GitHub Actions) to build and run tests on PRs.
- Add a `LICENSE` file (e.g., MIT) if you want to make the project's license explicit.
- Provide Dockerfiles and compose files for a reproducible demo (there is already `docker-compose.yml`—consider documenting the exact service layout).

---

## Troubleshooting

- If build fails due to missing headers, ensure OpenSSL and any development packages are installed (e.g., `libssl-dev` on Debian/Ubuntu).
- For MongoDB connection issues, check `mongo/init.js` and ensure `mongod` is running and reachable.
- If TLS fails, check that certificates are readable and not expired.

---

## Roadmap (ideas)

- Add a small HTTP/REST API for management and metadata browsing.
- Optional P2P discovery layer for NAT traversal.
- GUI client for easier adoption.

---

## Credits & references

- BLAKE3 reference implementation included under `deps/blake3/`.
- Uses OpenSSL for TLS and AEAD primitives.

---

## License

This repository does not include an explicit `LICENSE` file. If you're the project owner, add a `LICENSE` (MIT, Apache-2.0, etc.) to clarify usage terms.

---

If you'd like, I can also:

- Add a sample `LICENSE` (MIT) and commit it.
- Create a simple `Makefile` target for `make test` and `make all`.
- Produce a short `docker-compose` demo that wires `mongod` and the daemon for local testing.

Tell me which of these you'd like next and I'll implement it.
