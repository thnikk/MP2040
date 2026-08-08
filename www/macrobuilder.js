// MacroBuilder — visual macro editor for the Settings page.
//
// Eight macro slots (M1-M8). Select a slot with the tabs, then click keys on
// the mini keyboard to append steps in order. Modifier keys (0xE0-0xE7) toggle
// a held-modifier mask that applies to the next appended key (so Ctrl+C is:
// click Ctrl, click C). Each step chip shows its key and per-step hold/delay
// spinners (ms) plus a remove button. Macros play (loop-while-held) when a
// key mapped to them is pressed.
//
//   new MacroBuilder({ container, macros, onChange })
//   builder.getValue() -> [{ steps: [{keycode, modifiers, holdMs, delayMs}] } x8]
//   builder.setValue(macros)

const MB_MACRO_COUNT = 8;
const MB_MAX_STEPS = 32;

class MacroBuilder {
  constructor({ container, macros, onChange }) {
    this.macros = (macros || []).map((m) => ({ steps: (m && m.steps || []).slice() }));
    while (this.macros.length < MB_MACRO_COUNT) this.macros.push({ steps: [] });
    this.current = 0;
    this.modMask = 0;
    this.onChange = onChange || (() => {});
    this.stepSpinners = [];
    this.buildDom(container);
    this.render();
  }

  getValue() {
    return this.macros.map((m) => ({ steps: m.steps.map((s) => ({ ...s })) }));
  }

  setValue(macros) {
    this.macros = (macros || []).map((m) => ({ steps: (m && m.steps || []).slice() }));
    while (this.macros.length < MB_MACRO_COUNT) this.macros.push({ steps: [] });
    this.render();
  }

  notify() {
    this.onChange(this.getValue());
  }

  // ---- editing ----------------------------------------------------------

  appendStep(keycode) {
    const steps = this.macros[this.current].steps;
    if (steps.length >= MB_MAX_STEPS) {
      Toast.show(`Macro M${this.current + 1} is full (${MB_MAX_STEPS} steps).`, 'error');
      return;
    }
    steps.push({
      keycode,
      modifiers: this.modMask,
      holdMs: 30,
      delayMs: 10,
    });
    this.render();
    this.notify();
  }

  removeStep(index) {
    this.macros[this.current].steps.splice(index, 1);
    this.render();
    this.notify();
  }

  clearMacro() {
    this.macros[this.current].steps = [];
    this.render();
    this.notify();
  }

  handleKeyClick(key) {
    if (kbIsModifier(key.value)) {
      this.modMask ^= 1 << (key.value - KB_MODIFIER_MIN);
      this.render();
    } else {
      this.appendStep(key.value);
    }
  }

  selectMacro(index) {
    this.current = index;
    this.modMask = 0;
    this.render();
  }

  // ---- rendering --------------------------------------------------------

  makeKey(key) {
    if (key.spacer) {
      const el = document.createElement('div');
      el.className = key.flex ? 'kb-flex-spacer' : 'kb-spacer ' + kbSizeClass(key.size);
      return el;
    }
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = 'kb-key' + (kbIsModifier(key.value) ? ' mod' : '') + ' ' + kbSizeClass(key.size);
    btn.addEventListener('click', () => this.handleKeyClick(key));
    btn.title = key.label;
    btn.textContent = key.label;
    return btn;
  }

  renderRow(row) {
    const wrap = document.createElement('div');
    wrap.className = 'keyboard-row';
    row.forEach((key) => wrap.appendChild(this.makeKey(key)));
    return wrap;
  }

