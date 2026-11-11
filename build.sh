#!/bin/sh
# Arma el sitio publicable en docs/ a partir de app.html y banco.html.
# GitHub Pages publica esa carpeta; servir.ps1 también la sirve en localhost.
cd "$(dirname "$0")"
mkdir -p docs/banco docs/icons

# $1 = fuente, $2 = destino, $3 = prefijo para volver a la raíz del sitio ('' o '../')
wrap() {
  {
    printf '<!doctype html>\n<html lang="es">\n<head>\n<meta charset="utf-8">\n'
    printf '<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">\n'
    printf '<link rel="manifest" href="%smanifest.webmanifest">\n' "$3"
    printf '<meta name="theme-color" content="#141A21">\n'
    printf '<link rel="icon" href="%sicons/icon.svg" type="image/svg+xml">\n' "$3"
    printf '<link rel="apple-touch-icon" href="%sicons/icon-192.png">\n' "$3"
    sed -n '1,/<\/style>/p' "$1"
    printf '</head>\n<body>\n'
    # Los enlaces a las páginas publicadas como Artifact pasan a ser enlaces internos del sitio
    sed '1,/<\/style>/d' "$1" | sed \
      -e "s#https://claude.ai/artifact/2eZQvSY2NMdFqk3ZN9uaBF#${3}banco/#g" \
      -e "s#https://claude.ai/artifact/2Fc9UajSxSaHAJzJCFMLvv#${3}#g"
    printf '<script src="%spwa.js"></script>\n</body>\n</html>\n' "$3"
  } > "$2"
}

wrap app.html docs/index.html ''
wrap banco.html docs/banco/index.html '../'
cp pwa/sw.js pwa/pwa.js pwa/manifest.webmanifest docs/
cp pwa/icons/* docs/icons/
cp puente-ble/puente-ble.ino docs/puente-ble.ino
touch docs/.nojekyll
echo "Sitio listo en docs/"
