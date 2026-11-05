#!/usr/bin/env bash
# Regenerate the .txt and .pdf copies of the docs from their .md sources.
# Needs pandoc and wkhtmltopdf on PATH. Run from the repository root:
#   bash docs/make_docs.sh
set -euo pipefail
cd "$(dirname "$0")"

for doc in README DeepTrance_User_Guide; do
    # Plain text, wrapped at 90 columns.
    pandoc -f markdown -t plain --columns=90 "$doc.md" -o "$doc.txt"
    # PDF: standalone HTML with the embedded stylesheet, then wkhtmltopdf.
    title=$(head -1 "$doc.md" | sed 's/^# *//')
    pandoc -f markdown -t html5 -s --embed-resources --css pdf.css \
        --metadata title="$title" -V title= "$doc.md" -o "/tmp/$doc.html"
    wkhtmltopdf --quiet --enable-local-file-access -s Letter \
        -T 10mm -B 10mm -L 10mm -R 10mm "/tmp/$doc.html" "$doc.pdf"
    echo "wrote docs/$doc.txt and docs/$doc.pdf"
done
