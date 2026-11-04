# Alexa02

Six drum voices from the open-source LXR-02 sound engine, as a VCV Rack module. The sequencer is left out. Instead, every voice has trigger, accent, V/Oct and morph jacks. There are two kits, A and B, and a MORPH control that blends one into the other.

- The engine is the sound engine from the Erica Synths LXR-02 Eurorack firmware, compiled unchanged apart from the few marked portability fixes.
- Kits from the hardware (`.snd` files) load and save.
- Six factory kits are included, plus the firmware's init kit.

This is an unofficial port and is not affiliated with or endorsed by Erica Synths. It is licensed GPL-3.0-only with the additional terms in [NOTICE.md](NOTICE.md). Files changed from the firmware are marked "Alexa02 port".

## Build

    make RACK_DIR=<Rack SDK>        # plugin
    make test                       # engine test (needs only a C compiler)

GitHub Actions builds the mac-arm64 plugin and publishes it to the `latest` release.

## Regenerating files

    python3 tools/gen_meta.py       # src/ParamMeta.hpp from the firmware headers
    python3 tools/make_kits.py      # src/FactoryKits.hpp from tools/kits.py
    python3 tools/make_panel.py     # res/Alexa02.svg and src/Layout.hpp (needs fonttools)
