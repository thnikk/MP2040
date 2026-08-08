// KeyboardWidget — vanilla port of GP2040-th's visual keyboard picker
// (www/src/components/widgets/KeyboardWidget.tsx). Renders a QMK-style
// keyboard; clicking a key selects it as the keycode (click again to clear),
// modifier keys (0xE0-0xE7) toggle modifier bits.

const KB_MODIFIER_MIN = 0xe0;
const KB_MODIFIER_MAX = 0xe7;

const kbIsModifier = (v) => v >= KB_MODIFIER_MIN && v <= KB_MODIFIER_MAX;

const KB_MAIN_ROWS = [
  [
    { label: '', value: 0, size: '1u', spacer: true },
    { label: '', value: 0, flex: true, spacer: true },
    { label: 'F13', value: 0x68 },
    { label: 'F14', value: 0x69 },
    { label: 'F15', value: 0x6a },
    { label: 'F16', value: 0x6b },
    { label: '', value: 0, flex: true, spacer: true },
    { label: 'F17', value: 0x6c },
    { label: 'F18', value: 0x6d },
    { label: 'F19', value: 0x6e },
    { label: 'F20', value: 0x6f },
    { label: '', value: 0, flex: true, spacer: true },
    { label: 'F21', value: 0x70 },
    { label: 'F22', value: 0x71 },
    { label: 'F23', value: 0x72 },
    { label: 'F24', value: 0x73 },
  ],
  [
    { label: 'Esc', value: 0x29 },
    { label: '', value: 0, flex: true, spacer: true },
    { label: 'F1', value: 0x3a },
    { label: 'F2', value: 0x3b },
    { label: 'F3', value: 0x3c },
    { label: 'F4', value: 0x3d },
    { label: '', value: 0, flex: true, spacer: true },
    { label: 'F5', value: 0x3e },
    { label: 'F6', value: 0x3f },
    { label: 'F7', value: 0x40 },
    { label: 'F8', value: 0x41 },
    { label: '', value: 0, flex: true, spacer: true },
    { label: 'F9', value: 0x42 },
    { label: 'F10', value: 0x43 },
    { label: 'F11', value: 0x44 },
    { label: 'F12', value: 0x45 },
  ],
  [
    { label: '`', value: 0x35, sub: '~' },
    { label: '1', value: 0x1e, sub: '!' },
    { label: '2', value: 0x1f, sub: '@' },
    { label: '3', value: 0x20, sub: '#' },
    { label: '4', value: 0x21, sub: '$' },
    { label: '5', value: 0x22, sub: '%' },
    { label: '6', value: 0x23, sub: '^' },
    { label: '7', value: 0x24, sub: '&' },
    { label: '8', value: 0x25, sub: '*' },
    { label: '9', value: 0x26, sub: '(' },
    { label: '0', value: 0x27, sub: ')' },
    { label: '-', value: 0x2d, sub: '_' },
    { label: '=', value: 0x2e, sub: '+' },
    { label: 'Backspace', value: 0x2a, size: '2u' },
  ],
  [
    { label: 'Tab', value: 0x2b, size: '1.5u' },
    { label: 'Q', value: 0x14 },
    { label: 'W', value: 0x1a },
    { label: 'E', value: 0x08 },
    { label: 'R', value: 0x15 },
    { label: 'T', value: 0x17 },
    { label: 'Y', value: 0x1c },
    { label: 'U', value: 0x18 },
    { label: 'I', value: 0x0c },
    { label: 'O', value: 0x12 },
    { label: 'P', value: 0x13 },
    { label: '[', value: 0x2f, sub: '{' },
    { label: ']', value: 0x30, sub: '}' },
    { label: '\\', value: 0x31, size: '1.5u', sub: '|' },
  ],
  [
    { label: 'CapsLock', value: 0x39, size: '1.75u' },
    { label: 'A', value: 0x04 },
    { label: 'S', value: 0x16 },
    { label: 'D', value: 0x07 },
    { label: 'F', value: 0x09 },
    { label: 'G', value: 0x0a },
    { label: 'H', value: 0x0b },
    { label: 'J', value: 0x0d },
    { label: 'K', value: 0x0e },
    { label: 'L', value: 0x0f },
    { label: ';', value: 0x33, sub: ':' },
    { label: "'", value: 0x34, sub: '"' },
    { label: 'Enter', value: 0x28, size: '2.25u' },
  ],
  [
    { label: 'Shift', value: 0xe1, size: '2.25u' },
    { label: 'Z', value: 0x1d },
    { label: 'X', value: 0x1b },
    { label: 'C', value: 0x06 },
    { label: 'V', value: 0x19 },
    { label: 'B', value: 0x05 },
    { label: 'N', value: 0x11 },
    { label: 'M', value: 0x10 },
    { label: ',', value: 0x36, sub: '<' },
    { label: '.', value: 0x37, sub: '>' },
    { label: '/', value: 0x38, sub: '?' },
    { label: 'Shift', value: 0xe5, size: '2.75u' },
  ],
  [
    { label: 'Ctrl', value: 0xe0, size: '1.25u' },
    { label: 'Win', value: 0xe3, size: '1.25u' },
    { label: 'Alt', value: 0xe2, size: '1.25u' },
    { label: 'Space', value: 0x2c, size: '6.25u' },
    { label: 'Alt', value: 0xe6, size: '1.25u' },
    { label: 'Win', value: 0xe7, size: '1.25u' },
    { label: '', value: 0, size: '1.5u', spacer: true },
    { label: 'Ctrl', value: 0xe4, size: '1.25u' },
  ],
];

