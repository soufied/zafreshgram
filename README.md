# zaFreshGram

zaFreshGram is a desktop Telegram client based on Telegram Desktop. It is a fork of [freshGram](https://github.com/Snowy-Fluffy/freshGram) with improvements from [ZaStoGram](https://cwdnvtsqdc.zapret.moe/zastogram/ZaStoGram_desktop):

Made for myself, so don’t expect fixes or other support.

Based on 5.2.9 beta | commit - daccd7e77579a4f946d30de35ea642a60d9231e1

- WSS support.
- MTProto improvements.
- An updated WEB Proxy.
- Material Design look and AyuGram features.

## Build

The Linux build runs inside a Docker container and produces a stripped binary. The script then packages it with `makepkg` and installs it as an Arch Linux package. The script was written for Arch-based systems such as CachyOS.

### Requirements

- Git
- Docker
- `base-devel` (provides `makepkg`)
- Internet access for the first build, which downloads the libraries

### 1. Clone the repository with submodules

The project depends on many git submodules, so clone it recursively:

```bash
git clone --recursive https://github.com/soufied/zafreshgram.git zafreshgram
cd zafreshgram
```

If you already cloned the repository without `--recursive`, run:

```bash
git submodule update --init --recursive
```

### 2. Get Telegram API credentials

Every Telegram client needs its own `api_id` and `api_hash`. Get them at [my.telegram.org](https://my.telegram.org). See [docs/api_credentials.md](docs/api_credentials.md) for details.

Set them at the top of `build_zafreshgram.sh`:

```bash
API_ID="your_api_id"
API_HASH="your_api_hash"
```

The script ships with default values. Replace them with your own credentials and do not publish real values in a public repository.

### 3. Run the build

Do not run the script as root, because `makepkg` refuses to run as root.

```bash
./build_zafreshgram.sh
```

The script does the following:

1. Builds the Docker image `zafreshgram:centos_env`. This happens only once, unless you pass `--rebuild-image`.
2. Compiles zaFreshGram in the container with `DEBUG=0` and stripped symbols.
3. Removes intermediate files in `out/` and keeps `out/install`.
4. Builds and installs the package with `makepkg -sfic`.

The binary is placed at `out/install/usr/local/bin/zaFreshGram`, and the package installs it to `/usr/bin/zaFreshGram`.

### Build without packaging

To build the binary without installing a package:

```bash
./build_zafreshgram.sh --no-pkg
```

### Manual Docker build

To run the container build yourself, use the same command as the script:

```bash
docker run --rm -it \
    -u $(id -u) \
    -v "$PWD:/usr/src/tdesktop" \
    zafreshgram:centos_env \
    /usr/src/tdesktop/Telegram/build/docker/centos_env/build.sh \
    -D TDESKTOP_API_ID=YOUR_API_ID \
    -D TDESKTOP_API_HASH=YOUR_API_HASH
```

For a debug build, add `-e CONFIG=Debug` after `--rm -it`.

## Credits

### Forks

* [freshGram](https://github.com/Snowy-Fluffy/freshGram)
* [ZaStoGram](https://cwdnvtsqdc.zapret.moe/zastogram/ZaStoGram_desktop)

### Telegram clients

* [Telegram Desktop](https://github.com/telegramdesktop/tdesktop)
* [AyuGram Desktop](https://github.com/AyuGram/AyuGramDesktop)
* [materialgram](https://github.com/kukuruzka165/materialgram)
* [Kotatogram](https://github.com/kotatogram/kotatogram-desktop)
* [64Gram](https://github.com/TDesktop-x64/tdesktop)
* [Forkgram](https://github.com/forkgram/tdesktop)

### Libraries used

* [JSON for Modern C++](https://github.com/nlohmann/json)
* [SQLite](https://github.com/sqlite/sqlite)
* [sqlite_orm](https://github.com/fnc12/sqlite_orm)
* [androidx sources](https://github.com/androidx/androidx)
* Qt 6, OpenSSL, WebRTC, FFmpeg, Opus, OpenAL Soft and the other libraries listed in the Telegram Desktop repository
* Vazirmatn font ([SIL Open Font License 1.1](https://github.com/rastikerdar/vazirmatn/blob/master/OFL.txt))

### Icons

* [Solar Icon Set](https://www.figma.com/community/file/1166831539721848736)

## License

zaFreshGram is licensed under the GNU General Public License v3.0 only. See [LICENSE](LICENSE) for the full text.