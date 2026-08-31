// MultiSelect — a vanilla react-select-style multi-select widget.
//
// Options are passed as [{ group, label, value }]. The "modifiers" group allows
// any number of selections; the "keys" group allows at most one (selecting a
// second key replaces the first).

class MultiSelect {
  constructor({ container, options, groups, onChange, disabled = false }) {
    this.options = options;
    this.groups = groups;
    this.onChange = onChange || (() => {});
    this.disabled = disabled;
    this.selected = [];
    this.query = '';
    this.open = false;
    this.visibleRows = [];
    this.highlight = -1;
    this.buildDom(container);
  }

  buildDom(container) {
    this.root = document.createElement('div');
    this.root.className = 'ms' + (this.disabled ? ' disabled' : '');

    this.control = document.createElement('div');
    this.control.className = 'ms-control';
    this.control.tabIndex = 0;

    this.tagsWrap = document.createElement('div');
    this.tagsWrap.className = 'ms-tags';
    this.placeholder = document.createElement('span');
    this.placeholder.className = 'ms-placeholder';
    this.placeholder.textContent = 'Select…';
    this.tagsWrap.appendChild(this.placeholder);

    this.caret = document.createElement('span');
    this.caret.className = 'ms-caret';
    this.caret.textContent = '\u25be';

    this.control.appendChild(this.tagsWrap);
    this.control.appendChild(this.caret);

    this.menu = document.createElement('div');
    this.menu.className = 'ms-menu';
    this.search = document.createElement('input');
    this.search.className = 'ms-search';
    this.search.name = 'ms-search';
    this.search.setAttribute('aria-label', 'Search options');
    this.search.placeholder = 'Search…';
    this.list = document.createElement('div');
    this.list.className = 'ms-list';
    this.menu.appendChild(this.search);
    this.menu.appendChild(this.list);

    this.root.appendChild(this.control);
    this.root.appendChild(this.menu);
    container.appendChild(this.root);

    // Clicks inside the widget don't reach the document close handler.
    this.root.addEventListener('click', (e) => e.stopPropagation());

    this.control.addEventListener('click', () => this.toggleOpen());
    this.control.addEventListener('keydown', (e) => this.onControlKey(e));

    this.search.addEventListener('input', () => {
      this.query = this.search.value.toLowerCase();
      this.highlight = -1;
      this.renderList();
    });
    this.search.addEventListener('keydown', (e) => this.onSearchKey(e));

    this.list.addEventListener('mousedown', (e) => {
      const row = e.target.closest('.ms-option');
      if (row) {
        e.preventDefault();
        this.toggleOption(Number(row.dataset.value), row.dataset.group);
      }
    });

    document.addEventListener('click', () => this.close());

    this.render();
  }

  // ---- Public API -------------------------------------------------------

  // Swap the option list (e.g. keyboard keys vs gamepad controls) and
  // re-resolve the selection against the new list.
  setOptions(options) {
    this.options = options;
    this.selected = this.selected.filter(
      (o) => this.options.some((opt) => opt.group === o.group && opt.value === o.value));
    this.render();
  }

  setValue(keycode, mask, macroIndex) {
    this.selected = [];
    if (macroIndex > 0) {
      const opt = this.options.find((o) => o.group === 'macros' && o.value === macroIndex);
      if (opt) this.selected.push(opt);
    } else {
      if (keycode > 0) {
        const opt = this.options.find((o) => o.group === 'keys' && o.value === keycode);
        if (opt) this.selected.push(opt);
      }
      for (const bit of [1, 2, 4, 8, 16, 32, 64, 128]) {
        if (mask & bit) {
          const opt = this.options.find((o) => o.group === 'modifiers' && o.value === bit);
          if (opt) this.selected.push(opt);
        }
      }
    }
    this.render();
  }

  // Select every option in `group` whose value bit is set in `mask`. Used by
  // the gamepad picker, where several controls share one per-pin mask.
  setGroupMask(group, mask) {
    this.selected = this.selected.filter((o) => o.group !== group);
    for (const opt of this.options) {
      if (opt.group === group && opt.value !== 0 && (mask & opt.value) === opt.value) {
        this.selected.push(opt);
      }
    }
    this.render();
  }

  // OR of the selected option values in `group` (0 when none selected).
  getGroupMask(group) {
    let mask = 0;
    for (const opt of this.selected) {
      if (opt.group === group) mask |= opt.value;
    }
    return mask;
  }

  // Select every option in `group` whose value appears in `values` (used by
  // pickers that store a plain list of indices, e.g. the menu combo). Values
  // without a matching option are dropped.
  setGroupValues(group, values) {
    this.selected = this.selected.filter((o) => o.group !== group);
    for (const opt of this.options) {
      if (opt.group === group && values.includes(opt.value)) this.selected.push(opt);
    }
    this.render();
  }

  // Values of the selected options in `group` (empty when none selected).
  getGroupValues(group) {
    return this.selected
      .filter((o) => o.group === group)
      .map((o) => o.value);
  }

  getValue() {
    let keycode = 0;
    let mask = 0;
    let macroIndex = 0;
    for (const opt of this.selected) {
      if (opt.group === 'keys') keycode = opt.value;
      else if (opt.group === 'modifiers') mask |= opt.value;
      else if (opt.group === 'macros') macroIndex = opt.value;
    }
    return { keycode, mask, macroIndex };
  }

  // ---- Selection --------------------------------------------------------

  isSelected(opt) {
    return this.selected.some((o) => o.group === opt.group && o.value === opt.value);
  }

