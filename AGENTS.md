Publish distributable ZIP archives as GitHub Release assets attached to version tags. Do not add archives or a downloads directory to the repository tree.

For a new release, update the version in CMakeLists.txt, the runtime and installer, then push a matching vX.Y.Z tag. The release workflow builds, validates and uploads the package. Keep README and GitBook download links pointing to Release assets. Keep existing releases available.

Before creating a tag, write .github/release-notes/vX.Y.Z.md describing that version's actual added features, fixed bugs, and any known limitations. Mention validation where useful. Do not reuse a generic product description or invent fixes. Write release notes in English and link the Russian documentation. Publishing requires this file; updates to notes on main also update the corresponding existing releases.
