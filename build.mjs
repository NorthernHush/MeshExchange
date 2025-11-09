#!/usr/bin/env node
// Minimal ESM build script for MeshExchange
// Usage: node build.mjs [target] [--dry-run]
// Targets: all, daemon, client, server, mongo, tests, clean

import { spawn } from 'child_process';
import chalk from 'chalk';
import ora from 'ora';
import figlet from 'figlet';
import gradient from 'gradient-string';
import inquirer from 'inquirer';
import { env, exit } from 'process';

const argv = process.argv.slice(2);
const dryRun = argv.includes('--dry-run') || argv.includes('-n');
const targetArg = argv.find(a => !a.startsWith('-'));
const target = targetArg || null;

function run(cmd, args = [], opts = {}) {
	const full = `${cmd} ${args.join(' ')}`.trim();
	if (dryRun) {
		console.log('[dry-run]', full);
		return Promise.resolve({ code: 0 });
	}
	console.log('[run]', full);
	return new Promise((resolve, reject) => {
		const p = spawn(cmd, args, { stdio: 'inherit', shell: false, ...opts });
		p.on('close', code => {
			if (code === 0) resolve({ code });
			else reject(new Error(`${cmd} exited with ${code}`));
		});
	});
}

async function which(cmd) {
	try {
		await run('which', [cmd]);
		return true;
	} catch (e) {
		return false;
	}
}

async function pkgConfig(flags) {
	try {
		const out = await new Promise((res, rej) => {
			const p = spawn('pkg-config', flags.split(' '), { shell: false });
			let s = '';
			p.stdout.on('data', d => s += d.toString());
			p.on('close', code => code === 0 ? res(s.trim()) : rej(new Error('pkg-config failed')));
		});
		return out;
	} catch (e) {
		return '';
	}
}

async function checkEnvironment() {
	const haveGcc = await which('gcc') || await which('cc');
	if (!haveGcc) throw new Error('gcc/cc not found in PATH');
	// pkg-config is optional but helpful
	const havePkg = await which('pkg-config');
	return { haveGcc, havePkg };
}

const BLAKE3_DIR = 'deps/blake3';

//* собираем демона жеска
async function buildDaemon() {
	// gcc -o exchange-daemon src/main.c src/db/mongo_ops.c $(pkg-config --cflags --libs libmongoc-1.0)
	const pkg = (await pkgConfig('--cflags --libs libmongoc-1.0')) || '';
	const args = ['-o', 'exchange-daemon', 'src/main.c', 'src/db/mongo_ops.c', ...pkg.split(' ').filter(Boolean)];
	return run('gcc', args);
}

