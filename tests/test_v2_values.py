#!/usr/bin/env python3
"""Check the isolated v2 value graph against the approved D1 semantics."""
import os
from pathlib import Path
import random
import shlex
import subprocess
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[1]


def i32(value):
    return (value + (1 << 31)) % (1 << 32) - (1 << 31)


def expected(op, a, b):
    """Independent Python oracle; do not import or emulate the C helpers."""
    if op in (8, 9) and b == 0:
        return 'E ' + ('division' if op == 8 else 'modulo') + ' by zero in expression'
    if op in (10, 11) and not 0 <= b < 32:
        return 'E invalid shift count in expression'
    if op == 0:
        value = -a
    elif op == 1:
        value = ~a
    elif op == 2:
        value = not a
    elif op == 3:
        value = a & 255
    elif op == 4:
        value = (a >> 8) & 255
    elif op == 5:
        value = a + b
    elif op == 6:
        value = a - b
    elif op == 7:
        value = a * b
    elif op in (8, 9):
        quotient = abs(a) // abs(b) * (-1 if (a < 0) != (b < 0) else 1)
        value = quotient if op == 8 else a - quotient * b
    elif op == 10:
        value = a << b
    elif op == 11:
        value = (a % (1 << 32)) >> b
    elif op == 12:
        value = a & b
    elif op == 13:
        value = a | b
    elif op == 14:
        value = a ^ b
    else:
        value = (a == b, a != b, a < b, a <= b, a > b, a >= b)[op - 15]
    return f'K {i32(value)}'


class V2Values(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = tempfile.TemporaryDirectory(prefix='xorcyst-v2-values-')
        cls.addClassCleanup(build.cleanup)
        cls.executable = Path(build.name) / 'values'
        compiler = [*shlex.split(os.environ.get('CC', 'cc')), '-std=c99',
                    '-Wall', '-Wextra', '-pedantic', '-I', str(REPO),
                    *shlex.split(os.environ.get('TEST_V2_CFLAGS', '-O0 -g -Werror'))]
        # The wrapper includes the real module and substitutes only allocation
        # functions. The production source contains no fault-injection hooks.
        result = subprocess.run([
            *compiler, str(REPO / 'v2/heap.c'), str(REPO / 'tests/test_v2_alloc.c'),
            str(REPO / 'tests/test_v2_values.c'), '-o', str(cls.executable),
        ], capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stderr)

    def run_case(self, case, data=None):
        result = subprocess.run([str(self.executable), case], input=data,
                                capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertEqual(result.stderr, '')
        return result.stdout

    def test_integer_operations_match_oracle_before_and_after_resolution(self):
        bounds = [-2147483648, -2147483647, -65536, -32768, -129, -128, -7,
                  -1, 0, 1, 2, 7, 31, 32, 127, 255, 256, 65536, 2147483647]
        rng = random.Random(0x584F52)
        cases = []
        for op in range(21):
            pairs = [(a, b) for a in bounds for b in (bounds if op >= 5 else [0])]
            pairs += [(rng.randrange(-2**31, 2**31), rng.randrange(-2**31, 2**31))
                      for _ in range(100)]
            if op in (10, 11):
                pairs += [(a, b) for a in bounds for b in range(-1, 34)]
            cases.extend((mode, op, a, b) for mode in range(3) for a, b in pairs)
        output = self.run_case('arithmetic-stream', ''.join(
            f'{mode} {op} {a} {b}\n' for mode, op, a, b in cases)).splitlines()
        self.assertEqual(len(output), len(cases))
        for (mode, op, a, b), actual in zip(cases, output):
            with self.subTest(mode=mode, op=op, a=a, b=b):
                self.assertEqual(actual, expected(op, a, b))

    def test_packed_and_large_source_locations_survive_index_release(self):
        self.run_case("locations")

    def test_unit_owns_names_locations_and_counted_bytes(self):
        self.run_case('ownership')

    def test_shared_graphs_match_oracle_across_placement_changes(self):
        rng = random.Random(0x444147)
        commands = ['a'] * 8
        nodes = [('a', i) for i in range(8)]
        for value in [-2147483648, -1, 0, 1, 2, 7, 31, 32, 65536, 2147483647]:
            commands.append(f'i {value}')
            nodes.append(('i', value))
        for _ in range(250):
            op, x, y = rng.randrange(5, 21), rng.randrange(len(nodes)), rng.randrange(len(nodes))
            commands.append(f'b {op} {x} {y}')
            nodes.append(('b', op, x, y))
        answers = []
        anchors = [None] * 8
        for epoch in range(20):
            if epoch:
                for i in range(8):
                    value = rng.choice([None, -2147483648, -1, 0, 1, 2, 31, 32, 2147483647])
                    anchors[i] = value
                    commands.append(f'p {i} {int(value is not None)} {value or 0}')
            values = []
            for node in nodes:
                if node[0] in ('a', 'i'):
                    value = anchors[node[1]] if node[0] == 'a' else node[1]
                    result = 'D' if value is None else f'K {value}'
                else:
                    _, op, x, y = node
                    left, right = values[x], values[y]
                    if left.startswith('E'):
                        result = left
                    elif right.startswith('E'):
                        result = right
                    elif left == 'D' or right == 'D':
                        result = 'D'
                    else:
                        result = expected(op, int(left[2:]), int(right[2:]))
                values.append(result)
            order = list(range(len(nodes)))
            rng.shuffle(order)
            for index in order:
                commands.append(f'e {index}')
                answers.append(values[index])
        output = self.run_case('graph-stream', '\n'.join(commands) + '\n').splitlines()
        self.assertEqual(output, answers)

    def test_captured_definitions_and_forward_bindings_have_distinct_ids(self):
        self.run_case('bindings')

    def test_anchor_inspection_checks_ids_without_evaluating_prefixes(self):
        self.run_case('anchor-inspection')

    def test_string_kinds_concatenation_comparisons_and_lifetimes(self):
        self.run_case('strings')

    def test_dependencies_are_unique_and_placement_invalidates_results(self):
        self.run_case('dependencies')

    def test_cycles_and_cached_errors_keep_locations_and_recheck_dependencies(self):
        self.run_case('diagnostics')

    def test_rollover_rechecks_epoch_one_caches_and_dependency_visits(self):
        self.run_case('rollover')

    def test_placement_reselects_the_first_error(self):
        self.run_case('placement-error-order')

    def test_resolved_dependencies_reselect_error_before_a_cached_cycle(self):
        self.run_case('cycle-error-order')

    def test_integer_identities_do_not_accept_strings(self):
        self.run_case('string-identities')

    def test_deep_and_shared_graphs_do_not_expand_or_use_the_c_stack(self):
        self.run_case('deep-graphs')

    def test_string_budgets_fail_before_overallocation(self):
        self.run_case('limits')

    def test_concatenations_obey_the_total_live_string_budget(self):
        self.run_case('concat-budget')

    def test_invalid_builders_fail_closed(self):
        self.run_case('invalid-builders')

    def test_bad_references_cannot_publish_unowned_diagnostic_locations(self):
        self.run_case('builder-locations')

    def test_each_allocation_failure_leaves_a_destroyable_invalid_unit(self):
        self.run_case('allocations')


if __name__ == '__main__':
    unittest.main()
