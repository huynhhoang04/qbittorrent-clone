"""Shared layout helpers for the course report.

Kept separate from the content so the content file reads as prose and the
styling can be tuned without touching it. Everything here is presentation only.
"""

import os
from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER, TA_JUSTIFY, TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import (
    BaseDocTemplate,
    Frame,
    PageTemplate,
    Paragraph,
    Spacer,
)
from reportlab.platypus.tableofcontents import TableOfContents

SANS = "TimesNewRoman"
SANS_BOLD = "TimesNewRoman-Bold"
SANS_ITALIC = "TimesNewRoman-Italic"
MONO = SANS

ACCENT = colors.HexColor("#1f3864")
ACCENT_LIGHT = colors.HexColor("#dbe5f1")
GREY = colors.HexColor("#666666")
RULE = colors.HexColor("#b7c4d6")


def register_fonts():
    """Embed actual Times New Roman; never silently substitute another family."""
    from pathlib import Path
    import subprocess

    configured = os.environ.get("QBT_REPORT_FONT_DIR")
    directories = [Path(configured)] if configured else []
    if not configured:
        try:
            matched = subprocess.check_output(
                ["fc-match", "-f", "%{file}", "Times New Roman"],
                text=True, stderr=subprocess.DEVNULL,
            ).strip()
            if matched:
                directories.append(Path(matched).parent)
        except (OSError, subprocess.CalledProcessError):
            pass
        directories += [
            Path(os.environ.get("WINDIR", "C:/Windows")) / "Fonts",
            Path("/usr/share/fonts/truetype/msttcorefonts"),
            Path("/usr/share/fonts/TTF"),
            Path.home() / ".local/share/fonts",
        ]
    variants = [
        (SANS, "times.ttf"),
        (SANS_BOLD, "timesbd.ttf"),
        (SANS_ITALIC, "timesi.ttf"),
        ("TimesNewRoman-BoldItalic", "timesbi.ttf"),
    ]
    for directory in directories:
        if not directory.is_dir():
            continue
        files = {p.name.lower(): p for p in directory.rglob("*") if p.is_file()}
        if all(file in files for _, file in variants):
            for family, file in variants:
                font = TTFont(family, str(files[file]))
                if not font.face.name.startswith(b"TimesNewRoman"):
                    raise RuntimeError("The report requires actual Times New Roman fonts")
                pdfmetrics.registerFont(font)
            pdfmetrics.registerFontFamily(
                SANS, normal=SANS, bold=SANS_BOLD,
                italic=SANS_ITALIC, boldItalic="TimesNewRoman-BoldItalic",
            )
            return
    raise FileNotFoundError(
        "Times New Roman was not found. Install the family or set QBT_REPORT_FONT_DIR "
        "to the directory containing times.ttf, timesbd.ttf, timesi.ttf, timesbi.ttf. "
        "See scripts/report/README.md."
    )




