// BootKeysPanel — configurable boot keys editor for the Settings page.
//
// Each boot key maps a key (pin / linear index) held at power-on to an input
// mode to boot into. Rows list the configured keys: a key dropdown, a mode
// dropdown, and a remove button. Rows with no key (pin -1) are ignored by
// getValue(). The selected mode also becomes the persisted default (matches
// GP2040-th).
//
//   new BootKeysPanel({ container, bootKeys, keyOptions, modes, onChange })
//   panel.getValue() -> [{ pin, mode }, ...]
//   panel.setKeyOptions(options)   // refresh the key list
//
// Mode values mirror InputMode in proto/enums.proto.

const BOOT_MODES = [
  { value: 1, label: 'Keyboard' },
  { value: 2, label: 'MIDI' },
  { value: 3, label: 'XInput' },
  { value: 4, label: 'Switch Pro' },
];

const BK_MAX_BOOT_KEYS = 8;

function fillSelect(select, options, value) {
  select.innerHTML = '';
  for (const opt of options) {
    const o = document.createElement('option');
    o.value = opt.value;
    o.textContent = opt.label;
    select.appendChild(o);
  }
  select.value = String(value);
}

class BootKeysPanel {
  constructor({ container, bootKeys, keyOptions, modes, onChange }) {
    this.keyOptions = keyOptions || [];
    this.modes = modes || BOOT_MODES;
    this.onChange = onChange || (() => {});
    this.rows = [];
    this.buildDom(container);
    this.setValue(bootKeys || []);
  }

  buildDom(container) {
    this.root = document.createElement('div');
    this.root.className = 'hotkeys-panel';

    this.rowsEl = document.createElement('div');
    this.rowsEl.className = 'hotkeys-rows';
    this.root.appendChild(this.rowsEl);

    this.addBtn = document.createElement('button');
    this.addBtn.type = 'button';
    this.addBtn.textContent = 'Add Boot Key';
    this.addBtn.addEventListener('click', () => this.addRow());
    this.root.appendChild(this.addBtn);

    container.appendChild(this.root);
  }

  setValue(bootKeys) {
    this.rowsEl.innerHTML = '';
    this.rows = [];
    for (const bk of (bootKeys || [])) {
      this.addRow({ pin: Number(bk.pin) || -1, mode: Number(bk.mode) || 1 });
    }
  }

  setKeyOptions(options) {
    this.keyOptions = options;
    for (const row of this.rows) fillSelect(row.pinSelect, options, row.pinSelect.value);
  }

  // Configured boot keys only: rows with a pin assigned.
  getValue() {
    const out = [];
    for (const row of this.rows) {
      const pin = Number(row.pinSelect.value);
      if (pin < 0) continue;
      out.push({ pin, mode: Number(row.modeSelect.value) });
    }
    return out;
  }

  addRow(value) {
    if (this.rows.length >= BK_MAX_BOOT_KEYS) {
      Toast.show(`Max ${BK_MAX_BOOT_KEYS} boot keys.`, 'error');
      return;
    }

    const row = {};
    const el = document.createElement('div');
    el.className = 'hotkey-row';

    const pinWrap = document.createElement('div');
    pinWrap.className = 'hotkey-keys';
    row.pinSelect = document.createElement('select');
    row.pinSelect.className = 'hotkey-action';
    fillSelect(row.pinSelect, this.keyOptions, value ? value.pin : -1);
    row.pinSelect.addEventListener('change', () => this.notify());
    pinWrap.appendChild(row.pinSelect);

    const modeWrap = document.createElement('div');
    modeWrap.className = 'hotkey-action-wrap';
    row.modeSelect = document.createElement('select');
    row.modeSelect.className = 'hotkey-action';
    for (const m of this.modes) {
      const o = document.createElement('option');
      o.value = m.value;
      o.textContent = m.label;
      row.modeSelect.appendChild(o);
    }
    row.modeSelect.value = String((value && value.mode) || 1);
    row.modeSelect.addEventListener('change', () => this.notify());
    modeWrap.appendChild(row.modeSelect);

    const removeBtn = document.createElement('button');
    removeBtn.type = 'button';
    removeBtn.className = 'hotkey-remove';
    removeBtn.textContent = '✕';
    removeBtn.setAttribute('aria-label', 'Remove boot key');
    removeBtn.addEventListener('click', () => {
      const index = this.rows.indexOf(row);
      if (index < 0) return;
      this.rows.splice(index, 1);
      el.remove();
      this.notify();
    });

    el.appendChild(pinWrap);
    el.appendChild(modeWrap);
    el.appendChild(removeBtn);
    this.rowsEl.appendChild(el);
    this.rows.push(row);
    row.el = el;
  }

  notify() {
    this.onChange(this.getValue());
  }
}