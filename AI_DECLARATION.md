# Declaration of generative AI use

**Course:** IA736001 Internet of Things and Cloud Computing
**Student:** Bhanu Gupta
**Student ID:** 100012452
**Cohort:** Block 3, 2026
**Applies to:** Part 1 Case Study Analysis (individual) and Part 2 Project Portfolio (group)

Part 2 was produced jointly with **Roshan Aryal (1000123440)**, an equal 50/50 split. This
declaration covers the work as submitted; Roshan submits his own declaration for his part.

---

> **Read this before submitting.**
>
> This is a draft written to be accurate rather than flattering. Check it against your course's
> Responsible Use of AI guidelines — those govern, not this document — and against your own
> recollection. Edit anything that overstates or understates what happened.
>
> A declaration that minimises AI involvement is worse than no declaration at all. If a marker
> forms a different impression from the work itself, an understated disclosure becomes the
> problem rather than the AI use. The version below errs toward completeness for that reason.

---

## Statement

Generative AI (Claude, Anthropic) was used substantially in the production of this submission.
The specific division of work was as follows.

### Where AI was used

**Firmware.** The five Arduino sketches were written with AI assistance, including the ESP-NOW
packet structure, the deduplication logic, the MQTT integration, and the concurrency handling
between the Wi-Fi and application tasks.

**Written documents.** Both the project portfolio and this case study analysis were drafted
with AI assistance, including structure, technical explanation and prose. Figures and diagrams
were generated programmatically.

**Code review.** The firmware was submitted to an independent AI review (OpenAI Codex) on two
occasions. The first identified twelve issues, including a cross-task data race and several
inaccurate claims in the portfolio. The second reviewed the resulting changes and rejected the
proposed security mitigation as inadequate, which led directly to implementing ESP-NOW link
encryption.

**Demonstration video.** The video was assembled with AI assistance from footage filmed by the
authors, and the on-screen subtitles were written by AI. The screen recordings of the
ThingsBoard dashboard were captured by an AI agent driving the browser; the telemetry shown in
them is genuine output from the authors' own hardware. **The spoken narration is a human
recording, not synthesised.**

**Debugging assistance.** Including diagnosis of the ESP-IDF 5.5 callback signature change,
the institutional Wi-Fi authentication realm, and the channel roam described in the portfolio.

### Where AI was not used

**All physical work.** Hardware assembly, wiring, and every act of connecting, powering and
observing the boards.

**All empirical evidence.** Every figure, log and measurement in the evidence directory was
captured from the physical system in operation. No result was estimated, extrapolated or
fabricated. Where a claim could not be substantiated from a capture, it was removed rather
than asserted — the portfolio records one such correction.

**Project decisions.** The choice of institutional Wi-Fi over a phone hotspot despite its known
risks; the decision to submit the design for independent adversarial review; the division of
work with my project partner; and the repository visibility settings.

**Prior experience.** The account of previous work in the Embedded Systems module, and the
sections of the case study concerning industry parallels and personal background.

### Verification

Every technical claim in both documents was checked against captured evidence or against the
source code. Where an earlier draft asserted something the evidence did not support — a
duplicate-rejection ratio, and a claim that a compile-time assertion guaranteed more than it
does — the claim was corrected rather than left standing.

The case study carries eighteen references. Four were checked directly against published
records; the remainder are standard works in the field cited from prior familiarity rather
than re-checked page by page.

---

**Signature:** ______________________    **Date:** ______________

---

## Notes for you, not for submission — delete this section

Two things worth being ready for, since the demonstration includes five minutes of questions.

**You should be able to explain any part of this work.** The marking scheme rewards "confident,
well-informed responses in Q&A", and a portfolio you cannot defend under questioning is a
liability regardless of how it was produced. `DEMO_PLAN.md` contains prepared answers for the
questions the design invites. Read them until they are yours rather than recited.

**Keep your drafts.** The Part 2 brief says: *"If you are willing to get the assistance of AI,
please submit your original document as well."* Check what your course means by "original
document" — it may want to see your own working alongside the final version.
