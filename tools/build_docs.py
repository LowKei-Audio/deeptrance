#!/usr/bin/env python3
"""Regenerates README.txt and README.pdf from README.md (LowKei house style).

Requires pandoc, wkhtmltopdf (run under xvfb-run on headless Linux) and
beautifulsoup4. Usage, from the repo root:  python3 tools/build_docs.py
"""
import pathlib
import re
import subprocess
import sys
import tempfile

from bs4 import BeautifulSoup

ROOT = pathlib.Path(__file__).resolve().parent.parent

# House style: black body text, dark code blocks with white text, navy table headers.
CSS = """
body { font-family: "DejaVu Sans", Arial, sans-serif; color: #111; font-size: 11pt; line-height: 1.5; margin: 0 6px; }
h1 { font-size: 26pt; margin-bottom: 4px; color: #111; }
h2 { font-size: 16pt; border-bottom: 2px solid #1e2a4a; padding-bottom: 3px; margin-top: 26px; color: #111; }
h3 { font-size: 13pt; margin-top: 18px; color: #111; }
p, li { color: #111; }
a { color: #1e3a8a; text-decoration: none; }
code { background: #1a1d27; color: #fff; padding: 1px 5px; border-radius: 3px; font-family: "DejaVu Sans Mono", monospace; font-size: 9.5pt; }
pre { background: #1a1d27; color: #fff; padding: 12px 14px; border-radius: 6px; page-break-inside: avoid; }
pre code { background: none; padding: 0; color: #fff; }
table { border-collapse: collapse; width: 100%; margin: 10px 0 16px; page-break-inside: avoid; }
th { background: #1e2a4a; color: #fff; text-align: left; padding: 7px 9px; font-size: 10pt; }
td { border-bottom: 1px solid #d1d5db; padding: 6px 9px; vertical-align: top; color: #111; font-size: 10pt; }
img { max-width: 100%; border-radius: 8px; margin: 8px 0; page-break-inside: avoid; }
hr { border: none; border-top: 1px solid #d1d5db; margin: 20px 0; }
"""


def run(cmd):
    subprocess.run(cmd, check=True, stdin=subprocess.DEVNULL)


def main() -> int:
    md = ROOT / "README.md"
    run(["pandoc", str(md), "-t", "plain", "--wrap=auto", "--columns=90", "-o", str(ROOT / "README.txt")])

    with tempfile.TemporaryDirectory() as tmp:
        css = pathlib.Path(tmp) / "style.css"
        css.write_text(CSS)
        html = pathlib.Path(tmp) / "README.html"
        run(["pandoc", str(md), "-s", "--metadata", "pagetitle=README", "-c", str(css), "-o", str(html)])

        text = html.read_text()
        text = re.sub(r'<header id="title-block-header">.*?</header>', "", text, flags=re.S)
        text = text.replace('src="docs/', f'src="file://{ROOT}/docs/')
        soup = BeautifulSoup(text, "html.parser")
        # Keep every heading on the same page as what follows it. h3 first, so
        # an h2 directly above an h3 then binds to the h3's whole block.
        # Label paragraphs ("**Project layout:**") count as headings too.
        labels = [p for p in soup.find_all("p")
                  if p.get_text(strip=True).endswith(":") and p.find("strong") is not None
                  and p.get_text(strip=True) == p.find("strong").get_text(strip=True)]
        for tag in ("label", "h3", "h2"):
            for h in (labels if tag == "label" else soup.find_all(tag)):
                nxt = h.find_next_sibling()
                if nxt is None:
                    continue
                wrap = soup.new_tag("div", style="page-break-inside: avoid;")
                h.insert_before(wrap)
                wrap.append(h.extract())
                wrap.append(nxt.extract())
        html.write_text(str(soup))

        prefix = ["xvfb-run", "-a"] if sys.platform.startswith("linux") else []
        run(prefix + ["wkhtmltopdf", "--enable-local-file-access", "-q", "--page-size", "Letter",
                      "-T", "14mm", "-B", "14mm", "-L", "14mm", "-R", "14mm", str(html), str(ROOT / "README.pdf")])
    print("Wrote README.txt and README.pdf")
    return 0


if __name__ == "__main__":
    sys.exit(main())