const KB_EXTRA_CLUSTERS = [
  {
    label: 'Navigation',
    keys: [
      [
        { label: 'Ins', value: 0x49 },
        { label: 'Home', value: 0x4a },
        { label: 'PgUp', value: 0x4b },
      ],
      [
        { label: 'Del', value: 0x4c },
        { label: 'End', value: 0x4d },
        { label: 'PgDn', value: 0x4e },
      ],
    ],
  },
  {
    label: 'Arrows',
    keys: [
      [
        { label: '', value: 0, spacer: true },
        { label: '\u2191', value: 0x52 },
        { label: '', value: 0, spacer: true },
      ],
      [
        { label: '\u2190', value: 0x50 },
        { label: '\u2193', value: 0x51 },
        { label: '\u2192', value: 0x4f },
      ],
    ],
  },
  {
    label: 'Media',
    keys: [
      [
        { label: 'Mute', value: 0xf2 },
        { label: 'Vol-', value: 0xf4 },
        { label: 'Vol+', value: 0xf3 },
        { label: '', value: 0, spacer: true },
      ],
      [
        { label: 'Prev', value: 0xe9 },
        { label: 'Play', value: 0xf1 },
        { label: 'Next', value: 0xe8 },
        { label: 'Stop', value: 0xf0 },
      ],
    ],
  },
  {
    label: 'Mouse',
    keys: [
      [
        { label: 'LMB', value: 0xf5 },
        { label: 'RMB', value: 0xf6 },
        { label: 'MMB', value: 0xf7 },
      ],
      [
        { label: 'Back', value: 0xf8 },
        { label: '', value: 0, spacer: true },
        { label: 'Fwd', value: 0xf9 },
      ],
    ],
  },
];

const kbSizeClass = (s) => (s ? 'sz-' + s.replace('.', '_') : 'sz-1u');

class KeyboardWidget {
  constructor({ container, keycode, mask, onChange }) {
    this.keycode = keycode || 0;
    this.mask = mask || 0;
    this.onChange = onChange || (() => {});
    this.keys = [];
    this.buildDom(container);
    this.render();
  }

  setValue(keycode, mask) {
    this.keycode = keycode || 0;
    this.mask = mask || 0;
    this.render();
  }

  getValue() {
    return { keycode: this.keycode, mask: this.mask };
  }

