// PillToggle — vanilla port of GP2040-th's PillToggle
// (www/src/components/ui/PillToggle.tsx): a pill-shaped on/off button with the
// label rendered inside. Wraps an existing <button> (the label text and any
// info icon are authored in HTML, so tooltips keep working) and tracks its
// checked state via an 'active' class + aria-pressed.
class PillToggle {
  constructor(button, { checked = false, onChange } = {}) {
    this.button = button;
    this.onChange = onChange || (() => {});
    this._checked = null;
    button.type = 'button';
    button.classList.add('pill-toggle');
    button.addEventListener('click', () => this.setChecked(!this.checked));
    this.setChecked(checked, true); // silent initial state
  }

  get checked() {
    return this._checked;
  }

  setChecked(value, silent = false) {
    value = !!value;
    if (value === this._checked) return;
    this._checked = value;
    this.button.classList.toggle('active', value);
    this.button.setAttribute('aria-pressed', String(value));
    if (!silent) this.onChange(value);
  }
}