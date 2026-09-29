#!/bin/sh
# SPDX-FileCopyrightText: 2026 calibancode
# SPDX-License-Identifier: CC0-1.0
#
# Extracts translatable strings into po/katwhisker.pot and merges them into
# every existing po/<lang>/katwhisker.po. Run from anywhere.
set -e
cd "$(dirname "$0")/.."

keywords="-ki18n:1 -ki18nc:1c,2 -ki18np:1,2 -ki18ncp:1c,2,3"
common="--from-code=UTF-8 --add-comments=TRANSLATORS --package-name=katwhisker --msgid-bugs-address=https://github.com/calibancode/katwhisker/issues"

xgettext $common $keywords -C -o po/cpp.pot src/*.cpp
# xgettext has no QML mode; QML's JavaScript parses fine as JavaScript.
xgettext $common $keywords -L JavaScript -o po/qml.pot src/qml/*.qml
msgcat --use-first -o po/katwhisker.pot po/cpp.pot po/qml.pot
rm po/cpp.pot po/qml.pot

for po in po/*/katwhisker.po; do
    [ -e "$po" ] || continue
    msgmerge --quiet --update --backup=none "$po" po/katwhisker.pot
done
