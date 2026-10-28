"""Bounded product matrix and checks independent of differential equality."""
import json
import csv
import io


CONFIGS = {
    'plain': [],
    'all': ['--xref=x.json', '--xref-data=true', '--xref-include-owner=true',
            '--xref-instructions=true', '--xref-include-locals=true', '--xref-include-anon=true',
            '--instruction-records-output=r.json', '--dependency-manifest=d.json',
            '--listing=l.json', '--listing-format=json', '--fceux-nl-ram-output=ram.nl'],
    'xref-default': ['--xref=x.json'],
    'xref-data': ['--xref=x.json', '--xref-data=true'],
    'xref-owner': ['--xref=x.json', '--xref-data=true', '--xref-include-owner=true',
                   '--xref-include-locals=true'],
    'records': ['--instruction-records-output=r.json', '--dependency-manifest=d.json'],
    'nl-ram': ['--fceux-nl-ram-output=ram.nl'],
    'nl-rom-listing': ['--fceux-nl-rom-prefix=rom', '--listing=l.txt'],
    'listing-text': ['--listing=l.txt'],
    'listing-debug': ['--listing=l.txt', '--debug'],
    'listing-json': ['--listing=l.json', '--listing-format=json'],
    'listing-ndjson': ['--listing=l.ndjson', '--listing-format=ndjson', '--debug'],
    'xref-csv': ['--xref=x', '--xref-format=csv', '--xref-include-locals=true'],
    'xref-text': ['--xref=x.txt', '--xref-format=text', '--xref-include-anon=true'],
    'case-insensitive': ['--case-insensitive', '--xref=x.json', '--listing=l.json',
                         '--listing-format=json'],
    'warnings': ['--warn-unused-equ', '--Werror=unused-equ'],
}
for fmt in ('json', 'text', 'ndjson'):
    args = []
    for flag, prefix, name in (
            ('xref-summary', 'xref-summary', 's'),
            ('analyze-index-patterns', 'index-patterns', 'i'),
            ('data-consumers', 'data-consumers', 'c'),
            ('analyze-data-coverage', 'data-coverage', 'cov')):
        args.extend([f'--{flag}', f'--{prefix}-output={name}.{fmt}', f'--{prefix}-format={fmt}'])
    CONFIGS['derived-' + fmt] = args
CONFIGS['derived-filters'] = [*CONFIGS['derived-json'], '--include-locals=true',
    '--include-anon=true', '--include-overlaps=true', '--xref-summary-kind=data',
    '--xref-summary-limit=3', '--xref-summary-top-referrers=2',
    '--xref-summary-nearby-window=16', '--xref-summary-include=^[A-Z]',
    '--xref-summary-exclude=^Unused', '--index-patterns-split-pairs=Lo:Hi']
for fmt in ('json', 'text'):
    CONFIGS['audit-' + fmt] = ['--audit-raw-addresses', '--audit-rom-range=$8000-$FFFF',
                             '--audit-level=error', '--audit-output-format=' + fmt]
    CONFIGS['compare-' + fmt] = ['--compare=ref.bin', '--compare-format=' + fmt,
                               '--compare-cpu-base=$8000', '--compare-max-mismatches=3']
    CONFIGS['compare-match-' + fmt] = ['--compare=match.bin', '--compare-format=' + fmt]


def expected_products(args):
    outputs = {'o.bin'}
    for arg in args:
        if '=' not in arg:
            continue
        flag, value = arg.split('=', 1)
        if flag in ('--listing', '--instruction-records-output', '--dependency-manifest',
                    '--fceux-nl-ram-output', '--xref-summary-output', '--index-patterns-output',
                    '--data-consumers-output', '--data-coverage-output'):
            outputs.add(value)
        elif flag == '--xref':
            outputs.update((value + '.symbols.csv', value + '.refs.csv')
                           if '--xref-format=csv' in args else (value,))
    return outputs