class Styles:
    """All paragraph styles, built once from registered fonts."""

    def __init__(self):
        self.body = ParagraphStyle(
            "Body",
            fontName=SANS,
            bulletFontName=SANS,
            bulletFontSize=12,
            fontSize=12,
            leading=17,
            alignment=TA_JUSTIFY,
            spaceAfter=5,
        )
        self.body_left = ParagraphStyle("BodyLeft", parent=self.body, alignment=TA_LEFT)
        self.h1 = ParagraphStyle(
            "H1",
            fontName=SANS_BOLD,
            fontSize=16,
            leading=21,
            textColor=ACCENT,
            spaceBefore=16,
            spaceAfter=8,
            keepWithNext=True,
        )
        self.h2 = ParagraphStyle(
            "H2",
            fontName=SANS_BOLD,
            fontSize=13.5,
            leading=18,
            textColor=ACCENT,
            spaceBefore=11,
            spaceAfter=5,
            keepWithNext=True,
        )
        self.h3 = ParagraphStyle(
            "H3",
            fontName=SANS_BOLD,
            fontSize=12,
            leading=16,
            textColor=colors.black,
            spaceBefore=8,
            spaceAfter=3,
            keepWithNext=True,
        )
        self.bullet = ParagraphStyle(
            "Bullet",
            parent=self.body,
            leftIndent=14,
            bulletIndent=4,
            spaceAfter=2,
        )
        self.caption = ParagraphStyle(
            "Caption",
            fontName=SANS_ITALIC,
            fontSize=10.5,
            leading=13,
            alignment=TA_CENTER,
            textColor=GREY,
            spaceBefore=3,
            spaceAfter=10,
        )
        self.mono = ParagraphStyle(
            "Mono",
            fontName=MONO,
            fontSize=9.8,
            leading=12.2,
            alignment=TA_LEFT,
            leftIndent=8,
            backColor=colors.HexColor("#f4f6f8"),
            borderPadding=5,
            spaceBefore=4,
            spaceAfter=8,
        )
        self.cover_title = ParagraphStyle(
            "CoverTitle",
            fontName=SANS_BOLD,
            fontSize=25,
            leading=32,
            alignment=TA_CENTER,
            textColor=ACCENT,
            spaceAfter=6,
        )
        self.cover_sub = ParagraphStyle(
            "CoverSub",
            fontName=SANS,
            fontSize=13,
            leading=19,
            alignment=TA_CENTER,
            textColor=GREY,
        )
        self.cover_meta = ParagraphStyle(
            "CoverMeta",
            fontName=SANS,
            fontSize=10.5,
            leading=17,
            alignment=TA_CENTER,
        )
        self.toc_title = ParagraphStyle(
            "TocTitle",
            fontName=SANS_BOLD,
            fontSize=16,
            leading=21,
            textColor=ACCENT,
            spaceAfter=12,
        )
        self.toc1 = ParagraphStyle(
            "TOC1", fontName=SANS_BOLD, fontSize=10.5, leading=13, leftIndent=0
        )
        self.toc2 = ParagraphStyle(
            "TOC2", fontName=SANS, fontSize=10, leading=12, leftIndent=16
        )
        self.table_cell = ParagraphStyle(
            "Cell", fontName=SANS, fontSize=10, leading=12, alignment=TA_LEFT
        )
        self.table_head = ParagraphStyle(
            "CellHead",
            fontName=SANS_BOLD,
            fontSize=10,
            leading=12,
            alignment=TA_LEFT,
            textColor=colors.white,
        )


