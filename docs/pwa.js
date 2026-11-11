// Chispa EFI · instalación como app (solo en el sitio publicado o en localhost)
(() => {
  const base = new URL('.', document.currentScript.src);
  const seguro = location.protocol === 'https:' || location.hostname === 'localhost' || location.hostname === '127.0.0.1';
  if ('serviceWorker' in navigator && seguro) {
    window.addEventListener('load', () => {
      navigator.serviceWorker.register(new URL('sw.js', base), { scope: base.pathname }).catch(() => {});
    });
  }
  // Botón "Instalar Chispa como app" (Chrome y Edge lo ofrecen cuando se puede)
  let pedido = null;
  const boton = () => document.getElementById('installBtn');
  window.addEventListener('beforeinstallprompt', e => {
    e.preventDefault();
    pedido = e;
    if (boton()) boton().hidden = false;
  });
  document.addEventListener('click', async e => {
    if (!pedido || !e.target.closest || !e.target.closest('#installBtn')) return;
    pedido.prompt();
    try { await pedido.userChoice; } catch (err) {}
    pedido = null;
    if (boton()) boton().hidden = true;
  });
  window.addEventListener('appinstalled', () => { if (boton()) boton().hidden = true; });
})();
