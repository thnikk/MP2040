#!/usr/bin/env node
// Screenshot automation for the MP2040 display simulator.
//
// Drives tools/display_sim (the real firmware display code compiled on the
// host) through a set of mini-menu / layout / remap / splash / saver states and
// writes one PNG per state. No board, browser or flash involved.
//
// The simulator is built for a single board config; rebuild it for another
// board first:
//   tools/display_sim/build.sh -b Springboard
//
// Usage:
//   node tools/display-screenshots.mjs                  # default board's binary
//   node tools/display-screenshots.mjs --scale 4        # 4x nearest-neighbour
//   node tools/display-screenshots.mjs --dir out --fg 7fd0ff   # OLED tint
//   node tools/display-screenshots.mjs --sim path/to/mp2040-display-sim
//   node tools/display-screenshots.mjs --size 128x32    # 128x32 panels only
//
// Output: screenshots/display-<state>.png (gitignored).

import { execFileSync } from 'node:child_process';
import { existsSync, mkdirSync } from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import { DISPLAY_STATES } from './display-state-table.mjs';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.resolve(__dirname, '..');

const argv = process.argv.slice(2);
const arg = (flag, dflt) => {
  const i = argv.indexOf(flag);
  return i >= 0 && argv[i + 1] ? argv[i + 1] : dflt;
};

const SIM = path.resolve(ROOT, arg('--sim', 'tools/display_sim/build/mp2040-display-sim'));
const OUT = path.resolve(ROOT, arg('--dir', 'screenshots'));
const SCALE = arg('--scale', '4');
const FG = arg('--fg', null);
const BG = arg('--bg', null);
const SIZE = arg('--size', null); // null = board default

if (!existsSync(SIM)) {
  console.error(`simulator not built: ${SIM}\nrun tools/display_sim/build.sh first`);
  process.exit(1);
}
mkdirSync(OUT, { recursive: true });

const STATES = DISPLAY_STATES;

let count = 0;
for (const [name, state] of STATES) {
  const args = ['--screen', state.screen, '--png', path.join(OUT, `display-${name}.png`), '--scale', String(SCALE)];
  if (state.nav) args.push('--nav', state.nav);
  if (state.saverMode !== undefined) args.push('--saver-mode', String(state.saverMode));
  if (state.timeMs !== undefined) args.push('--time-ms', String(state.timeMs));
  if (state.inputMode) args.push('--input-mode', state.inputMode);
  if (SIZE) args.push('--size', SIZE);
  if (FG) args.push('--fg', FG);
  if (BG) args.push('--bg', BG);
  try {
    const out = execFileSync(SIM, args).toString().trim();
    console.log(`  ${out}`);
    count++;
  } catch (e) {
    console.error(`FAILED ${name}: ${e.message}`);
  }
}
console.log(`done -> ${count}/${STATES.length} states in ${path.relative(ROOT, OUT)}/`);
