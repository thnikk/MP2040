// MidiKeyboard — visual piano picker for MIDI notes. Renders the selected
// octave (C..B) with large keys on a narrow piano that fits the modal well;
// every key is clickable. Mirrors KeyboardWidget's API (setValue / getValue /
// onChange).

const MK_NOTE_NAMES = ['C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B'];

// Full octave range the spinner can select (C-1..B8 = notes 0..119). The top
// five notes of the MIDI range (C9..G9) are skipped.
const MIDI_OCTAVE_MIN = -1;
const MIDI_OCTAVE_MAX = 8;

// Black-key pitch classes (not laid out as white keys).
const BLACK_PC = new Set([1, 3, 6, 8, 10]);

// White-key pitch classes that have a black key following them (C D F G A).
const BLACK_AFTER = new Set([0, 2, 5, 7, 9]);

// Black keys are a narrow fraction of a white key.
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
      name: 'midi-octave',
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
      name: 'midi-velocity',
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
    // A single octave: C..B of the selected octave. Bigger keys than the
    // wider two-octave window, and the piano is narrower overall.
    const base = (this.octave + 1) * 12; // C of the selected octave
    const endNote = base + 11;           // B of the selected octave

    // White keys share the width equally; black keys are a fraction of one.
    const pc = (note) => ((note % 12) + 12) % 12;
    const whites = [];
    for (let note = base; note <= endNote; note++) {
      if (BLACK_PC.has(pc(note))) continue;
      whites.push(note);
    }
    const keyPct = 100 / whites.length;
    const blackPct = keyPct * MK_BLACK_RATIO;
    const blackHalf = blackPct / 2;

    this.whites.innerHTML = '';
    this.blacks.innerHTML = '';
    this.labels.innerHTML = '';

    whites.forEach((note, gi) => {
      this.whites.appendChild(this.makeKey(note, false, gi * keyPct, keyPct));
      // A black key sits on the boundary after a C, D, F, G or A white key.
      if (gi + 1 < whites.length && BLACK_AFTER.has(pc(note))) {
        this.blacks.appendChild(
          this.makeKey(note + 1, true, (gi + 1) * keyPct - blackHalf, blackPct));
      }
    });

    // Single centered label for the selected octave; the per-key note names
    // already identify the surrounding notes.
    const label = document.createElement('div');
    label.className = 'midi-kb-octave current';
    label.textContent = `Octave ${this.octave}`;
    label.style.left = '0';
    label.style.width = '100%';
    this.labels.appendChild(label);

    this.statusLabel.textContent = this.value > 0 ? `Selected: ${mkNoteName(this.value)}` : 'Selected: None';
  }

  // No width measurement is needed anymore, but kept so callers can re-render
  // after layout changes (e.g. the modal opening).
  refresh() {
    this.render();
  }
}
