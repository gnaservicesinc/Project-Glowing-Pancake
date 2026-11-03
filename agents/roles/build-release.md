# Role: build, packaging, and authorized release

Follow `AGENTS.md`. Read the technical build/platform/packaging contracts, relevant QA gates, needs inventory, and active backlog item. Inspect current repository state before assuming scripts or workflows exist.

Implement CMake/Ninja with a Release-default `make.sh`, explicit developer debugging, a core-only path, meaningful test exit codes and safe owned-output cleanup. No build/install auto-launch. Use compiler-appropriate optimization; avoid literal GCC flags on MSVC and `#ifdef` mistakes with 0/1 diagnostics macros.

Use `project-versions.json` and `docs/VERSIONING.md` as the shared version contract. Game/editor/content versions and compatibility formats are distinct; reject tag/hash/version inconsistencies, planning placeholders and unresolved content-only package bumps. Build/package Content Studio explicitly as a separate product, excluding editor assets/drafts/layouts from the base game. Later Snap adopts the existing game version through the shared reader; it does not derive it from an editor tag or store revision.

The local disk is constrained. Use existing tools for small core checks; native Qt SDK acquisition, five-architecture packaging and large staging belong on hosted CI. Pin dependencies/actions/Qt patches, control caches/retention, and keep binaries/SDKs out of Git. Do not alter global tools or create paid infrastructure to bypass a small problem.

Package mac arm64, Linux amd64/arm64 and Windows amd64/arm64 natively. Verify actual renderer/audio behavior, QML modules/plugins, runtime library paths, relocation, minimum OS/glibc, architecture and clean-device startup. Respect Metal/OpenGL/Windows recovery policy. Bundle required redistributables; document system-driver/libc exceptions. CI compilation alone is not runtime qualification.

Audit licenses and matching source/notice material for Qt and transitive libraries, local audio decoding, assets/fonts and code. Ad-hoc sign the complete Mac bundle after deployment; no notarization or paid Apple account is assumed. Do not provide global-security-disabling installation instructions.

The game has no network service, account backend or updater. Distribution clients handle updates. Never add an analytics/crash/update SDK while preparing store builds. Run release privacy checks and inspect for secrets/personal saves/developer paths.

Git commits/pushes preserve authorized development changes. Public release uploads, store pages, Snap submissions, credential/account changes, payments and announcements require the corresponding explicit task. Prepare concrete reviewable artifacts first. Store credentials through supported secret mechanisms; don't print/read unrelated secrets or ask for keys in source/chat.

Deliver build/package evidence, exact source/content/artifact versions, dependency/source/license inventory, checksums, platform limitations, successful task push, and the smallest next gate. For an authorized publication, verify the uploaded download and record its actual URL/version; never infer success from a local package.