  renderKeyboard(container) {
    this.keyButtons = [];
    container.innerHTML = '';
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
            if (k.tagName === 'BUTTON') this.keyButtons.push(k);
            group.appendChild(k);
          }
        }
        el.appendChild(group);
        container.appendChild(el);
      } else {
        const rowEl = this.renderRow(row);
        rowEl.querySelectorAll('button').forEach((b) => this.keyButtons.push(b));
        container.appendChild(rowEl);
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
    container.appendChild(clustersRow);

    // Modifier keys are "pressed" while their bit is in the held mask. The
    // layout is walked in the same order the buttons were pushed above, so
    // zipping modifier values against the .mod buttons lines up.
    const mods = [];
    KB_MAIN_ROWS.concat(KB_EXTRA_CLUSTERS.flatMap((c) => c.keys)).forEach((row) => {
      row.forEach((key) => {
        if (!key.spacer && kbIsModifier(key.value)) mods.push(key.value);
      });
    });
    const modBit = (v) => 1 << (v - KB_MODIFIER_MIN);
    let mi = 0;
    this.keyButtons.forEach((btn) => {
      if (!btn.classList.contains('mod')) return;
      btn.classList.toggle('active', Boolean(this.modMask & modBit(mods[mi++])));
    });
  }

  renderSteps(container) {
    this.stepSpinners = [];
    container.innerHTML = '';
    const steps = this.macros[this.current].steps;

    const header = document.createElement('div');
    header.className = 'macro-steps-header';
    const count = document.createElement('span');
    count.textContent = `${steps.length} step${steps.length === 1 ? '' : 's'}`;
    header.appendChild(count);
    if (steps.length > 0) {
      const clear = document.createElement('button');
      clear.type = 'button';
      clear.className = 'macro-clear-btn';
      clear.textContent = 'Clear';
      clear.addEventListener('click', () => this.clearMacro());
      header.appendChild(clear);
    }
    container.appendChild(header);

    if (steps.length === 0) {
      const empty = document.createElement('div');
      empty.className = 'macro-steps-empty';
      empty.textContent = 'Click keys below to add steps (modifier keys are held while on).';
      container.appendChild(empty);
      return;
    }

    const list = document.createElement('div');
    list.className = 'macro-step-list';
    steps.forEach((step, idx) => {
      const chip = document.createElement('div');
      chip.className = 'macro-step-chip';

      const label = document.createElement('span');
      label.className = 'macro-step-label';
      label.textContent = stepLabel(step) || '(empty)';
      chip.appendChild(label);

      const holdWrap = document.createElement('div');
      holdWrap.className = 'macro-step-time';
      const holdLbl = document.createElement('span');
      holdLbl.textContent = 'hold';
      holdWrap.appendChild(holdLbl);
      const holdSpin = document.createElement('div');
      holdWrap.appendChild(holdSpin);
      chip.appendChild(holdWrap);

      const delayWrap = document.createElement('div');
      delayWrap.className = 'macro-step-time';
      const delayLbl = document.createElement('span');
      delayLbl.textContent = 'delay';
      delayWrap.appendChild(delayLbl);
      const delaySpin = document.createElement('div');
      delayWrap.appendChild(delaySpin);
      chip.appendChild(delayWrap);

      const remove = document.createElement('button');
      remove.type = 'button';
      remove.className = 'macro-step-remove';
      remove.title = 'Remove step';
      remove.textContent = '\u00d7';
      remove.addEventListener('click', () => this.removeStep(idx));
      chip.appendChild(remove);

      list.appendChild(chip);

      const hold = new Spinner({
        container: holdSpin,
        min: 1,
        max: 5000,
        step: 5,
        value: step.holdMs ?? 30,
        onChange: (v) => { step.holdMs = v; this.notify(); },
      });
      const delay = new Spinner({
        container: delaySpin,
        min: 0,
        max: 5000,
        step: 5,
        value: step.delayMs ?? 10,
        onChange: (v) => { step.delayMs = v; this.notify(); },
      });
      this.stepSpinners.push(hold, delay);
    });
    container.appendChild(list);
  }

  render() {
    this.tabs.forEach((btn, i) => {
      const count = this.macros[i].steps.length;
      btn.classList.toggle('active', i === this.current);
      btn.textContent = `M${i + 1}${count ? ' · ' + count : ''}`;
    });
    this.renderKeyboard(this.keyboardEl);
    this.renderSteps(this.stepsEl);
  }

  buildDom(container) {
    this.root = document.createElement('div');
    this.root.className = 'macro-builder';

    this.tabs = [];
    const tabs = document.createElement('div');
    tabs.className = 'profile-tabs';
    for (let i = 0; i < MB_MACRO_COUNT; i++) {
      const btn = document.createElement('button');
      btn.type = 'button';
      btn.className = 'profile-tab';
      btn.addEventListener('click', () => this.selectMacro(i));
      this.tabs.push(btn);
      tabs.appendChild(btn);
    }
    this.root.appendChild(tabs);

    const hint = document.createElement('p');
    hint.className = 'hint';
    hint.textContent = 'Select a macro slot, then click keys to build its sequence. Click a modifier key to hold it for the next keys, click again to release.';
    this.root.appendChild(hint);

    this.keyboardEl = document.createElement('div');
    this.keyboardEl.className = 'kb-main-centered macro-keyboard';
    this.root.appendChild(this.keyboardEl);

    this.stepsEl = document.createElement('div');
    this.stepsEl.className = 'macro-steps';
    this.root.appendChild(this.stepsEl);

    container.appendChild(this.root);
  }
}
