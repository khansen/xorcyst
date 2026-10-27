"""Coverage aggregation and durable checkpoints, independent of the execution loop."""
from collections import Counter, defaultdict
import json
import os
import re

from cases import features, jobs


def write_json(path, data):
    temporary = path.with_name(path.name + '.tmp')
    temporary.write_text(json.dumps(data, indent=2, sort_keys=True) + '\n')
    os.replace(temporary, path)


def rejection_categories(steps):
    categories = set()
    for step in steps:
        for line in step['diagnostics'].splitlines():
            if not re.search(r'\berror\b', line, re.I):
                continue
            message = re.split(r'\berror(?:\s+\w+)?:\s*', line, maxsplit=1, flags=re.I)[-1]
            lower = message.lower()
            for needle, category in (
                    ('operand out of range', 'operand out of range'),
                    ('ram placement failed', 'RAM placement failed'),
                    ('layout exceeds', 'layout exceeds limits'),
                    ('layout limit', 'layout exceeds limits'),
                    ('unresolved storage', 'current PC after unresolved storage'),
                    ('fceux rom export', 'FCEUX ROM export restriction'),
                    ('defined by .equ is never used', 'unused equate (W0201)')):
                if needle in lower:
                    categories.add(category)
                    break
            else:
                # Raw, located diagnostics stay in every result. Group only the
                # explanatory summary, removing incidental operands and numbers.
                message = re.sub(r"[`'][^`']*[`']", '<name>', message)
                message = re.sub(r'\$[0-9A-Fa-f]+|\b\d+\b', '<number>', message)
                categories.add(message)
    return sorted(categories or {'<no error diagnostic>'})


def zero_success_errors(coverage):
    return [f'{key}: no validated {side} successes'
            for key, counts in sorted(coverage.items()) for side in ('baseline', 'candidate')
            if counts[side + '_validated'] == 0]


def validated(row, side):
    # Normalization also validates producer identity and JSON shape. Those
    # side-specific errors must not count as successful product coverage.
    return (row[side]['accepted'] and not row[side]['errors']
            and not any(item.startswith(side + ': ') for item in row['differences']))


def aggregate(report, cases, workload):
    coverage, products, rejections = {}, {}, {}
    rows_by_case = defaultdict(list)
    finished = set()
    for case, _ in workload:
        key = case['kind'] + '/' + case['config']
        coverage.setdefault(key, Counter())['planned'] += 1
    case_index = {case['id']: (case, inputs) for case, inputs in cases}
    for row in report['results']:
        case, _ = case_index[row['case']]
        key = case['kind'] + '/' + row['config']
        bucket = coverage[key]
        bucket['attempted'] += 1
        bucket['completed'] += row['completed']
        bucket['equal'] += row['completed'] and not row['differences']
        bucket['budget_interrupted_pairs'] += row['budget_interrupted']
        rows_by_case[row['case']].append(row)
        if row['completed']:
            finished.add(row['case'] + '/' + row['config'])
        for side in ('baseline', 'candidate'):
            result = row[side]
            valid = validated(row, side)
            timeouts = sum(step['timed_out'] for step in result['steps'])
            cut = result.get('budget_exhausted', False)
            bucket[side + '_invocations'] += len(result['steps'])
            bucket[side + '_accepted'] += result['accepted']
            bucket[side + '_validated'] += valid
            bucket[side + '_rejected'] += not result['accepted'] and not cut and not timeouts
            bucket[side + '_timeouts'] += timeouts
            bucket[side + '_budget_cuts'] += sum(step.get('budget_exhausted', False) for step in result['steps'])
            bucket[side + '_budget_exhausted'] += cut
            bucket[side + '_warnings'] += sum(step['warnings'] for step in result['steps'])
            for step in result['steps']:
                bucket[side + '_exit:' + str(step['returncode'])] += 1
            categories = rejections.setdefault(key, {}).setdefault(side, Counter())
            if not result['accepted'] and not cut and not timeouts:
                categories.update(rejection_categories(result['steps']))
            for name, identity in result['artifacts'].items():
                counts = products.setdefault(key, {}).setdefault(side, {}).setdefault(name, Counter())
                counts['present'] += 1
                if valid:
                    counts['fresh_successes'] += 1
                    counts['nonempty_bytes'] += identity['size'] > 0
                    records = result['content'].get(name, {}).get('records', {})
                    if records:
                        counts['runs_with_records'] += any(records.values())
                        counts['runs_without_records'] += not any(records.values())
                        for field, count in records.items():
                            counts['records:' + field] += count
        if not row['budget_interrupted']:
            bucket['acceptance_disagreements'] += row['baseline']['accepted'] != row['candidate']['accepted']
    report.update(coverage=coverage, products=products, configuration_rejection_reasons=rejections)
    report['omitted'] = [case['id'] + '/' + case['config'] for case, _ in workload
                         if case['id'] + '/' + case['config'] not in finished]
    feature_counts = {key: Counter() for key in ('generated', 'baseline_accepted', 'candidate_accepted')}
    report['programs'] = {}
    for kind in ('binary', 'link'):
        selected = [(case, inputs) for case, inputs in cases
                    if case['kind'] == kind and case['generator'] != 'fixture']
        counts = Counter(generated=len(selected))
        reasons = {'baseline': Counter(), 'candidate': Counter()}
        for case, inputs in selected:
            source_features = features(inputs)
            feature_counts['generated'].update(source_features)
            rows = rows_by_case[case['id']]
            counts['attempted'] += bool(rows)
            counts['completed'] += len(rows) == len(list(jobs(case))) and all(r['completed'] for r in rows)
            counts['timed_out'] += any(step['timed_out'] for row in rows for side in reasons
                                       for step in row[side]['steps'])
            counts['budget_interrupted'] += any(row['budget_interrupted'] for row in rows)
            primary = next((row for row in rows if row['config'] in ('plain', 'link')), None)
            for side in reasons:
                if any(validated(row, side) for row in rows):
                    feature_counts[side + '_accepted'].update(source_features)
                if primary and primary['completed']:
                    accepted = primary[side]['accepted']
                    counts[side + '_accepted'] += accepted
                    counts[side + '_rejected'] += not accepted
                    if not accepted:
                        reasons[side].update(rejection_categories(primary[side]['steps']))
            if primary and primary['completed']:
                counts['acceptance_disagreements'] += primary['baseline']['accepted'] != primary['candidate']['accepted']
        report['programs'][kind] = {'counts': counts, 'rejection_reasons': reasons}
    feature_counts['uncovered_on_candidate'] = sorted(
        feature_counts['generated'].keys() - feature_counts['candidate_accepted'].keys())
    report['source_features'] = feature_counts