class ReportDoc(BaseDocTemplate):
    """A4 document with a running header/footer and a live table of contents."""

    def __init__(self, filename, title, **kwargs):
        self.report_title = title
        BaseDocTemplate.__init__(
            self,
            filename,
            pagesize=A4,
            leftMargin=20 * mm,
            rightMargin=20 * mm,
            topMargin=22 * mm,
            bottomMargin=18 * mm,
            title=title,
            author="Nhom 3 - PTIT",
            **kwargs,
        )
        frame = Frame(
            self.leftMargin,
            self.bottomMargin,
            self.width,
            self.height,
            id="body",
        )
        self.addPageTemplates(
            [
                PageTemplate(id="cover", frames=[frame], onPage=self._cover_page),
                PageTemplate(id="body", frames=[frame], onPage=self._body_page),
            ]
        )

    # ---------------------------------------------------------------- painting
    def _cover_page(self, canvas, doc):
        from reportlab.lib.styles import ParagraphStyle
        from reportlab.platypus import Paragraph
        from reportlab.pdfbase.ttfonts import TTFont
        from reportlab.pdfbase import pdfmetrics
        from reportlab.lib.enums import TA_CENTER, TA_LEFT
        from reportlab.lib.colors import HexColor, black

        canvas.saveState()
        navy = HexColor("#245783")
        def centered(text, top, size, font=SANS, color=black, width=505, leading=None):
            style=ParagraphStyle("cover", fontName=font, fontSize=size,
                                 leading=leading or size*1.25,
                                 textColor=color, alignment=TA_CENTER)
            p=Paragraph(text,style); _,height=p.wrap(width,150)
            p.drawOn(canvas,(A4[0]-width)/2,top-height)
            return height
        centered("HỌC VIỆN CÔNG NGHỆ BƯU CHÍNH VIỄN THÔNG<br/>"
                 "KHOA CÔNG NGHỆ THÔNG TIN 1", 768, 14,
                 SANS_BOLD,leading=17)
        canvas.setStrokeColor(navy)
        canvas.setLineWidth(.6)
        canvas.line(A4[0]/2-87, 714, A4[0]/2+87, 714)
        centered("HỌC PHẦN: HỆ THỐNG PHÂN TÁN", 654, 14,
                 SANS_BOLD, HexColor("#414141"))
        centered("BÀI TẬP LỚN", 627, 13.5, color=HexColor("#444444"))
        centered("BÁO CÁO BÀI TẬP LỚN", 559, 22, SANS_BOLD,navy)
        centered("Đề tài DS02", 526, 15, SANS_ITALIC,navy)
        centered("Hệ thống chia sẻ tệp P2P<br/>mô phỏng BitTorrent", 482, 17,
                 SANS_BOLD,leading=23)

        labelstyle=ParagraphStyle("cover-label",fontName=SANS_BOLD,
                                  fontSize=13,leading=16,alignment=TA_LEFT)
        valuestyle=ParagraphStyle("cover-value",fontName=SANS,
                                  fontSize=13,leading=16,alignment=TA_LEFT)
        rows=[("Tên hệ thống", "DS02 — Hệ thống chia sẻ tệp P2P<br/>mô phỏng BitTorrent"),
              ("Nhóm thực hiện", "Nhóm 11"),
              ("Lớp học phần", "INT1405_CLC-20261-01"),
              ("Ngày nộp", "22/11/2026")]
        top=390
        for label,value in rows:
            lp=Paragraph(label,labelstyle);vp=Paragraph(value,valuestyle)
            _,lh=lp.wrap(154,80);_,vh=vp.wrap(310,80)
            height=max(lh,vh)+10
            lp.drawOn(canvas,76,top-4-lh)
            vp.drawOn(canvas,242,top-4-vh)
            canvas.setStrokeColor(HexColor("#999999"))
            canvas.setLineWidth(.35);canvas.setDash(1,1.5)
            canvas.line(44,top-height,551,top-height)
            canvas.setDash()
            top-=height
        centered("Hà Nội, tháng 11 năm 2026", 81, 13.5, SANS_ITALIC)
        canvas.restoreState()

    def _body_page(self, canvas, doc):
        canvas.saveState()
        canvas.setFont(SANS, 9)
        canvas.setFillColor(GREY)
        canvas.drawString(doc.leftMargin, A4[1] - 13 * mm, self.report_title)
        canvas.drawRightString(
            A4[0] - doc.rightMargin, A4[1] - 13 * mm, "Hệ thống phân tán - DS02"
        )
        canvas.setStrokeColor(RULE)
        canvas.setLineWidth(0.4)
        canvas.line(
            doc.leftMargin,
            A4[1] - 15 * mm,
            A4[0] - doc.rightMargin,
            A4[1] - 15 * mm,
        )
        canvas.line(
            doc.leftMargin, 14 * mm, A4[0] - doc.rightMargin, 14 * mm
        )
        canvas.drawCentredString(A4[0] / 2.0, 8.5 * mm, str(doc.page))
        canvas.restoreState()

    # ------------------------------------------------------------------- toc
    def afterFlowable(self, flowable):
        """Feed headings into the TOC (multiBuild reruns until it converges)."""
        if not isinstance(flowable, Paragraph):
            return
        style = flowable.style.name
        if style not in ("H1", "H2"):
            return
        text = flowable.getPlainText()
        level = 0 if style == "H1" else 1
        key = "h%d-%s" % (level, text)
        self.canv.bookmarkPage(key)
        self.canv.addOutlineEntry(text, key, level=level, closed=False)
        self.notify(
            "TOCEntry",
            (level, text, self.page, key),
        )


