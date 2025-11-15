#!/bin/bash

# Environment Setup Script for Secure File Exchange System
# Sets up development environment with all dependencies

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
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

log_step() {
    echo -e "${BLUE}[STEP]${NC} $1"
}

# Detect OS
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        if command -v apt-get &> /dev/null; then
            echo "ubuntu"
        elif command -v yum &> /dev/null; then
            echo "centos"
        elif command -v pacman &> /dev/null; then
            echo "arch"
        else
            echo "linux"
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    else
        echo "unknown"
    fi
}

# Install dependencies for Ubuntu/Debian
install_ubuntu() {
    log_step "Installing dependencies for Ubuntu/Debian..."

    sudo apt-get update

    # Core dependencies
    sudo apt-get install -y build-essential pkg-config

    # Libraries
    sudo apt-get install -y \
        libsodium-dev \
        libevent-dev \
        libncurses-dev \
        libssl-dev \
        libglib2.0-dev \
        libreadline-dev \
        libmongoc-dev \
        mongodb-server

    # Development tools
    sudo apt-get install -y git cmake valgrind gdb

    log_info "Ubuntu/Debian dependencies installed"
}

# Install dependencies for CentOS/RHEL/Fedora
install_centos() {
    log_step "Installing dependencies for CentOS/RHEL/Fedora..."

    # Enable EPEL if on CentOS/RHEL
    if command -v yum &> /dev/null; then
        sudo yum install -y epel-release
    fi

    # Core dependencies
    sudo yum install -y gcc gcc-c++ make pkgconfig

    # Libraries
    sudo yum install -y \
        libsodium-devel \
        libevent-devel \
        ncurses-devel \
        openssl-devel \
        glib2-devel \
        readline-devel \
        mongo-c-driver-devel \
        mongodb-server

    # Development tools
    sudo yum install -y git cmake valgrind gdb

    log_info "CentOS/RHEL/Fedora dependencies installed"
}

# Install dependencies for Arch Linux
install_arch() {
    log_step "Installing dependencies for Arch Linux..."

    sudo pacman -Syu --noconfirm

    # Libraries
    sudo pacman -S --noconfirm \
        libsodium \
        libevent \
        ncurses \
        openssl \
        glib2 \
        readline \
        mongo-c-driver \
        mongodb

    # Development tools
    sudo pacman -S --noconfirm \
        base-devel \
        git \
        cmake \
        valgrind \
        gdb

    log_info "Arch Linux dependencies installed"
}

# Install dependencies for macOS
install_macos() {
    log_step "Installing dependencies for macOS..."

    # Check if Homebrew is installed
    if ! command -v brew &> /dev/null; then
        log_error "Homebrew is not installed. Please install Homebrew first:"
        echo "  /bin/bash -c \"\$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)\""
        exit 1
    fi

    # Update Homebrew
    brew update

    # Install libraries
    brew install \
        libsodium \
        libevent \
        ncurses \
        openssl \
        glib \
        readline \
        mongo-c-driver \
        mongodb-community

    # Development tools
    brew install \
        cmake \
        valgrind \
        gdb

    log_info "macOS dependencies installed"
}

# Setup MongoDB
setup_mongodb() {
    log_step "Setting up MongoDB..."

    OS=$(detect_os)

    case $OS in
        ubuntu)
            sudo systemctl enable mongodb
            sudo systemctl start mongodb
            ;;
        centos)
            sudo systemctl enable mongod
            sudo systemctl start mongod
            ;;
        arch)
            sudo systemctl enable mongodb
            sudo systemctl start mongodb
            ;;
        macos)
            brew services start mongodb-community
            ;;
        *)
            log_warn "Please manually start MongoDB service for your OS"
            ;;
    esac

    # Wait for MongoDB to start
    sleep 5

    # Test connection
    if command -v mongosh &> /dev/null; then
        mongosh --eval "db.runCommand('ping')" &> /dev/null && log_info "MongoDB connection test: PASSED"
    elif command -v mongo &> /dev/null; then
        mongo --eval "db.runCommand('ping')" &> /dev/null && log_info "MongoDB connection test: PASSED"
    else
        log_warn "Could not test MongoDB connection - mongo/mongosh not found"
    fi
}

