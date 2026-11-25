// iconpicker.js — per-key Font Awesome icon picker for the color keypad.
//
// The manifest (fontawesome-solid.js) and the WOFF2 are only embedded in the
// ScreenPad firmware image, so the manifest is loaded lazily when the picker
// is first opened.

let iconManifestPromise = null;
let iconPickerBound = false;

function iconManifestLoaded() {
  return typeof FONTAWESOME_SOLID !== 'undefined';
}

function loadIconManifest() {
  if (iconManifestLoaded()) return Promise.resolve(FONTAWESOME_SOLID);
  if (!iconManifestPromise) {
    iconManifestPromise = new Promise((resolve, reject) => {
      const src = typeof assetUrl === 'function'
        ? assetUrl('/js/core/fontawesome-solid.js')
        : '/js/core/fontawesome-solid.js';
      const script = document.createElement('script');
      script.src = src;
      script.onload = () => resolve(iconManifestLoaded() ? FONTAWESOME_SOLID : []);
      script.onerror = () => reject(new Error('icon manifest failed to load'));
      document.head.appendChild(script);
    });
  }
  return iconManifestPromise;
}

function ensureIconIds() {
  const count = (currentOptions?.keycodes || []).length || 12;
  if (!Array.isArray(currentOptions.iconIds)) currentOptions.iconIds = [];
  while (currentOptions.iconIds.length < count) currentOptions.iconIds.push(0);
  return currentOptions.iconIds;
}

// Codepoints that existed in Font Awesome 6 but have no glyph in 7. Treat
// them as "no icon" so an old profile shows its text label instead of tofu.
const REMOVED_SOLID_CODEPOINTS = new Set([0xF5CB]);

function iconPreviewText(codepoint) {
  return codepoint && !REMOVED_SOLID_CODEPOINTS.has(codepoint)
    ? String.fromCodePoint(codepoint) : 'Text';
}

function syncKeyIconSection(pin) {
  const colorKeypad = currentOptions?.display?.colorKeypad === true;
  const wrap = document.getElementById('key-modal-icon');
  if (wrap) wrap.hidden = !colorKeypad;
  if (!colorKeypad) return;
  ensureIconIds();
  const codepoint = Number(currentOptions.iconIds[pin] || 0);
  const preview = document.getElementById('key-icon-preview');
  if (!preview) return;
  preview.textContent = iconPreviewText(codepoint);
  const showGlyph = codepoint > 0 && !REMOVED_SOLID_CODEPOINTS.has(codepoint);
  preview.classList.toggle('fa-glyph', showGlyph);
  preview.classList.toggle('icon-preview-text', !showGlyph);
}

function closeIconPicker() {
  const modal = document.getElementById('icon-modal');
  if (modal) modal.hidden = true;
}

function applyIcon(codepoint) {
  if (editingPin < 0) return;
  ensureIconIds()[editingPin] = codepoint;
  syncKeyIconSection(editingPin);
  if (boardView) boardView.setOptions(currentOptions);
  if (typeof refreshDirtyUi === 'function') refreshDirtyUi();
  closeIconPicker();
}

function renderIconGrid(entries, query) {
  const grid = document.getElementById('icon-grid');
  if (!grid) return;
  const q = (query || '').trim().toLowerCase();
  const frag = document.createDocumentFragment();
  for (const entry of entries) {
    const [label, codepoint, search] = entry;
    if (q && !label.toLowerCase().includes(q) && !(search || '').includes(q)) continue;
    const btn = document.createElement('button');
    btn.type = 'button';
    btn.className = 'icon-grid-btn fa-glyph';
    btn.textContent = String.fromCodePoint(codepoint);
    btn.title = label;
    btn.addEventListener('click', () => applyIcon(codepoint));
    frag.appendChild(btn);
  }
  grid.replaceChildren(frag);
  if (!grid.childElementCount) {
    const empty = document.createElement('p');
    empty.className = 'hint';
    empty.textContent = 'No matching icons.';
    grid.replaceChildren(empty);
  }
}

async function openIconPicker() {
  if (currentOptions?.display?.colorKeypad !== true) return;
  ensureIconIds();
  const modal = document.getElementById('icon-modal');
  const grid = document.getElementById('icon-grid');
  const search = document.getElementById('icon-search');
  if (!modal || !grid || !search) return;
  modal.hidden = false;
  grid.replaceChildren();
  search.value = '';
  try {
    const entries = await loadIconManifest();
    renderIconGrid(entries, '');
    search.oninput = () => renderIconGrid(entries, search.value);
    search.focus();
  } catch (e) {
    grid.textContent = 'Failed to load the icon library.';
  }
}

function initKeyIconSection(display) {
  if (display?.colorKeypad !== true || iconPickerBound) return;
  iconPickerBound = true;
  document.getElementById('key-icon-btn')?.addEventListener('click', openIconPicker);
  document.getElementById('icon-modal-close')?.addEventListener('click', closeIconPicker);
  document.getElementById('icon-modal-cancel')?.addEventListener('click', closeIconPicker);
  document.getElementById('icon-none')?.addEventListener('click', () => applyIcon(0));
  document.getElementById('icon-modal')?.addEventListener('click', (e) => {
    if (e.target?.id === 'icon-modal') closeIconPicker();
  });
}
