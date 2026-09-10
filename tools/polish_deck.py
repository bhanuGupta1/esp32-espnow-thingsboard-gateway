"""Tidy the presentation without redesigning it.

Three things only, all of them corrections rather than decoration:

  1. The two diagram pictures overflow the bottom of the slide. Fit them to the
     space that actually exists, preserving aspect ratio.
  2. Body text sits in one block per slide with no breathing room. Give the
     paragraphs consistent spacing so a reader's eye can find the lines.
  3. Long bullet lines run the full 11.93 inch width, which is too wide to read
     comfortably. Narrow the text column on text-only slides.

Everything else - the palette, the typeface, the slide order - is left alone.
It is a lecturer's ten minutes, not a design portfolio.

    python tools/polish_deck.py PRESENTATION.pptx PRESENTATION_polished.pptx
"""

import sys

from pptx import Presentation
from pptx.util import Inches, Pt

SRC = sys.argv[1] if len(sys.argv) > 1 else "PRESENTATION.pptx"
DST = sys.argv[2] if len(sys.argv) > 2 else "PRESENTATION_polished.pptx"

SLIDE_W, SLIDE_H = 13.33, 7.50
PICTURE = 13

prs = Presentation(SRC)

fitted = 0
spaced = 0
narrowed = 0

for idx, slide in enumerate(prs.slides, start=1):
    pics = [sh for sh in slide.shapes if sh.shape_type == PICTURE]

    # --- 1. Fit any picture inside the slide -----------------------------
    for pic in pics:
        top_in = pic.top / 914400
        avail_h = SLIDE_H - top_in - 0.35
        avail_w = 11.93
        # The current width and height already carry the aspect ratio, since
        # the picture was inserted proportionally.
        ar = pic.height / pic.width
        w = avail_w
        h = w * ar
        if h > avail_h:
            h = avail_h
            w = h / ar
        if abs(w - pic.width / 914400) > 0.01 or abs(h - pic.height / 914400) > 0.01:
            pic.width, pic.height = Inches(w), Inches(h)
            pic.left, pic.top = Inches((SLIDE_W - w) / 2), Inches(top_in)
            fitted += 1

    # --- 2 and 3. Body text: spacing, and a narrower measure -------------
    for sh in slide.shapes:
        if not sh.has_text_frame:
            continue
        top_in = sh.top / 914400
        if top_in < 1.7:            # this is the slide header, leave it
            continue
        tf = sh.text_frame
        if not tf.text.strip():
            continue

        for par in tf.paragraphs:
            if not par.text.strip():
                continue
            par.space_after = Pt(11)
            par.line_spacing = 1.18
            spaced += 1

        # Only narrow slides that are text alone. A slide sharing space with a
        # picture already has its width decided by that picture.
        if not pics and sh.width / 914400 > 11.0:
            sh.width = Inches(10.4)
            narrowed += 1

prs.save(DST)
print("pictures refitted : %d" % fitted)
print("paragraphs spaced : %d" % spaced)
print("columns narrowed  : %d" % narrowed)
print("written           : %s" % DST)
