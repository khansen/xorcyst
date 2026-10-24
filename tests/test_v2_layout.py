#!/usr/bin/env python3
"""Direct IR controls for I2; no v2 source parser or object path is implied."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

REPO = Path(__file__).resolve().parents[1]

class Layout(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        build = tempfile.TemporaryDirectory(prefix='xorcyst-v2-layout-')
        cls.addClassCleanup(build.cleanup)
        cls.executable = Path(build.name) / 'layout'
        result = subprocess.run([
            *shlex.split(os.environ.get('CC', 'cc')), '-std=gnu11',
            '-Wall', '-Wextra', '-I', str(REPO),
            *shlex.split(os.environ.get('TEST_V2_CFLAGS', '-O0 -g -Werror')),
            *[str(REPO / name) for name in
              ('tests/test_v2_alloc.c', 'tests/test_v2_layout_alloc.c', 'tests/test_v2_cache.c', 'opcode.c', 'tests/test_v2_layout.c')],
            '-o', str(cls.executable)], capture_output=True, text=True)
        if result.returncode:
            raise AssertionError(result.stderr)

    def run_case(self, case, mode=0, *args):
        r = subprocess.run([str(self.executable), case, str(mode), *map(str, args)],
                           capture_output=True, text=True, timeout=30)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)
        self.assertEqual(r.stderr, '')
        return r.stdout.splitlines()

    def test_span_lowering_only_visits_extent_reachable_roots(self):
        self.assertEqual(self.run_case('rooted-spans')[0], 'rooted spans: OK')

    def test_contract_narrowing_and_first_walk_order(self):
        expected = ['A510' * 43 + 'D0A8', 'ERROR 5 0 7', 'A510ADFFFFEA',
                    'A510AD0001EA60', 'A5FEA5FAEAEAEAEA60',
                    'EAA500AD0001EA60', 'ERROR 5 0 7', 'EAAD0001A50060']
        for mode in (0, 1):
            for case, wanted in enumerate(expected):
                with self.subTest(mode=mode, case=case):
                    self.assertEqual(self.run_case('controls', mode, case)[0], wanted)

    def test_span_cancels_unknown_prefix_and_rejects_cycle(self):
        for mode in (0, 1):
            self.assertEqual(self.run_case('spans', mode, 0)[0], '0000EAEA60')
            self.assertEqual(self.run_case('spans', mode, 1)[0], 'ERROR 3 6 7')
            self.assertEqual(self.run_case('spans', mode, 3)[0], '000000000060')
            self.assertEqual(self.run_case('spans', mode, 4)[0], 'ERROR 3 6 7')
            self.assertEqual(self.run_case('spans', mode, 5)[0], '0003')

    def test_span_instruction_operands_keep_previous_walk_addresses(self):
        for mode in (0, 1):
            for case, expected in ((8, 'EAA500AD0001EA60'), (9, 'ERROR 5 0 7')):
                with self.subTest(mode=mode, case=case):
                    self.assertEqual(self.run_case('controls', mode, case)[0], expected)

    def test_ram_retains_failed_alignment_splits_and_excludes_block_end(self):
        for case, expected in enumerate(('00080010', 'ERROR 6 0 7', '50001100', '00000001')):
            with self.subTest(case=case):
                self.assertEqual(self.run_case('ram-failed-split', 1, case)[0], expected)

    def test_mixed_query_modes_repick_first_error_after_placement(self):
        self.assertEqual(self.run_case('mixed-error')[0], '')

    def test_mixed_cached_queries_match_fresh_queries(self):
        self.assertIn('0 mismatches, 0 cycle-location-only differences',
                      self.run_case('cache-differential')[0])

    def test_existing_addresses_follow_new_prefix_dependencies(self):
        self.assertEqual(self.run_case('prefix-edges')[0], '')

    def test_span_lowering_stops_at_origins_banks_and_dynamic_pads(self):
        for case, expected in enumerate(('EAEA0C', 'EA000000EA0C0000', 'EA' + '00'*11 + 'EA0C')):
            with self.subTest(case=case):
                self.assertEqual(self.run_case('span-barriers', 1, case)[0], expected)

    def test_early_literal_policy_is_distinct_from_late_operand_ranges(self):
        for mode in (0, 1):
            self.assertEqual(self.run_case('early-truncation', mode, 0)[0], 'ERROR 5 0 7')
            self.assertEqual(self.run_case('early-truncation', mode, 1)[0], 'A9FF')

    def test_concatenations_keep_owned_bytes_without_budget_drift(self):
        for mode in (0, 1):
            for case, expected in ((0, '414243440000EAEA60'), (1, '')):
                with self.subTest(mode=mode, case=case):
                    self.assertEqual(self.run_case('concat-reuse', mode, case)[0], expected)

    def test_ram_exact_tie_permutation_source_spans_and_zero_labels(self):
        expected = [('101113141617191D1E2021', '101A1116121C1E13181415'),
                    ('101011', '101011'), ('1014', '1110'), ('12', 'ERROR 4 0 7')]
        for case, modes in enumerate(expected):
            for mode, wanted in enumerate(modes):
                with self.subTest(case=case, mode=mode):
                    self.assertEqual(self.run_case('ram', mode, case)[0], wanted)

    def test_reversed_link_units_keep_v1_ram_permutation(self):
        self.assertEqual(self.run_case('ram', 1, 4)[0],
                         '1C001E0010001600110012001300180014001A001500')

    def test_alternating_prefix_queries_keep_all_deferred_dependencies(self):
        self.assertEqual(self.run_case('dependency-modes')[0], '')

    def test_strings_maps_pc_widening_and_counted_nul(self):
        for mode in (0, 1):
            result = self.run_case('strings', mode)
            self.assertEqual(result[0], '01004803804100000042000000410042')
            self.assertIn('32771:3:2:00:32771:0', result[1])

    def test_checked_extents_do_not_wrap_or_validate_absent_elements(self):
        self.assertEqual(self.run_case('extents')[0], '00' * 80000)
        self.assertEqual(self.run_case('extents', 1)[0], 'ERROR 7 0 7')

    def test_banks_padding_and_bank_operator(self):
        self.assertEqual(self.run_case('banks', 1)[0], 'FF0080000000550000010000AA')

    def test_byte_operands_and_zero_page_boundaries(self):
        for mode in (0, 1):
            for opcode in (0xa9, 0xa1, 0xb1, 0xa5, 0xb5, 0xb6):
                for value in (-129, -128, -1, 0, 255, 256):
                    with self.subTest(mode=mode, opcode=opcode, value=value):
                        out = self.run_case('range', mode, opcode, value, 0)
                        minimum = 0 if opcode in (0xa5, 0xb5, 0xb6) else -128
                        if not minimum <= value <= 255:
                            self.assertEqual(out[0], 'ERROR 5 0 7')
                        else:
                            self.assertEqual(out[0], bytes((opcode, value & 255)).hex().upper())
                            self.assertIn(f':{value}:0', out[1])

    def test_word_operand_mode_policy_and_data_warnings(self):
        for value in (-32769, -32768, -1, 0, 65535, 65536):
            for mode in (0, 1):
                with self.subTest(value=value, mode=mode):
                    overflow = not -32768 <= value <= 65535
                    result = self.run_case('range', mode, 0x4c, value, 0)
                    if overflow and mode:
                        self.assertEqual(result[0], 'ERROR 5 0 7')
                    else:
                        self.assertEqual(result[0], ('4c' + (value & 65535).to_bytes(2, 'little').hex()).upper())
                        self.assertIn(f':{value}:{int(overflow)}', result[1])
                    result = self.run_case('range', mode, 0, value, 2)
                    self.assertEqual(result[0], (value & 65535).to_bytes(2, 'little').hex().upper())
                    self.assertIn(f':{value}:{int(overflow)}', result[1])

    def test_branch_edges(self):
        for mode in (0, 1):
            for delta in (-129, -128, -1, 0, 127, 128):
                r = self.run_case('range', mode, 0xd0, 2 + delta, 0)
                self.assertEqual(r[0], 'ERROR 5 0 7' if delta in (-129, 128)
                                 else f'D0{delta & 255:02X}')

    def test_inline_integer_operands_match_expression_fixups_and_facts(self):
        for mode in (0,1):
            for opcode in (0xa9,0xa1,0xb1,0xa5,0xb5,0xb6,0xad,0xbd,0xb9,0x4c,0x6c,0xd0):
                for value in (-32769,-32768,-129,-128,-126,-1,0,127,129,130,255,256,65535,65536):
                    for wide,early in ((0,0),(1,0),(0,1)):
                        with self.subTest(mode=mode,opcode=opcode,value=value,wide=wide,early=early):
                            args=(mode,opcode,value,wide,early)
                            self.assertEqual(self.run_case('literal',*args),self.run_case('expression-literal',*args))
        for opcode in (0xea,0x02):
            self.assertEqual(self.run_case('literal',0,opcode,0,0,0)[0],'ERROR 2 0 7')

    def test_copy_is_captured_and_rechecked_before_results(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'copy.bin'
            for mutate in (0, 1, 2, 3):
                path.write_bytes(b'NES\x1a')
                result = self.run_case('copy', 1, mutate, path)
                self.assertEqual(result[0], 'ERROR 8 0 7' if mutate else '4E45531A')

    def test_shared_operands_and_many_fragments(self):
        for n in (1000, 5000):
            walks, visits, narrowings, nodes = map(int, self.run_case('work', 1, n)[0].split())
            self.assertEqual(walks, 2)
            self.assertEqual(visits, 2 * (2 * n + 2))
            self.assertEqual(narrowings, 0)
            self.assertLess(nodes, 20 * n + 1000)

    def test_every_documented_6502_opcode_encodes_its_fixed_operand_width(self):
        # Independent byte/width inventory, not imported from opcode.c.
        groups = {
            1: '00 08 0A 18 28 2A 38 40 48 4A 58 60 68 6A 78 88 8A 98 9A A8 AA B8 BA C8 CA D8 E8 EA F8',
            2: ('01 05 06 09 10 11 15 16 21 24 25 26 29 30 31 35 36 '
                '41 45 46 49 50 51 55 56 61 65 66 69 70 71 75 76 '
                '81 84 85 86 90 91 94 95 96 A0 A1 A2 A4 A5 A6 A9 '
                'B0 B1 B4 B5 B6 C0 C1 C4 C5 C6 C9 D0 D1 D5 D6 '
                'E0 E1 E4 E5 E6 E9 F0 F1 F5 F6'),
            3: ('0D 0E 19 1D 1E 20 2C 2D 2E 39 3D 3E 4C 4D 4E '
                '59 5D 5E 6C 6D 6E 79 7D 7E 8C 8D 8E 99 9D '
                'AC AD AE B9 BC BD BE CC CD CE D9 DD DE EC ED EE F9 FD FE'),
        }
        self.assertEqual(sum(len(g.split()) for g in groups.values()), 151)
        for mode in (0, 1):
            for width, group in groups.items():
                for opcode in group.split():
                    with self.subTest(mode=mode, opcode=opcode):
                        result = self.run_case('opcode', mode, int(opcode, 16), width)
                        expected = opcode + ('3412' if width == 3 else '7F' if width == 2 else '')
                        self.assertEqual(result[0], expected)
                        if width > 1:
                            self.assertIn(f'0:0:{width}:{opcode}:', result[1])

    def test_allocation_failures_keep_partial_layout_destroyable(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'copy.bin'
            path.write_bytes(b'NES\x1a')
            values, layout = map(int, self.run_case('allocations', 1, 0, path)[0].split())
        self.assertGreater(values, 100)
        self.assertGreater(layout, 20)

    def test_limits_and_fixed_width_selection(self):
        for mode in (0, 1):
            expected = ['EA01' if not mode else 'ERROR 7 0 7',
                        'ERROR 7 0 7', 'ERROR 7 0 7', 'ERROR 7 0 7',
                        '' if not mode else 'ERROR 6 0 7', '10',
                        '' if not mode else 'ERROR 6 0 7', 'ERROR 2 0 7',
                        'ERROR 2 0 7', 'ERROR 4 0 7', 'ERROR 3 11 7',
                        'AD0300EA', 'A9FF']
            for case, wanted in enumerate(expected):
                with self.subTest(mode=mode, case=case):
                    self.assertEqual(self.run_case('limits', mode, case)[0], wanted)
        self.assertEqual(self.run_case('limits', 0, 13)[0], 'EAEA10')
        self.assertEqual(self.run_case('limits', 0, 15)[0], 'EAEA12')
        self.assertEqual(self.run_case('limits', 0, 14)[0], 'EA0000010004000100')
        self.assertEqual(self.run_case('limits', 1, 14)[0], 'ERROR 7 0 7')
        self.assertEqual(self.run_case('limits', 0, 16)[0], 'A5101012')
        self.assertEqual(self.run_case('limits', 1, 16)[0], 'A5101110')
        self.assertEqual(self.run_case('limits', 0, 17)[0], 'ERROR 2 0 0')
        self.assertEqual(self.run_case('limits', 0, 18)[0], 'ERROR 5 0 9')
        self.assertEqual(self.run_case('limits', 0, 19)[0], 'ERROR 2 0 0')

    def test_link_alignment_holes_and_binary_source_order(self):
        self.assertEqual(self.run_case('alignment', 0)[0], '111316')
        self.assertEqual(self.run_case('alignment', 1)[0], '201211')

    def test_long_cascade_terminates_within_candidate_bound(self):
        for n in (1, 20, 200):
            walks, visits = map(int, self.run_case('cascade', 1, n)[0].split())
            self.assertEqual(walks, n + 2)
            self.assertEqual(visits, (n + 2) * (n + 1))

    def test_adversarial_shared_dependencies_have_measured_work_budget(self):
        for n in (100, 200):
            walks, nodes, edges, deps = map(int, self.run_case('cache-work', 1, n)[0].split())
            self.assertEqual(walks, 3)
            # This graph changes a shared operand at many points within a walk.
            # The approved erratum accounts for revisited edges, not just input
            # graph size. Generous ceilings catch accidental extra full scans.
            self.assertLess(nodes, 2 * n * n)
            self.assertLess(edges, 2 * n * n)
            self.assertLess(deps, 4 * n * n)

if __name__ == '__main__':
    unittest.main()
