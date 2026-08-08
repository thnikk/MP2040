// KeyboardWidget — vanilla port of GP2040-th's visual keyboard picker
// (www/src/components/widgets/KeyboardWidget.tsx). Renders a QMK-style
// keyboard; clicking a key selects it as the keycode (click again to clear),
// modifier keys (0xE0-0xE7) toggle modifier bits. A row of macro slots
// (M1-M8) sits below the keyboard: clicking one assigns that macro to the pin
// (exclusive with a normal key selection).
//
// The layout data and label helpers live in kblayout.js.

const KB_MACRO_COUNT = 8;

class KeyboardWidget {
  constructor({ container, keycode, mask, macroIndex, macroSlots, onChange }) {
    this.keycode = keycode || 0;
    this.mask = mask || 0;
    this.macroIndex = macroIndex || 0;
    // The macro slot row is optional (hidden e.g. inside the macro builder's
    // key picker, where M1-M8 have no meaning).
    this.macroSlots = macroSlots !== false;
    this.onChange = onChange || (() => {});
    this.keys = [];
    this.macroButtons = [];
    this.buildDom(container);
    this.render();
  }

  setValue(keycode, mask, macroIndex) {
    this.keycode = keycode || 0;
    this.mask = mask || 0;
    if (macroIndex !== undefined) this.macroIndex = macroIndex || 0;
    this.render();
  }

  getValue() {
    return { keycode: this.keycode, mask: this.mask, macroIndex: this.macroIndex };
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
    // Any key click switches out of macro mode (a pin is either a key or a
    // macro trigger, never both).
    const newMacro = 0;
    this.setValue(newKc, newMask, newMacro);
    this.onChange(newKc, newMask, newMacro);
  }

  handleMacroClick(index) {
    // Clicking the already-selected macro clears it (back to a plain key).
    const newMacro = this.macroIndex === index ? 0 : index;
    this.setValue(0, 0, newMacro);
    this.onChange(0, 0, newMacro);
  }

  isSelected(key) {
    if (kbIsModifier(key.value)) {
      return Boolean(this.mask & (1 << (key.value - KB_MODIFIER_MIN))) || key.value === this.keycode;
    }
    return key.value === this.keycode;
  }

  makeKey(key) {
    const units = key.size ? parseFloat(key.size) : 1;
    if (key.spacer) {
      const el = document.createElement('div');
      if (key.flex) {
        el.className = 'kb-flex-spacer';
      } else {
        el.className = 'kb-spacer ' + kbSizeClass(key.size);
        el.style.flex = units + ' 0 0';
      }
      return el;
    }
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = 'kb-key' + (kbIsModifier(key.value) ? ' mod' : '') + ' ' + kbSizeClass(key.size);
    btn.style.flex = units + ' 0 0';
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

    // Macro slot row: M1-M8. A pin is either a plain key or a macro trigger.
    if (this.macroSlots) {
      const macroRow = document.createElement('div');
      macroRow.className = 'kb-macro-row';
      const macroLabel = document.createElement('div');
      macroLabel.className = 'kb-macro-label';
      macroLabel.textContent = 'Macro';
      macroRow.appendChild(macroLabel);
      for (let i = 1; i <= KB_MACRO_COUNT; i++) {
        const btn = document.createElement('button');
        btn.type = 'button';
        btn.className = 'kb-macro-slot';
        btn.textContent = 'M' + i;
        btn.title = 'Assign macro M' + i + ' (click again to clear)';
        btn.addEventListener('click', () => this.handleMacroClick(i));
        this.macroButtons.push(btn);
        macroRow.appendChild(btn);
      }
      this.root.appendChild(macroRow);
    }
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
        btn.classList.toggle('selected', this.macroIndex === 0 && this.isSelected(key));
      });
    });
    // Macro slots highlight the assigned macro (or none).
    this.macroButtons.forEach((btn, idx) => {
      btn.classList.toggle('selected', this.macroIndex === idx + 1);
    });
  }
}
