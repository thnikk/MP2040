// Shared display-sim state table for the screenshot scripts.
//
// Each entry: name -> { screen, nav?, saverMode?, timeMs?, inputMode?, config? }.
// `nav` is replayed as menu/remap actions (up/down/left/right/select/back),
// `timeMs` fixes the virtual clock for animated screens (splash/savers).
export const DISPLAY_STATES = [
  ['splash', { screen: 'splash', timeMs: 0 }],
  ['buttons', { screen: 'buttons' }],
  ['buttons-config', { screen: 'buttons', config: true }],
  ['menu', { screen: 'menu' }],
  ['menu-display', { screen: 'menu', nav: 'down,down,down,down,select' }],
  ['menu-display-idle', { screen: 'menu', nav: 'down,down,down,down,select,select' }],
  ['menu-display-saver', { screen: 'menu', nav: 'down,down,down,down,select,down,select' }],
  ['menu-led', { screen: 'menu', nav: 'down,down,down,down,down,select' }],
  ['menu-led-brightness', { screen: 'menu', nav: 'down,down,down,down,down,select,down,select' }],
  ['menu-led-colors', { screen: 'menu', nav: 'down,down,down,down,down,select,down,down,down,select' }],
  ['menu-led-colors-normal', { screen: 'menu', nav: 'down,down,down,down,down,select,down,down,down,select,select' }],
  ['menu-profiles', { screen: 'menu', nav: 'down,down,down,select' }],
  ['menu-info', { screen: 'menu', nav: 'down,down,down,down,down,down,down,down,select' }],
  ['remap-gamepad', { screen: 'remap', inputMode: 'xinput' }],
  ['remap-keyboard', { screen: 'remap', inputMode: 'keyboard' }],
  ['remap-midi', { screen: 'remap', inputMode: 'midi' }],
  ['saver-snow', { screen: 'saver', saverMode: 1, timeMs: 2000 }],
  ['saver-bounce', { screen: 'saver', saverMode: 2, timeMs: 2000 }],
  ['saver-pipes', { screen: 'saver', saverMode: 3, timeMs: 2000 }],
  ['saver-toast', { screen: 'saver', saverMode: 4, timeMs: 2000 }],
  ['saver-stars', { screen: 'saver', saverMode: 5, timeMs: 2000 }],
];