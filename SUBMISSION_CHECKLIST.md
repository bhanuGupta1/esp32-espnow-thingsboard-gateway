# Submission checklist and templates

Everything the brief asks for, what exists, and what is still yours to produce.

---

## 0. There are two assessments, not one

This is worth stating plainly because the two are easy to conflate and they have different
rules.

| | **Part 1 — Case Study Analysis** | **Part 2 — Project** |
|---|---|---|
| Weighting | **40%** | **60%** |
| Type | **Individual** | Group (your 50/50 split) |
| Deliverable | PDF report, 15+ APA7 references, 3 original diagrams | Portfolio, slides, video, working prototype |
| Due | See `submission/Coversheet-Part1.docx` | See `submission/Coversheet-Part2.docx` |
| Status | **Not addressed in this repository** | Substantially complete |

**Everything in this repository is Part 2.** The Part 1 brief is included at
`submission/Brief-Part1-CaseStudy.docx` for reference, but no Part 1 work exists here.

Two consequences worth checking:

1. **Part 1 is an individual assessment.** The cover sheet says so explicitly. The 50/50 split
   agreed for the project does not apply to it — each member writes their own.
2. **Part 2 is marked partly on alignment with Part 1.** The Part 2 marking scheme awards top
   marks where the project "aligns completely with case study", and the brief opens with
   *"you will implement the project you explained in the Case Study Analysis"*. If the Case
   Study described something materially different from a two-node ESP-NOW sensor network, that
   gap needs addressing in §1 of the portfolio — either by explaining the evolution or by
   aligning the framing.

---

## 1. What the Part 2 brief requires

| Deliverable | Status | File |
|---|---|---|
| Project portfolio | Ready | `REPORT.docx` (12 figures, 15 tables, 7.0 MB) |
| Presentation / demo materials | Ready | `PRESENTATION.pptx` (17 slides, speaker notes, 2.2 MB) |
| Video recordings | Partly done — see §3 | `P1_demo_cloud_and_hardware.mp4` (1:49, 7.0 MB) |
| Complementary evidence | Ready | `evidence/` (11 logs, 3 photos, 8 screenshots, 1 diagram, 2 screen recordings, 1 video) |
| Cover sheet | **Template below — needs your details** | §2 |
| Original document, if AI-assisted | **Yours to decide and produce** | §4 |
| Code | Ready | 5 sketches + private GitHub repo |

---

## 2. Cover sheet

The official cover sheets are in `submission/`. Use those — not a substitute.

- `submission/Coversheet-Part2.docx` — for this project
- `submission/Coversheet-Part1.docx` — for the Case Study Analysis

Everything except your personal details is already printed on them: course code, lecturer
(Senaka Amarakeerthi), class times, weighting, due date. **Four fields are blank and they are
all yours to complete:**

| Field | Note |
|---|---|
| Student Name | |
| Student ID | |
| Cohort number | |
| Signature and Date | Signed by hand, after reading the declaration |

**Read the declaration before signing it.** It states, verbatim:

> "This is an original assessment and is entirely my own work. Where ideas, tables, diagrams
> etc. of other writers have been used, I have acknowledged the source in every case."

Note what this does *not* say: it does not mention AI. The project brief separately requires
disclosure of AI assistance and directs you to the Responsible Use of AI guidelines. Signing
the declaration and making that disclosure are two different obligations, and both apply.
See §4.

### Note on the group split

**The Part 2 cover sheet has a single Student Name field.** It does not have a section for
recording a contribution split. If both of you are submitting jointly, check on Moodle or with
the lecturer how a group submission should be recorded — whether each member submits their own
cover sheet, or one is submitted with both names added.

The 50/50 split described in `REPORT_OUTLINE.md` should also be stated somewhere in the
portfolio itself, since the cover sheet has nowhere to put it.

---

## 3. Video evidence

Required by the brief, and named explicitly in the top mark band of **two** criteria
(Portfolio Quality, and Documentation & Supporting Materials).

**`P1_demo_cloud_and_hardware.mp4`** exists: 1280×720, 1 min 49 s, **7.0 MB**. That is under
Moodle's 40 MB limit, so it is submitted directly with the portfolio and no OneDrive link is
needed. It covers the boards at rest, the whole cloud tier including the RPC round trip, the
dashboard filmed beside the hardware, and Board 1 being disconnected. Subtitles are burned in.

Three shots are still missing, and the first matters most:

| Missing shot | Why |
|---|---|
| The dashboard **after** the 20 s threshold | The disconnection is filmed but its consequence is not. Without it the video pulls a cable and asks the viewer to take the result on trust |
| Board 1 plugged back in | New `boot_id`, sequence restarts, no duplicate flood — §8.6 |
| Warming the sensor by hand | The whole chain reacting in about fifteen seconds |

`VOICEOVER_SCRIPT.md` carries the narration, timed against the assembled cut.

