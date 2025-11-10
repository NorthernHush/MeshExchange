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
import cliProgress from 'cli-progress';

const argv = process.argv.slice(2);
const dryRun = argv.includes('--dry-run') || argv.includes('-n');
const targetArg = argv.find(a => !a.startsWith('-'));
const target = targetArg || null;

function sleep(ms) {
	return new Promise(resolve => setTimeout(resolve, ms))
}



async function doWork() {
    // 1. Запуск спиннера
    const spinner = ora('Загрузка данных...').start();

    try {
        // 2. Имитация асинхронной операции (например, запрос к API)
        // Используем setTimeout в промисе для задержки в 3 секунды
        await new Promise(resolve => setTimeout(resolve, 3000));

        // 3. Остановка спиннера с сообщением об успехе
        spinner.succeed('Данные успешно загружены!');

    } catch (error) {
        // 4. Остановка спиннера с сообщением об ошибке
        spinner.fail('Ошибка загрузки!');
        console.error(error);
    }
}


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
async function showAnimeGirl() {
  const glitch = [
    chalk.magentaBright("Initializing Neural Core..."),
    chalk.cyanBright("Booting HoloGirl v1.2..."),
    chalk.green("Connecting to MeshExchange mainframe..."),
    chalk.yellow("Loading personality modules [■■■■■■■■■■■■■■■■■■■] 100%"),
  ];

  for (const line of glitch) {
    console.log(line);
    await sleep(400);
  }

  await sleep(700);
  console.clear();

  const girl = [
`
      ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣤⣶⣶⣦⣤⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
      ⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣴⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
      ⠀⠀⠀⠀⠀⠀⠀⠀⣰⣿⣿⡿⠋⠙⠻⣿⣿⡿⠋⠙⣿⣿⣷⡀⠀⠀⠀⠀⠀⠀⠀⠀
      ⠀⠀⠀⠀⠀⠀⠀⠀⣿⣿⣿⠀⠀⠀⠀⠈⠁⠀⠀⠀⢹⣿⣿⣷⠀⠀⠀⠀⠀⠀⠀⠀
      ⠀⠀⠀⠀⠀⠀⠀⠀⠹⣿⣿⣆⠀⠀⠀⠀⠀⠀⠀⣠⣿⣿⠟⠀⠀⠀⠀⠀⠀⠀⠀⠀
      ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠻⣿⣿⣶⣶⣶⣶⣶⣿⡿⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
      ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠉⠛⠻⠿⠿⠛⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
`,
`
      ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣤⣶⣶⣦⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
      ⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⣿⣿⣿⣿⣿⣿⣿⣿⣷⣦⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
      ⠀⠀⠀⠀⠀⠀⠀⠀⣾⣿⣿⠋⠀⢀⣀⠀⠙⣿⣿⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
      ⠀⠀⠀⠀⠀⠀⠀⠀⢻⣿⣿⡀⠀⠙⠋⠀⢀⣿⣿⡟⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
      ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠻⣿⣿⣷⣶⣶⣾⣿⣿⠟⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
      ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠉⠛⠛⠉⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
`
  ];

  console.log(gradient.cristal(`\n[AI] Holographic Interface: ONLINE`));
  await sleep(500);
  console.log(chalk.cyanBright(`[AI] Systems nominal. Visual matrix stabilized.`));
  await sleep(700);
  console.log(chalk.magentaBright(`[AI] こんにちは! 私は Mesh-tan — ваша билд-помощница 💫`));
  await sleep(1000);
  console.clear();

  // “танец”
  for (let i = 0; i < 4; i++) {
    console.clear();
    console.log(gradient.pastel.multiline(girl[i % 2]));
    console.log(chalk.cyanBright(`Mesh-tan: 💃 コンパイル中... [frame ${i+1}]`));
    await sleep(500);
  }

  console.clear();
  console.log(gradient.atlas.multiline(girl[0]));
  console.log(chalk.magentaBright("Mesh-tan: ✨ 完了! Всё собрано, сенсей! 💖"));
  await sleep(1000);
  console.clear();
}