def make_toc(styles):
    """A two-level TOC with right-aligned page numbers and a leader of dots."""
    toc = TableOfContents()
    toc.levelStyles = [styles.toc1, styles.toc2]
    # 0 means "leader dots on every level", which is what the dotted rule the
    # course template expects; negative would only dot the deep levels.
    toc.dotsMinLevel = 0
    return toc


# --------------------------------------------------------------------- flowables
def h1(text):
    return Paragraph(_inline(text), _styles().h1)


def h2(text):
    return Paragraph(_inline(text), _styles().h2)


def h3(text):
    return Paragraph(_inline(text), _styles().h3)


def p(text):
    return Paragraph(_escape(text), _styles().body)


def bullets(items):
    return [Paragraph(_inline(item), _styles().bullet, bulletText="•") for item in items]


def numbered(items):
    out = []
    for index, item in enumerate(items, start=1):
        out.append(
            Paragraph(_inline(item), _styles().bullet, bulletText="%d." % index)
        )
    return out


def code(text):
    lines = text.strip("\n").split("\n")
    escaped = "<br/>".join(_escape(line).replace(" ", "&nbsp;") for line in lines)
    return Paragraph(escaped, _styles().mono)


def caption(text):
    return Paragraph(_escape(text), _styles().caption)


def spacer(millimetres=3):
    return Spacer(1, millimetres * mm)


def _escape(text):
    """Escape the XML that Paragraph parses, so prose can use <, > and &."""
    return (
        str(text)
        .replace("&", "&amp;")
        .replace("<", "&lt;")
        .replace(">", "&gt;")
    )


def rich(text):
    """A paragraph with **bold**, *italic* and `monospace` inline markup.

    Escapes everything first, then turns the markup into ReportLab tags, so a
    literal '<' in the prose can never be mistaken for a tag.
    """
    import re

    escaped = _escape(text)
    escaped = re.sub(
        r"\*\*(.+?)\*\*", r'<font name="%s">\1</font>' % SANS_BOLD, escaped
    )
    escaped = re.sub(
        r"(?<!\*)\*([^*]+?)\*(?!\*)", r'<i>\1</i>', escaped
    )
    escaped = re.sub(
        r"`(.+?)`",
        r'<font name="%s" size="11" color="#243b53">\1</font>' % MONO,
        escaped,
    )
    return Paragraph(escaped, _styles().body)


def rich_bullets(items, style=None):
    style = style or _styles().bullet
    return [Paragraph(_inline(item), style, bulletText="•") for item in items]


def _inline(text):
    import re

    escaped = _escape(text)
    escaped = re.sub(
        r"\*\*(.+?)\*\*", r'<font name="%s">\1</font>' % SANS_BOLD, escaped
    )
    escaped = re.sub(r"(?<!\*)\*([^*]+?)\*(?!\*)", r"<i>\1</i>", escaped)
    escaped = re.sub(
        r"`(.+?)`",
        r'<font name="%s" size="11" color="#243b53">\1</font>' % MONO,
        escaped,
    )
    return escaped


_STYLES = None


def _styles():
    """One shared Styles instance; styles are stateless, building them is cheap."""
    global _STYLES
    if _STYLES is None:
        _STYLES = Styles()
    return _STYLES


