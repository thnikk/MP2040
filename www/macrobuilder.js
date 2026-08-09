// MacroBuilder — visual macro editor for the Settings page.
//
// Eight macro slots (M1-M8). Select a slot with the tabs, then use the
// "Add keys…" button to open a modal with the QMK keyboard. The keyboard
// behaves like the regular key picker: clicking a non-modifier selects it
// (replacing the previous selection) and modifier keys (0xE0-0xE7) toggle
// modifier bits. Press "Add" to append the current key/combo as a step; press
// "Add" repeatedly to keep appending. Each step chip shows its key and
// per-step hold/delay spinners (ms) plus a remove button. A press plays the
// whole macro once; holding the button repeats it until released.
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
    this.onChange = onChange || (() => {});
    this.stepSpinners = [];
    this.dragIndex = -1;
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

  appendStep(keycode, modifiers) {
    const steps = this.macros[this.current].steps;
    if (steps.length >= MB_MAX_STEPS) {
      Toast.show(`Macro M${this.current + 1} is full (${MB_MAX_STEPS} steps).`, 'error');
      return;
    }
    steps.push({ keycode, modifiers, holdMs: 30, delayMs: 10 });
    this.render();
    this.notify();
  }

  removeStep(index) {
    this.macros[this.current].steps.splice(index, 1);
    this.render();
    this.notify();
  }

  // Move the chip being dragged onto another chip's position (drop "over" an
  // item): the dragged chip takes that item's position.
  dropDraggedOn(targetIndex) {
    const steps = this.macros[this.current].steps;
    const from = this.dragIndex;
    this.dragIndex = -1;
    if (from < 0 || from >= steps.length || targetIndex < 0 ||
        targetIndex >= steps.length || from === targetIndex) return;
    const [moved] = steps.splice(from, 1);
    steps.splice(targetIndex, 0, moved);
    this.render();
    this.notify();
  }

  // Insert the dragged chip in a gap, before the item at `insertIndex`
  // (0..steps.length). The index is adjusted for the removal when the chip
  // moves downward.
  insertDraggedAt(insertIndex) {
    const steps = this.macros[this.current].steps;
    const from = this.dragIndex;
    this.dragIndex = -1;
    if (from < 0 || from >= steps.length || insertIndex < 0 ||
        insertIndex > steps.length) return;
    if (insertIndex === from || insertIndex === from + 1) return; // no move
    const [moved] = steps.splice(from, 1);
    let at = insertIndex;
    if (from < at) at -= 1;
    steps.splice(at, 0, moved);
    this.render();
    this.notify();
  }

  clearDragFeedback() {
    if (this.stepsEl) {
      this.stepsEl.querySelectorAll('.macro-step-chip').forEach((c) =>
        c.classList.remove('dragging', 'drag-over', 'insert-before', 'insert-after'));
    }
    this.dropMode = null;
  }

  // Compute the drop target from the pointer Y: inside the top/bottom band of
  // a chip (or in the gap) means "between items" (insert, shown with a line);
  // the middle of a chip means "over the item" (swap, shown with an outline).
  onListDragover(e, list) {
    e.preventDefault();
    if (e.dataTransfer) e.dataTransfer.dropEffect = 'move';
    const chips = [...list.querySelectorAll('.macro-step-chip')];
    chips.forEach((c) => c.classList.remove('drag-over', 'insert-before', 'insert-after'));

    const clientY = e.clientY;
    let index = -1;
    let mode = 'none';
    for (let i = 0; i < chips.length; i++) {
      const rect = chips[i].getBoundingClientRect();
      const band = Math.min(14, rect.height * 0.25);
      if (clientY < rect.top + band) {
        index = i;
        mode = 'insert';
        chips[i].classList.add('insert-before');
        break;
      }
      if (clientY < rect.bottom) {
        index = i;
        mode = 'swap';
        chips[i].classList.add('drag-over');
        break;
      }
    }
    if (mode === 'none') {
      index = chips.length;
      mode = 'insert';
      if (chips.length) chips[chips.length - 1].classList.add('insert-after');
    }
    this.dropMode = { index, mode };
  }

  onListDrop(e) {
    e.preventDefault();
    const dm = this.dropMode;
    this.clearDragFeedback();
    if (!dm || dm.mode === 'none' || dm.index < 0) {
      this.dragIndex = -1;
      return;
    }
    if (dm.mode === 'swap') this.dropDraggedOn(dm.index);
    else this.insertDraggedAt(dm.index);
  }

  clearMacro() {
    this.macros[this.current].steps = [];
    this.render();
    this.notify();
  }

  selectMacro(index) {
    this.current = index;
    this.render();
  }

  // ---- modal keyboard ----------------------------------------------------

  openKeyboardModal() {
    if (!this.modalEl) return;
    document.getElementById('macro-key-modal-title').textContent =
      `Add keys to M${this.current + 1}`;
    this.keyWidget.setValue(0, 0, 0);
    if (this.modalSelect) this.modalSelect.setValue(0, 0, 0);
    this.modalEl.hidden = false;
  }

  closeKeyboardModal() {
    if (this.modalEl) this.modalEl.hidden = true;
  }

  // Append the current selection as a step, then close the modal.
  addSelection() {
    const { keycode, mask } = this.keyWidget.getValue();
    if (!keycode && !mask) {
      Toast.show('Pick a key (and any modifiers) first.', 'error');
      return;
    }
    this.appendStep(keycode, mask);
    this.closeKeyboardModal();
  }

  // ---- rendering --------------------------------------------------------

  renderSteps(container) {
    this.stepSpinners = [];
    container.innerHTML = '';
    const steps = this.macros[this.current].steps;

    const header = document.createElement('div');
    header.className = 'macro-steps-header';
    const count = document.createElement('span');
    count.textContent = `${steps.length} step${steps.length === 1 ? '' : 's'}`;
    header.appendChild(count);
    const headerBtns = document.createElement('div');
    headerBtns.className = 'macro-steps-left';
    const add = document.createElement('button');
    add.type = 'button';
    add.className = 'macro-add-btn';
    add.textContent = 'Add keys…';
    add.addEventListener('click', () => this.openKeyboardModal());
    headerBtns.appendChild(add);
    if (steps.length > 0) {
      const clear = document.createElement('button');
      clear.type = 'button';
      clear.className = 'macro-clear-btn';
      clear.textContent = 'Clear';
      clear.addEventListener('click', () => this.clearMacro());
      headerBtns.appendChild(clear);
    }
    header.appendChild(headerBtns);
    container.appendChild(header);

    const list = document.createElement('div');
    list.className = 'macro-step-list';

    // Empty state: a centered placeholder inside the fixed-height list so the
    // page layout doesn't shift as steps are added.
    if (steps.length === 0) {
      const empty = document.createElement('div');
      empty.className = 'macro-step-list-empty';
      empty.textContent = 'No steps yet — press "Add keys…" to build the sequence.';
      list.appendChild(empty);
      container.appendChild(list);
      return;
    }

    steps.forEach((step, idx) => {
      const chip = document.createElement('div');
      chip.className = 'macro-step-chip';

      // Drag handle at the start of the row: only the handle starts a drag.
      const grip = document.createElement('span');
      grip.className = 'macro-step-grip';
      grip.textContent = '\u2630';
      grip.title = 'Drag to reorder';
      grip.draggable = true;
      grip.addEventListener('dragstart', (e) => {
        this.dragIndex = idx;
        chip.classList.add('dragging');
        if (e.dataTransfer) {
          e.dataTransfer.effectAllowed = 'move';
          e.dataTransfer.setData('text/plain', String(idx));
        }
      });
      grip.addEventListener('dragend', () => {
        this.clearDragFeedback();
        this.dragIndex = -1;
      });
      chip.appendChild(grip);

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
        name: 'macro-hold',
        min: 1,
        max: 5000,
        step: 5,
        value: step.holdMs ?? 30,
        onChange: (v) => { step.holdMs = v; this.notify(); },
      });
      const delay = new Spinner({
        container: delaySpin,
        name: 'macro-delay',
        min: 0,
        max: 5000,
        step: 5,
        value: step.delayMs ?? 10,
        onChange: (v) => { step.delayMs = v; this.notify(); },
      });
      this.stepSpinners.push(hold, delay);
    });
    container.appendChild(list);

    // Drag-and-drop reordering, handled at the list level: dragging starts on
    // a chip's grip; hovering the middle of a chip outlines it for a swap,
    // hovering a gap (chip edges) draws a line for an insert.
    list.addEventListener('dragover', (e) => this.onListDragover(e, list));
    list.addEventListener('drop', (e) => this.onListDrop(e));
    list.addEventListener('dragleave', (e) => {
      if (!list.contains(e.relatedTarget)) this.clearDragFeedback();
    });
  }

  render() {
    this.tabs.forEach((btn, i) => {
      const count = this.macros[i].steps.length;
      btn.classList.toggle('active', i === this.current);
      btn.textContent = `M${i + 1}${count ? ' \u00b7 ' + count : ''}`;
    });
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

    // The key picker lives in its own modal (#macro-key-modal in index.html)
    // and reuses the KeyboardWidget's selection behavior (no macro slots).
    this.modalEl = document.getElementById('macro-key-modal');
    const selectContainer = document.getElementById('macro-key-modal-select');
    const kbContainer = document.getElementById('macro-key-modal-keyboard');
    if (this.modalEl && kbContainer) {
      // MultiSelect (modifiers + keys only — no macro group inside a macro).
      if (selectContainer && typeof MultiSelect !== 'undefined' &&
          typeof MULTISELECT_OPTIONS !== 'undefined' &&
          typeof MULTISELECT_GROUPS !== 'undefined') {
        const msOptions = MULTISELECT_OPTIONS.filter((o) => o.group !== 'macros');
        const msGroups = MULTISELECT_GROUPS.filter((g) => g.id !== 'macros');
        this.modalSelect = new MultiSelect({
          container: selectContainer,
          options: msOptions,
          groups: msGroups,
          onChange: () => {
            const { keycode, mask } = this.modalSelect.getValue();
            this.keyWidget.setValue(keycode, mask, 0);
          },
        });
      }

      this.keyWidget = new KeyboardWidget({
        container: kbContainer,
        keycode: 0,
        mask: 0,
        macroSlots: false,
        onChange: (keycode, mask) => {
          if (this.modalSelect) this.modalSelect.setValue(keycode, mask, 0);
        },
      });

      const closeBtn = document.getElementById('macro-key-modal-close');
      const addBtn = document.getElementById('macro-key-modal-add');
      if (closeBtn) closeBtn.addEventListener('click', () => this.closeKeyboardModal());
      if (addBtn) addBtn.addEventListener('click', () => this.addSelection());
      this.modalEl.addEventListener('click', (e) => {
        if (e.target === this.modalEl) this.closeKeyboardModal();
      });
      document.addEventListener('keydown', (e) => {
        if (e.key === 'Escape' && !this.modalEl.hidden) this.closeKeyboardModal();
      });
    }

    this.stepsEl = document.createElement('div');
    this.stepsEl.className = 'macro-steps';
    this.root.appendChild(this.stepsEl);

    container.appendChild(this.root);
  }
}
