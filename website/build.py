# Copyright (c) 2026 Cyberalien
# SPDX-License-Identifier: GPL-3.0-only
"""Build an offline static showcase using Python 3.11+ and the standard library."""
import argparse
import hashlib
import html
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import struct
from urllib.parse import unquote, urlsplit

SOURCE = Path(__file__).resolve().parent
ROOT = SOURCE.parent
GUIDES = {
    'installation': 'INSTALLATION.md', 'logs': 'LOGS.md', 'metrics': 'METRICS.md',
    'files': 'FILES.md', 'mirroring': 'MIRRORING.md', 'plugins': 'PLUGINS.md',
    'assistant': 'AI_MCP_SDK.md', 'updates': 'UPDATES.md',
}
CHAPTERS = ('logs', 'metrics', 'device', 'files', 'plugins', 'assistant', 'setup', 'documentation')
escape = lambda value: html.escape(str(value), quote=True)


class References(HTMLParser):
    def __init__(self):
        super().__init__()
        self.links, self.ids = [], set()

    def handle_starttag(self, tag, attrs):
        for key, value in attrs:
            if key in {'href', 'src'} and value:
                self.links.append(value)
            if key == 'id':
                if value in self.ids:
                    raise ValueError('Duplicate HTML id: ' + value)
                self.ids.add(value)


def load_content():
    data = json.loads((SOURCE / 'content.json').read_text(encoding='utf-8'))
    if data['en'].keys() != data['fr'].keys():
        raise ValueError('English and French translation keys differ')
    for language in ('en', 'fr'):
        for key, value in data[language].items():
            if not isinstance(value, str) or not value.strip() or re.search(r'\b(?:TODO|TBD|Lorem ipsum)\b', value):
                raise ValueError(f'Empty or unfinished translation: {language}:{key}')
    for name in data['media']:
        for suffix in ('alt', 'caption'):
            if f'shot.{name}.{suffix}' not in data['en']:
                raise ValueError('Missing image translation: ' + name)
    version = (ROOT / 'VERSION').read_text(encoding='ascii').strip()
    if not re.fullmatch(r'\d+\.\d+\.\d+-alpha\.\d+', version):
        raise ValueError('Review the showcase status before changing release channel')
    for language in ('en', 'fr'):
        data[language] = {key: value.replace('{version}', version) for key, value in data[language].items()}
    data['links'] = {key: value.replace('{version}', version) for key, value in data['links'].items()}
    return data


