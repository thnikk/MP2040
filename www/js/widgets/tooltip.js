// Generic hover tooltip. Any element with a data-tooltip attribute shows a
// bubble below it on hover (GP2040-ce style); hidden on scroll/resize.
(function () {
  let tip = null;

  function show(trigger) {
    hide();
    tip = document.createElement('div');
    tip.className = 'tooltip';
    tip.textContent = trigger.getAttribute('data-tooltip');
    document.body.appendChild(tip);

    const tr = trigger.getBoundingClientRect();
    const tw = tip.offsetWidth;
    const th = tip.offsetHeight;
    const gap = 4;
    let left = tr.left + (tr.width - tw) / 2;
    let top = tr.bottom + gap;
    top = Math.max(gap, Math.min(top, window.innerHeight - th - gap));
    left = Math.max(gap, Math.min(left, window.innerWidth - tw - gap));
    tip.style.left = left + 'px';
    tip.style.top = top + 'px';
  }

  function hide() {
    if (tip) {
      tip.remove();
      tip = null;
    }
  }

  document.addEventListener('mouseover', (e) => {
    const trigger = e.target.closest('[data-tooltip]');
    if (trigger) show(trigger);
  });

  document.addEventListener('mouseout', (e) => {
    const trigger = e.target.closest('[data-tooltip]');
    if (trigger && !trigger.contains(e.relatedTarget)) hide();
  });

  document.addEventListener('click', (e) => {
    const trigger = e.target.closest('[data-tooltip]');
    if (trigger) e.preventDefault();
  });

  window.addEventListener('scroll', hide, { capture: true, passive: true });
  window.addEventListener('resize', hide);
})();
