# Maintainer: local build

pkgname=zafreshgram
pkgver=7.5.9
pkgrel=1
pkgdesc="Telegram Desktop client with Material Design look and AyuGram features"
arch=('x86_64')
url="https://github.com/Snowy-Fluffy/freshGram"
license=('GPL-3.0-only')

depends=(
    'qt6-base'
    'qt6-svg'
    'ffmpeg'
    'openal'
    'openssl'
    'zlib'
    'xz'
    'lz4'
    'xxhash'
    'opus'
    'libvpx'
    'libxkbcommon'
    'minizip'
    'glibmm-2.68'
    'pango'
    'hicolor-icon-theme'
)
makedepends=()
options=('!strip' '!debug')

source=()
sha256sums=()

package() {
    local src="$startdir/out/install"

    if [[ ! -f "$src/usr/local/bin/zaFreshGram" ]]; then
        error "Binary not found: $src/usr/local/bin/zaFreshGram (build the project first)"
        return 1
    fi

    install -Dm755 "$src/usr/local/bin/zaFreshGram" "$pkgdir/usr/bin/zaFreshGram"

    cp -r "$src/usr/local/share" "$pkgdir/usr/"

    sed -i 's|/usr/local/bin/zaFreshGram|/usr/bin/zaFreshGram|' \
        "$pkgdir/usr/share/dbus-1/services/io.github.snowyfluffy.freshgram.service"

    install -Dm644 "$startdir/LICENSE" "$pkgdir/usr/share/licenses/$pkgname/LICENSE"
}
