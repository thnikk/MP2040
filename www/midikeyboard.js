// MidiKeyboard — visual piano picker for MIDI notes. Renders a window of
// octaves around the selected octave; every key is clickable. Mirrors
// KeyboardWidget's API (setValue / getValue / onChange).

const MK_NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

// Full octave range shown (C-1..B8 = notes 0..119), so the piano always
// renders complete C..B rows. The top five notes of the MIDI range (C9..G9)
// are skipped.
const MIDI_OCTAVE_MIN = -1;
const MIDI_OCTAVE_MAX = 8;

const MIDI_WHITE_PC = [0, 2, 4, 5, 7, 9, 11]; // C D E F G A B
// Black keys: pitch class + the white-key index each one follows (C# after C,
// D# after D, F# after F, G# after G, A# after A).
const MIDI_BLACK = [
  { pc: 1, after: 0 },
  { pc: 3, after: 1 },
  { pc: 6, after: 3 },
  { pc: 8, after: 4 },
  { pc: 10, after: 5 },
];

// Number of octaves shown (window centered on the selected octave). White keys
// are laid out with flex so they always fill the piano width — no measurement
// needed, no scrolling. Black keys are a narrow fraction of a white key.
const MK_OCTAVE_WINDOW = 3;
const MK_BLACK_RATIO = 0.5;

function mkNoteName(note) {
  return `${MK_NOTE_NAMES[note % 12]}${Math.floor(note / 12) - 1}`;
}

class MidiKeyboard {
  constructor({ container, value, onChange }) {
    this.value = value || 0;
    this.velocity = 0; // 0 = use the global velocity
    this.onChange = onChange || (() => {});
    this.octave = 4;
    this.buildDom(container);
  }

  setValue(note) {
    this.value = note || 0;
    this.render();
  }

  // Per-pin accent velocity (0 = use the global velocity).
  setVelocity(v) {
    this.velocity = v || 0;
    if (this.velocitySpinner) this.velocitySpinner.setValue(this.velocity);
  }

  getValue() {
    return this.value;
  }

  getVelocity() {
    return this.velocity;
  }

  handleKeyClick(note) {
    this.value = note === this.value ? 0 : note; // click again to clear
    this.render();
    this.onChange(this.value);
  }

  makeControl(text, element) {
    const label = document.createElement('label');
    label.className = 'midi-kb-control';
    label.appendChild(document.createTextNode(text));
    if (element) label.appendChild(element);
    return label;
  }

  // White keys are flex items (no inline geometry); black keys get % left/width
  // so they sit centered on the white-key boundaries regardless of piano size.
  makeKey(note, isBlack, leftPct, widthPct) {
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = (isBlack ? 'midi-key-black' : 'midi-key-white')
      + (note === this.value ? ' selected' : '');
    btn.title = mkNoteName(note);
    if (isBlack) {
      btn.style.left = `${leftPct}%`;
      btn.style.width = `${widthPct}%`;
    }
    btn.addEventListener('click', () => this.handleKeyClick(note));

    if (!isBlack) {
      const sub = document.createElement('span');
      sub.className = 'midi-key-note';
      sub.textContent = mkNoteName(note);
      btn.appendChild(sub);
    }
    return btn;
  }

  buildDom(container) {
    this.root = document.createElement('div');
    this.root.className = 'midi-kb';
    container.appendChild(this.root);

    const controls = document.createElement('div');
    controls.className = 'midi-kb-controls';

    this.octaveSpinner = new Spinner({
      container: document.createElement('div'),
      min: MIDI_OCTAVE_MIN,
      max: MIDI_OCTAVE_MAX,
      value: 4,
      onChange: (v) => {
        this.octave = v;
        this.render();
      },
    });
    controls.appendChild(this.makeControl('Octave', this.octaveSpinner.root));

    this.velocitySpinner = new Spinner({
      container: document.createElement('div'),
      min: 0, // 0 = use the global velocity
      max: 127,
      value: 0,
      onChange: (v) => {
        this.velocity = v;
      },
    });
    controls.appendChild(this.makeControl('Velocity', this.velocitySpinner.root));

    this.root.appendChild(controls);

    const status = document.createElement('div');
    status.className = 'midi-kb-status';
    this.statusLabel = document.createElement('span');
    this.statusLabel.className = 'midi-kb-selected';
    status.appendChild(this.statusLabel);
    const clearBtn = document.createElement('button');
    clearBtn.type = 'button';
    clearBtn.className = 'midi-kb-clear';
    clearBtn.textContent = 'None';
    clearBtn.addEventListener('click', () => {
      this.value = 0;
      this.render();
      this.onChange(0);
    });
    status.appendChild(clearBtn);
    this.root.appendChild(status);

    const piano = document.createElement('div');
    piano.className = 'midi-kb-piano';
    this.whites = document.createElement('div');
    this.whites.className = 'midi-kb-whites';
    piano.appendChild(this.whites);
    this.blacks = document.createElement('div');
    this.blacks.className = 'midi-kb-blacks';
    piano.appendChild(this.blacks);
    this.labels = document.createElement('div');
    this.labels.className = 'midi-kb-labels';
    piano.appendChild(this.labels);
    this.root.appendChild(piano);

    this.render();
  }

  render() {
    const n = MK_OCTAVE_WINDOW;
    let start = this.octave - Math.floor((n - 1) / 2);
    let end = start + n - 1;
    if (start < -1) { start = -1; end = start + n - 1; }
    if (end > 8) { end = 8; start = end - n + 1; }
    if (start < -1) start = -1;

    const octaves = [];
    for (let o = start; o <= end; o++) octaves.push(o);

    // White keys share the width equally; black keys are a fraction of one.
    const totalKeys = octaves.length * 7;
    const keyPct = 100 / totalKeys;
    const blackPct = keyPct * MK_BLACK_RATIO;
    const blackHalf = blackPct / 2;

    this.whites.innerHTML = '';
    this.blacks.innerHTML = '';
    this.labels.innerHTML = '';

    octaves.forEach((oo, oi) => {
      const base = (oo + 1) * 12;

      MIDI_WHITE_PC.forEach((pc, wi) => {
        const gi = oi * 7 + wi;
        this.whites.appendChild(this.makeKey(base + pc, false, gi * keyPct, keyPct));
      });
      MIDI_BLACK.forEach(({ pc, after }) => {
        const gi = oi * 7 + after;
        this.blacks.appendChild(this.makeKey(base + pc, true, (gi + 1) * keyPct - blackHalf, blackPct));
      });

      const label = document.createElement('div');
      label.className = 'midi-kb-octave' + (oo === this.octave ? ' current' : '');
      label.textContent = `Octave ${oo}`;
      label.style.left = `${oi * 7 * keyPct}%`;
      label.style.width = `${7 * keyPct}%`;
      this.labels.appendChild(label);
    });

    this.statusLabel.textContent = this.value > 0 ? `Selected: ${mkNoteName(this.value)}` : 'Selected: None';
  }

  // No width measurement is needed anymore, but kept so callers can re-render
  // after layout changes (e.g. the modal opening).
  refresh() {
    this.render();
  }
}
