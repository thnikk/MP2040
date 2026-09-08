// Toast — a vanilla port of GP2040-th's ToastContext: stacked toast
// notifications in the top-right corner. Auto-dismiss after a duration,
// click to dismiss early, exit animation before removal.
//
//   Toast.show('Saved.', 'success');              // variant: success|error|info|warning
//   Toast.show('Reboot failed', 'error', 6000);   // custom duration (ms)
//   Toast.dismiss(id);                            // dismiss early

let container = null;
let nextId = 0;
const toasts = new Map();

function ensureContainer() {
  if (!container) {
    container = document.createElement('div');
    container.className = 'toast-container';
    document.body.appendChild(container);
  }
  return container;
}

function dismiss(id) {
  const toast = toasts.get(id);
  if (!toast) return;
  clearTimeout(toast.timer);
  toast.el.classList.add('exiting');
  setTimeout(() => {
    toast.el.remove();
    toasts.delete(id);
  }, 300);
}

const Toast = {
  show(message, variant = 'success', duration = 4000) {
    const id = ++nextId;
    const el = document.createElement('div');
    el.className = `toast toast-${variant}`;
    el.setAttribute('role', 'alert');
    el.textContent = message;
    el.addEventListener('click', () => dismiss(id));
    ensureContainer().appendChild(el);
    const timer = setTimeout(() => dismiss(id), duration);
    toasts.set(id, { el, timer });
    return id;
  },
  dismiss,
};