//* Собираем клиента жеска
async function buildClient() {
	const pkgCflags = (await pkgConfig('--cflags libmongoc-1.0')) || '';
	const pkgLibs = (await pkgConfig('--libs libmongoc-1.0')) || '';
	const common = ['-Iinclude', `-I${BLAKE3_DIR}`, '-Wall', '-Wextra'];

	const compile = [
		['gcc', ['-c', 'src/client/client.c', '-o', 'client.o', ...common, ...pkgCflags.split(' ').filter(Boolean)]],
		['gcc', ['-c', 'src/db/mongo_ops.c', '-o', 'mongo_ops.o', ...common, ...pkgCflags.split(' ').filter(Boolean)]],
		['gcc', ['-c', 'src/utils/utils.c', '-o', 'utils.o', ...common]],
		['gcc', ['-c', 'src/crypto/aes_gcm.c', '-o', 'aes_gcm.o', ...common]],
	];

	const blakeObjs = [
		['gcc', ['-c', `${BLAKE3_DIR}/blake3.c`, '-o', 'blake3.o', '-I' + BLAKE3_DIR, '-Wall', '-Wextra']],
		// Disable AVX512 dispatch calls unless an AVX512 object is explicitly built
		['gcc', ['-c', `${BLAKE3_DIR}/blake3_dispatch.c`, '-o', 'blake3_dispatch.o', '-I' + BLAKE3_DIR, '-Wall', '-Wextra', '-DBLAKE3_NO_AVX512']],
		['gcc', ['-c', `${BLAKE3_DIR}/blake3_portable.c`, '-o', 'blake3_portable.o', '-I' + BLAKE3_DIR, '-Wall', '-Wextra']],
		['gcc', ['-c', `${BLAKE3_DIR}/blake3_sse2.c`, '-o', 'blake3_sse2.o', '-I' + BLAKE3_DIR, '-Wall', '-Wextra', '-msse2']],
		['gcc', ['-c', `${BLAKE3_DIR}/blake3_sse41.c`, '-o', 'blake3_sse41.o', '-I' + BLAKE3_DIR, '-Wall', '-Wextra', '-mssse3', '-msse4.1']],
		['gcc', ['-c', `${BLAKE3_DIR}/blake3_avx2.c`, '-o', 'blake3_avx2.o', '-I' + BLAKE3_DIR, '-Wall', '-Wextra', '-mavx2']],
	];

	for (const [cmd, args] of [...compile, ...blakeObjs]) await run(cmd, args);

		const linkArgs = ['-o', 'client', 'client.o', 'mongo_ops.o', 'utils.o', 'aes_gcm.o', 'blake3.o', 'blake3_dispatch.o', 'blake3_portable.o', 'blake3_sse2.o', 'blake3_sse41.o', 'blake3_avx2.o', ...pkgLibs.split(' ').filter(Boolean), '-lssl', '-lcrypto', '-lpthread'];
	return run('gcc', linkArgs);
}
//* собираем сервер
async function buildServer() {
	const pkgCflags = (await pkgConfig('--cflags libmongoc-1.0')) || '';
	const pkgLibs = (await pkgConfig('--libs libmongoc-1.0')) || '';
	const common = ['-Iinclude', `-I${BLAKE3_DIR}`, '-Wall', '-Wextra'];

	const compile = [
		['gcc', ['-c', 'src/server/server.c', '-o', 'server.o', ...common, ...pkgCflags.split(' ').filter(Boolean)]],
		['gcc', ['-c', 'src/db/mongo_ops_server.c', '-o', 'mongo_ops_server.o', ...common, ...pkgCflags.split(' ').filter(Boolean)]],
		['gcc', ['-c', 'src/utils/utils.c', '-o', 'utils.o', ...common]],
		['gcc', ['-c', 'src/crypto/aes_gcm.c', '-o', 'aes_gcm.o', ...common]],
	];

	const blakeObjs = [
		['gcc', ['-c', `${BLAKE3_DIR}/blake3.c`, '-o', 'blake3.o', '-I' + BLAKE3_DIR, '-Wall', '-Wextra']],
		// Prevent dispatch from referencing AVX512 symbols unless the avx512 object is compiled
		['gcc', ['-c', `${BLAKE3_DIR}/blake3_dispatch.c`, '-o', 'blake3_dispatch.o', '-I' + BLAKE3_DIR, '-Wall', '-Wextra', '-DBLAKE3_NO_AVX512']],
		['gcc', ['-c', `${BLAKE3_DIR}/blake3_portable.c`, '-o', 'blake3_portable.o', '-I' + BLAKE3_DIR, '-Wall', '-Wextra']],
		['gcc', ['-c', `${BLAKE3_DIR}/blake3_sse2.c`, '-o', 'blake3_sse2.o', '-I' + BLAKE3_DIR, '-Wall', '-Wextra', '-msse2']],
		['gcc', ['-c', `${BLAKE3_DIR}/blake3_sse41.c`, '-o', 'blake3_sse41.o', '-I' + BLAKE3_DIR, '-Wall', '-Wextra', '-mssse3', '-msse4.1']],
		['gcc', ['-c', `${BLAKE3_DIR}/blake3_avx2.c`, '-o', 'blake3_avx2.o', '-I' + BLAKE3_DIR, '-Wall', '-Wextra', '-mavx2']],
	];

	for (const [cmd, args] of [...compile, ...blakeObjs]) await run(cmd, args);

		const linkArgs = ['-o', 'server', 'server.o', 'mongo_ops_server.o', 'utils.o', 'aes_gcm.o', 'blake3.o', 'blake3_dispatch.o', 'blake3_portable.o', 'blake3_sse2.o', 'blake3_sse41.o', 'blake3_avx2.o', ...pkgLibs.split(' ').filter(Boolean), '-lssl', '-lcrypto', '-lpthread'];
	return run('gcc', linkArgs);
}