# Generate certificates
generate_certs() {
    log_step "Generating SSL certificates..."

    if [ ! -f "scripts/generate_keys.sh" ]; then
        log_error "generate_keys.sh script not found"
        return 1
    fi

    chmod +x scripts/generate_keys.sh
    ./scripts/generate_keys.sh

    log_info "SSL certificates generated"
}

# Create necessary directories
create_dirs() {
    log_step "Creating project directories..."

    mkdir -p bin
    mkdir -p logs
    mkdir -p filetrade
    mkdir -p src/crypto
    mkdir -p src/server
    mkdir -p src/client
    mkdir -p src/utils
    mkdir -p include
    mkdir -p scripts
    mkdir -p tests

    log_info "Project directories created"
}

# Test build
test_build() {
    log_step "Testing build process..."

    if [ ! -f "Makefile" ]; then
        log_error "Makefile not found"
        return 1
    fi

    make clean
    make test-build

    if [ -f "bin/client" ] && [ -f "bin/server" ]; then
        log_info "Build test: PASSED"
    else
        log_error "Build test: FAILED"
        return 1
    fi
}

# Show usage information
show_usage() {
    echo "Secure File Exchange - Environment Setup Script"
    echo
    echo "Usage: $0 [OPTIONS]"
    echo
    echo "Options:"
    echo "  -h, --help          Show this help message"
    echo "  -d, --deps-only     Install dependencies only"
    echo "  -c, --certs-only    Generate certificates only"
    echo "  -b, --build-only    Test build only"
    echo "  --no-mongo          Skip MongoDB setup"
    echo "  --no-certs          Skip certificate generation"
    echo
    echo "Examples:"
    echo "  $0                  Full setup (dependencies + certificates + build test)"
    echo "  $0 --deps-only      Install dependencies only"
    echo "  $0 --certs-only     Generate certificates only"
}

# Main function
main() {
    local DEPS_ONLY=false
    local CERTS_ONLY=false
    local BUILD_ONLY=false
    local SKIP_MONGO=false
    local SKIP_CERTS=false

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -h|--help)
                show_usage
                exit 0
                ;;
            -d|--deps-only)
                DEPS_ONLY=true
                ;;
            -c|--certs-only)
                CERTS_ONLY=true
                ;;
            -b|--build-only)
                BUILD_ONLY=true
                ;;
            --no-mongo)
                SKIP_MONGO=true
                ;;
            --no-certs)
                SKIP_CERTS=true
                ;;
            *)
                log_error "Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
        shift
    done

    log_info "Secure File Exchange - Environment Setup"
    echo

    # Detect OS
    OS=$(detect_os)
    log_info "Detected OS: $OS"

    # Handle different modes
    if [ "$DEPS_ONLY" = true ]; then
        case $OS in
            ubuntu) install_ubuntu ;;
            centos) install_centos ;;
            arch) install_arch ;;
            macos) install_macos ;;
            *) log_error "Unsupported OS: $OS" && exit 1 ;;
        esac
        [ "$SKIP_MONGO" = false ] && setup_mongodb
        exit 0
    fi

    if [ "$CERTS_ONLY" = true ]; then
        generate_certs
        exit 0
    fi

    if [ "$BUILD_ONLY" = true ]; then
        test_build
        exit 0
    fi

    # Full setup
    create_dirs

    case $OS in
        ubuntu) install_ubuntu ;;
        centos) install_centos ;;
        arch) install_arch ;;
        macos) install_macos ;;
        *) log_error "Unsupported OS: $OS" && exit 1 ;;
    esac

    [ "$SKIP_MONGO" = false ] && setup_mongodb
    [ "$SKIP_CERTS" = false ] && generate_certs

    test_build

    echo
    log_info "Environment setup completed successfully!"
    echo
    log_info "Next steps:"
    echo "  1. Start the server: ./bin/server"
    echo "  2. In another terminal, start the client: ./bin/client"
    echo "  3. Use 'help' command in client for available commands"
    echo
    log_warn "Remember to keep certificates and private keys secure!"
}

# Run main function
main "$@"
