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
| Project portfolio | Ready | `REPORT.docx` (27+ pages, figures embedded) |
| Presentation / demo materials | Ready | `PRESENTATION.pptx` (16 slides, speaker notes) |
| Video recordings | **Not done — yours to record** | see §3 |
| Complementary evidence | Ready | `evidence/` (7 logs, 3 photos, 4 screenshots) |
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
(Portfolio Quality, and Documentation & Supporting Materials). Currently missing, and it is
the single largest remaining gap.

The brief says video may be uploaded to OneDrive with the link placed in the portfolio.

**Suggested shot list — about 4 minutes total.** Recording it on a phone is fine; clear audio
matters more than image quality.

| # | Length | Shot | Say |
|---|---|---|---|
| 1 | 20 s | Both boards on the desk, pan slowly between them | "Two boards, powered separately, no wire between them." |
| 2 | 20 s | Close on each breadboard | Wire colours, which GPIO, that the board straddles the centre gap |
| 3 | 40 s | Board 1 serial output | Point out `association: none`, and `send : OK` being a real link-layer ack |
| 4 | 40 s | Board 2 serial output | The same packet arriving, validated, deduplicated |
| 5 | 30 s | ThingsBoard dashboard | Same numbers, both traces on one chart |
| 6 | 45 s | **Warm Board 1's sensor by hand** | Watch the value climb on the dashboard, then release and watch it fall |
| 7 | 40 s | Unplug Board 1, wait | `node1_online` goes false after 20 s; last value still published |
| 8 | 30 s | Plug back in | New `boot_id`, sequence restarts, accepted with no duplicate flood |

Shot 6 is the one worth getting right. It demonstrates the entire chain — sensor, radio,
gateway, MQTT, cloud — responding to a physical action, in about fifteen seconds.

**Before uploading:** check no frame shows `secrets.h`, a password field, or your eduroam
username. Shot 3 and 4 show serial output, which includes the outer identity line on the
gateway's boot banner.

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

Generative AI was used during this project in the following ways:
    - [ e.g. drafting and structuring portfolio prose ]
    - [ e.g. code review and identification of defects ]
    - [ e.g. debugging assistance ]

The following were carried out without AI assistance:
    - [ e.g. hardware assembly and wiring ]
    - [ e.g. all physical testing and evidence capture ]
    - [ e.g. design decisions and their justification ]

All technical claims in this portfolio were verified against captured evidence,
which is included in the evidence/ directory.
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
