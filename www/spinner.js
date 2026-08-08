// Spinner — a compact numeric input: a text field with a "-" button on the
// left and a "+" button on the right. Values clamp to [min, max].
//
//   new Spinner({ container, min, max, value, onChange, step })
//   spinner.getValue() / setValue(v)
// `step` (default 1) is the increment applied by the -/+ buttons; typed
// values always clamp to [min, max].

class Spinner {
  constructor({ container, min, max, value, onChange, step }) {
    this.min = min;
    this.max = max;
    this.value = this.clamp(value == null ? min : value);
    this.stepSize = step || 1;
    this.onChange = onChange || (() => {});
    this.buildDom(container);
    this.input.value = String(this.value);
  }

  clamp(v) {
    return Math.max(this.min, Math.min(this.max, v));
  }

  getValue() {
    return this.value;
  }

  setValue(v) {
    this.value = this.clamp(v);
    if (this.input) this.input.value = String(this.value);
  }

  // Value shown in the field, even if it hasn't been committed yet.
  readInput() {
    const v = parseInt(this.input.value, 10);
    return Number.isNaN(v) ? this.value : v;
  }

  step(delta) {
    const next = this.clamp(this.readInput() + delta * this.stepSize);
    if (next === this.value && String(next) === this.input.value) return;
    this.setValue(next);
    this.onChange(this.value);
  }

  commit() {
    const next = this.clamp(this.readInput());
    if (next !== this.value) {
      this.value = next;
      this.onChange(this.value);
    }
    this.input.value = String(this.value);
  }

  buildDom(container) {
    this.root = document.createElement('div');
    this.root.className = 'spinner';

    const minus = document.createElement('button');
    minus.type = 'button';
    minus.className = 'spinner-btn';
    minus.textContent = '\u2212';
    minus.addEventListener('click', () => this.step(-1));

    this.input = document.createElement('input');
    this.input.type = 'text';
    this.input.className = 'spinner-input';
    this.input.addEventListener('change', () => this.commit());
    this.input.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') this.commit();
      if (e.key === 'ArrowUp') { e.preventDefault(); this.step(1); }
      if (e.key === 'ArrowDown') { e.preventDefault(); this.step(-1); }
    });

    const plus = document.createElement('button');
    plus.type = 'button';
    plus.className = 'spinner-btn';
    plus.textContent = '+';
    plus.addEventListener('click', () => this.step(1));

    this.root.appendChild(minus);
    this.root.appendChild(this.input);
    this.root.appendChild(plus);
    container.appendChild(this.root);
  }
}
