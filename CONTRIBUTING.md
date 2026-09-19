# Make one interesting change

Start with [the browser workshop](https://rozgo.github.io/alienwars-gym/learn/)
or [run the repo locally](docs/START_HERE.md). Coding agents are welcome: give
them [AGENTS.md](AGENTS.md) and the [onboarding skill](.agents/skills/alienwars-start/SKILL.md).
The same walkthrough works by hand.

Good first contributions include a reproducible sensor experiment, a clearer
inspection control, an alien material variation or a minimal failing navigation
scenario. Pick a visible outcome and keep one change easy to understand.

1. Fork the repository and create a branch for your change.
2. Record the starting seed/settings or baseline result. Explain your prediction.
3. Edit authored sources. Use the [validation table](AGENTS.md#choose-validation-for-the-change)
   and show the result in the actual viewer when appearance or controls change.
4. Open a pull request describing what changed, why and what you checked. Include
   a screenshot for visual changes, or measured before/after results for behavior.
   Mention checks you could not run.

Keep SDKs, credentials, raw checkpoints and logs out of Git. Share concise
experiment reports with exact commands, source revision and seeds. Published
browser artifacts have a separate [release workflow](docs/WEB.md#publish-from-main);
a documentation or lesson contribution does not require a full rebuild or GPU.
Preserve asset licenses and the upstream MIT license.

Reinforcement-learning contributions need held-out evaluation against a baseline.
A successful training process or an attractive patrol is not a reliability score.
Our [recorded failures](docs/runs/navigation-reliability-2026-09-18.md) are useful
starting points. Cultivation and combat are still [design work](docs/BIOLOGICAL_WARFARE.md).