def product_metrics(name, data):
    """Separate physical presence/bytes from meaningful structured record counts."""
    counts = {}
    if name.endswith('.ndjson'):
        rows = [json.loads(line) for line in data.splitlines()]
        if not all(isinstance(row, dict) for row in rows):
            raise ValueError('NDJSON records must be objects')
        counts['records'] = len(rows)
    elif name.endswith('.json'):
        document = json.loads(data)
        if name in ('i.json', 'c.json', 'cov.json'):
            if not isinstance(document, list) or not all(isinstance(row, dict) for row in document):
                raise ValueError('analysis records must be an array of objects')
            counts['records'] = len(document)
        else:
            if not isinstance(document, dict):
                raise ValueError('expected a JSON object')
            fields = {'s.json': ('top_callables', 'top_jump_targets', 'top_data_labels'),
                      'x.json': ('symbols', 'references'), 'd.json': ('inputs', 'missing_paths'),
                      'l.json': ('records',), 'r.json': ('records',)}.get(name, ())
            if name == 's.json':
                # --xref-summary-kind emits only the selected section(s).
                fields = tuple(field for field in fields if field in document)
                if not fields:
                    raise ValueError('summary has no sections')
            for field in fields:
                if not isinstance(document[field], list):
                    raise ValueError(field + ' must be an array')
                counts[field] = len(document[field])
    elif name.endswith('.csv'):
        rows = list(csv.reader(io.StringIO(data.decode())))
        if not rows:
            raise ValueError('CSV header is missing')
        counts['records'] = len(rows) - 1
    elif name.endswith('.nl'):
        counts['names'] = len(data.splitlines())
    return {'bytes': len(data), 'records': counts,
            'measurement': 'records' if counts else 'bytes only'}


def check_products(artifacts, args, accepted):
    """Return errors; require products before checking their rows (no vacuous pass)."""
    errors = []
    if not accepted:
        return errors
    missing = expected_products(args) - artifacts.keys()
    if missing:
        return ['missing requested products: ' + ', '.join(sorted(missing))]
    binary = artifacts['o.bin']
    if '--fceux-nl-rom-prefix=rom' in args:
        for page in range((len(binary) + 16383) // 16384):
            if f'rom{page:X}.nl' not in artifacts:
                errors.append(f'missing ROM names for page {page}')
    try:
        for name in ('l.json', 'l.ndjson'):
            if name not in artifacts:
                continue
            listing = json.loads(artifacts[name]) if name.endswith('.json') else None
            rows = listing['records'] if listing is not None else [
                json.loads(line) for line in artifacts[name].splitlines()]
            if listing is not None and (listing['version'] != '2' or not listing['binary_generated']):
                errors.append('listing envelope does not describe a generated binary')
            if binary and not rows:
                errors.append('nonempty binary has no listing rows')
            emitted = bytearray()
            for row in rows:
                if row['emits_bytes']:
                    count = row['byte_count']
                    data = bytes.fromhex(''.join(row['bytes_hex']))
                    if row['fill_byte'] is not None:
                        if count < 0 or count > len(binary):
                            raise ValueError('invalid listing fill count')
                        data = bytes.fromhex(row['fill_byte']) * count
                    if (len(data) != count or row['output_offset_start'] != len(emitted)
                            or row['output_offset_end'] != len(emitted) + count - 1):
                        errors.append('listing byte count or output offsets disagree')
                    emitted.extend(data)
                elif (row['output_offset_start'] is not None or row['output_offset_end'] is not None
                      or row['bytes_hex']):
                    errors.append('non-emitting listing row has output bytes or offsets')
            if bytes(emitted) != binary:
                errors.append('listing does not reconstruct the binary')
        if 'r.json' in artifacts:
            document = json.loads(artifacts['r.json'])
            records = document['records']
            for row in records:
                offset, size = row['output_offset'], row['size']
                data = bytes(row['bytes'])
                if offset < 0 or size != len(data) or data != binary[offset:offset + size]:
                    errors.append('instruction bytes disagree with binary')
                if row['branch_displacement'] is not None:
                    if len(data) != 2 or row['branch_displacement'] != int.from_bytes(data[1:], 'little', signed=True):
                        errors.append('branch displacement disagrees with bytes')
                elif row['operand_value'] is not None and size > 1:
                    if row['operand_value'] & ((1 << (8 * (size - 1))) - 1) != int.from_bytes(data[1:], 'little'):
                        errors.append('instruction operand disagrees with bytes')
            if 'x.json' in artifacts and '--xref-instructions=true' in args:
                if json.loads(artifacts['x.json'])['instruction_records'] != document:
                    errors.append('embedded and standalone instruction records disagree')
        if 'x.json' in artifacts:
            document = json.loads(artifacts['x.json'])
            if not isinstance(document, dict):
                raise ValueError('xref must be a JSON object')
            for row in document.get('data_directive_references', []):
                offset, width = row['use_output_offset'], row['width_bytes']
                if offset is None:
                    continue
                if width not in (1, 2, 4) or offset < 0 or offset + width > len(binary):
                    errors.append('data reference outside binary')
                elif row['emitted_value'] & ((1 << (8 * width)) - 1) != int.from_bytes(binary[offset:offset + width], 'little'):
                    errors.append('data reference disagrees with binary')
    except (ValueError, KeyError, TypeError, IndexError, OverflowError, AttributeError) as exc:
        errors.append(f'invalid product structure: {exc}')
    return sorted(set(errors))
