// PillSlider — a vanilla port of GP2040-th's PillSlider: a pill-shaped slider
// where the fill shows the value and the label is drawn in two colors via a
// clipped copy over the fill. Drag (mouse) or arrow/Home/End keys to change.

class PillSlider {
  constructor({ container, min, max, label, unit = '', value = min, onChange }) {
    this.min = min;
    this.max = max;
    this.label = label || '';
    this.unit = unit || '';
    this.value = Math.max(min, Math.min(max, value));
    this.onChange = onChange;
    this.dragging = false;
    this.buildDom(container);
  }

  buildDom(container) {
    this.root = document.createElement('div');
    this.root.className = 'pill-slider';
    this.root.setAttribute('role', 'slider');
    this.root.setAttribute('tabindex', '0');
    this.root.setAttribute('aria-valuemin', String(this.min));
    this.root.setAttribute('aria-valuemax', String(this.max));

    this.fill = document.createElement('div');
    this.fill.className = 'pill-slider-fill';

    this.labelEl = document.createElement('span');
    this.labelEl.className = 'pill-slider-label';

    this.labelFill = document.createElement('span');
    this.labelFill.className = 'pill-slider-label-fill';

    this.root.appendChild(this.fill);
    this.root.appendChild(this.labelEl);
    this.root.appendChild(this.labelFill);
    container.appendChild(this.root);

    const onDown = (e) => {
      this.dragging = true;
      this.setValueFromEvent(e);
    };
    const onMove = (e) => {
      if (this.dragging) this.setValueFromEvent(e);
    };
    const onUp = () => {
      if (this.dragging) {
        this.dragging = false;
        if (this.onChange) this.onChange(this.value);
      }
    };

    this.root.addEventListener('mousedown', onDown);
    window.addEventListener('mousemove', onMove);
    window.addEventListener('mouseup', onUp);

    this.root.addEventListener('keydown', (e) => {
      let v = this.value;
      if (e.key === 'ArrowRight' || e.key === 'ArrowUp') v = Math.min(this.max, v + 1);
      else if (e.key === 'ArrowLeft' || e.key === 'ArrowDown') v = Math.max(this.min, v - 1);
      else if (e.key === 'Home') v = this.min;
      else if (e.key === 'End') v = this.max;
      else return;
      e.preventDefault();
      this.value = v;
      this.render();
      if (this.onChange) this.onChange(v);
    });

    this.render();
  }

  setValueFromEvent(e) {
    const rect = this.root.getBoundingClientRect();
    const x = Math.max(0, Math.min(1, (e.clientX - rect.left) / rect.width));
    this.value = Math.round(this.min + x * (this.max - this.min));
    this.render();
  }

  setValue(v) {
    this.value = Math.max(this.min, Math.min(this.max, Math.round(v)));
    this.render();
  }

  getValue() {
    return this.value;
  }

  render() {
    const pct = Math.max(0, Math.min(100, ((this.value - this.min) / (this.max - this.min)) * 100));
    this.fill.style.width = `${pct}%`;
    const text = `${this.label}: ${this.value}${this.unit}`;
    this.labelEl.textContent = text;
    this.labelFill.textContent = text;
    this.labelFill.style.clipPath = `inset(0 ${100 - pct}% 0 0)`;
    this.root.setAttribute('aria-valuenow', String(this.value));
  }
}
