# FnQL design reviews and plans

Reviews and implementation plans written for FnQL's renderer and audio work in
May and June 2026, before the merger that became fnTech3. They are kept as
history, renamed by date; the plans mention one another by their earlier
paths. The work they plan has since landed, changed course or been dropped,
and their status claims are not fnTech3's: the
[compatibility ledger](../../fntech3/COMPATIBILITY.md) and the
[FnQL design notes](../README.md) describe what exists now.

| Date | Document | Subject | Earlier path |
| --- | --- | --- | --- |
| 2026-05-07 | [GLx plan](2026-05-07-glx-plan.md) | The GLx renderer's first implementation plan. | `docs/plans/glx-7-5-26.md` |
| 2026-05-07 | [Modern audio and HRTF](2026-05-07-modern-audio-hrtf.md) | Spatial audio, HRTF and the OpenAL backend. | `docs/plans/modern-audio-hrtf-7-5-26.md` |
| 2026-05-08 | [Audio review](2026-05-08-audio-review.md) | The audio system measured against that plan. | `docs/plans/audio-review-8-5-26.md` |
| 2026-05-08 | [Audio review, second pass](2026-05-08-audio-review-2.md) | A second review of the same audio work. | `docs/plans/audio-review(2)-8-5-26.md` |
| 2026-05-08 | [GLx review](2026-05-08-glx-review.md) | GLx measured against its 7 May plan. | `docs/plans/glx-review-8-5-26.md` |
| 2026-05-08 | [Vulkan review](2026-05-08-vk-review.md) | The Vulkan renderer in the id Tech 3 context. | `docs/plans/vk-review-8-5-26.md` |
| 2026-05-09 | [GLx replacement plan](2026-05-09-glx-review.md) | The task list that made GLx the default; `tests/glx/glx_runtime_sweep_tests.py` checks that every task is marked implemented. | `docs/plans/glx-review-9-5-26.md` |
| 2026-05-10 | [GLx goals review](2026-05-10-glx-review.md) | GLx against its documented goals. | `docs/plans/glx-review-10-5-26.md` |
| 2026-05-11 | [HDR review](2026-05-11-hdr-review.md) | GLx's HDR and post-processing pipeline. | `docs/plans/hdr-review-2026-05-11.md` |
| 2026-05-12 | [HDR review, second pass](2026-05-12-hdr-review.md) | The same pipeline's split between planning and implementation. | `docs/plans/hdr-review-2026-05-12.md` |
| 2026-06-03 | [GLx dynamic lights](2026-06-03-glx-dlight-plan.md) | Optimising GLx's dynamic-light pipeline. | `docs-dev/plans/2026-06-03-glx-dlight-plan.md` |
| 2026-06-03 | [Vulkan shadow mapping](2026-06-03-vk-shadowmapping.md) | The Vulkan shadow-mapping subsystem. | `docs-dev/plans/2026-06-03-vk-shadowmapping.md` |
| 2026-06-15 | [Shadow-mapping remediation](2026-06-15-shadowmapping.md) | A checklist for OpenGL and Vulkan shadow-mapping defects. | `docs-dev/plans/2026-06-15-shadowmapping.md` |
