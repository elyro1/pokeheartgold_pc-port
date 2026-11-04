#!/usr/bin/env python3
"""Copy reviewed fixed-width assembly constants into host assembler syntax.

Reject stored relocations, storage reservations and data expressions:
pointer-bearing data and runtime allocations need typed native definitions.
Aliases may refer to a local data label plus a literal byte offset; they store
no pointers and preserve the overlapping constant tables used by the DS game.

A terminal DS `.data` block is accepted only for labels listed in
pc/fixed_data_readonly.json after review of every use: C reads them through
const declarations and no code writes them. They are emitted after the
`.rodata` bytes, matching the DS section order.

Pointer tables in the middle of a file are omitted by name through
pc/fixed_data_typed_tables.json after the converter checks their complete
word list and `.size` terminator; typed C supplies the native-width symbols.
Everything around them keeps its literal bytes. `.global` is treated like
`.public` and `.size` is dropped.
"""
import argparse
import ast
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[2]


def reviewed_sources():
    return json.loads((ROOT / 'pc/fixed_data_sources.json').read_text()) + list(typed_tails())


def typed_tails():
    return json.loads((ROOT / 'pc/fixed_data_typed_tails.json').read_text())


def readonly_data(name):
    """Labels in a terminal DS .data block that were reviewed as never written."""
    return frozenset(json.loads((ROOT / 'pc/fixed_data_readonly.json').read_text()).get(name, []))


INCLUDE_DIRS = ['include', 'include/library', 'files', 'pc/include', 'pc/generated']


def preprocess(source, generated=None):
    """Expand `#include "constants/..."` the way the DS build's C preprocessor does.

    Only sources that include constant headers are run through cpp; includes of
    headers that are not in the tree (generated message IDs) are dropped first.
    The result must still be literal data: the converter rejects anything cpp
    leaves symbolic."""
    if not re.search(r'^#include\s+"(?:constants/|msgdata/)', source, re.M):
        return source
    dirs = ([Path(generated)] if generated is not None else []) + [ROOT / d for d in INCLUDE_DIRS if (ROOT / d).is_dir()]

    def keep(match):
        return match[0] if any((d / match[1]).is_file() for d in dirs) else ''
    source = re.sub(r'^#include\s+"([^"]+)"[^\n]*$', keep, source, flags=re.M)
    result = subprocess.run(['cpp', '-undef', '-x', 'assembler-with-cpp'] + ['-I' + str(d) for d in dirs] + ['-'],
                            input=source, capture_output=True, text=True)
    if result.returncode != 0:
        raise ValueError('preprocessing failed: ' + result.stderr.strip().splitlines()[-1])
    # Keep only the assembly file's own lines: the constant headers also hold C declarations.
    lines, current = [], None
    for line in result.stdout.splitlines():
        marker = re.match(r'# \d+ "([^"]*)"', line)
        if marker:
            current = marker[1]
        elif current == '<stdin>':
            lines.append(line)
    return '\n'.join(lines) + '\n'


def selected_constants(name, source):
    """Keep reviewed literal blocks from files whose other data is defined in C."""
    selected = json.loads((ROOT / 'pc/fixed_data_subsets.json').read_text()).get(name)
    if selected is None:
        return source
    parts = re.split(r'^([\w$]+):[^\n]*\n', source, flags=re.M)
    blocks = dict(zip(parts[1::2], parts[2::2]))
    output = ['.rodata']
    previous = False
    for label in parts[1::2]:
        if label not in selected:
            previous = False
            continue
        if not previous:
            output.append('.balign 8')
        output += ['.public ' + label, label + ':']
        previous = True
        for line in blocks[label].splitlines():
            if re.match(r'\s*\.(?:byte|short|word|ascii|asciz|space|balign)\s', line):
                output.append(line)
    return '\n'.join(output) + '\n'


