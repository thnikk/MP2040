// HotkeysPanel — configurable hotkeys editor for the Settings page.
//
// Each hotkey is a set of keys (key indices, up to 8) held simultaneously that
// triggers one action (SOCD mode, profile switch, macro playback). Combos are
// matched in order, so the first row whose keys are all held wins. The panel
// lists the configured hotkeys as rows: a MultiSelect for the trigger keys, an
// action dropdown, and a remove button. Rows with no keys and no action are
// ignored by getValue().
//
//   new HotkeysPanel({ container, hotkeys, keyOptions, onChange })
//   panel.getValue() -> [{ keys: [index...], action: N }, ...]
//   panel.setKeyOptions(options)   // refresh the trigger-key list
//
// The action values mirror HotkeyAction in proto/enums.proto.

const HOTKEY_ACTIONS = [
  { value: 0, label: 'None' },
  { value: 1, label: 'SOCD: Up Priority' },
  { value: 2, label: 'SOCD: Neutral' },
  { value: 3, label: 'SOCD: Last Input' },
  { value: 4, label: 'SOCD: First Input' },
  { value: 5, label: 'SOCD: Bypass' },
  { value: 6, label: 'Load Profile 1' },
  { value: 7, label: 'Load Profile 2' },
  { value: 8, label: 'Load Profile 3' },
  { value: 9, label: 'Load Profile 4' },
  { value: 10, label: 'Next Profile' },
  { value: 11, label: 'Previous Profile' },
  { value: 12, label: 'Macro 1' },
  { value: 13, label: 'Macro 2' },
  { value: 14, label: 'Macro 3' },
  { value: 15, label: 'Macro 4' },
  { value: 16, label: 'Macro 5' },
  { value: 17, label: 'Macro 6' },
  { value: 18, label: 'Macro 7' },
  { value: 19, label: 'Macro 8' },
  // Only offered for boards with a display (see HotkeysPanel menuToggle).
  { value: 20, label: 'Mini Menu Toggle' },
];

const HK_MAX_HOTKEYS = 16;
const HK_MAX_KEYS = 8;

class HotkeysPanel {
  constructor({ container, hotkeys, keyOptions, menuToggle, onChange }) {
    this.keyOptions = keyOptions || [];
    this.onChange = onChange || (() => {});
    this.rows = [];
    this.actions = menuToggle
      ? HOTKEY_ACTIONS
      : HOTKEY_ACTIONS.filter((o) => o.value !== 20);
    this.buildDom(container);
    this.setValue(hotkeys || []);
  }

  buildDom(container) {
    this.root = document.createElement('div');
    this.root.className = 'hotkeys-panel';

    this.rowsEl = document.createElement('div');
    this.rowsEl.className = 'hotkeys-rows';
    this.root.appendChild(this.rowsEl);

    this.addBtn = document.createElement('button');
    this.addBtn.type = 'button';
    this.addBtn.appendChild(iconSpan('circle-plus'));
    this.addBtn.appendChild(document.createTextNode('Add Hotkey'));
    this.addBtn.addEventListener('click', () => this.addRow());
    this.root.appendChild(this.addBtn);

    container.appendChild(this.root);
  }

  // Canonical form of a combo for duplicate comparison: sorted ids.
  static comboKey(keys) {
    return (keys || []).slice().sort((a, b) => a - b).join(',');
  }

  // Rebuild from a saved hotkeys array. Duplicates are kept and flagged as
  // conflicts (see markConflicts) so imported configs stay editable.
  setValue(hotkeys) {
    this.rowsEl.innerHTML = '';
    this.rows = [];
    for (const hotkey of (hotkeys || [])) {
      this.addRow({
        keys: Array.isArray(hotkey.keys) ? hotkey.keys : [],
        action: Number(hotkey.action) || 0,
      });
    }
    this.markConflicts();
  }

  // Refresh the trigger-key option list (e.g. after an input-mode change).
  setKeyOptions(options) {
    this.keyOptions = options;
    for (const row of this.rows) row.select.setOptions(options);
  }

  // Configured hotkeys only: rows with at least one key or a non-zero action.
  getValue() {
    const out = [];
    for (const row of this.rows) {
      const keys = row.select.getGroupValues('hotkey').slice(0, HK_MAX_KEYS);
      const action = Number(row.actionSelect.value) || 0;
      if (keys.length === 0 && action === 0) continue;
      out.push({ keys, action });
    }
    return out;
  }

