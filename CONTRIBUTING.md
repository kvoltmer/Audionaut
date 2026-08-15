## Contributing to Audionaut

Thanks for your interest in contributing!

### Contributor License Agreement

Audionaut is dual-licensed under GPL3 (or later) and a commercial license (see [LICENSE.md](LICENSE.md)), so the project can keep offering both a free/open-source option and a commercial one to businesses that need closed-source use. To keep that possible, **all contributions require agreeing to the [Contributor License Agreement](CLA.md)**. By submitting a pull request, you certify that you've read and agree to it. You keep copyright over your own contribution — you're granting the maintainer a license broad enough to keep relicensing the project (including your contribution) under both GPL and commercial terms.

### How to contribute

1. For anything non-trivial, open an issue first so we can agree on direction before you invest time.
2. Fork the repo and branch off `develop`.
3. Follow the build and test setup in [CLAUDE.md](CLAUDE.md).
4. Make sure the Catch2 test suite passes for anything touching the engine (see CLAUDE.md's Tests section).
5. Open a pull request against `develop` describing what changed and why.

### Code style

Match the conventions already used in the surrounding code — see CLAUDE.md's Conventions section (namespacing, constructor-injected dependencies, `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR`, the GPL/commercial license header on new files, etc).

### Reporting bugs / security issues

Open a GitHub issue for regular bugs. For anything security-sensitive, please email [vltmrkls@gmail.com](mailto:vltmrkls@gmail.com) directly instead of filing a public issue.
