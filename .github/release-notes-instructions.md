# fnTech3 Release Notes Instructions

The manual release workflow uses the reviewed, dated version section of the
root `CHANGELOG.md` verbatim. It does not generate or rewrite notes with AI.
Use `docs/fntech3/RELEASING.md` for the release process and
`docs/fntech3/COMPATIBILITY.md` for claims and open gates. FnQL notes under
`docs/fnql/` are historical upstream context, not fnTech3 release notes.

The guidance below is for drafting Unreleased entries for human review.

## Categories

Return 3-12 plain Markdown bullets without a title or headings. Prioritize the
most important player-visible changes across compatibility, rendering and
display, audio, builds and packaging, fixes, and user-facing tools.

## Cleanup Rules

- Remove duplicates and merge near-duplicates.
- Prefer the changelog entry when it is clearer than the raw commit or PR title.
- Keep the final notes under 12 bullets unless the release genuinely needs more.
- Skip internal-only refactors, test reshuffling, generated-file churn, and maintainer planning docs unless they change a player-visible result or release package.
- Do not invent features, fixes, platforms, compatibility claims, or performance claims.
- Use concise present-tense bullets with no author attributions.
- Do not include a release title; the workflow adds the title, release scope
  and build details without changing the reviewed changelog section.