  // Complete hotkeys only: rows with keys AND a real action. Incomplete rows
  // are inert (never sent to the firmware); see buildOptionsBody.
  getActiveValue() {
    return this.getValue().filter((h) => h.keys.length > 0 && h.action !== 0);
  }

  addRow(value) {
    if (this.rows.length >= HK_MAX_HOTKEYS) {
      Toast.show(`Max ${HK_MAX_HOTKEYS} hotkeys.`, 'error');
      return;
    }

    const row = {};

    const el = document.createElement('div');
    el.className = 'hotkey-row';

    const keysWrap = document.createElement('div');
    keysWrap.className = 'hotkey-keys';

    row.select = new MultiSelect({
      container: keysWrap,
      groups: [{ id: 'hotkey', label: 'Keys' }],
      options: this.keyOptions,
      onChange: () => this.onRowKeysChanged(row),
    });

    const actionWrap = document.createElement('div');
    actionWrap.className = 'hotkey-action-wrap';
    row.actionSelect = document.createElement('select');
    row.actionSelect.className = 'hotkey-action';
    for (const opt of this.actions) {
      const o = document.createElement('option');
      o.value = opt.value;
      o.textContent = opt.label;
      row.actionSelect.appendChild(o);
    }
    row.actionSelect.addEventListener('change', () => this.notify());
    actionWrap.appendChild(row.actionSelect);

    const removeBtn = document.createElement('button');
    removeBtn.type = 'button';
    removeBtn.className = 'hotkey-remove';
    removeBtn.textContent = '✕';
    removeBtn.setAttribute('aria-label', 'Remove hotkey');
    removeBtn.addEventListener('click', () => this.removeRow(row, el));

    el.appendChild(keysWrap);
    el.appendChild(actionWrap);
    el.appendChild(removeBtn);
    this.rowsEl.appendChild(el);

    this.rows.push(row);
    row.el = el;

    if (value) {
      row.select.setGroupValues('hotkey', Array.isArray(value.keys) ? value.keys : []);
      row.actionSelect.value = String(Number(value.action) || 0);
    }
    this.markIncomplete();

    return row;
  }

  // Rows sharing an exact combo are flagged, not reverted: blocking the pick
  // would punish the natural flow of starting from an overlap and adding a
  // key to differentiate. Firmware matches the first row whose keys are all
  // held, so later duplicates are dead until edited apart.
  onRowKeysChanged(row) {
    this.notify();
  }

  // Indices of rows in an exact-duplicate combo. Only live rows count
  // (keys picked AND a real action): incomplete rows are inert and never
  // reach the firmware, so flagging them would be noise. Partial overlap is
  // legal and never flagged.
  conflictIndices() {
    const seen = new Map();
    const dupes = new Set();
    this.rows.forEach((row, i) => {
      const ck = HotkeysPanel.comboKey(row.select.getGroupValues('hotkey'));
      if (!ck || Number(row.actionSelect.value) === 0) return;
      if (seen.has(ck)) {
        dupes.add(seen.get(ck));
        dupes.add(i);
      } else {
        seen.set(ck, i);
      }
    });
    return dupes;
  }

  hasConflicts() {
    return this.conflictIndices().size > 0;
  }

  markConflicts() {
    const dupes = this.conflictIndices();
    this.rows.forEach((row, i) => {
      const on = dupes.has(i);
      row.el.classList.toggle('conflict', on);
      if (on) row.el.title = 'This combo is already used in another hotkey row';
      else if (!row.el.classList.contains('incomplete')) row.el.removeAttribute('title');
    });
  }

  removeRow(row, el) {
    const index = this.rows.indexOf(row);
    if (index < 0) return;
    this.rows.splice(index, 1);
    el.remove();
    this.notify();
  }

  // Rows that won't be saved (empty, or keys without an action / action
  // without keys) are dimmed so it's legible that they're inert.
  markIncomplete() {
    for (const row of this.rows) {
      const keys = row.select.getGroupValues('hotkey');
      const action = Number(row.actionSelect.value) || 0;
      const incomplete = keys.length === 0 || action === 0;
      row.el.classList.toggle('incomplete', incomplete);
      if (incomplete) row.el.title = 'Pick keys and an action to activate this hotkey';
      else row.el.removeAttribute('title');
    }
  }

  notify() {
    this.markIncomplete();
    this.markConflicts();
    this.onChange(this.getValue());
  }
}
