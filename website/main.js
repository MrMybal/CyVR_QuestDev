// Copyright (c) 2026 Cyberalien
// SPDX-License-Identifier: GPL-3.0-only
(() => {
  const $ = selector => document.querySelector(selector);
  const all = selector => [...document.querySelectorAll(selector)];
  let language;
  const copy = () => SHOWCASE.copy[language];
  const viewer = $('#image-viewer');
  const picture = $('#viewer-image');
  let opener, activeShot, scrollBefore = 0, previousOverflow = '';
  const menu = $('.menu-button');

  function localize() {
    const url = new URL(location.href);
    language = url.searchParams.has('lang') ? (url.searchParams.get('lang') === 'fr' ? 'fr' : 'en') : document.documentElement.lang === 'fr' && location.pathname.endsWith('/fr.html') ? 'fr' : 'en';
    if (url.searchParams.has('lang') && url.searchParams.get('lang') !== language) {
      url.searchParams.set('lang', language);
      history.replaceState(null, '', url);
    }
    document.documentElement.lang = language;
    document.title = copy()['meta.title'];
    $('meta[name="description"]').content = copy()['meta.description'];
    $('meta[property="og:title"]').content = copy()['meta.title'];
    $('meta[property="og:description"]').content = copy()['meta.description'];
    all('[data-i18n]').forEach(el => { el.textContent = copy()[el.dataset.i18n]; });
    all('[data-i18n-label]').forEach(el => el.setAttribute('aria-label', copy()[el.dataset.i18nLabel]));
    all('[data-i18n-alt]').forEach(el => { el.alt = copy()[el.dataset.i18nAlt]; });
    all('[data-lang]').forEach(el => el.setAttribute('aria-pressed', String(el.dataset.lang === language)));
    all('[data-shot]').forEach(el => el.setAttribute('aria-label', `${copy()['viewer.open']}: ${copy()[`shot.${el.dataset.shot}.caption`]}`));
    all('[data-doc]').forEach(el => { el.href = `./manual/Documentation.html#/${language}/${el.dataset.doc}`; });
    if (viewer?.open) showImage(activeShot);
  }

  all('[data-lang]').forEach(button => button.addEventListener('click', () => {
    const url = new URL(location.href);
    url.searchParams.set('lang', button.dataset.lang);
    // URL keeps the current chapter and any unrelated query parameters.
    history.pushState(null, '', url);
    localize();
  }));
  window.addEventListener('popstate', localize);

  function closeMenu() {
    if (!menu) return;
    menu.setAttribute('aria-expanded', 'false');
    $('#primary-nav').classList.remove('open');
  }
  menu?.addEventListener('click', () => {
    const open = menu.getAttribute('aria-expanded') !== 'true';
    menu.setAttribute('aria-expanded', String(open));
    $('#primary-nav').classList.toggle('open', open);
  });
  all('#primary-nav a').forEach(link => link.addEventListener('click', closeMenu));
  document.addEventListener('keydown', event => {
    if (event.key === 'Escape' && menu?.getAttribute('aria-expanded') === 'true') {
      closeMenu();
      menu.focus();
    }
  });

  const tabs = all('[data-log-tab]');
  function selectTab(button, focus) {
    tabs.forEach(tab => {
      const selected = tab === button;
      tab.setAttribute('aria-selected', String(selected));
      tab.tabIndex = selected ? 0 : -1;
      document.getElementById(tab.getAttribute('aria-controls')).hidden = !selected;
    });
    if (focus) button.focus();
  }
  tabs.forEach(tab => {
    tab.addEventListener('click', () => selectTab(tab, false));
    tab.addEventListener('keydown', event => {
      if (!['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) return;
      event.preventDefault();
      let index = tabs.indexOf(tab);
      index = event.key === 'Home' ? 0 : event.key === 'End' ? tabs.length - 1 : (index + (event.key === 'ArrowRight' ? 1 : -1) + tabs.length) % tabs.length;
      selectTab(tabs[index], true);
    });
  });

  function visibleShots() {
    return all('[data-shot]').filter(button => !button.closest('[hidden]'));
  }
  function showImage(button) {
    activeShot = button;
    const img = button.querySelector('img');
    picture.src = img.src;
    picture.alt = copy()[`shot.${button.dataset.shot}.alt`];
    $('#viewer-caption').textContent = copy()[`shot.${button.dataset.shot}.caption`];
  }
  function nextImage(step) {
    const shots = visibleShots();
    showImage(shots[(shots.indexOf(activeShot) + step + shots.length) % shots.length]);
  }
  all('[data-shot]').forEach(button => button.addEventListener('click', () => {
    opener = button;
    scrollBefore = window.scrollY;
    previousOverflow = document.body.style.overflow;
    showImage(button);
    viewer.showModal();
    document.body.style.overflow = 'hidden';
    $('[data-viewer-close]').focus();
  }));
  $('[data-viewer-close]')?.addEventListener('click', () => viewer.close());
  $('[data-viewer-prev]')?.addEventListener('click', () => nextImage(-1));
  $('[data-viewer-next]')?.addEventListener('click', () => nextImage(1));
  viewer?.addEventListener('keydown', event => {
    if (event.key === 'ArrowLeft' || event.key === 'ArrowRight') {
      event.preventDefault();
      nextImage(event.key === 'ArrowRight' ? 1 : -1);
    }
    if (event.key === 'Tab') {
      const buttons = [...viewer.querySelectorAll('button')];
      const index = buttons.indexOf(document.activeElement);
      event.preventDefault();
      buttons[(index + (event.shiftKey ? -1 : 1) + buttons.length) % buttons.length].focus();
    }
  });
  viewer?.addEventListener('click', event => {
    const box = viewer.getBoundingClientRect();
    if (event.target === viewer && (event.clientX < box.left || event.clientX > box.right || event.clientY < box.top || event.clientY > box.bottom)) viewer.close();
  });
  viewer?.addEventListener('close', () => {
    document.body.style.overflow = previousOverflow;
    window.scrollTo({top: scrollBefore, behavior: 'instant'});
    opener?.focus({preventScroll: true});
    picture.removeAttribute('src');
  });
  localize();
})();