def table(header, rows, widths=None, align=None):
    """A ruled, header-shaded table. Cells accept the same inline markup."""
    from reportlab.platypus import Table, TableStyle

    styles = _styles()
    data = [[Paragraph(_inline(cell), styles.table_head) for cell in header]]
    for row in rows:
        data.append([Paragraph(_inline(str(cell)), styles.table_cell) for cell in row])

    col_widths = None
    if widths:
        total = A4[0] - 40 * mm
        col_widths = [total * w for w in widths]

    tbl = Table(data, colWidths=col_widths, repeatRows=1, hAlign="LEFT")
    commands = [
        ("BACKGROUND", (0, 0), (-1, 0), ACCENT),
        ("VALIGN", (0, 0), (-1, -1), "TOP"),
        ("GRID", (0, 0), (-1, -1), 0.35, RULE),
        ("LINEBELOW", (0, 0), (-1, 0), 0.6, ACCENT),
        ("TOPPADDING", (0, 0), (-1, -1), 3),
        ("BOTTOMPADDING", (0, 0), (-1, -1), 3),
        ("LEFTPADDING", (0, 0), (-1, -1), 4),
        ("RIGHTPADDING", (0, 0), (-1, -1), 4),
    ]
    for index in range(1, len(data)):
        if index % 2 == 0:
            commands.append(("BACKGROUND", (0, index), (-1, index), colors.HexColor("#f2f5f9")))
    tbl.setStyle(TableStyle(commands))
    if align:
        tbl.hAlign = align
    return tbl


# ------------------------------------------------------------------- diagrams
def _box(d, x, y, w, h, lines, fill=ACCENT_LIGHT, stroke=ACCENT, size=8.0, bold=True):
    """A rounded box holding one or more centred lines of text.

    ReportLab's String does not break on newlines, so each line is its own
    String placed on a baseline below the previous one.
    """
    from reportlab.graphics.shapes import Rect, String, Group

    if isinstance(lines, str):
        lines = [lines]
    children = [
        Rect(x, y, w, h, fillColor=fill, strokeColor=stroke, strokeWidth=0.9, rx=2, ry=2)
    ]
    leading = size + 1.6
    total = leading * len(lines)
    top = y + h / 2.0 + total / 2.0 - leading / 2.0
    for index, line in enumerate(lines):
        children.append(
            String(
                x + w / 2.0,
                top - index * leading - size * 0.35,
                line,
                fontName=SANS_BOLD if bold else SANS,
                fontSize=size,
                fillColor=colors.HexColor("#12304d"),
                textAnchor="middle",
            )
        )
    return Group(*children)


def _arrow(d, x1, y1, x2, y2, label=None, dashed=False, colour=None, label_dy=3,
           label_pos=0.5):
    from reportlab.graphics.shapes import Line, String
    import math

    colour = colour or colors.HexColor("#3c5a7d")
    d.add(
        Line(
            x1,
            y1,
            x2,
            y2,
            strokeColor=colour,
            strokeWidth=0.9,
            strokeDashArray=[2, 2] if dashed else None,
        )
    )
    angle = math.atan2(y2 - y1, x2 - x1)
    head = 4.5
    for offset in (-0.4, 0.4):
        d.add(
            Line(
                x2,
                y2,
                x2 - head * math.cos(angle + offset),
                y2 - head * math.sin(angle + offset),
                strokeColor=colour,
                strokeWidth=0.9,
            )
        )
    if label:
        lx = x1 + (x2 - x1) * label_pos
        ly = y1 + (y2 - y1) * label_pos
        d.add(
            String(
                lx,
                ly + label_dy,
                label,
                fontName=SANS,
                fontSize=8,
                fillColor=GREY,
                textAnchor="middle",
            )
        )