def typed_tables(name):
    """Reviewed native-defined regions to omit from the literal data: {symbol: entries}.

    Entries are the table's pointer words, one `.space N` reservation, or
    `b:XX` literal bytes of native-defined data.
    An "items" list keeps the order of mixed bytes and words."""
    entry = json.loads((ROOT / 'pc/fixed_data_typed_tables.json').read_text()).get(name, {})
    return {symbol: table['items'] if 'items' in table else
            table.get('words', []) + (['.space %d' % table['space']] if 'space' in table else [])
            + ['b:' + table['bytes'][i:i + 2] for i in range(0, len(table.get('bytes', '')), 2)]
            for symbol, table in entry.items()}


def table_size(entries):
    """Bytes an omitted table or storage reservation occupied in the DS image."""
    return sum(int(entry.split()[1]) if entry.startswith('.space ') else 1 if entry.startswith('b:') else 4
               for entry in entries)


def extra_exports(name):
    """Local labels that native C references, so the converted object must export them."""
    return frozenset(json.loads((ROOT / 'pc/fixed_data_exports.json').read_text()).get(name, []))


def literal_prefix(name, source):
    """Split only an explicitly reviewed terminal callback table.

    Check the complete tail so later additions cannot silently disappear from
    the native data. Its public symbol is supplied by typed C instead. A tail
    marked "section": ".data" must begin its own `.data` block (the DS keeps
    such tables writable although the C code never writes them); the typed C
    definition is const.
    """
    tail = typed_tails().get(name)
    if tail is None:
        return source
    prefix, separator, actual = strip_comments(source).partition(tail['symbol'] + ':')
    if 'lines' in tail:
        # Mixed tail (pointer words plus fixed bytes): compare every directive.
        matches = separator and [' '.join(line.split()) for line in actual.splitlines() if line.strip()] == tail['lines']
    else:
        words = []
        for line in actual.strip().splitlines():
            match = re.fullmatch(r'\s*\.word\s+(.+?)\s*', line)
            if match is None:
                words = None
                break
            words.extend(token.strip() for token in match[1].split(','))
        matches = separator and words == tail['words']
    if not matches:
        raise ValueError(f'{name}: reviewed terminal callback table changed')
    section = tail.get('section')
    if section is not None:
        lines = prefix.rstrip().splitlines()
        if section != '.data' or not lines or lines[-1].strip() != section:
            raise ValueError(f'{name}: reviewed terminal callback table left its section')
        prefix = '\n'.join(lines[:-1]) + '\n'
    return prefix


def strip_comments(source):
    lines = []
    for raw in source.splitlines():
        quoted = escaped = False
        for i, char in enumerate(raw):
            if escaped:
                escaped = False
            elif char == '\\' and quoted:
                escaped = True
            elif char == '"':
                quoted = not quoted
            elif not quoted and (char == ';' or raw[i:i + 2] == '//'):
                raw = raw[:i]
                break
        lines.append(raw)
    return '\n'.join(lines)


def literal_expression(token):
    """Evaluate a pure integer-literal expression (+ - * << >> | & ~ and parentheses), else None."""
    if not re.fullmatch(r'[0-9a-fA-FxX\s+\-*|&~<>()]+', token) or re.search(r'[A-Za-z_]\w*', re.sub(r'0[xX][0-9a-fA-F]+', '', token)):
        return None
    try:
        tree = ast.parse(token.strip(), mode='eval')
    except SyntaxError:
        return None

    def walk(node):
        if isinstance(node, ast.Expression):
            return walk(node.body)
        if isinstance(node, ast.Constant) and type(node.value) is int:
            return node.value
        if isinstance(node, ast.UnaryOp) and isinstance(node.op, (ast.USub, ast.Invert)):
            value = walk(node.operand)
            return None if value is None else (-value if isinstance(node.op, ast.USub) else ~value)
        if isinstance(node, ast.BinOp):
            ops = {ast.Add: lambda a, b: a + b, ast.Sub: lambda a, b: a - b, ast.Mult: lambda a, b: a * b,
                   ast.BitOr: lambda a, b: a | b, ast.BitAnd: lambda a, b: a & b,
                   ast.LShift: lambda a, b: a << b if 0 <= b < 64 else None, ast.RShift: lambda a, b: a >> b if 0 <= b < 64 else None}
            left, right = walk(node.left), walk(node.right)
            if left is None or right is None or type(node.op) not in ops:
                return None
            return ops[type(node.op)](left, right)
        return None
    return walk(tree)