def build(output):
    output = output.resolve()
    if output == ROOT or ROOT.is_relative_to(output) or output.is_relative_to(SOURCE):
        raise ValueError('Choose a separate generated output directory')
    manifest = output / 'showcase-build.json'
    if output.exists() and any(output.iterdir()) and not manifest.is_file():
        raise ValueError('Output is not an existing showcase build; choose an empty folder')
    previous = json.loads(manifest.read_text(encoding='utf-8'))['files'] if manifest.exists() else []
    output.mkdir(parents=True, exist_ok=True)
    files = []

    def write(name, content):
        path = output / name
        if not path.resolve().is_relative_to(output) or path.is_symlink():
            raise ValueError('Output reference leaves the artifact: ' + name)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content if isinstance(content, bytes) else content.encode('utf-8'))
        files.append(name)

    def asset(stem, suffix, content):
        name = f'assets/{stem}.{hashlib.sha256(content).hexdigest()[:12]}{suffix}'
        write(name, content)
        return './' + name

    data = load_content()
    media = {}
    for name, source in data['media'].items():
        path = (ROOT / source).resolve()
        if not path.is_relative_to(ROOT / 'docs/images'):
            raise ValueError('Only selected public screenshots may be bundled')
        content = path.read_bytes()
        if content[:8] != b'\x89PNG\r\n\x1a\n':
            raise ValueError('Screenshot must be a PNG: ' + source)
        width, height = struct.unpack('>II', content[16:24])
        media[name] = {'src': asset(name, '.png', content), 'width': width, 'height': height}
    logo = asset('questdev-logo', '.png', (ROOT / 'assets/branding/cyvr-questdev-logo-gradient.png').read_bytes())
    css = asset('showcase', '.css', (SOURCE / 'styles.css').read_bytes())
    payload = json.dumps({'copy': {lang: data[lang] for lang in ('en', 'fr')}}, ensure_ascii=False).replace('<', '\\u003c')
    script = asset('showcase', '.js', ('"use strict";\nconst SHOWCASE = ' + payload + ';\n' + (SOURCE / 'main.js').read_text(encoding='utf-8')).encode('utf-8'))

    def render(language, guides=False):
        def raw(key):
            return escape(data[language][key])

        def text(tag, key, attrs=''):
            return f'<{tag} data-i18n="{key}" {attrs}>{raw(key)}</{tag}>'

        def link(href, key, cls='', attrs=''):
            return f'<a class="{cls}" href="{escape(href)}" {attrs}>{text("span", key)}</a>'

        def guide(name, key):
            return link(f'./guides/docs/{GUIDES[name]}', key, 'text-link')

        def shot(name, eager=False):
            item = media[name]
            key = 'shot.' + name
            loading = 'fetchpriority="high"' if eager else 'loading="lazy"'
            return (f'<figure class="screen"><button class="shot" type="button" data-shot="{name}" '
                    f'aria-label="{raw("viewer.open")}: {raw(key + ".caption")}">'
                    f'<img src="{item["src"]}" width="{item["width"]}" height="{item["height"]}" '
                    f'alt="{raw(key + ".alt")}" data-i18n-alt="{key}.alt" {loading} decoding="async"></button>'
                    f'<figcaption data-i18n="{key}.caption">{raw(key + ".caption")}</figcaption></figure>')

        def heading(name):
            return text('p', name + '.kicker', 'class="eyebrow"') + text('h2', name + '.title', f'id="{name}-title"')

        def items(name):
            return '<ul class="feature-list">' + ''.join(text('li', f'{name}.item{i}') for i in (1, 2, 3)) + '</ul>'

        lang_buttons = (f'<div class="languages" role="group" aria-label="{raw("nav.language")}" data-i18n-label="nav.language">' +
                        ''.join(f'<button type="button" data-lang="{lang}" lang="{lang}" aria-pressed="{str(lang == language).lower()}">{lang.upper()}</button>' for lang in ('en', 'fr')) + '</div>')
        links = data['links']
        head = (f'<!doctype html><html lang="{language}"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">'
                '<meta name="color-scheme" content="dark"><meta name="theme-color" content="#111318">'
                f'<title>{raw("docs.indexTitle" if guides else "meta.title")}</title><meta name="description" content="{raw("meta.description")}">'
                f'<meta property="og:title" content="{raw("meta.title")}"><meta property="og:description" content="{raw("meta.description")}">'
                f'<link rel="icon" href="{logo}"><link rel="stylesheet" href="{css}"><script src="{script}" defer></script></head><body' + (' data-guide-index="true"' if guides else '') + '>')
        if guides:
            result = head + link('#main', 'skip', 'skip') + '<header class="site-header"><div class="wrap header-inner"><strong>CyVR QuestDev</strong>' + lang_buttons + '</div></header>'
            result += '<main id="main" class="wrap guide-index">' + text('h1', 'docs.indexTitle') + text('p', 'docs.format') + text('p', 'docs.body')
            result += '<div class="guide-grid">' + ''.join(f'<a class="guide-card" href="./docs/{file}">{text("strong", "docs." + name)}{text("span", "docs.open")}</a>' for name, file in GUIDES.items()) + '</div>'
            result += link('../index.html?lang=' + language + '#documentation', 'docs.back', 'button', 'data-back') + '</main></body></html>'
            return result.replace('href="./assets/', 'href="../assets/').replace('src="./assets/', 'src="../assets/')
        result = head + link('#main', 'skip', 'skip')
        result += f'<header class="site-header"><div class="wrap header-inner"><a class="brand" href="#main"><img src="{logo}" width="30" height="30" alt=""><span>CyVR QuestDev</span></a>'
        result += f'<nav id="primary-nav" aria-label="{raw("nav.features")}" data-i18n-label="nav.features">' + link('#logs', 'nav.features') + link('./guides/index.html?lang=' + language, 'nav.docs', attrs='data-guide-index-link') + link('#download', 'nav.download') + '</nav>'
        result += '<div class="header-actions">' + link(links['repository'], 'nav.github', 'github-link') + lang_buttons
        result += f'<button class="menu-button" type="button" aria-controls="primary-nav" aria-expanded="false" aria-label="{raw("nav.menu")}" data-i18n-label="nav.menu"><svg viewBox="0 0 24 24" aria-hidden="true"><path d="M4 6h16M4 12h16M4 18h16"/></svg></button></div></div></header>'
        result += '<main id="main"><section class="hero wrap" aria-labelledby="hero-title"><div class="hero-copy">' + text('p', 'hero.status', 'class="status"')
        result += f'<h1 id="hero-title"><img src="{logo}" width="54" height="54" alt=""><span>CyVR QuestDev</span></h1>' + text('h2', 'hero.lead') + text('p', 'hero.body', 'class="lead"')
        result += '<div class="actions">' + link(links['installer'], 'hero.action', 'button primary') + link('#logs', 'hero.explore', 'text-link') + '</div>' + text('p', 'hero.note', 'class="fineprint"') + '</div><div class="hero-screen">' + shot('console', True) + '</div></section>'
        result += '<nav class="chapters" aria-label="' + raw('nav.features') + '" data-i18n-label="nav.features"><div class="wrap">' + ''.join(link('#' + name, 'chapter.' + name) for name in CHAPTERS) + '</div></nav>'
        result += '<div class="wrap demo-note">' + text('p', 'demo', 'class="fineprint"') + '</div>'
        result += '<section id="logs" class="section wrap" aria-labelledby="logs-title"><div class="section-head">' + heading('logs') + text('p', 'logs.body') + '</div><div class="log-intro">' + items('logs') + guide('logs', 'logs.guide') + '</div>'
        result += '<div class="log-tabs" role="tablist" aria-label="' + raw('logs.tabs') + '" data-i18n-label="logs.tabs">'
        tabs = [('columns', 'sorted'), ('raw', 'raw'), ('compare', 'comparison')]
        for i, (tab, name) in enumerate(tabs):
            result += f'<button type="button" id="tab-{tab}" role="tab" aria-controls="panel-{tab}" aria-selected="{str(i == 0).lower()}" tabindex="{0 if i == 0 else -1}" data-log-tab="{tab}">' + text('span', 'logs.tab.' + tab) + '</button>'
        result += '</div>'
        for i, (tab, name) in enumerate(tabs):
            result += f'<div role="tabpanel" id="panel-{tab}" aria-labelledby="tab-{tab}" tabindex="0"' + (' hidden' if i else '') + '>' + shot(name) + '</div>'
        result += '</section><section id="metrics" class="section tinted" aria-labelledby="metrics-title"><div class="wrap feature-row"><div class="feature-copy">' + heading('metrics') + text('p', 'metrics.body') + text('p', 'metrics.detail') + guide('metrics', 'metrics.guide') + '</div>' + shot('metrics') + '</div></section>'
        result += '<section id="device" class="section wrap" aria-labelledby="device-title"><div class="section-head">' + heading('device') + text('p', 'device.body') + '</div>' + shot('device') + '<div class="two-up">'
        for name, image in [('apps', 'apps'), ('mirror', 'mirror')]:
            result += '<article>' + text('h3', f'device.{name}.title') + text('p', f'device.{name}.body') + shot(image) + '</article>'
        result += '</div>' + guide('mirroring', 'device.guide') + '</section>'
        result += '<section id="files" class="section tinted" aria-labelledby="files-title"><div class="wrap"><div class="section-head">' + heading('files') + text('p', 'files.body') + '</div>' + shot('files') + items('files') + guide('files', 'files.guide') + '</div></section>'
        result += '<section id="plugins" class="section wrap" aria-labelledby="plugins-title"><div class="section-head">' + heading('plugins') + text('p', 'plugins.body') + '</div><div class="two-up">'
        for name, image in [('ovr', 'ovr'), ('insight', 'insight')]:
            result += '<article>' + text('h3', f'plugins.{name}.title') + text('p', f'plugins.{name}.body') + shot(image) + '</article>'
        result += '</div>' + guide('plugins', 'plugins.guide') + '</section>'
        result += '<section id="assistant" class="section tinted" aria-labelledby="assistant-title"><div class="wrap feature-row"><div class="feature-copy">' + heading('assistant') + text('p', 'assistant.body') + text('p', 'assistant.detail') + text('p', 'assistant.policy', 'class="fineprint"') + guide('assistant', 'assistant.guide') + '</div>' + shot('assistant') + '</div></section>'
        result += '<section id="setup" class="section wrap" aria-labelledby="setup-title"><div class="section-head">' + heading('setup') + text('p', 'setup.body') + '</div><div class="two-up">'
        for name, image in [('logs', 'retention'), ('updates', 'updates')]:
            result += '<article>' + text('h3', f'setup.{name}.title') + text('p', f'setup.{name}.body') + shot(image) + '</article>'
        result += '</div>' + guide('updates', 'setup.guide') + '</section>'
        result += '<section id="documentation" class="section tinted" aria-labelledby="docs-title"><div class="wrap documentation">' + heading('docs') + text('p', 'docs.body') + link('./guides/index.html?lang=' + language, 'docs.action', 'button', 'data-guide-index-link') + '</div></section>'
        result += '<section class="section wrap faq" aria-labelledby="faq-title">' + text('h2', 'faq.title', 'id="faq-title"') + '<div>' + ''.join('<details><summary>' + text('span', f'faq.q{i}') + '</summary>' + text('p', f'faq.a{i}') + '</details>' for i in (1, 2, 3)) + '</div></section>'
        result += '<section id="download" class="section wrap download" aria-labelledby="download-title">' + heading('download') + text('p', 'download.body') + '<div class="actions">' + link(links['installer'], 'download.installer', 'button primary') + link(links['portable'], 'download.portable', 'button') + '</div>' + link(links['release'], 'download.release', 'text-link') + text('p', 'download.note', 'class="fineprint"') + '</section></main>'
        result += '<footer class="wrap site-footer"><div><strong>CyVR QuestDev</strong>' + text('p', 'footer.copyright', 'class="fineprint"') + text('p', 'footer.license', 'class="fineprint"') + '</div><div class="footer-links">' + link('./guides/index.html?lang=' + language, 'nav.docs', attrs='data-guide-index-link') + link(links['repository'], 'nav.github') + '<a href="./LICENSE.txt">GPL-3.0-only</a>' + link('./THIRD_PARTY.md', 'footer.notices') + '</div></footer>'
        result += '<dialog id="image-viewer" aria-labelledby="viewer-caption"><div class="viewer-controls"><button type="button" data-viewer-prev aria-label="' + raw('viewer.previous') + '" data-i18n-label="viewer.previous">←</button><button type="button" data-viewer-next aria-label="' + raw('viewer.next') + '" data-i18n-label="viewer.next">→</button><button type="button" data-viewer-close>' + text('span', 'viewer.close') + ' ×</button></div><img id="viewer-image" alt=""><p id="viewer-caption" aria-live="polite"></p>' + text('p', 'viewer.help', 'class="fineprint"') + '</dialog></body></html>'
        return result

    write('index.html', render('en'))
    write('fr.html', render('fr'))
    write('guides/index.html', render('en', True))
    # Preserve the actual documentation format and its relative image links.
    for filename in GUIDES.values():
        content = (ROOT / 'docs' / filename).read_text(encoding='utf-8')
        # The developer's packaging guide belongs to the source repository.
        # Keep user guides local without dragging its build/audit tree into the site.
        content = content.replace('](RELEASE.md)', '](' + data['links']['repository'] + '/blob/main/docs/RELEASE.md)')
        write('guides/docs/' + filename, content)
    for image in (ROOT / 'docs/images').glob('*.png'):
        write('guides/docs/images/' + image.name, image.read_bytes())
    for name in ('LICENSE', 'COPYRIGHT'):
        write(name, (ROOT / name).read_bytes())
        write(name + '.txt', (ROOT / name).read_bytes())
    write('runtime/requirements.lock', (ROOT / 'runtime/requirements.lock').read_bytes())
    write('guides/examples/download_file.py', (ROOT / 'examples/download_file.py').read_bytes())
    for name in ('THIRD_PARTY.md',):
        write(name, (ROOT / name).read_bytes())
    for folder in ('licenses', 'LICENCES'):
        for path in (ROOT / folder).iterdir():
            if path.is_file():
                write(folder + '/' + path.name, path.read_bytes())
    # Every emitted HTML reference is checked, including anchors, under a subpath.
    documents = {}
    for name in files:
        if name.endswith('.html'):
            parsed = References()
            parsed.feed((output / name).read_text(encoding='utf-8'))
            documents[name] = parsed
    links_checked = 0
    for name, parsed in documents.items():
        for reference in parsed.links:
            parts = urlsplit(reference)
            if parts.scheme or parts.netloc:
                if parts.scheme != 'https':
                    raise ValueError('Unsupported external link: ' + reference)
                continue
            if parts.path.startswith('/'):
                raise ValueError('Root-relative links break static subpaths: ' + reference)
            target = ((output / name).parent / unquote(parts.path)).resolve() if parts.path else output / name
            if not target.is_relative_to(output) or not target.is_file():
                raise ValueError(f'Broken local link in {name}: {reference}')
            if parts.fragment and target.suffix == '.html' and unquote(parts.fragment) not in documents[target.relative_to(output).as_posix()].ids:
                raise ValueError('Broken anchor: ' + reference)
            links_checked += 1
    for name in files:
        if not name.endswith('.md'):
            continue
        for reference in re.findall(r'\]\(([^)]+)\)', (output / name).read_text(encoding='utf-8')):
            parts = urlsplit(reference)
            if parts.scheme or not parts.path:
                continue
            target = ((output / name).parent / unquote(parts.path)).resolve()
            if not target.is_relative_to(output) or not target.is_file():
                raise ValueError(f'Broken Markdown link in {name}: {reference}')
            links_checked += 1
    for name in previous:
        path = (output / name).resolve()
        if name not in files and path.is_relative_to(output) and path.is_file() and not path.is_symlink():
            path.unlink()  # Remove only files owned by the previous build manifest.
    write('showcase-build.json', json.dumps({'files': sorted(files), 'languages': ['en', 'fr'], 'screenshots': len(media)}, indent=2))
    print(json.dumps({'entry': str(output / 'index.html'), 'translations': len(data['en']), 'screenshots': len(media), 'localReferences': links_checked, 'files': len(files)}, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT / 'dist/website')
    args = parser.parse_args()
    build(args.output)