def architecture_diagram(width, height):
    """Tracker + three peers, with the tracker-announce and peer-data paths."""
    from reportlab.graphics.shapes import Drawing, Line, String

    d = Drawing(width, height)
    d.add(
        Line(
            6,
            height - 6,
            width - 6,
            height - 6,
            strokeColor=RULE,
            strokeWidth=0.5,
            strokeDashArray=[3, 3],
        )
    )

    tracker_w, tracker_h = width * 0.42, height * 0.24
    tracker_x = (width - tracker_w) / 2.0
    tracker_y = height * 0.62
    d.add(
        _box(
            d,
            tracker_x,
            tracker_y,
            tracker_w,
            tracker_h,
            ["TrackerApp (HTTP, in-memory)", "POST /announce · GET /peers", "metadata only — never file bytes"],
            size=8.7,
        )
    )

    peer_w, peer_h = width * 0.23, height * 0.24
    peer_y = height * 0.10
    xs = [width * 0.04, width * 0.385, width * 0.73]
    labels = [["Peer 1", "(seeder)"], ["Peer 2", "(leecher → seeder)"], ["Peer 3", "(leecher)"]]
    for x, label in zip(xs, labels):
        d.add(_box(d, x, peer_y, peer_w, peer_h, label, size=8.7))

    # Control plane: every peer announces to the tracker (dashed).
    for x in xs:
        d.add(
            Line(
                x + peer_w / 2.0,
                peer_y + peer_h,
                tracker_x + tracker_w / 2.0,
                tracker_y,
                strokeColor=colors.HexColor("#9aa9bd"),
                strokeWidth=0.55,
                strokeDashArray=[2, 2],
            )
        )
    d.add(
        String(
            tracker_x + tracker_w + 10,
            (peer_y + peer_h + tracker_y) / 2.0,
            "announce / discover (HTTP)",
            fontName=SANS,
            fontSize=6.1,
            fillColor=GREY,
        )
    )

    # Data plane: chunk bytes move only peer-to-peer (solid).
    mid = peer_y + peer_h / 2.0
    _arrow(d, xs[0] + peer_w, mid, xs[1], mid, "TCP")
    _arrow(d, xs[1] + peer_w, mid, xs[2], mid, None)
    d.add(
        String(
            xs[1] + peer_w + (xs[2] - xs[1] - peer_w) / 2.0,
            mid - 7,
            "TCP",
            fontName=SANS,
            fontSize=6.1,
            fillColor=GREY,
            textAnchor="middle",
        )
    )
    # A curved long path from Peer 1 to Peer 3 hugs the bottom.
    d.add(
        Line(
            xs[0] + peer_w * 0.5,
            peer_y,
            xs[2] + peer_w * 0.5,
            peer_y,
            strokeColor=colors.HexColor("#3c5a7d"),
            strokeWidth=0.8,
        )
    )
    d.add(
        Line(
            xs[0] + peer_w * 0.5,
            peer_y,
            xs[0] + peer_w * 0.5,
            peer_y - 5,
            strokeColor=colors.HexColor("#3c5a7d"),
            strokeWidth=0.8,
        )
    )
    d.add(
        String(
            (xs[0] + xs[2] + peer_w) / 2.0,
            peer_y - 9,
            "chunk bytes directly between any two peers",
            fontName=SANS,
            fontSize=6.1,
            fillColor=GREY,
            textAnchor="middle",
        )
    )
    return d


def frame_diagram(width, height):
    """The length-prefixed wire frame on the byte grid."""
    from reportlab.graphics.shapes import Drawing, Rect, String

    d = Drawing(width, height)
    fields = [
        ("Length (4 B, big-endian)", 0.22, ACCENT_LIGHT),
        ("Msg ID (1 B)", 0.17, colors.HexColor("#e8eef6")),
        ("Payload (length - 1 B)", 0.61, ACCENT_LIGHT),
    ]
    x = 4
    y = height * 0.34
    h = height * 0.34
    usable = width - 8
    for label, fraction, fill in fields:
        w = usable * fraction
        d.add(Rect(x, y, w, h, fillColor=fill, strokeColor=ACCENT, strokeWidth=0.8))
        d.add(
            String(
                x + w / 2.0,
                y + h / 2.0 - 2.5,
                label,
                fontName=SANS,
                fontSize=7,
                fillColor=colors.HexColor("#12304d"),
                textAnchor="middle",
            )
        )
        x += w
    legend = [
        "0x00 HANDSHAKE · 0x01 BITFIELD · 0x02 REQUEST · 0x03 PIECE · 0x04 CANCEL",
        "0x05 KEEPALIVE · 0x06 INTERESTED · 0x07 NOT_INTERESTED · 0x08 CHOKE · 0x09 UNCHOKE",
    ]
    for index, line in enumerate(legend):
        d.add(
            String(
                4,
                y + h + 8 - index * 6.5,
                line,
                fontName=SANS,
                fontSize=5.9,
                fillColor=GREY,
            )
        )
    return d