def expand_macros(source):
    """Expand simple `.macro name a, b` ... `.endm` definitions by textual substitution of \\a, \\b."""
    lines, macros, out, current = source.splitlines(), {}, [], None
    for line in lines:
        stripped = line.strip()
        header = re.fullmatch(r'\.macro\s+(\w+)\s*(.*)', stripped)
        if header and current is None:
            current = (header[1], [a.strip() for a in header[2].split(',') if a.strip()], [])
            continue
        if stripped == '.endm' and current is not None:
            macros[current[0]] = current[1:]
            current = None
            continue
        if current is not None:
            current[2].append(line)
            continue
        call = re.match(r'\s*(\w+)(?:\s+(.*))?$', line)
        if call and call[1] in macros:
            params, body = macros[call[1]]
            values = [v.strip() for v in call[2].split(',')] if call[2] else []
            if len(values) != len(params):
                raise ValueError(f'macro {call[1]} called with {len(values)} arguments, expects {len(params)}')
            for row in body:
                for name, value in zip(params, values):
                    row = row.replace('\\' + name, value)
                out.append(row)
            continue
        out.append(line)
    if current is not None:
        raise ValueError('unterminated .macro')
    return '\n'.join(out)


def convert(source, reviewed_readonly=frozenset(), omitted=None, exports=frozenset(), platform='linux'):
    omitted = omitted or {}
    source = expand_macros(strip_comments(source))
    data_labels = set(re.findall(r"^([A-Za-z_]\w*):\s*$", source, re.M))
    alias_pattern = r'\.set\s+([A-Za-z_]\w*),\s*([A-Za-z_]\w*)(?:\s*([+-])\s*(0x[0-9a-fA-F]+|[0-9]+))?'
    aliases = re.findall(r'^\s*' + alias_pattern + r'\s*$', source, re.M)
    alias_names = [alias[0] for alias in aliases]
    if len(alias_names) != len(set(alias_names)) or set(alias_names) & data_labels:
        raise ValueError('constant aliases must have unique names distinct from data labels')
    labels = data_labels | set(alias_names)
    rodata_output = ['.section .rdata,"dr"' if platform == 'win64' else '.section .rodata', '.balign 4']
    data_output = []
    output = rodata_output
    exported = set()
    in_data = False
    data_seen = set()
    skipping = None
    skipped_words = []
    in_bss = False
    data_labelled = False
    omitted_seen = set()
    if set(omitted) & set(alias_names):
        raise ValueError('typed tables cannot be alias names')
    if any(target in omitted for _, target, _, _ in aliases):
        raise ValueError('constant aliases cannot target typed tables')
    for number, raw in enumerate(source.splitlines(), 1):
        line = raw.strip()
        if re.match(r'\.(byte|short|word)\s', line):
            line = line.rstrip().rstrip(',').rstrip()  # tolerate a trailing comma
        if skipping is not None:
            word = re.fullmatch(r'\.word\s+(.+)', line)
            space = re.fullmatch(r'\.space\s+(0x[0-9a-fA-F]+|[0-9]+)', line)
            byte = re.fullmatch(r'\.byte\s+(.+)', line)
            if byte:
                for token in byte[1].split(','):
                    token = token.strip()
                    if not re.fullmatch(r'0x[0-9a-fA-F]{1,2}|\d{1,3}', token) or int(token, 0) > 255:
                        raise ValueError(f'line {number}: non-literal byte in typed table: {token}')
                    skipped_words.append('b:%02x' % int(token, 0))
                continue
            if word:
                skipped_words.extend(token.strip() for token in word[1].split(','))
                continue
            short = re.fullmatch(r'\.short\s+(.+)', line)
            if short:
                for token in short[1].split(','):
                    value = literal_expression(token.strip())
                    if value is None or not -(1 << 15) <= value < 1 << 16:
                        raise ValueError(f'line {number}: non-literal short in typed table: {token}')
                    skipped_words.extend('b:%02x' % byte for byte in (value & 0xFFFF).to_bytes(2, 'little'))
                continue
            align = re.fullmatch(r'\.balign\s+(\d+)(?:\s*,\s*(\d+))?', line)
            if align and int(align[1]) in (2, 4, 8, 16):
                boundary, fill = int(align[1]), int(align[2] or 0)
                size = table_size(skipped_words)
                skipped_words.extend('b:%02x' % fill for _ in range(-size % boundary))
                continue
            if space:
                skipped_words.append('.space ' + str(int(space[1], 0)))
                continue
            if not line:
                continue
            if skipped_words != omitted[skipping]:
                raise ValueError(f'line {number}: reviewed typed table {skipping} changed')
            omitted_seen.add(skipping)
            ended = skipping
            skipping = None
            # A terminating .size belongs to the table; anything else is processed normally.
            if re.fullmatch(r'\.size\s+' + ended + r'\s*,\s*\.-' + ended, line):
                continue
        if line in ('.text', '.rodata', '.data', '.bss'):
            # Sections may appear in any order in the source; the generated object keeps the
            # DS image order: literal rodata first, then reviewed read-only .data (typed
            # .bss storage emits nothing).
            in_bss = line == '.bss'
            in_data = line == '.data'
            if in_data:
                data_labelled = False
            output = data_output if in_data else rodata_output
            continue
        if in_bss and line.startswith('.') and not line.startswith(('.public', '.global', '.extern', '.size')):
            raise ValueError(f'line {number}: reviewed typed storage must hold only omitted labels')
        if not line or line.startswith('#include ') or line.startswith('.include ') or line in ('.text', '.rodata'):
            continue
        if re.fullmatch(r'\.size\s+[A-Za-z_]\w*\s*,\s*\.-[A-Za-z_]\w*', line):
            continue
        public = re.fullmatch(r'\.(?:public|global) ([A-Za-z_]\w*)', line)
        if public:
            name = public[1]
            if name in labels and name not in exported and name not in omitted:
                output.append('.global ' + name)
                exported.add(name)
            continue
        if re.fullmatch(r'\.extern [A-Za-z_]\w*', line):
            continue
        if re.fullmatch(r'[A-Za-z_]\w*:', line):
            if line[:-1] in omitted:
                skipping, skipped_words = line[:-1], []
                continue
            if in_bss:
                raise ValueError(f'line {number}: {line[:-1]} in .bss needs typed native storage')
            data_labelled = True
            if in_data:
                if line[:-1] not in reviewed_readonly:
                    raise ValueError(f'line {number}: {line[:-1]} was not reviewed as read-only')
                data_seen.add(line[:-1])
            elif line[:-1] in reviewed_readonly:
                raise ValueError(f'line {number}: {line[:-1]} moved out of the reviewed .data block')
            output.append(line)
            if line[:-1] in exports and line[:-1] not in exported:
                output.append('.global ' + line[:-1])
                exported.add(line[:-1])
            continue
        alias = re.fullmatch(alias_pattern, line)
        if alias:
            name, target, sign, offset = alias.groups()
            if target not in data_labels:
                raise ValueError(f'line {number}: constant alias must target a local data label')
            expression = target + (f' {sign} {offset}' if sign else '')
            output.append(f'.set {name}, {expression}')
            continue
        align = re.fullmatch(r'\.balign\s+(\d+)(?:,\s*(\d+))?', line)
        if align:
            boundary, fill = int(align[1]), int(align[2] or 0)
            if boundary < 1 or boundary > 4096 or boundary & (boundary - 1) or not 0 <= fill <= 255:
                raise ValueError(f'line {number}: invalid alignment or fill')
            output.append(f'.balign {boundary}, {fill}')
            continue
        # Fixed-size padding after a string: `.space N-(.-label)` or `.space N-(label-.)`.
        pad = re.fullmatch(r'\.space\s+(\d+)\s*-\s*\(\s*(?:\.\s*-\s*([A-Za-z_]\w*)|([A-Za-z_]\w*)\s*-\s*\.)\s*\)', line)
        if pad:
            target = pad[2] or pad[3]
            if target not in data_labels or int(pad[1]) > 4096:
                raise ValueError(f'line {number}: padding must refer to a local data label')
            output.append(f'.space {pad[1]}-(' + ('.-' + target if pad[2] else target + '-.') + ')')
            continue
        zero = re.fullmatch(r'\.space\s+([0-9a-fA-FxX \t+*()\-]+)', line)
        if zero and not in_bss and literal_expression(zero[1]) is not None and 0 <= literal_expression(zero[1]) <= 4096:
            output.append('.zero ' + str(literal_expression(zero[1])))
            continue
        # Keep assembler string escapes intact; accept only literal strings.
        if re.fullmatch(r'\.(?:ascii|asciz)\s+"(?:[^"\\\n]|\\[\\"nt]|\\x[0-9a-fA-F]{2}|\\[0-7]{1,3})*"', line):
            output.append(line)
            continue
        floats = re.fullmatch(r'\.float\s+(.+)', line)
        if floats:
            tokens = [token.strip() for token in floats[1].split(',')]
            if not all(re.fullmatch(r'-?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][+-]?\d+)?', token) for token in tokens):
                raise ValueError(f'line {number}: non-literal float: {floats[1]}')
            output.append('.float ' + ', '.join(tokens))
            continue
        data = re.fullmatch(r'\.(byte|short|word)\s+(.+)', line)
        if data and in_data and not data_labelled:
            raise ValueError(f'line {number}: data without a reviewed label in .data')
        if data:
            width = {'byte': 8, 'short': 16, 'word': 32}[data[1]]
            values = []
            for token in data[2].split(','):
                token = token.strip()
                # Integer-literal arithmetic (flags, offsets) is a constant, not a pointer.
                value = literal_expression(token)
                if value is None:
                    raise ValueError(f'line {number}: non-literal constant: {token}')
                if not -(1 << (width - 1)) <= value < 1 << width:
                    raise ValueError(f'line {number}: value does not fit {width} bits')
                values.append(str(value))
            directive = 'long' if width == 32 else data[1]
            output.append('.' + directive + ' ' + ', '.join(values))
            continue
        raise ValueError(f'line {number}: unsupported fixed data statement: {line}')
    if skipping is not None:
        if skipped_words != omitted[skipping]:
            raise ValueError(f'reviewed typed table {skipping} changed')
        omitted_seen.add(skipping)
        skipping = None
    if omitted_seen != set(omitted):
        raise ValueError('reviewed typed tables changed: ' + ', '.join(sorted(set(omitted) - omitted_seen)))
    if not set(exports) <= (data_labels | set(alias_names)) | set(omitted):
        raise ValueError('exported labels missing: ' + ', '.join(sorted(set(exports) - data_labels)))
    if data_seen != reviewed_readonly:
        raise ValueError('reviewed read-only data labels changed: ' + ', '.join(sorted(data_seen ^ reviewed_readonly)))
    rodata_output.extend(data_output)
    if platform != 'win64':
        rodata_output.append('.section .note.GNU-stack,"",@progbits')
    return '\n'.join(rodata_output) + '\n'


def convert_source(name, source, generated, platform='linux'):
    source = preprocess(source, generated)
    if name in json.loads((ROOT / 'pc/fixed_data_subsets.json').read_text()):
        return convert(selected_constants(name, source), frozenset(), {}, [], platform)
    return convert(literal_prefix(name, source), readonly_data(name), typed_tables(name), extra_exports(name), platform)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--list-sources', action='store_true')
    parser.add_argument('--platform', choices=['linux', 'win64'], default='linux')
    parser.add_argument('source', type=Path, nargs='?')
    parser.add_argument('output', type=Path, nargs='?')
    args = parser.parse_args()
    if args.list_sources:
        print('\n'.join('../' + name for name in reviewed_sources()))
    elif args.source and args.output:
        name = args.source.resolve().relative_to(ROOT).as_posix()
        args.output.write_text(convert_source(name, args.source.read_text(), args.output.resolve().parent / "generated", args.platform))
    else:
        parser.error('source and output are required')
