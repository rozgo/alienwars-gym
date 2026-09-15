# PufferLib provenance

Imported from [PufferAI/PufferLib, branch 5.0](https://github.com/PufferAI/PufferLib/tree/5.0)
on 2026-09-14, retaining Git history and the MIT license.

- Source commit: `6ffa5b10dbbbe4d1e8288367c7d9d3acd3bad4a2`
- Source commit date: 2026-09-13
- `upstream`: `https://github.com/PufferAI/PufferLib.git`
- Project branch: `main`

The [official docs](https://puffer.ai/docs.html) recommend editing source,
starting from the minimal environment, developing with CPU debug builds, and
training on NVIDIA hardware. The [official installer](https://github.com/PufferAI/PufferTank/blob/5.0/install.sh)
documents native dependencies. Inspect installers before running them: the full
installer uses system package management and adds dependencies for other games.

Implementation details are pinned by this checkout:

- [Environment interface](https://github.com/PufferAI/PufferLib/blob/6ffa5b10dbbbe4d1e8288367c7d9d3acd3bad4a2/src/pufferenv.h)
- [Minimal example](https://github.com/PufferAI/PufferLib/blob/6ffa5b10dbbbe4d1e8288367c7d9d3acd3bad4a2/ocean/minimal/minimal.h)
- [Native build script](https://github.com/PufferAI/PufferLib/blob/6ffa5b10dbbbe4d1e8288367c7d9d3acd3bad4a2/build.sh)

Website caveats at import: `binding.c` references describe an older interface;
current metadata lives in environment headers. The upstream Mac build used Bash
4 uppercase expansion despite its `/bin/bash` shebang, and disabled sanitizers.
This project uses Bash 3-compatible uppercase conversion and enables Mac
ASan/UBSan. The native log retains loss series (upstream omitted them) and adds
exact steps, training time and final score in a `[run]` section for finite-metric
checks and reproducible reports. The native trainer algorithm is unchanged.
Upstream example environments remain available as reference code; the project
bootstrap demo and its dedicated tooling have been retired.

To inspect future changes, use `git fetch upstream 5.0` and review the diff
before merging. Rebuild and repeat the native checks and bounded GPU baseline
after a deliberate upstream update.