def sequence_diagram(width, height):
    """Download handshake: leecher -> seeder, with the tracker in between."""
    from reportlab.graphics.shapes import Drawing, Line, String

    d = Drawing(width, height)
    lanes = [("Leecher", width * 0.14), ("Tracker", width * 0.5), ("Seeder", width * 0.86)]
    top = height - 8
    bottom = 6
    for name, x in lanes:
        d.add(_box(d, x - width * 0.075, top - 13, width * 0.15, 13, name, size=7))
        d.add(
            Line(x, top - 13, x, bottom, strokeColor=RULE, strokeWidth=0.6,
                 strokeDashArray=[2, 3])
        )

    steps = [
        (0, 1, "POST /announce (file_id, bitfield, port)"),
        (0, 1, "GET /peers?file_id"),
        (1, 0, "[{peer_id, ip, port, bitfield}]"),
        (0, 2, "HANDSHAKE(peer_id, file_id)"),
        (2, 0, "HANDSHAKE + BITFIELD"),
        (0, 2, "INTERESTED"),
        (2, 0, "UNCHOKE (slot granted)"),
        (0, 2, "REQUEST(i), pipelined"),
        (2, 0, "PIECE(i, data), repeat for missing chunks"),
    ]
    step_h = (top - 22 - bottom) / (len(steps) + 0.5)
    y = top - 20
    for a, b, label in steps:
        x1 = lanes[a][1]
        x2 = lanes[b][1]
        y -= step_h
        if x1 == x2:
            continue
        _arrow(d, x1, y, x2, y, colour=colors.HexColor("#3c5a7d"))
        d.add(
            String(
                min(x1, x2) + 4,
                y + 2.5,
                label,
                fontName=SANS,
                fontSize=8,
                fillColor=colors.HexColor("#243b53"),
            )
        )

    d.add(
        String(
            2,
            2,
            "Every PIECE is verified with SHA-256 against the metadata before it is written; "
            "a mismatch blacklists that peer for that chunk and requeues it.",
            fontName=SANS,
            fontSize=6.0,
            fillColor=GREY,
        )
    )
    return d


def peer_flow_diagram(width=470,height=125):
    from reportlab.graphics.shapes import Drawing,Line,String
    d=Drawing(width,height)
    gap=10;bw=(width-4*gap)/5;bh=48;y=30
    labels=[['TrackerClient','Tìm nguồn HTTP'],['DownloadManager','Tải + kiểm tra hash'],
            ['FileManager','Ghi theo offset'],['StateManager','Bitfield + SQLite'],
            ['UploadServer','Phục vụ mảnh tốt']]
    for i,label in enumerate(labels):
        x=i*(bw+gap)
        d.add(_box(d,x,y,bw,bh,label,size=8.4))
        if i<4:
            _arrow(d,x+bw,y+bh/2,x+bw+gap,y+bh/2)
    x1=3*(bw+gap)+bw/2;x2=bw/2;top=103
    d.add(Line(x1,y+bh,x1,top,strokeColor=RULE,strokeWidth=.8,strokeDashArray=[3,2]))
    _arrow(d,x1,top,x2,top)
    d.add(Line(x2,top,x2,y+bh,strokeColor=RULE,strokeWidth=.8,strokeDashArray=[3,2]))
    d.add(String((x1+x2)/2,top+5,'Announce khi có mảnh mới',fontName=SANS_ITALIC,
                 fontSize=9,textAnchor='middle',fillColor=GREY))
    d.add(String(width/2,7,'Các mô-đun thuộc cùng một tiến trình PeerApp; không phải các node riêng.',
                 fontName=SANS_ITALIC,fontSize=9,textAnchor='middle',fillColor=GREY))
    return d