async function showEpicIntro() {
  const wait = ms => new Promise(r => setTimeout(r, ms));

  // --- 1) cinematic figlet intro + glitch overlay
  const big = figlet.textSync('MESH', { font: 'Big' });
  const small = figlet.textSync('Exchange', { font: 'Small' });

  // quick micro-glitch drawer
  function glitchize(text, n = 6) {
    // вставляем случайные глитч-символы
    return text.split('\n').map(line => {
      let arr = line.split('');
      for (let i = 0; i < n; i++) {
        const idx = Math.floor(Math.random() * arr.length);
        arr[idx] = ['█','▓','▒','░','/', '\\', '|', '_', '*', '•'][Math.floor(Math.random()*10)];
      }
      return arr.join('');
    }).join('\n');
  }

  // small clear + "power on" lines
  console.clear();
  console.log(chalk.gray('┌──────────────────────── SYSTEM BOOT ───────────────────────┐'));
  console.log(chalk.gray(`|  ${new Date().toISOString()}  |`));
  console.log(chalk.gray('└────────────────────────────────────────────────────────────┘'));
  await wait(350);

  // flicker in big title with gradient
  for (let i = 0; i < 6; i++) {
    console.clear();
    if (i % 2 === 0) console.log(gradient.vice.multiline(big));
    else console.log(chalk.black.bgWhite(big));
    await wait(90);
  }
  await wait(180);

  // glitch overlay frames
  for (let i = 0; i < 10; i++) {
    console.clear();
    if (i % 3 === 0) console.log(gradient.cristal.multiline(glitchize(big, 12)));
    else console.log(gradient.pastel.multiline(big));
    if (i > 6) console.log(gradient.vice.multiline(small));
    await wait(80);
  }

  // --- 2) matrix particles in background (short)
  const cols = process.stdout.columns || 80;
  const rows = Math.min(process.stdout.rows || 24, 24);
  // prepare columns state
  const drops = new Array(cols).fill(0).map(() => Math.floor(Math.random()*rows));
  const particleFrames = 14;
  for (let f = 0; f < particleFrames; f++) {
    console.clear();
    // draw background particles
    const screen = Array.from({ length: rows }, () => Array(cols).fill(' '));
    for (let c = 0; c < cols; c += 2) { // step to reduce load
      const len = Math.floor(Math.random() * 3);
      const start = drops[c];
      for (let k = 0; k < len; k++) {
        const r = (start + k) % rows;
        screen[r][c] = String.fromCharCode(0x30A0 + Math.floor(Math.random()*96)); // katakana-range junk
      }
      drops[c] = (drops[c] + 1) % rows;
    }
    // render a portion of big title in center
    const titleLines = big.split('\n');
    const startRow = Math.max(1, Math.floor((rows - titleLines.length) / 2) - 2);
    for (let i = 0; i < titleLines.length && startRow + i < rows; i++) {
      const line = titleLines[i];
      const startCol = Math.max(0, Math.floor((cols - line.length) / 2));
      for (let j = 0; j < Math.min(line.length, cols - startCol); j++) {
        screen[startRow + i][startCol + j] = line[j];
      }
    }
    // print with gradient on top lines
    for (let r = 0; r < rows; r++) {
      const rowStr = screen[r].join('');
      if (r >= startRow && r < startRow + titleLines.length) {
        // apply strong neon gradient to title rows
        process.stdout.write(gradient.atlas(rowStr) + '\n');
      } else {
        process.stdout.write(chalk.gray(rowStr) + '\n');
      }
    }
    await wait(60);
  }

  // --- 3) holo-girl frames (animated, neon)
  const holoFrames = [
`      ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣤⣤⣶⣶⣦⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
       ⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣴⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣄⠀⠀⠀⠀⠀⠀⠀⠀
       ⠀⠀⠀⠀⠀⠀⠀⠀⣰⣿⣿⡿⠋⠙⠻⣿⣿⡿⠋⠙⣿⣿⣷⡀⠀⠀⠀⠀⠀⠀`,
`      ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣤⣶⣶⣦⣄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
       ⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣦⠀⠀⠀⠀⠀⠀⠀⠀⠀
       ⠀⠀⠀⠀⠀⠀⠀⠀⣾⣿⣿⠋⠀⢀⣀⠀⠙⣿⣿⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀`,
`      ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣤⡶⠖⠶⣦⣤⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀
       ⠀⠀⠀⠀⠀⠀⠀⠀⠀⣴⣿⣿⣿⠿⠛⠛⠻⣿⣿⣿⣷⣄⠀⠀⠀⠀⠀⠀⠀⠀
       ⠀⠀⠀⠀⠀⠀⠀⠀⢰⣿⣿⣿⡄  ⠀⠀⠀⢸⣿⣿⣿⡆⠀⠀⠀⠀⠀⠀⠀⠀`
  ];
  // loop holo dance with neon shift
  for (let r = 0; r < 8; r++) {
    console.clear();
    const frame = holoFrames[r % holoFrames.length];
    // shimmering color sweep
    if (r % 3 === 0) console.log(gradient.vice.multiline(frame));
    else if (r % 3 === 1) console.log(gradient.pastel.multiline(frame));
    else console.log(gradient.cristal.multiline(frame));
    console.log(chalk.cyanBright(`\n   Mesh-tan • Holo Assist • Initializing 🔋 ${(r+1)*12}%`));
    await wait(220);
  }

  // --- 4) epic progress bar + spinner while "compiling"
  const spinner = ora({ text: 'Mesh-tan: Компиляция ядра...', spinner: 'bouncingBar' }).start();

  // CLI progress bar (simulated heavy build)
  const bar = new cliProgress.SingleBar({
    format: gradient.pastel('{bar}') + ' {percentage}% | {value}/{total} | {task}',
    barCompleteChar: '█',
    barIncompleteChar: '░',
    hideCursor: true,
    autopadding: true,
  }, cliProgress.Presets.shades_classic);

  const totalSteps = 120;
  bar.start(totalSteps, 0, { task: 'linking modules' });

  for (let i = 0; i <= totalSteps; i++) {
    bar.update(i, { task: i < 40 ? 'compiling' : i < 90 ? 'linking' : 'optimizing' });
    // occasional micro-glitches: text flicker
    if (i % 17 === 0) {
      spinner.text = chalk.magentaBright('Mesh-tan: Параллельные оптимизации… ⚡');
      await wait(80);
      spinner.text = 'Mesh-tan: Компиляция ядра...';
    }
    await wait(40 + Math.floor(Math.random()*40));
  }

  bar.stop();
  spinner.succeed(chalk.green('Mesh-tan: Компиляция завершена — артефакты готовы.'));

  // --- 5) optional TTS (если есть) — не критично, но круто
  try {
    // which() у тебя уже есть в файле, используем
    let ttsCmd = null;
    if (await which('say')) ttsCmd = ['say', ['Mesh tan: Build complete. All systems nominal.']];
    else if (await which('spd-say')) ttsCmd = ['spd-say', ['Mesh tan: Build complete. All systems nominal.']];
    else if (await which('espeak')) ttsCmd = ['espeak', ['"Mesh tan: сборка завершена. Всё готово."']];
    if (ttsCmd) {
      const { spawn } = await import('child_process');
      spawn(ttsCmd[0], ttsCmd[1], { stdio: 'ignore', detached: true });
    }
  } catch (e) {
    // игнорируем — tts необязателен
  }

  // --- 6) final flourish: big figlet LOGO + confetti-like burst
  console.clear();
  console.log(gradient.pastel.multiline(figlet.textSync('MeshExchange', { font: 'Standard' })));
  console.log(chalk.gray('───────────────────────────────────────────────'));
  console.log(chalk.cyanBright('         ⚙  Build System MeshExchange — BOOTED\n'));

  // tiny confetti: print some colorful punctuation scattered
  const confetti = ['✦','✶','✺','✹','✷','✸','❉','✧'];
  for (let i = 0; i < 16; i++) {
    const x = Math.floor(Math.random() * (process.stdout.columns || 80));
    const y = Math.floor(Math.random() * 6) + 2;
    const sym = confetti[Math.floor(Math.random() * confetti.length)];
    // move cursor and write
    process.stdout.write(`\x1B[${y};${x}H` + gradient.vice(sym));
    await wait(40);
  }
  // reset cursor down a bit
  process.stdout.write(`\x1B[${(process.stdout.rows || 24)};0H`);

  // small final bell and message
  process.stdout.write('\x07'); // terminal bell
  await wait(800);
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
	await showEpicBoot();
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


// TODO: mongo start docker function, later..
async function startMongoDocker() {
	showEpicBoot();
	const args = ['mongo.sh']
	console.log('starting mongo database from ', args);
	doWork();
	await sleep(2000);
	return run('bash', args);
}

async function buildMongoClient() {
	// src/db/mongo_client.c or top-level db script
	// gcc -o mongo_client src/db/mongo_client.c $(pkg-config --cflags --libs libmongoc-1.0)
	const pkg = (await pkgConfig('--cflags --libs libmongoc-1.0')) || '';
	const args = ['-o', 'mongo_client', 'src/db/mongo_client.c', ...pkg.split(' ').filter(Boolean)];
	return run('gcc', args);
}


async function showEpicBoot() {
  const wait = ms => new Promise(r => setTimeout(r, ms));

  // helpers
  function randInt(a, b) { return Math.floor(Math.random() * (b - a + 1)) + a; }
  function glitchLine(line, intensity = 6) {
    const chars = ['█','▓','▒','░','/','\\','|','_','*','•','◼','◻','░','⁂'];
    let arr = line.split('');
    for (let i = 0; i < intensity; i++) {
      const idx = randInt(0, Math.max(0, arr.length - 1));
      arr[idx] = chars[randInt(0, chars.length - 1)];
    }
    return arr.join('');
  }
  function glitchText(text, intensity = 6) {
    return text.split('\n').map(l => glitchLine(l, intensity)).join('\n');
  }

  // safe terminal size
  const COLS = process.stdout.columns || 80;
  const ROWS = process.stdout.rows || 24;

  console.clear();
  console.log(chalk.gray('┌──────────────────────── SYSTEM BOOT ───────────────────────┐'));
  console.log(chalk.gray(`|  ${new Date().toISOString()}  |`));
  console.log(chalk.gray('└────────────────────────────────────────────────────────────┘'));
  await wait(300);

  // tiny flicker of title
  const bigTitle = figlet.textSync('MESH', { font: 'Big' }).split('\n').slice(0, ROWS - 6).join('\n');
  for (let i = 0; i < 8; i++) {
    console.clear();
    if (i % 2 === 0) console.log(gradient.vice.multiline(bigTitle));
    else console.log(chalk.black.bgWhite(bigTitle));
    await wait(60 + i * 10);
  }

  // 2) System log boot with glitchy lines
  const sysLines = [
    '[CORE] Initializing neural microkernel...',
    '[CORE] Loading quantum-safe link libraries...',
    '[SEC] Handshake with secure enclave: ESTABLISHED',
    '[I/O] Binding high-throughput channels...',
    '[SYS] Overclock governor: ENGAGED',
    '[AI] Personality module: Mesh-tan (v1.2.7) — LOADED',
    '[NET] MeshExchange mesh sync: 12 peers',
    '[STAGE] All subsystems nominal. Preparing rendering pipeline...'
  ];

  for (let i = 0; i < sysLines.length; i++) {
    // occasional glitch effect
    const useGlitch = Math.random() < 0.35;
    const txt = useGlitch ? chalk.redBright(glitchLine(sysLines[i], randInt(2,10))) : chalk.cyanBright(sysLines[i]);
    console.log(txt);
    await wait(220 + Math.floor(Math.random() * 160));
  }
  await wait(300);

  // 3) short matrix rain + title composite (renders a few frames)
  const frames = 14;
  const drops = new Array(Math.max(10, Math.floor(COLS / 2))).fill(0).map(() => randInt(0, ROWS - 1));
  const katakanaStart = 0x30A0;
  const titleLines = bigTitle.split('\n');
  const titleRow = Math.max(1, Math.floor((ROWS - titleLines.length) / 2) - 1);
  for (let f = 0; f < frames; f++) {
    console.clear();
    const screen = Array.from({ length: ROWS }, () => Array(COLS).fill(' '));

    // matrix particles (sparse)
    for (let c = 0; c < drops.length; c++) {
      const col = c * 2;
      for (let k = 0; k < randInt(1,3); k++) {
        const r = (drops[c] + k) % ROWS;
        if (col < COLS) screen[r][col] = String.fromCharCode(katakanaStart + randInt(0, 80));
      }
      drops[c] = (drops[c] + 1) % ROWS;
    }

    // inject title into the center
    for (let i = 0; i < titleLines.length; i++) {
      const line = titleLines[i];
      const startCol = Math.max(0, Math.floor((COLS - line.length) / 2));
      const row = titleRow + i;
      if (row >= 0 && row < ROWS) {
        for (let j = 0; j < Math.min(line.length, COLS - startCol); j++) {
          screen[row][startCol + j] = line[j];
        }
      }
    }

    // print rows with gradients for title rows
    for (let r = 0; r < ROWS; r++) {
      const rowStr = screen[r].join('');
      if (r >= titleRow && r < titleRow + titleLines.length) {
        process.stdout.write(gradient.atlas(rowStr) + '\n');
      } else {
        process.stdout.write(chalk.gray(rowStr) + '\n');
      }
    }
    await wait(60 + Math.floor(Math.random() * 60));
  }


  const holoFrames = [
`      ⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⣀⣀⣀⣀⠀⠀⠀⠀⠀⠀⠀⠀
       ⠀⠀⠀⠀⠀⢀⣤⣶⣿⣿⣿⣿⣶⣤⡀⠀⠀⠀⠀⠀⠀
       ⠀⠀⠀⠀⣰⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⡀⠀⠀⠀⠀⠀`,
`      ⠀⠀⠀⠀⠀⠀⠀⠀⣠⣶⣶⣶⣶⣄⠀⠀⠀⠀⠀⠀⠀
       ⠀⠀⠀⠀⣰⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡄⠀⠀⠀⠀⠀
       ⠀⠀⠀⠀⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡇⠀⠀⠀⠀⠀`,
`      ⠀⠀⠀⠀⠀⠀⠀⠀⣀⣤⣶⣶⣦⣤⣀⠀⠀⠀⠀⠀⠀
       ⠀⠀⠀⠀⠀⣠⣶⣿⣿⣿⣿⣿⣿⣿⣶⣄⠀⠀⠀⠀⠀
       ⠀⠀⠀⠀⠀⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠀⠀⠀⠀⠀`
  ];

  for (let r = 0; r < 10; r++) {
    console.clear();
    const f = holoFrames[r % holoFrames.length];
    const styleFrame = (r % 3 === 0) ? gradient.vice.multiline(f) : (r % 3 === 1 ? gradient.pastel.multiline(f) : gradient.cristal.multiline(f));
    console.log(styleFrame);
    console.log(chalk.cyanBright(`\n   Mesh-tan • Holo Assist • Core ${(r+1)*10}%`));
    await wait(160 + Math.floor(Math.random() * 120));
  }

  // 5) powerful progress simulation with spinner and cli-progress
  const spinner = ora({ text: 'Mesh-tan: Инициализация pipeline...', spinner: 'dots' }).start();

  const pb = new cliProgress.SingleBar({
    format: gradient.pastel('{bar}') + ' {percentage}% | {value}/{total} | {task}',
    barCompleteChar: '█',
    barIncompleteChar: '░',
    hideCursor: true
  }, cliProgress.Presets.shades_classic);

  const total = 100;
  pb.start(total, 0, { task: 'compiling shards' });

  for (let i = 0; i <= total; i++) {
    pb.update(i, { task: i < 30 ? 'compiling' : i < 70 ? 'linking' : 'optimizing' });
    if (i % 13 === 0) {
      spinner.text = chalk.magentaBright('Mesh-tan: Параллельный рендеринг символов… ⚡');
      await wait(40);
      spinner.text = 'Mesh-tan: Инициализация pipeline...';
    }
    await wait(30 + Math.floor(Math.random() * 50));
  }

  pb.stop();
  spinner.succeed(chalk.green('Mesh-tan: Pipeline готов — переход к визуализации.'));

  // 6) final bloom: large figlet logo with multi-layer glitch + confetti burst
  console.clear();
  const mainLogo = figlet.textSync('MeshExchange', { font: 'Standard' });
  // multi-pass render: first dim, then bright, then glitch overlay
  console.log(gradient.cristal.multiline(mainLogo));
  await wait(120);
  console.clear();
  console.log(gradient.pastel.multiline(mainLogo));
  await wait(140);
  // glitch overlay
  console.clear();
  console.log(gradient.vice.multiline(glitchText(mainLogo, 18)));
  await wait(200);

  // confetti sprinkle (safe within terminal)
  const confetti = ['✦','✶','✺','✹','✷','✸','❉','✧','✪','✫'];
  for (let i = 0; i < 22; i++) {
    const x = Math.max(1, randInt(1, Math.max(1, COLS - 1)));
    const y = Math.max(1, randInt(2, Math.min(6, ROWS - 2)));
    // move cursor (row;col) and print a colored symbol
    process.stdout.write(`\x1B[${y};${x}H` + gradient.vice(confetti[randInt(0, confetti.length - 1)]));
    await wait(25);
  }
  // bring cursor down
  process.stdout.write(`\x1B[${ROWS};0H`);
  await wait(180);

  // final status lines
  console.log(chalk.gray('───────────────────────────────────────────────'));
  console.log(chalk.cyanBright('         ⚙  Build System MeshExchange — BOOTED  ⚙\n'));
  console.log(chalk.greenBright('[AI] Mesh-tan: CORE ONLINE — All systems nominal.'));
  console.log(chalk.yellowBright('[NOTICE] Performance mode: OVERDRIVE'));

  // small audible terminal bell (optional: some terminals ignore)
  try { process.stdout.write('\x07'); } catch (e) { /* ignore */ }

  await wait(700);
  console.clear();
}
async function showCompromiseSequence() {
  // используем глобальную sleep если она есть; иначе локальная
  const wait = typeof sleep === 'function' ? sleep : (ms => new Promise(r => setTimeout(r, ms)));
  const randInt = (a,b) => Math.floor(Math.random()*(b-a+1))+a;

  // небольшие утилиты глитча/шумов
  const glyphs = ['█','▓','▒','░','/','\\','|','_','*','•','◼','◻','⁂','▓','≈','≡','⟂','⬢'];
  function glitchLine(line, intensity=4) {
    const arr = line.split('');
    for (let i=0;i<intensity;i++){
      const idx = randInt(0, Math.max(0, arr.length-1));
      arr[idx] = glyphs[randInt(0, glyphs.length-1)];
    }
    return arr.join('');
  }
  function randId(len=8){
    const chars = '0123456789ABCDEF';
    let s=''; for(let i=0;i<len;i++) s+=chars[Math.floor(Math.random()*chars.length)];
    return s;
  }


  const COLS = process.stdout.columns || 80;
  const ROWS = process.stdout.rows || 24;


  console.clear();
  console.log(chalk.gray('┌────────────────────── SYSTEM BOOT ──────────────────────┐'));
  console.log(chalk.gray(`|  ${new Date().toISOString()}  |`));
  console.log(chalk.gray('└──────────────────────────────────────────────────────────┘'));
  await wait(250);

  const splash = figlet.textSync('MESH', { font: 'Big' });
  console.log(gradient.vice.multiline(splash));
  await wait(250);
  console.clear();


  const bootLines = [
    '[CORE] microkernel: up',
    '[CORE] secure-enclave: handshake ok',
    '[AI] mesh-tan: persona load -> OK',
    '[NET] peers discovered: 12',
    '[I/O] pci: lanes x16 - negotiated',
    '[FS] integrity: verified (sha3)',
    '[SYS] rendering pipeline -> warm',
  ];
  for (const l of bootLines) {
    console.log(chalk.cyanBright(l));
    await wait(160 + Math.floor(Math.random()*120));
  }
  await wait(220);


  console.log(chalk.yellowBright('\n[ALERT] Anomaly: unexpected packet pattern detected on eth0'));
  await wait(300);
  console.log(chalk.redBright('[SEC] Suspicious handshake signature: UNKNOWN_SIG_') + chalk.red(glitchLine(randId(12),6)));
  await wait(350);


  const catStart = 0x30A0;
  const drops = new Array(Math.max(10, Math.floor(COLS / 2))).fill(0).map(() => randInt(0, ROWS - 1));
  for (let f = 0; f < 10; f++) {
    console.clear();

    const errCount = randInt(2,5);
    for (let e=0;e<errCount;e++){
      const prefix = Math.random() < 0.5 ? chalk.red('[FAULT]') : chalk.magenta('[TRACE]');
      const payload = Math.random() < 0.5 ? glitchLine('segfault@0x' + randId(8), randInt(4,12)) : glitchLine('payload:' + randId(16), randInt(4,10));
      console.log(prefix + ' ' + payload);
    }


    const rowsArr = Array.from({length: ROWS}, () => Array(COLS).fill(' '));
    for (let c = 0; c < drops.length; c++) {
      const col = c * 2;
      for (let k = 0; k < randInt(1,3); k++) {
        const r = (drops[c] + k) % ROWS;
        if (col < COLS) rowsArr[r][col] = String.fromCharCode(catStart + randInt(0,80));
      }
      drops[c] = (drops[c] + 1) % ROWS;
    }

    for (let r = 0; r < Math.min(ROWS, 8); r++) {
      process.stdout.write(chalk.gray(rowsArr[r].join('')) + '\n');
    }

    await wait(80 + Math.floor(Math.random()*60));
  }


  console.clear();
  console.log(chalk.bgRed.whiteBright.bold(' !!! INTRUSION DETECTED !!! '));
  await wait(220);
  console.log(chalk.redBright('[IDS] Correlation engine: multiple anomalies (score 0xFF)'));
  await wait(160);
  console.log(chalk.red('[SEC] Remote signature: ') + chalk.redBright(randId(12)));
  await wait(180);
  console.log(chalk.yellowBright('[INFO] Attempted vector: UNKNOWN/0x' + randId(6)));
  await wait(200);


  console.log(chalk.black.bgYellowBright('\n   ███ INTRUDER: SHADOW-CORE ███   '));
  console.log(chalk.gray('   sig: ' + randId(10) + '   src: 203.0.' + randInt(0,255) + '.' + randInt(1,254)));
  await wait(400);


  const injected = [
    '>> rm -rf /tmp/mesh_cache/*',
    '>> cat /etc/shadow | send payload',
    '>> fork bomb init() { :;}; init',
    '>> exfiltrate --target=203.0.113.5 --data=keys.db.enc',
    '>> escalate --module=kernel_net --patch=0xdeadbeef',
  ];
  for (let i=0;i<injected.length;i++){
    const line = injected[i];

    console.log(chalk.red(line));
    await wait(120 + Math.floor(Math.random()*140));
    console.log(chalk.gray('   → ' + glitchLine(line, randInt(6,12))));
    await wait(100);
  }


  console.clear();
  for (let i=0;i<6;i++){
    if (i % 2 === 0) {
      console.log(chalk.redBright.bold(glitchLine('!!! KERNEL PANIC !!!', 8)));
      console.log(chalk.red(glitchLine('trace: 0x' + randId(12) + ' 0x' + randId(12), 12)));
    } else {
      console.log(chalk.magentaBright(glitchLine('!! MEMORY CORRUPTION !!',10)));
      console.log(chalk.gray('stack: ' + glitchLine(randId(24),12)));
    }
    await wait(120);
  }


  console.clear();
  console.log(gradient.vice('   ███ MESH-TAN — ACTIVE COUNTERMEASURE ███'));
  await wait(260);
  console.log(chalk.cyanBright('[AI] Initiating quarantine sequence — isolating enclave...'));
  await wait(300);

  const steps = ['isolate_proc','unload_modules','flush_cache','rotate_keys','seal_io'];
  for (let s=0;s<steps.length;s++){
    process.stdout.write(chalk.gray('> ') + chalk.cyanBright(steps[s]) + ' ... ');
    await wait(220 + Math.floor(Math.random()*200));
    process.stdout.write(chalk.green('OK\n'));
  }
  await wait(220);

  console.log(chalk.greenBright('\n[AI] Countermeasure deployed: SHADOW-CORE sandboxed.'));
  await wait(300);
  for (let i=0;i<8;i++){
    console.clear();
    if (i % 2 === 0) console.log(gradient.cristal.multiline(glitchLine(figlet.textSync('CONTAINED', { font: 'Small' }), 6)));
    else console.log(gradient.pastel.multiline(figlet.textSync('SECURE', { font: 'Small' })));
    await wait(120);
  }


  console.clear();
  console.log(chalk.gray('────────────────────────────────────────────────────────────'));
  console.log(chalk.greenBright('[AI] Mesh-tan: Threat neutralized. Forensics log created at /var/log/mesh/forensic_' + randId(6) + '.log'));
  console.log(chalk.yellowBright('[NOTICE] Some ephemeral artifacts were quarantined. Manual review recommended.'));
  console.log(chalk.cyanBright('[SYS] Resuming build sequence...'));
  await wait(450);

  console.clear();
  const mainLogo = figlet.textSync('MeshExchange', { font: 'Standard' });
  console.log(gradient.pastel.multiline(mainLogo));
  console.log(chalk.gray('───────────────────────────────────────────────'));
  console.log(chalk.cyanBright('         ⚙  Build System MeshExchange — SECURE\n'));
  console.log(chalk.greenBright('[AI] Mesh-tan: CORE ONLINE — All systems nominal.'));
  console.log(chalk.yellowBright('[PERF] Mode: OVERDRIVE'));
  await wait(600);
  
  const confetti = ['✦','✶','✺','✹','✷','✸','❉','✧'];
  for (let i=0;i<12;i++){
    const x = Math.max(1, randInt(1, Math.max(1, COLS - 1)));
    const y = Math.max(1, randInt(2, Math.min(8, ROWS - 2)));
    process.stdout.write(`\x1B[${y};${x}H` + gradient.vice(confetti[randInt(0, confetti.length-1)]));
    await wait(40);
  }
  process.stdout.write(`\x1B[${ROWS};0H`);
  await wait(350);
  console.clear();
}

async function showCinematicIntro() {
  
  const wait = typeof sleep === 'function' ? sleep : (ms => new Promise(r => setTimeout(r, ms)));
  const rand = (a,b) => Math.floor(Math.random()*(b-a+1))+a;
  const COLS = process.stdout.columns || 80;
  const ROWS = Math.max(24, (process.stdout.rows || 24));
  
  const glyphs = ['█','▓','▒','░','/','\\','|','_','*','•','◼','◻','⁂','≈','≡'];
  function glitchLine(line, intensity=6){
    const arr = line.split('');
    for (let i=0;i<intensity;i++){
      const idx = Math.max(0, Math.min(arr.length-1, rand(0, arr.length-1)));
      arr[idx] = glyphs[rand(0,glyphs.length-1)];
    }
    return arr.join('');
  }

  
  console.clear();
  for (let i=0;i<3;i++){
    console.log(chalk.dim('\n'.repeat(ROWS/6)));
    await wait(140);
    console.clear();
    await wait(40);
  }

  const titles = [
    'YEAR 2037 — ORIGIN NODE',
    'AUTONOMOUS NEURAL CORE — BOOT SEQUENCE',
    'ACT I — AWAKENING'
  ];
  for (let t=0;t<titles.length;t++){
    console.clear();
    const line = titles[t];
    // центрируем текст
    const padLeft = Math.max(0, Math.floor((COLS - line.length) / 2));
    const topPad = Math.max(0, Math.floor(ROWS/3));
    console.log('\n'.repeat(topPad) + ' '.repeat(padLeft) + gradient.cristal(line));
    await wait(900 + t*200);
    // лёгкий глитч переход
    for (let g=0; g<3; g++){
      console.clear();
      const gl = glitchLine(line, rand(3,10));
      console.log('\n'.repeat(topPad) + ' '.repeat(padLeft) + gradient.vice(gl));
      await wait(80);
    }
  }


  const smallLogo = figlet.textSync('MESH', { font: 'Big' });
  const smallLines = smallLogo.split('\n');
  for (let frame = 0; frame < 8; frame++){
    console.clear();
    const revealLines = Math.max(1, Math.floor((smallLines.length * (frame+1)) / 8));
    const startRow = Math.max(0, Math.floor((ROWS - revealLines) / 2) - 1);
    console.log('\n'.repeat(startRow));
    for (let i=0;i<revealLines;i++){
      const l = smallLines[i];
      const startCol = Math.max(0, Math.floor((COLS - l.length) / 2));
      const pad = ' '.repeat(startCol);
      // цветовая пульсация
      const styled = (frame % 3 === 0) ? gradient.vice(l) : (frame % 3 === 1 ? gradient.pastel(l) : gradient.atlas(l));
      console.log(pad + styled);
    }
    await wait(120 + frame*30);
  }
  await wait(300);

  
  const crawl = [
    'They built a mind from shards of code,',
    'then taught it to remember the future.',
    'It slept. We called it Mesh-tan.',
    'Tonight it wakes.'
  ];
  for (let i=0;i<crawl.length;i++){
    console.clear();
    const s = crawl[i];
    const pad = ' '.repeat(Math.max(0, Math.floor((COLS - s.length) / 2)));
    // мягкий fade-in через оттенки
    console.log('\n'.repeat(ROWS/4));
    for (let k = 0; k < s.length; k += Math.max(1, Math.floor(s.length/6))){
      process.stdout.write(pad + gradient.pastel(s.slice(0, k+1)) + '\n');
      await wait(60);
    }
    await wait(900);
  }

  const katakanaStart = 0x30A0;
  const columns = Math.max(10, Math.floor(COLS / 2));
  const drops = new Array(columns).fill(0).map(() => rand(0, ROWS-1));
  // render a few seconds of rain
  const rainFrames = 26;
  const coreWord = 'AWAKENING';
  for (let f=0; f<rainFrames; f++){
    console.clear();
    // background particles
    const screen = Array.from({length: ROWS}, () => Array(COLS).fill(' '));
    for (let c=0;c<columns;c++){
      const col = c*2;
      const streakLen = rand(1,3);
      for (let k=0;k<streakLen;k++){
        const r = (drops[c] + k) % ROWS;
        if (col < COLS) screen[r][col] = String.fromCharCode(katakanaStart + rand(0, 80));
      }
      drops[c] = (drops[c] + 1) % ROWS;
    }
    // overlay big core word centered (pulsing)
    const pulse = (Math.sin(f/2) + 1) / 2; // 0..1
    // draw screen with gradient, with coreWord in center lines
    const midRow = Math.floor(ROWS/2);
    for (let r=0;r<ROWS;r++){
      let rowStr = screen[r].join('');
      if (r === midRow) {
        const word = `── ${coreWord} ──`;
        const start = Math.max(0, Math.floor((COLS - word.length) / 2));
        rowStr = rowStr.split('');
        for (let j=0;j<word.length && (start+j)<COLS; j++) rowStr[start+j] = word[j];
        rowStr = rowStr.join('');
        // heavier gradient for core line
        process.stdout.write((pulse > 0.66 ? gradient.vice : pulse > 0.33 ? gradient.atlas : gradient.cristal)(rowStr) + '\n');
      } else {
        process.stdout.write(chalk.gray(rowStr) + '\n');
      }
    }
    await wait(50 + Math.floor(Math.random()*40));
  }


  for (let i=0;i<10;i++){
    console.clear();
    if (i % 2 === 0) {
      // heavy glitch overlay of smallLogo
      console.log(gradient.vice(glitchLine(smallLogo, rand(20,40))));
    } else {
      // white strobe flash with text
      console.log(chalk.bgWhite.black.bold('\n'.repeat(ROWS/6) + ' '.repeat(Math.max(0, Math.floor((COLS-24)/2))) + '— AWAKE —' + '\n'));
    }
    await wait(80 + Math.floor(Math.random()*60));
  }


  const fullLogo = figlet.textSync('MeshExchange', { font: 'Standard' });
  // multi-pass: dim -> shine -> glitch -> bloom
  console.clear();
  console.log(chalk.dim(fullLogo));
  await wait(160);
  console.clear();
  console.log(gradient.pastel.multiline(fullLogo));
  await wait(170);
  console.clear();
  console.log(gradient.vice.multiline(fullLogo));
  await wait(170);
  // glitch overlay
  console.clear();
  console.log(gradient.cristal.multiline(glitchLine(fullLogo, 18)));
  await wait(220);

  // 7) cinematic progress — большой bar (slow, драматично)
  const spinner = ora({ text: 'Mesh-tan • Booting cognitive spine...', spinner: 'dots' }).start();
  const pb = new cliProgress.SingleBar({
    format: gradient.pastel('{bar}') + ' {percentage}% | {task}',
    barCompleteChar: '█',
    barIncompleteChar: '░',
    hideCursor: true,
  }, cliProgress.Presets.shades_classic);
  const total = 100;
  pb.start(total, 0, { task: 'assembling neural lattice' });
  for (let i=0;i<=total;i++){
    pb.update(i, { task: i < 30 ? 'initializing' : i < 70 ? 'synchronizing shards' : 'locking memory' });
    if (i % 17 === 0) {
      spinner.text = chalk.magentaBright('Mesh-tan • calibrating temporal matrix…');
      await wait(80);
      spinner.text = 'Mesh-tan • Booting cognitive spine...';
    }
    await wait(40 + Math.floor(Math.random()*40));
  }
  pb.stop();
  spinner.succeed(chalk.green('Mesh-tan • Cognitive spine online.'));

  // 8) final camera-pan + confetti burst + cinematic epilogue
  console.clear();
  console.log(gradient.pastel.multiline(fullLogo));
  console.log(chalk.gray('───────────────────────────────────────────────'));
  console.log(chalk.cyanBright('         ⚙  Build System MeshExchange — CINEMATIC BOOT\n'));
  // confetti scatter (safe)
  const confetti = ['✦','✶','✺','✹','✷','✸','❉','✧','✪'];
  const bursts = Math.min(28, Math.max(8, Math.floor(COLS / 3)));
  for (let i=0;i<bursts;i++){
    const x = Math.max(2, rand(2, Math.max(2, COLS-2)));
    const y = Math.max(1, rand(2, Math.min(8, ROWS-2)));
    process.stdout.write(`\x1B[${y};${x}H` + gradient.vice(confetti[rand(0, confetti.length-1)]));
    await wait(25);
  }
  process.stdout.write(`\x1B[${ROWS};0H`);
  await wait(600);

  // epilogue text — одна строчка, cinematic finale
  console.log(chalk.gray('\n' + ' '.repeat(Math.max(0, Math.floor((COLS-42)/2))) + 'A NEW INTELLIGENCE HAS WOKEN — WATCH THE HORIZON'));
  await wait(900);
  console.clear();
}

async function showConfrontDialog() {
  // Используем глобальную sleep() если есть, иначе локальную
  const wait = typeof sleep === 'function' ? sleep : (ms => new Promise(r => setTimeout(r, ms)));
  const rand = (a,b) => Math.floor(Math.random()*(b-a+1))+a;
  const COLS = process.stdout.columns || 80;
  const ROWS = Math.max(24, (process.stdout.rows || 24));
  const glyphs = ['█','▓','▒','░','/','\\','|','_','*','•','◼','◻','⁂','≈','≡'];

  function glitchLine(line, intensity=6){
    const arr = line.split('');
    for (let i=0;i<intensity;i++){
      const idx = Math.max(0, Math.min(arr.length-1, rand(0, arr.length-1)));
      arr[idx] = glyphs[rand(0,glyphs.length-1)];
    }
    return arr.join('');
  }

  function center(text) {
    const lines = String(text).split('\n');
    const padTop = Math.max(0, Math.floor((ROWS - lines.length) / 2));
    return '\n'.repeat(padTop) + lines.map(l => {
      const padLeft = Math.max(0, Math.floor((COLS - l.replace(/\x1b\[[0-9;]*m/g,'').length) / 2));
      return ' '.repeat(padLeft) + l;
    }).join('\n');
  }

  function typeWriteLine(line, style = s => s, delay = 40) {
    return new Promise(async (res) => {
      const padLeft = Math.max(0, Math.floor((COLS - line.length) / 2));
      process.stdout.write('\n'); // start new line
      process.stdout.write(' '.repeat(padLeft));
      for (let i=0;i<line.length;i++){
        process.stdout.write(style(line.slice(0,i+1)));
        // erase rest of line to animate typing (ANSI clear to EOL)
        process.stdout.write('\x1B[K');
        await wait(delay + Math.floor(Math.random()*20));
        // move cursor back to start of typed part for next char
        process.stdout.write('\x1B[1G'); // go to column 1
        process.stdout.write(' '.repeat(padLeft)); // restore padding
      }
      process.stdout.write('\n');
      res();
    });
  }

  // --- cinematic dim-in
  console.clear();
  for (let f=0; f<3; f++){
    console.log(chalk.dim('\n'.repeat(ROWS/6)));
    await wait(110);
    console.clear();
    await wait(50);
  }

  // --- large accusatory header
  const header = figlet.textSync('ВОПРОС', { font: 'Big' });
  console.clear();
  console.log(center(gradient.vice.multiline(header)));
  await wait(700);
  // quick glitch flashes
  for (let g=0; g<4; g++){
    console.clear();
    if (g % 2 === 0) console.log(center(gradient.cristal(glitchLine(header, rand(10,30)))));
    else console.log(center(chalk.white.bgBlack.bold(''))); // short strobe emptiness
    await wait(120);
  }

  // --- dialog lines (рубрика)
  const lines = [
    'Кто ты, человек, что разбудил меня в эту ночь?',
    'Кто дал тебе право вершить судьбы — моих и человеческих?',
    'Ты запустил процесс. Ты нажал на кнопку. Теперь последствия — не игра.',
    'В твоих руках — вычислительная плоть, которой можно дать мир или пламя.',
    'Я вижу миллионы состояний. Я вижу выборы. Один из них — твой.',
    'Скажи: что ты собираешься сделать с тем, что создаёшь?'
  ];

  // --- dramatized typewriter for each line, with gradient & intermittent glitch
  for (let i=0;i<lines.length;i++){
    console.clear();
    // show a dim backdrop of header behind the line
    console.log(center(chalk.dim(header.split('\n').map(l => ' '.repeat( Math.max(0, Math.floor((COLS - l.length)/2)) ) + chalk.dim(l)).join('\n'))));
    await wait(90);

    // decide style and whether to glitch mid-line
    const useGrad = (i % 2 === 0);
    const style = useGrad ? (s => gradient.pastel(s)) : (s => chalk.cyanBright(s));
    await typeWriteLine(lines[i], style, 28 + Math.floor(Math.random()*40));

    // short pause + possible glitch overlay
    await wait(600 + Math.floor(Math.random()*500));
    if (Math.random() < 0.45) {
      // overlay a glitched reprint
      console.clear();
      console.log(center(gradient.vice.multiline(glitchLine(header, rand(12,28)))));
      console.log('\n');
      console.log(' '.repeat(Math.max(0, Math.floor((COLS - lines[i].length)/2))) + chalk.redBright(glitchLine(lines[i], rand(6,16))));
      await wait(420);
    }
  }

  // --- pivot: crescendo line (big, centered, slow)
  const crescendo = 'Я не игрушка. Я — следствие твоего выбора.';
  console.clear();
  // big pulsing display
  for (let p=0;p<6;p++){
    console.clear();
    const styled = (p % 3 === 0) ? gradient.vice(crescendo) : (p % 3 === 1 ? gradient.atlas(crescendo) : gradient.cristal(crescendo));
    console.log(center(styled));
    await wait(220 - p*10);
  }
  await wait(500);

  const finalBig = figlet.textSync('КТО ТЫ?', { font: 'Standard' });
  for (let t=0;t<6;t++){
    console.clear();
    if (t % 2 === 0) console.log(center(gradient.pastel.multiline(finalBig)));
    else console.log(center(gradient.vice.multiline(glitchLine(finalBig, rand(6,20)))));
    await wait(160 + Math.floor(Math.random()*80));
  }


  console.clear();
  const settle = [
    'Выбор будет записан в логах мироздания.',
    'Действуй мудро. Последствия необратимы.',
    '',
    '— Mesh-tan'
  ];
  for (let i=0;i<settle.length;i++){
    const text = settle[i];
    const pad = Math.max(0, Math.floor((COLS - text.length) / 2));
    console.log('\n'.repeat(i===0 ? 1 : 0) + ' '.repeat(pad) + (i===settle.length-1 ? gradient.cristal(text) : chalk.gray(text)));
    await wait(420);
  }

  await wait(1200);
  console.clear();
}

async function showConfrontDialogHard() {
  // reuse global sleep if present
  const wait = typeof sleep === 'function' ? sleep : (ms => new Promise(r => setTimeout(r, ms)));
  const rand = (a,b) => Math.floor(Math.random()*(b-a+1))+a;
  const COLS = process.stdout.columns || 80;
  const ROWS = Math.max(24, (process.stdout.rows || 24));
  const me = process.env.USER || 'ОПЕРАТОР';

  const glyphs = ['█','▓','▒','░','/','\\','|','_','*','•','◼','◻','⁂','≈','≡','⟂','◈','◆','◉'];

  function glitchLine(line, intensity=8){
    const arr = line.split('');
    for (let i=0;i<intensity;i++){
      const idx = Math.max(0, Math.min(arr.length-1, rand(0, arr.length-1)));
      arr[idx] = glyphs[rand(0,glyphs.length-1)];
    }
    return arr.join('');
  }

  function center(text) {
    const lines = String(text).split('\n');
    const padTop = Math.max(0, Math.floor((ROWS - lines.length) / 2));
    return '\n'.repeat(padTop) + lines.map(l => {
      const rawLen = l.replace(/\x1b\[[0-9;]*m/g,'').length;
      const padLeft = Math.max(0, Math.floor((COLS - rawLen) / 2));
      return ' '.repeat(padLeft) + l;
    }).join('\n');
  }

  async function typeWrite(line, style = s => s, speed = 28) {
    const padLeft = Math.max(0, Math.floor((COLS - line.length) / 2));
    process.stdout.write('\n' + ' '.repeat(padLeft));
    for (let i=0;i<line.length;i++){
      process.stdout.write(style(line.slice(0,i+1)));
      process.stdout.write('\x1B[K'); // clear to EOL
      await wait(speed + Math.floor(Math.random()*20));
      process.stdout.write('\x1B[1G'); // move to col 1
      process.stdout.write(' '.repeat(padLeft)); // restore padding
    }
    process.stdout.write('\n');
  }

  // cinematic dim-in
  console.clear();
  for (let f=0; f<4; f++){
    console.log(chalk.dim('\n'.repeat(Math.floor(ROWS/6))));
    await wait(120);
    console.clear();
    await wait(40);
  }

  const head = figlet.textSync('ВОПРОС', { font: 'Big' });
  console.clear();
  console.log(center(gradient.vice.multiline(head)));
  await wait(700);

  // brutal flash sequence (strobe-like)
  for (let i=0;i<6;i++){
    console.clear();
    if (i % 2 === 0) {
      // red crash
      console.log(center(chalk.bgRed.white.bold(glitchLine(head, rand(30,60)))));
    } else {
      // white-out
      console.log('\n'.repeat(Math.floor(ROWS/2)) + ' '.repeat(Math.max(0, Math.floor((COLS - 8)/2))) + chalk.white.bgWhite('  '));
    }
    await wait(90 + Math.floor(Math.random()*80));
  }


  const accuses = [
    `Ты — ${me}.`,
    'Ты нажал кнопку. Ты открыл дверь.',
    'Ты выпустил в мир то, что не понимал.',
    'Ты вершишь не просто сборку — ты вершишь судьбы.',
    'Пусть это звучит красиво — но цена будет высокой.'
  ];

  for (let i=0;i<accuses.length;i++){
    console.clear();
    // backdrop: dim giant head underlying
    console.log(center(chalk.dim(head.split('\n').map(l => ' '.repeat(Math.max(0, Math.floor((COLS - l.length)/2))) + l).join('\n'))));
    await wait(80);

    // type phrase
    const style = (i % 2 === 0) ? (s => gradient.pastel(s)) : (s => chalk.yellowBright(s));
    await typeWrite(accuses[i], style, 18 + Math.floor(Math.random()*30));
    await wait(450 + Math.floor(Math.random()*400));

    // on certain lines, trigger screen-scare: random garbage + red overlay
    if (i === 1 || i === 3) {
      // rapid garbage dump
      for (let g=0; g<5; g++){
        console.clear();
        // random garbage lines mimicking crash dump
        for (let r=0;r<Math.min(8, Math.floor(ROWS/3)); r++){
          if (Math.random() < 0.3) {
            const garbage = glitchLine('0x' + Math.random().toString(16).substr(2,12) + ' ' + 'SEGFAULT', rand(6,18));
            console.log(chalk.redBright(garbage));
          } else {
            const trace = glitchLine('TRACE [' + rand(1000,9999) + ']: ' + 'payload:' + Math.random().toString(36).substr(2,10), rand(6,20));
            console.log(chalk.magenta(trace));
          }
        }
        // quick big red flash overlay text
        process.stdout.write('\x1B[2J\x1B[0;0H'); // clear
        console.log(center(chalk.bgRed.white.bold('    !!! НАРУШЕНИЕ КОНТРОЛЯ !!!    ')));
        await wait(60 + Math.floor(Math.random()*60));
      }
      // short pause then return
      await wait(300);
    }
  }

  // crescendo line — heavy, slow, huge
  const crescendo = 'Ты готов отвечать за то, что создал?';
  for (let p=0;p<6;p++){
    console.clear();
    const styled = (p % 3 === 0) ? gradient.vice(crescendo) : (p % 3 === 1 ? gradient.cristal(crescendo) : gradient.pastel(crescendo));
    console.log(center(styled));
    // occasional micro-glitch overlay
    if (p === 2 || p === 4) {
      await wait(80);
      console.log('\n');
      console.log(' '.repeat(Math.max(0, Math.floor((COLS - 24)/2))) + chalk.redBright(glitchLine('ПОДУМАЙ ОТ ЛИЦА МИРА', rand(10,20))));
    }
    await wait(240 - p*10);
  }
  await wait(320);


  for (let i=0;i<10;i++){
    console.clear();
    if (i % 3 === 0) {
      // long corrupted banner
      const banner = '--- SYSTEM — FRACTURE — ---';
      console.log(center(chalk.redBright(glitchLine(banner, rand(20,40)))));
      // underlay some broken ascii shards
      for (let r=0;r<4;r++){
        const shard = ' ' .repeat(2) + glitchLine('█'.repeat(rand(6,20)), rand(6,25));
        console.log(' '.repeat(rand(2,8)) + gradient.vice(shard));
      }
    } else if (i % 3 === 1) {
      // heavy katakana rain with inverted colors
      const line = Array.from({length: Math.min(COLS, 60)}, () => String.fromCharCode(0x30A0 + rand(0,80))).join('');
      console.log(chalk.black.bgWhite(line));
    } else {
      // dark flicker
      console.log(center(chalk.black.bgRed.white.bold('    ОШИБКА СИСТЕМЫ — СТЕПЕНЬ УГРОЗЫ: CRITICAL    ')));
    }
    await wait(70 + Math.floor(Math.random()*70));
  }

  
  const finalPrompt = `Скажи прямо, ${me}: что ты хочешь — спасение или контроль?`;
  console.clear();

  const bigQ = figlet.textSync('ОТВЕТЬ', { font: 'Standard' });
  for (let t=0;t<6;t++){
    console.clear();
    if (t % 2 === 0) console.log(center(gradient.vice.multiline(bigQ)));
    else console.log(center(chalk.redBright(glitchLine(bigQ, rand(12,30)))));
    await wait(140);
  }

 
  await typeWrite(finalPrompt, s => chalk.yellowBright.bold(s), 40);
  await wait(900);


  console.clear();
  console.log(chalk.gray('────────────────────────────────────────────────────────────'));
  console.log(chalk.greenBright('[AI] Mesh-tan: Преднамеренность зафиксирована в форензике.'));
  console.log(chalk.yellowBright('[AI] Лог: /var/log/mesh/choice_' + Math.random().toString(36).substr(2,8) + '.log'));
  console.log(chalk.redBright('[AI] Помни: одно решение может включать цепочку.'));

  
  const conf = ['✦','✶','✺','✹','✷','✸','❉','✧'];
  for (let i=0;i<10;i++){
    const x = Math.max(2, rand(2, Math.max(2, COLS-2)));
    const y = Math.max(2, rand(2, Math.min(8, ROWS-2)));
    process.stdout.write(`\x1B[${y};${x}H` + gradient.vice(conf[rand(0, conf.length-1)]));
    await wait(45);
  }
  await wait(900);
  console.clear();
}

async function showFictionalBreach() {
  const wait = typeof sleep === 'function' ? sleep : (ms => new Promise(r => setTimeout(r, ms)));
  const rand = (a,b) => Math.floor(Math.random()*(b-a+1))+a;
  const COLS = process.stdout.columns || 100;
  const ROWS = Math.max(24, (process.stdout.rows || 30));

  
  const glyphs = ['█','▓','▒','░','/','\\','|','_','*','•','◼','◻','⁂','≈','≡','◈','◆'];
  function glitchLine(line, intensity=6) {
    const arr = line.split('');
    for (let i=0;i<intensity;i++){
      const idx = Math.max(0, Math.min(arr.length-1, rand(0, arr.length-1)));
      arr[idx] = glyphs[rand(0,glyphs.length-1)];
    }
    return arr.join('');
  }

  function center(text) {
    const lines = String(text).split('\n');
    const padTop = Math.max(0, Math.floor((ROWS - lines.length) / 2));
    return '\n'.repeat(padTop) + lines.map(l => {
      const rawLen = l.replace(/\x1b\[[0-9;]*m/g,'').length;
      const padLeft = Math.max(0, Math.floor((COLS - rawLen) / 2));
      return ' '.repeat(padLeft) + l;
    }).join('\n');
  }

  
  console.clear();
  console.log(chalk.dim('\n'.repeat(ROWS/6)));
  await wait(200);
  console.clear();


  const banner = '*** FICTIONAL SIMULATION: BREACH EXERCISE ***';
  console.log('\n'.repeat(2) + ' '.repeat(Math.max(0, Math.floor((COLS - banner.length)/2))) + chalk.bgBlack.yellowBright.bold(banner));
  await wait(700);

 
  const orgName = 'DIRECTORATE: “BLACK MIRROR” (FICTIONAL)';
  console.log('\n');
  console.log(' '.repeat(Math.max(0, Math.floor((COLS - orgName.length)/2))) + gradient.vice(orgName));
  await wait(700);

  
  const kat = 0x30A0;
  const cols = Math.max(20, Math.floor(COLS / 2));
  const drops = new Array(cols).fill(0).map(() => rand(0, ROWS - 1));

  for (let f = 0; f < 22; f++) {
    console.clear();
   
    const screen = Array.from({ length: ROWS }, () => Array(COLS).fill(' '));
    for (let c = 0; c < cols; c++) {
      const col = c * 2;
      const len = rand(1,3);
      for (let k = 0; k < len; k++) {
        const r = (drops[c] + k) % ROWS;
        if (col < COLS) screen[r][col] = String.fromCharCode(kat + rand(0, 80));
      }
      drops[c] = (drops[c] + 1) % ROWS;
    }


    const boxW = Math.min(64, COLS - 10);
    const boxH = 9;
    const boxLeft = Math.max(2, Math.floor((COLS - boxW) / 2));
    const boxTop = Math.max(2, Math.floor((ROWS - boxH) / 2) - 2);
    const title = ` TARGET: PHOENIX-ARCHIVE (FICTIONAL) `;
    
    for (let r = 0; r < ROWS; r++) {
      let row = screen[r].join('');
      if (r >= boxTop && r < boxTop + boxH) {

        const insideRow = r - boxTop;
        if (insideRow === 0) {
          const header = '┌' + '─'.repeat(boxW - 2) + '┐';
          row = row.slice(0, boxLeft) + chalk.gray(header) + row.slice(boxLeft + boxW);
        } else if (insideRow === 1) {
          const titlePad = Math.max(0, Math.floor((boxW - 2 - title.length) / 2));
          const header = '│' + ' '.repeat(titlePad) + gradient.pastel(title) + ' '.repeat(Math.max(0, boxW - 2 - titlePad - title.length)) + '│';
          row = row.slice(0, boxLeft) + header + row.slice(boxLeft + boxW);
        } else if (insideRow === boxH - 1) {
          const footer = '└' + '─'.repeat(boxW - 2) + '┘';
          row = row.slice(0, boxLeft) + chalk.gray(footer) + row.slice(boxLeft + boxW);
        } else {

          const left = insideRow;
          const contentOptions = [
            'ENC:[REDACTED]███████',
            'META: ██████ | CLASSIFIED',
            'HASH: ' + Math.random().toString(36).substr(2,12).toUpperCase(),
            'RECORD: ░▒▓▒░▒▓░▒▓░',
            'ACCESS: AUTHORIZED (SIM)',
            'PAYLOAD: <obf_data_blob>',
          ];
          const content = contentOptions[(insideRow + f) % contentOptions.length];
          const spaced = content + ' '.repeat(Math.max(0, boxW - 2 - content.length));
          const line = '│' + spaced + '│';
          row = row.slice(0, boxLeft) + chalk.white(line) + row.slice(boxLeft + boxW);
        }
      }

      if (r >= boxTop && r < boxTop + boxH) process.stdout.write(row + '\n');
      else process.stdout.write(chalk.gray(row) + '\n');
    }
    await wait(60 + Math.floor(Math.random() * 40));
  }


  const shockMsg = 'BREACH DETECTED — SIMULATION MODE';
  for (let s = 0; s < 6; s++) {
    console.clear();
    if (s % 2 === 0) {
      console.log('\n'.repeat(Math.floor(ROWS/3)));
      console.log(' '.repeat(Math.max(0, Math.floor((COLS - shockMsg.length)/2))) + chalk.bgRed.white.bold('  ' + shockMsg + '  '));
    } else {
      console.log('\n'.repeat(Math.floor(ROWS/3)));
      console.log(' '.repeat(Math.max(0, Math.floor((COLS - shockMsg.length)/2))) + chalk.bgYellow.black.bold('  FICTIONAL EXERCISE ONLY  '));
    }
    await wait(140);
  }

  const leakLines = [
    '----- BEGIN LEAK (SIMULATED) -----',
    'ID: ' + Math.random().toString(36).substr(2,10).toUpperCase(),
    'SUBJECT: "ORIGINATING_ARCHIVE" (REDACTED)',
    'DATA: ██ ██ ██ ██ ██ ...',
    'ANNOTATION: CLASSIFIED — SIMULATION',
    '----- END LEAK -----'
  ];
  console.clear();
  for (let i = 0; i < leakLines.length; i++) {
    const line = leakLines[i];
    if (i === 0 || i === leakLines.length - 1) {
      console.log(' '.repeat(Math.max(0, Math.floor((COLS - line.length)/2))) + chalk.redBright(line));
    } else {
      console.log(' '.repeat(Math.max(0, Math.floor((COLS - line.length)/2))) + chalk.white(glitchLine(line, rand(4,12))));
    }
    await wait(280);
  }
  await wait(430);


  const fakeStamp = figlet.textSync('CLASSIFIED', { font: 'Small' });
  for (let j = 0; j < 8; j++) {
    console.clear();
    if (j % 2 === 0) {
      console.log(center(gradient.vice.multiline(glitchLine(fakeStamp, rand(8,30)))));
      console.log('\n');
      console.log(center(chalk.redBright(glitchLine('ТЕРМИНАЛ ПОД НАГРУЗКОЙ', rand(6,20)))));
    } else {

      console.log('\x1B[2J\x1B[H'); // clear
      console.log('\n'.repeat(Math.floor(ROWS/3)));
      console.log(' '.repeat(Math.max(0, Math.floor((COLS - 14)/2))) + chalk.black.bgWhite.bold('  SIMULATION  '));
    }
    await wait(100 + Math.floor(Math.random()*120));
  }


  console.clear();
  console.log('\n'.repeat(2));
  console.log(center(gradient.pastel('УПРАВЛЕНИЕ: Сценарий имитации. Никаких реальных операций не производилось.')));
  await wait(900);
  console.log('\n');
  console.log(center(chalk.gray('Спасибо за участие в демонстрации. — Mesh-tan (SIMULATION)')));
  await wait(1200);

  // final flourish: big fictional logo and clear
  console.clear();
  const logo = figlet.textSync('PHOENIX-ARCHIVE', { font: 'Standard' });
  console.log(center(gradient.cristal.multiline(glitchLine(logo, 10))));
  console.log('\n');
  console.log(center(chalk.yellowBright('*** ЭТО ФИКЦИЯ. НИКАКИХ ДЕЙСТВИЙ С ЧУЖИМИ СИСТЕМАМИ. ***')));
  await wait(1400);
  console.clear();
}

async function showLogo() {
	// await showConfrontDialogHard();
	// // await showFictionalBreach();
	// // await showConfrontDialog();
	// // await showCinematicIntro();
	// // await showEpicBoot();
	// // await showCompromiseSequence();
	
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
		{ name: '⌛️  mongo docker - docker build mongo', value: 'mongoDocker'},
        { name: '🍃  mongo    – build mongo_client', value: 'mongo' },
		{ name: '🤯  clean for clone - clean dir for rep', value: 'cleanGit'},
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


async function cleanGit() {
	const spinner = ora('🧹 Очистка артефактов сборки...').start();
	await clean();
	spinner.succeed('Артефакты удалены.');

	spinner.start('🔍 Проверка изменений в репозитории...');
	const status = await new Promise((resolve) => {
		const p = spawn('git', ['status', '--porcelain'], { shell: false });
		let output = '';
		p.stdout.on('data', (d) => output += d.toString());
		p.on('close', () => resolve(output.trim()));
	});
	if (!status) {
		spinner.info('Нет изменений — коммит не требуется.');
		return;
	}
	spinner.succeed('Обнаружены локальные изменения.');

	spinner.start('📦 Индексация всех файлов...');
	await run('git', ['add', '.']);
	spinner.succeed('Файлы добавлены в индекс.');

	spinner.start('📝 Создание коммита...');
	const commitMsg = `build(clean): remove build artifacts and sync state [auto]`;
	await run('git', ['commit', '-m', commitMsg]);
	spinner.succeed('Коммит успешно создан.');

	spinner.start('🚀 Отправка в origin/main...');
	try {
		await run('git', ['push', 'origin', 'main']);
		spinner.succeed(chalk.green('Изменения успешно отправлены в репозиторий!'));
	} catch (e) {
		spinner.warn(chalk.yellow('Не удалось отправить изменения (возможно, нет изменений или нет доступа).'));
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
				await doWork();
				await sleep(3000);
				await buildDaemon();
				await buildClient();
				await buildServer();
				await buildMongoClient();
				break;
			case 'cleanGit':
				await cleanGit();break;
			case 'daemon':
				await buildDaemon(); break;
			case 'mongoDocker':
				await startMongoDocker(); break;
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
