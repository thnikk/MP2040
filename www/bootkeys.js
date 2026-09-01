// BootKeysPanel — configurable boot keys editor for the Settings page.
//
// Each boot key maps a key (pin / linear index) held at power-on to an input
// mode to boot into. Rows list the configured keys: a pin MultiSelect (single
// pick, so the action pill renders like the hotkey picker), a mode dropdown,
// and a remove button. Rows with no key (empty selection) are ignored by
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
  { value: 5, label: 'Xbox One' },
];

const BK_MAX_BOOT_KEYS = 8;

const BK_GROUP = { id: 'bootkey', label: 'Boot Key', single: true };

class BootKeysPanel {
  constructor({ container, bootKeys, keyOptions, modes, fixedKeys, onChange }) {
    this.keyOptions = keyOptions || [];
    this.modes = modes || BOOT_MODES;
    this.onChange = onChange || (() => {});
    this.rows = [];
    this.fixedKeys = [];
    this.buildDom(container);
    this.setValue(bootKeys || []);
    this.setFixedKeys(fixedKeys || []);
  }

  buildDom(container) {
    this.root = document.createElement('div');
    this.root.className = 'hotkeys-panel';

    // Board-fixed boot pins (web config / USB boot loader) shown as greyed-out
    // (disabled) selects for reference; not editable and hidden when a pin is
    // undefined.
    this.fixedEl = document.createElement('div');
    this.fixedEl.className = 'bootkeys-fixed';
    this.root.appendChild(this.fixedEl);

    this.rowsEl = document.createElement('div');
    this.rowsEl.className = 'hotkeys-rows';
    this.root.appendChild(this.rowsEl);

    this.addBtn = document.createElement('button');
    this.addBtn.type = 'button';
    this.addBtn.appendChild(iconSpan('circle-plus'));
    this.addBtn.appendChild(document.createTextNode('Add Boot Key'));
    this.addBtn.addEventListener('click', () => this.addRow());
    this.root.appendChild(this.addBtn);

    container.appendChild(this.root);
  }

  // Renders the board-fixed pins as disabled versions of the editable rows (pin
  // multi-select + mode select + remove button), greyed out to show they can't
  // be changed. The mode select carries the fixed key's label ("Web Config" /
  // "USB Bootloader"). Rows without a defined pin are not shown at all.
  setFixedKeys(fixedKeys) {
    this.fixedKeys = fixedKeys || [];
    this.fixedEl.innerHTML = '';
    for (const fk of this.fixedKeys) {
      if (Number(fk.pin) < 0) continue;

      const row = document.createElement('div');
      row.className = 'hotkey-row bootkey-fixed-row';

      const keysWrap = document.createElement('div');
      keysWrap.className = 'hotkey-keys';
      const pinSelect = new MultiSelect({
        container: keysWrap,
        groups: [BK_GROUP],
        options: this.keyOptions,
        disabled: true,
      });
      pinSelect.setGroupValues('bootkey', [Number(fk.pin)]);

      const modeWrap = document.createElement('div');
      modeWrap.className = 'hotkey-action-wrap';
      const modeSelect = document.createElement('select');
      modeSelect.className = 'hotkey-action';
      modeSelect.disabled = true;
      const modeOpt = document.createElement('option');
      modeOpt.textContent = fk.label;
      modeSelect.appendChild(modeOpt);
      modeWrap.appendChild(modeSelect);

      const removeBtn = document.createElement('button');
      removeBtn.type = 'button';
      removeBtn.className = 'hotkey-remove';
      removeBtn.disabled = true;
      removeBtn.textContent = '✕';
      removeBtn.setAttribute('aria-label', fk.label);

      row.appendChild(keysWrap);
      row.appendChild(modeWrap);
      row.appendChild(removeBtn);
      this.fixedEl.appendChild(row);
    }
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
    for (const row of this.rows) row.select.setOptions(options);
    // Fixed row labels can change with the input mode too.
    this.setFixedKeys(this.fixedKeys);
  }

  // Configured boot keys only: rows with a pin assigned.
  getValue() {
    const out = [];
    for (const row of this.rows) {
      const pins = row.select.getGroupValues('bootkey');
      const pin = pins.length ? Number(pins[0]) : -1;
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
    row.select = new MultiSelect({
      container: pinWrap,
      groups: [BK_GROUP],
      options: this.keyOptions,
      onChange: () => this.notify(),
    });
    if (value && value.pin >= 0) row.select.setGroupValues('bootkey', [value.pin]);

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