  toggleOption(value, group) {
    if (this.disabled) return;
    const existing = this.selected.findIndex((o) => o.group === group && o.value === value);
    if (existing >= 0) {
      this.selected.splice(existing, 1);
    } else {
      const opt = this.options.find((o) => o.group === group && o.value === value);
      if (!opt) return;
      // A key and a macro are mutually exclusive single selects (modifiers
      // stay additive): picking either clears the other. Groups marked
      // `single` in the group list behave the same way within themselves.
      const groupDef = this.groups.find((g) => g.id === group);
      if (group === 'keys' || group === 'macros' || (groupDef && groupDef.single)) {
        this.selected = this.selected.filter(
          (o) => o.group !== 'keys' && o.group !== 'macros' && o.group !== group);
      }
      this.selected.push(opt);
    }
    this.render();
    this.onChange();
  }

  // ---- Rendering --------------------------------------------------------

  // Render an option's label into `el`: the pin label followed by the action
  // in a small pill (when the option carries one), else the plain label text.
  appendOptionLabel(el, opt) {
    if (opt.action) {
      el.appendChild(document.createTextNode(opt.pin));
      const pill = document.createElement('span');
      pill.className = 'ms-pill';
      pill.textContent = opt.action;
      el.appendChild(pill);
    } else {
      el.textContent = opt.label;
    }
  }

  render() {
    this.renderTags();
    this.renderList();
  }

  renderTags() {
    this.tagsWrap.innerHTML = '';
    for (const opt of this.selected) {
      const tag = document.createElement('span');
      tag.className = 'ms-tag';
      this.appendOptionLabel(tag, opt);

      const x = document.createElement('button');
      x.className = 'ms-tag-x';
      x.type = 'button';
      x.textContent = '\u00d7';
      x.setAttribute('aria-label', `Remove ${opt.label}`);
      x.addEventListener('mousedown', (e) => e.preventDefault());
      x.addEventListener('click', (e) => {
        e.stopPropagation();
        this.toggleOption(opt.value, opt.group);
      });

      tag.appendChild(x);
      this.tagsWrap.appendChild(tag);
    }
    if (this.selected.length === 0) {
      this.tagsWrap.appendChild(this.placeholder);
    }
  }

  renderList() {
    this.list.innerHTML = '';
    this.visibleRows = [];
    this.highlight = -1;

    const filtered = this.query
      ? this.options.filter((o) => o.label.toLowerCase().includes(this.query))
      : this.options;

    for (const g of this.groups) {
      const groupOpts = filtered.filter((o) => o.group === g.id);
      if (groupOpts.length === 0) continue;

      const groupEl = document.createElement('div');
      groupEl.className = 'ms-group';

      const groupLabel = document.createElement('div');
      groupLabel.className = 'ms-group-label';
      groupLabel.textContent = g.label;
      groupEl.appendChild(groupLabel);

      for (const opt of groupOpts) {
        const row = document.createElement('div');
        row.className = 'ms-option' + (this.isSelected(opt) ? ' selected' : '');
        row.dataset.value = opt.value;
        row.dataset.group = opt.group;
        this.appendOptionLabel(row, opt);
        groupEl.appendChild(row);
        this.visibleRows.push({ el: row, value: opt.value, group: opt.group });
      }

      this.list.appendChild(groupEl);
    }

    if (this.visibleRows.length === 0) {
      const empty = document.createElement('div');
      empty.className = 'ms-empty';
      empty.textContent = 'No options';
      this.list.appendChild(empty);
    }
  }

  // ---- Open / close -----------------------------------------------------

  toggleOpen() {
    if (this.disabled) return;
    this.open ? this.close() : this.openMenu();
  }

  openMenu() {
    if (this.disabled) return;
    this.open = true;
    this.root.classList.add('open');
    this.query = '';
    this.search.value = '';
    this.highlight = -1;
    this.renderList();
    this.search.focus();
  }

  close() {
    if (!this.open) return;
    this.open = false;
    this.root.classList.remove('open');
  }

  // ---- Keyboard ---------------------------------------------------------

  onControlKey(e) {
    if (this.disabled) return;
    if (e.key === 'ArrowDown' || e.key === 'Enter' || e.key === ' ') {
      e.preventDefault();
      this.openMenu();
    } else if (e.key === 'Escape') {
      this.close();
    }
  }

  onSearchKey(e) {
    if (e.key === 'ArrowDown') {
      e.preventDefault();
      this.moveHighlight(1);
    } else if (e.key === 'ArrowUp') {
      e.preventDefault();
      this.moveHighlight(-1);
    } else if (e.key === 'Enter') {
      e.preventDefault();
      this.selectHighlight();
    } else if (e.key === 'Escape') {
      e.preventDefault();
      this.close();
      this.control.focus();
    }
  }

  moveHighlight(dir) {
    if (this.visibleRows.length === 0) return;
    if (this.highlight >= 0) this.visibleRows[this.highlight].el.classList.remove('highlighted');
    this.highlight = (this.highlight + dir + this.visibleRows.length) % this.visibleRows.length;
    const row = this.visibleRows[this.highlight];
    row.el.classList.add('highlighted');
    row.el.scrollIntoView({ block: 'nearest' });
  }

  selectHighlight() {
    if (this.highlight >= 0 && this.visibleRows[this.highlight]) {
      const row = this.visibleRows[this.highlight];
      this.toggleOption(row.value, row.group);
    }
  }
}
