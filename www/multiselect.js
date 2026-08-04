// MultiSelect — a vanilla react-select-style multi-select widget.
//
// Options are passed as [{ group, label, value }]. The "modifiers" group allows
// any number of selections; the "keys" group allows at most one (selecting a
// second key replaces the first).

class MultiSelect {
  constructor({ container, options, groups }) {
    this.options = options;
    this.groups = groups;
    this.selected = [];
    this.query = '';
    this.open = false;
    this.visibleRows = [];
    this.highlight = -1;
    this.buildDom(container);
  }

  buildDom(container) {
    this.root = document.createElement('div');
    this.root.className = 'ms';

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

  setValue(keycode, mask) {
    this.selected = [];
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
    this.render();
  }

  getValue() {
    let keycode = 0;
    let mask = 0;
    for (const opt of this.selected) {
      if (opt.group === 'keys') keycode = opt.value;
      else mask |= opt.value;
    }
    return { keycode, mask };
  }

  // ---- Selection --------------------------------------------------------

  isSelected(opt) {
    return this.selected.some((o) => o.group === opt.group && o.value === opt.value);
  }

  toggleOption(value, group) {
    const existing = this.selected.findIndex((o) => o.group === group && o.value === value);
    if (existing >= 0) {
      this.selected.splice(existing, 1);
    } else {
      const opt = this.options.find((o) => o.group === group && o.value === value);
      if (!opt) return;
      if (group === 'keys') {
        this.selected = this.selected.filter((o) => o.group !== 'keys');
      }
      this.selected.push(opt);
    }
    this.render();
  }

  // ---- Rendering --------------------------------------------------------

  render() {
    this.renderTags();
    this.renderList();
  }

  renderTags() {
    this.tagsWrap.innerHTML = '';
    for (const opt of this.selected) {
      const tag = document.createElement('span');
      tag.className = 'ms-tag';
      tag.textContent = opt.label;

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
        row.textContent = opt.label;
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
    this.open ? this.close() : this.openMenu();
  }

  openMenu() {
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