  handleKeyClick(value) {
    let newKc;
    let newMask;
    if (kbIsModifier(value)) {
      const bit = 1 << (value - KB_MODIFIER_MIN);
      if (value === this.keycode) {
        newKc = 0;
        newMask = this.mask | bit;
      } else {
        newKc = this.keycode;
        newMask = this.mask ^ bit;
      }
    } else {
      newKc = value === this.keycode ? 0 : value;
      newMask = this.mask;
    }
    // Update our own state and highlight before notifying the parent.
    this.keycode = newKc;
    this.mask = newMask;
    this.render();
    this.onChange(newKc, newMask);
  }

  isSelected(key) {
    if (kbIsModifier(key.value)) {
      return Boolean(this.mask & (1 << (key.value - KB_MODIFIER_MIN))) || key.value === this.keycode;
    }
    return key.value === this.keycode;
  }

  makeKey(key) {
    if (key.spacer) {
      const el = document.createElement('div');
      el.className = key.flex ? 'kb-flex-spacer' : 'kb-spacer ' + kbSizeClass(key.size);
      return el;
    }
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = 'kb-key' + (kbIsModifier(key.value) ? ' mod' : '') + ' ' + kbSizeClass(key.size);
    btn.addEventListener('click', () => this.handleKeyClick(key.value));
    btn.title = key.label;

    if (key.sub) {
      const stack = document.createElement('span');
      stack.className = 'kb-key-stack';
      const sub = document.createElement('span');
      sub.className = 'kb-key-sub';
      sub.textContent = key.sub;
      const main = document.createElement('span');
      main.className = 'kb-key-main';
      main.textContent = key.label;
      stack.appendChild(sub);
      stack.appendChild(main);
      btn.appendChild(stack);
    } else {
      btn.textContent = key.label;
    }
    return btn;
  }

  renderRow(row) {
    const wrap = document.createElement('div');
    wrap.className = 'keyboard-row';
    row.forEach((key) => {
      const el = this.makeKey(key);
      if (el.tagName === 'BUTTON') this.keys.push(el);
      wrap.appendChild(el);
    });
    return wrap;
  }

  buildDom(container) {
    this.root = document.createElement('div');
    this.root.className = 'keyboard-widget';
    container.appendChild(this.root);

    this.main = document.createElement('div');
    this.main.className = 'kb-main-centered';
    this.root.appendChild(this.main);

    KB_MAIN_ROWS.forEach((row, ri) => {
      if (ri < 2) {
        const el = document.createElement('div');
        el.className = 'keyboard-row f-row';
        let group = document.createElement('div');
        group.className = 'f-cluster';
        for (const key of row) {
          if (key.flex) {
            el.appendChild(group);
            group = document.createElement('div');
            group.className = 'f-cluster';
          } else {
            const k = this.makeKey(key);
            if (k.tagName === 'BUTTON') this.keys.push(k);
            group.appendChild(k);
          }
        }
        el.appendChild(group);
        this.main.appendChild(el);
      } else {
        this.main.appendChild(this.renderRow(row));
      }
    });

    const clustersRow = document.createElement('div');
    clustersRow.className = 'kb-clusters-row';
    KB_EXTRA_CLUSTERS.forEach((cluster) => {
      const box = document.createElement('div');
      box.className = 'kb-cluster';
      const label = document.createElement('div');
      label.className = 'kb-cluster-label';
      label.textContent = cluster.label;
      box.appendChild(label);
      cluster.keys.forEach((row) => box.appendChild(this.renderRow(row)));
      clustersRow.appendChild(box);
    });
    this.root.appendChild(clustersRow);
  }

  render() {
    // Toggle the selected state without rebuilding the DOM.
    let i = 0;
    const all = KB_MAIN_ROWS.concat(KB_EXTRA_CLUSTERS.flatMap((c) => c.keys));
    all.forEach((row) => {
      row.forEach((key) => {
        if (key.spacer) return;
        const btn = this.keys[i++];
        if (!btn) return;
        btn.classList.toggle('selected', this.isSelected(key));
      });
    });
  }
}