**Before you press record.** Both boards powered, and confirm the link is up — if
`espnow_received_count` is not climbing, the gateway has roamed and the node needs repinning
(see `DEMO_PLAN.md`). Identify the ports by MAC, not by number: Board 1 is
`44:1D:64:F5:FA:24`, Board 2 is `44:1D:64:F4:F1:C8`.

```
.\build.ps1 -Monitor -NoReset -Port <gateway> -Seconds 20
```

Use `-NoReset` throughout. Without it the board reboots when the monitor attaches, and the
boot banner prints your eduroam identity on screen.

**Shot list — about 5 minutes.** Phone camera is fine; clear audio matters more than image
quality. Screen-record the dashboard and terminal separately if that is easier than filming a
monitor.

| # | Length | Shot | Say |
|---|---|---|---|
| 1 | 20 s | Both boards on the desk, pan slowly between them | "Two boards, powered separately, no wire between them." |
| 2 | 20 s | Close on each breadboard | Wire colours, which GPIO, that the board straddles the centre gap |
| 3 | 40 s | Board 1 serial | `association: none`, and `send : OK` being a real link-layer ack, not an assumption |
| 4 | 40 s | Board 2 serial | The same sequence number arriving, validated, `invalid=0 wrong_sender=0` |
| 5 | 30 s | ThingsBoard dashboard | Same numbers, both traces on one chart |
| 6 | 45 s | **Warm Board 1's sensor by hand** | Watch it climb on the dashboard, release, watch it fall |
| 7 | 45 s | **Press "Fast publish (2 s)"** | The cloud talking back — then show `rpc_handled` and `publish_interval_ms` change in Latest Telemetry |
| 8 | 40 s | Unplug Board 1, wait | `node1_online` false after 20 s; last value still published |
| 9 | 30 s | Plug back in | New `boot_id`, sequence restarts, accepted with no duplicate flood |

Shots 6 and 7 are the two to get right, and they show opposite directions. Shot 6 proves the
whole upward chain — sensor, radio, gateway, MQTT, cloud — responding to a physical action in
about fifteen seconds. Shot 7 proves the downward one, which is what separates a managed
device from a telemetry feed. Say the interval out loud before and after so the change is
audible as well as visible.

If the buffer is worth showing, unplug the gateway's network rather than the board: payloads
accumulate in `buffered_now` and flush on reconnect with `dropped_total` still zero.

**Before uploading:** check no frame shows `secrets.h`, a password field, or your eduroam
username. Shots 3, 4 and 7 show serial output, and the gateway's boot banner carries the outer
identity line — which is why `-NoReset` matters above.

---

## 4. Generative AI disclosure

The brief states:

> "If you are willing to get the assistance of AI, please submit your original document as
> well" — and directs you to read the Responsible Use of AI guidelines.

This is a requirement of the assessment, not optional advice, and how you satisfy it is your
decision. Read your course's guidelines and follow them exactly; the wording below is a
starting point only, and needs editing to reflect what you actually did.

```
Use of generative AI

Generative AI (Claude) was used substantially during this project, in the
following ways:
    - Drafting and structuring the portfolio prose and the case study
    - Writing and revising firmware, including the ESP-NOW, deduplication,
      MQTT, RPC and buffering code
    - Diagnosing faults, including the channel-roam failure and a
      sender-authentication weakness that was subsequently fixed with
      link encryption
    - Generating the circuit diagram and configuring the cloud dashboard
    - Producing the presentation slides and supporting documentation

The following were carried out by the authors without AI assistance:
    - All hardware assembly, wiring and physical testing
    - All photographic and video evidence capture
    - Operating the boards during every test, including the physical
      interventions that produced the stale-detection and recovery results
    - Acceptance of the design decisions recorded in this portfolio,
      including the DHT11 substitution and the deployment choices

Every technical claim in this portfolio was verified against captured evidence
from the authors' own hardware, which is included in the evidence/ directory.
No result is reported that was not observed on the physical system.
```

Two things worth doing regardless of how the declaration is worded:

1. **Be able to defend every claim.** The demo is 10 minutes plus 5 minutes of questions, and
   the marking scheme rewards "confident, well-informed responses". Anything in the portfolio
   you cannot explain under questioning is a liability. `DEMO_PLAN.md` has prepared answers
   for the questions the design invites.
2. **Keep drafts if your course asks for the original.** "Submit your original document as
   well" implies they want to see your own work alongside the final version.

---

## 5. Final checks before submitting

- [ ] Course code, names and student IDs filled in on `REPORT.docx` (page 1) and
      `PRESENTATION.pptx` (slide 1)
- [ ] Personal reflection sentence added by each member at the end of §11.1
- [ ] Video recorded, uploaded, link inserted into the portfolio
- [ ] Cover sheet completed and signed
- [ ] AI declaration completed per course guidelines
- [ ] No credentials visible in any screenshot, video frame, or appendix
- [ ] eduroam password changed (it was compiled into Board 2's firmware during development)
- [ ] Board 2 re-flashed with sketch 02 before the hardware leaves your possession
- [ ] Scope described consistently as a two-node prototype in §1, §9 and §11
