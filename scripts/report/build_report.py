#!/usr/bin/env python3
"""Builds docs/report.pdf from the content modules.

    python3 scripts/report/build_report.py

Reads docs/bench/results.csv when it exists so the performance tables can be
cross-checked against the raw run, and writes the PDF next to it.
"""

import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from reportlib_env import ensure_reportlab  # noqa: E402

ensure_reportlab()

from reportlab.pdfbase import pdfmetrics  # noqa: E402
from reportlab.lib.styles import ParagraphStyle  # noqa: E402
from reportlab.platypus import (  # noqa: E402
    KeepTogether,
    NextPageTemplate,
    PageBreak,
    Paragraph,
    Table,
)

import report_lib  # noqa: E402
import content_a  # noqa: E402
import content_b  # noqa: E402


def main():
    repo_root = os.path.abspath(os.path.join(HERE, "..", ".."))
    out_path = os.path.join(repo_root, "docs", "report.pdf")

    report_lib.register_fonts()
    styles = report_lib.Styles()

    doc = report_lib.ReportDoc(out_path, content_a.TITLE_RUNNING)
    doc.author = "Nhóm 11 — Hoàng Văn Huynh; Nguyễn Gia Bách; Nguyễn Hữu Trường"
    # The cover/TOC helpers expect styles attached to the doc, so the content
    # modules can stay free of layout code.
    doc.styles_cover_title = styles.cover_title
    doc.styles_cover_sub = styles.cover_sub
    doc.styles_cover_meta = styles.cover_meta
    doc.styles_toc_title = styles.toc_title
    doc.toc = report_lib.make_toc(styles)

    story = []
    story += content_a.front_matter(doc)
    story += content_a.section_2_task_assignment(doc)
    story += content_a.section_3_abstract()
    story += content_a.section_4_requirements()
    story += content_a.section_5_issues()
    story += content_a.section_6_analysis()
    story += content_a.section_7_architecture()
    story += content_a.section_8_components()
    story += content_a.section_9_discovery()
    story += content_a.section_10_concurrency()
    story += content_b.section_11_fault()
    story += content_b.section_12_security()
    story += content_b.section_13_implementation()
    story += content_b.section_14_testing()
    story += content_b.section_15_performance()
    story += content_b.section_16_limitations()
    story += content_b.section_17_conclusion()
    story += content_b.section_18_references()
    story += content_b.section_19_appendix()

    # Some sections splice bullet lists in with ``*bullets(...)`` inside a list
    # literal, which yields nested lists. multiBuild wants a flat flowable list.
    flat = []

    def _flatten(items):
        for item in items:
            if isinstance(item, (list, tuple)):
                _flatten(item)
            else:
                flat.append(item)

    _flatten(story)

    # A table and the caption that names it must never be split across pages:
    # a caption stranded alone on the next page reads as an orphan. Wrap each
    # (table, caption) pair; KeepTogether splits anyway when the pair is taller
    # than a page, so long tables still paginate.
    glued = []
    index = 0
    while index < len(flat):
        item = flat[index]
        if (
            isinstance(item, Table)
            and index + 1 < len(flat)
            and isinstance(flat[index + 1], Paragraph)
            and flat[index + 1].style.name == "Caption"
        ):
            if glued and isinstance(glued[-1], Paragraph) and glued[-1].style.name in ("H1", "H2", "H3"):
                heading = glued.pop()
                glued.append(KeepTogether([heading, item, flat[index + 1]]))
            else:
                glued.append(KeepTogether([item, flat[index + 1]]))
            index += 2
            continue
        glued.append(item)
        index += 1
    flat = glued


    # Two passes: the TOC is filled during the first pass and rendered in the
    # second. ReportLab's multiBuild repeats until the page numbers settle.
    # multiBuild replays the story and polls the TOC between passes, so the
    # flowable list it keeps must survive being walked more than once; plain
    # lists are fine here because nothing mutates them in place.
    from functools import partial
    from reportlab.pdfgen.canvas import Canvas
    doc.multiBuild(flat, canvasmaker=partial(Canvas, initialFontName=report_lib.SANS))
    print("wrote %s" % out_path)


if __name__ == "__main__":
    main()