async function buildMongoClient() {
	// src/db/mongo_client.c or top-level db script
	// gcc -o mongo_client src/db/mongo_client.c $(pkg-config --cflags --libs libmongoc-1.0)
	const pkg = (await pkgConfig('--cflags --libs libmongoc-1.0')) || '';
	const args = ['-o', 'mongo_client', 'src/db/mongo_client.c', ...pkg.split(' ').filter(Boolean)];
	return run('gcc', args);
}

async function showLogo() {
  const logo = figlet.textSync('MeshExchange', { font: 'Standard' });
  console.log(gradient.pastel.multiline(logo));
  console.log(chalk.gray('───────────────────────────────────────────────'));
  console.log(chalk.cyanBright('         ⚙  Build System MeshExchange\n'));
}


async function buildTests() {

	const args = ['tests.py'];
	return run('python3', args);
}
//* вывод иттерактивного меню
async function menuPrint() {
  const { target } = await inquirer.prompt([
    {
      type: 'list',
      name: 'target',
      message: 'Выберите цель сборки:',
      choices: [
        { name: '🧩  all      – собрать всё', value: 'all' },
        { name: '🔁  daemon   – build exchange-daemon', value: 'daemon' },
        { name: '💻  client   – build client', value: 'client' },
        { name: '🖥️  server   – build server', value: 'server' },
        { name: '🍃  mongo    – build mongo_client', value: 'mongo' },
        { name: '🧪  tests    – run tests', value: 'tests' },
        { name: '🧹  clean    – remove artifacts', value: 'clean' },
        new inquirer.Separator(),
        { name: '❌  Выход', value: 'exit' },
      ],
    },
  ]);
  return target;
}


async function clean() {
	const files = ['exchange-daemon', 'client', 'server', 'mongo_client', 'tests/test_runner', 'obfuscator', 'client.o', 'mongo_ops_server.o', 'server.o', 'mongo_ops.o', 'utils.o', 'aes_gcm.o', 'blake3.o', 'blake3_dispatch.o', 'blake3_portable.o', 'blake3_sse2.o', 'blake3_sse41.o', 'blake3_avx2.o', 'blake3_avx512.o', 'blake3_sse41.o', 'blake3_sse2.o'];
	for (const f of files) {
		if (dryRun) console.log('[dry-run] rm -f', f);
		else await run('rm', ['-f', f]);
	}
}

async function main() {
	await showLogo();
	let target = argv.find(a => !a.startsWith('-')) || null;

	if (!target) {
	target = await menuPrint();
	if (target === 'exit') {
		console.log('Выход из билд-системы.');
		process.exit(0);
	}
	}

	try {
		await checkEnvironment();
		switch (target) {
			case 'all':
				await buildDaemon();
				await buildClient();
				await buildServer();
				await buildMongoClient();
				break;
			case 'daemon':
				await buildDaemon(); break;
			case 'client':
				await buildClient(); break;
			case 'server':
				await buildServer(); break;
			case 'mongo':
				await buildMongoClient(); break;
			case 'tests':
				await buildTests(); break;
			case 'clean':
				await clean(); break;
			default:
				console.error('Unknown target:', target);
				process.exit(2);
		}
		console.log('\nBuild finished.');
	} catch (e) {
		console.error('Build failed:', e.message || e);
		process.exit(1);
	}
}

if (import.meta.url === `file://${process.argv[1]}`) main();
