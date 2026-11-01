"""Independent test-only saved-model reader. Never imports or invokes xasm.

This is an oracle for the public producer contract, not a consumer-side
assembly parser or a production fallback. All reconstruction uses saved units;
all arithmetic checks use the exported expression tree.
"""
from bisect import bisect_left, bisect_right
from collections import defaultdict
from mos6502_oracle import OPCODES, expected_memory_access
import base64
import hashlib
import json


class ModelError(ValueError):
    pass


def require(condition, code):
    if not condition:
        raise ModelError(code)


def index(rows, code="MODEL_SCHEMA"):
    require(isinstance(rows, list), code)
    result = {}
    for row in rows:
        require(isinstance(row, dict) and isinstance(row.get("id"), str), code)
        require(row["id"] not in result, code)
        result[row["id"]] = row
    return result


def decode(encoded):
    try:
        return base64.b64decode(encoded, validate=True)
    except (TypeError, ValueError):
        raise ModelError("MODEL_SCHEMA") from None


def sha(data):
    return hashlib.sha256(data).hexdigest()


def reconstruct(model):
    """Return logical path -> bytes; no pathname in the model is ever opened."""
    files = index(model["files"])
    data = {}
    for fid, f in files.items():
        raw = decode(f["bytes_base64"])
        require(len(raw) == f["byte_length"] and sha(raw) == f["sha256"], "MODEL_SOURCE_HASH")
        data[fid] = raw
    positions = dict.fromkeys(files, 0)
    chunks = {fid: [] for fid in files}
    index(model["units"], "MODEL_SOURCE_PARTITION")
    for unit in model["units"]:
        fid, start, end = unit["file_id"], unit["byte_start"], unit["byte_end_exclusive"]
        require(fid in files and type(start) is int and type(end) is int, "MODEL_SOURCE_PARTITION")
        require(start == positions[fid] and start < end <= len(data[fid]), "MODEL_SOURCE_PARTITION")
        chunks[fid].append(data[fid][start:end])
        positions[fid] = end
    for fid, raw in data.items():
        require(positions[fid] == len(raw), "MODEL_SOURCE_PARTITION")
    require(len({f["path"] for f in files.values()}) == len(files), "MODEL_SOURCE_PARTITION")
    return {f["path"]: b"".join(chunks[fid]) for fid, f in files.items()}


def hash_fields(fields):
    raw = b""
    for text in fields:
        value = text.encode("utf-8")
        raw += str(len(value)).encode() + b":" + value
    return sha(raw)


def i32(value):
    return (value + 2**31) % 2**32 - 2**31


def integer_operator(op, args):
    a = args[0]
    b = args[1] if len(args) > 1 else None
    require(type(a) is int and (b is None or type(b) is int), "MODEL_VALUE")
    if op == "add": result = a + b
    elif op == "subtract": result = a - b
    elif op == "multiply": result = a * b
    elif op in ("divide", "remainder"):
        require(b != 0, "MODEL_VALUE")
        quotient = (abs(a) // abs(b)) * (-1 if (a < 0) != (b < 0) else 1)
        result = quotient if op == "divide" else a - quotient * b
    elif op in ("shift_left", "shift_right"):
        require(0 <= b < 32, "MODEL_VALUE")
        result = (a & 0xffffffff) << b if op == "shift_left" else (a & 0xffffffff) >> b
    elif op == "bit_and": result = a & b
    elif op == "bit_or": result = a | b
    elif op == "bit_xor": result = a ^ b
    elif op == "equal": result = int(a == b)
    elif op == "not_equal": result = int(a != b)
    elif op == "less": result = int(a < b)
    elif op == "less_equal": result = int(a <= b)
    elif op == "greater": result = int(a > b)
    elif op == "greater_equal": result = int(a >= b)
    elif op == "complement": result = ~a
    elif op == "not": result = int(not a)
    elif op == "negate": result = -a
    elif op == "low": result = a & 255
    elif op == "high": result = (a >> 8) & 255
    else: raise ModelError("MODEL_VALUE")
    return i32(result)


def validate(model):
    require(model.get("schema") == "xasm-source-model" and model.get("version") == 1
            and model.get("profile") == "asm-edit-v1" and model.get("target") == "mos6502", "MODEL_SCHEMA")
    saved = reconstruct(model)
    files = index(model["files"])
    syntax = index(model["syntax"])
    expressions = index(model["expressions"])
    declarations = index(model["declarations"])
    references = index(model["references"], "MODEL_REFERENCE_COVERAGE")
    scopes = index(model["scopes"])
    diagnostics = index(model["diagnostics"])
    identity, opts = model["identity"], model["semantic_options"]
    require(identity["schema_id"] == "xasm-source-model:1" and identity["profile_id"] == "asm-edit-v1", "MODEL_IDENTITY")
    fields = ["xasm-source-model-input-v1",
              "case-insensitive" if opts["case_insensitive"] else "case-sensitive",
              "swapped-parens" if opts["swap_parens"] else "native-parens"]
    for f in files.values():
        fields.extend([f["path"], f["sha256"]])
    require(hash_fields(fields) == identity["input_id"], "MODEL_IDENTITY")
    require(hash_fields(["xasm-source-model:1:asm-edit-v1", identity["input_id"],
                         identity["producer_sha256"]]) == identity["model_id"], "MODEL_IDENTITY")

    source_bytes = {fid: saved[f["path"]] for fid, f in files.items()}

    def span_bytes(span):
        require(isinstance(span, dict) and span["file_id"] in files, "MODEL_SOURCE_PARTITION")
        raw = source_bytes[span["file_id"]]
        b, e = span["byte_start"], span["byte_end_exclusive"]
        require(type(b) is int and type(e) is int and 0 <= b <= e <= len(raw), "MODEL_SOURCE_PARTITION")
        return raw[b:e]

    # Parent/children are reciprocal; scopes follow parsed global declarations,
    # independently of the reference's reported scope or declaration ID.
    child_sets = {sid: set(s["children"]) for sid, s in syntax.items()}
    for sid, s in syntax.items():
        if s["span"] is not None:
            span_bytes(s["span"])
        require(s["parent_id"] is None or s["parent_id"] in syntax, "MODEL_SCHEMA")
        require(s["scope_id"] in scopes, "MODEL_SCHEMA")
        for child in s["children"]:
            require(child in syntax and syntax[child]["parent_id"] == sid, "MODEL_SCHEMA")
        if s["parent_id"]:
            require(sid in child_sets[s["parent_id"]], "MODEL_SCHEMA")
    # Derive expressions and reference leaves from parsed syntax independently
    # of the exported expression/reference inventories and their counts.
    reference_kinds = {"IDENTIFIER_NODE", "LOCAL_ID_NODE", "FORWARD_BRANCH_NODE", "BACKWARD_BRANCH_NODE"}
    expression_kinds = reference_kinds | {"INTEGER_NODE", "STRING_NODE", "CURRENT_PC_NODE", "ARITHMETIC_NODE"}
    expected_expressions, expected_reference_syntax = set(), set()
    roots_by_context = {}
    for sid, node in syntax.items():
        children = node["children"]
        roots = (children[1:] if node["kind"] in ("EQU_NODE", "DATA_NODE") else
                 children if node["kind"] in ("ORG_NODE", "INSTRUCTION_NODE") else [])
        roots_by_context[sid] = roots
        todo = list(roots)
        while todo:
            child = todo.pop()
            if syntax[child]["kind"] in expression_kinds:
                require(child not in expected_expressions, "MODEL_REFERENCE_COVERAGE")
                expected_expressions.add(child)
                if syntax[child]["kind"] in reference_kinds:
                    expected_reference_syntax.add(child)
            todo.extend(syntax[child]["children"])
    expr_by_syntax = {e["syntax_id"]: e for e in expressions.values()}
    require(len(expr_by_syntax) == len(expressions) and set(expr_by_syntax) == expected_expressions,
            "MODEL_REFERENCE_COVERAGE")
    expected_refs = {expr_by_syntax[s]["reference_id"] for s in expected_reference_syntax}
    require(len(expected_refs) == len(expected_reference_syntax) and expected_refs == set(references),
            "MODEL_REFERENCE_COVERAGE")
    declaration_kinds = {"LABEL_NODE", "LOCAL_LABEL_NODE", "FORWARD_BRANCH_DECL_NODE", "BACKWARD_BRANCH_DECL_NODE", "EQU_NODE"}
    require(len({d["syntax_id"] for d in declarations.values()}) == len(declarations)
            and {d["syntax_id"] for d in declarations.values()} ==
                {sid for sid, node in syntax.items() if node["kind"] in declaration_kinds}, "MODEL_BINDING_TARGET")
    for eid, e in expressions.items():
        require(e["syntax_id"] in syntax and e["context_syntax_id"] in syntax, "MODEL_SCHEMA")
        require(all(c in expressions for c in e["children"]), "MODEL_SCHEMA")
        node = syntax[e["syntax_id"]]
        expected_kind = {"INTEGER_NODE": "integer", "STRING_NODE": "string", "CURRENT_PC_NODE": "pc",
                         "IDENTIFIER_NODE": "symbol", "LOCAL_ID_NODE": "local",
                         "FORWARD_BRANCH_NODE": "anonymous_forward", "BACKWARD_BRANCH_NODE": "anonymous_backward",
                         "ARITHMETIC_NODE": "unary" if len(node["children"]) == 1 else "binary"}[node["kind"]]
        require(e["kind"] == expected_kind and [expressions[c]["syntax_id"] for c in e["children"]] == node["children"],
                "MODEL_REFERENCE_COVERAGE")
        span_bytes(e["span"])
    for d in declarations.values():
        require(d["syntax_id"] in syntax, "MODEL_BINDING_TARGET")
        spelling = span_bytes(d["name_span"]).decode("ascii")
        require((spelling.upper() if opts["case_insensitive"] else spelling) == d["name"], "MODEL_BINDING_TARGET")
    global_anchors, anon_chains = defaultdict(list), defaultdict(list)
    for d in declarations.values():
        if d["kind"] == "global":
            global_anchors[d["name_span"]["file_id"]].append((d["name_span"]["byte_start"], d["id"]))
        elif d["kind"] == "anonymous":
            anon_chains[d["name_span"]["file_id"], d["name"]].append((d["name_span"]["byte_start"], d["id"]))
    for chain in list(global_anchors.values()) + list(anon_chains.values()):
        chain.sort()

    def owner(span):
        chain = global_anchors[span["file_id"]]
        at = bisect_right(chain, (span["byte_start"], "\uffff")) - 1
        return chain[at][1] if at >= 0 else None

    for rid, ref in references.items():
        require(ref["expression_id"] in expressions and expressions[ref["expression_id"]]["reference_id"] == rid,
                "MODEL_REFERENCE_COVERAGE")
        spelling = span_bytes(ref["span"]).decode("ascii")
        require((spelling.upper() if opts["case_insensitive"] else spelling) == ref["name"], "MODEL_BINDING_TARGET")
        if ref["declaration_id"] is None:
            require(model["status"] == "diagnostic" and ref["diagnostic_id"] in diagnostics, "MODEL_BINDING_TARGET")
            continue
        require(ref["declaration_id"] in declarations, "MODEL_BINDING_TARGET")
        target = declarations[ref["declaration_id"]]
        require(target["name"] == ref["name"], "MODEL_BINDING_TARGET")
        if ref["name"].startswith("@@"):
            require(target["kind"] == "local" and owner(ref["span"]) == owner(target["name_span"]), "MODEL_BINDING_TARGET")
        elif ref["name"].startswith(("+", "-")):
            chain = anon_chains[ref["span"]["file_id"], ref["name"]]
            at = (bisect_right(chain, (ref["span"]["byte_start"], "\uffff")) if ref["name"][0] == "+"
                  else bisect_left(chain, (ref["span"]["byte_start"], "")) - 1)
            require(0 <= at < len(chain) and target["id"] == chain[at][1], "MODEL_BINDING_TARGET")
        else:
            require(target["kind"] in ("global", "equ"), "MODEL_BINDING_TARGET")

    counts = model["coverage"]
    require(counts["units"] == len(model["units"]) and counts["syntax"] == len(syntax)
            and counts["expressions"] == len(expressions) and counts["declarations"] == len(declarations)
            and counts["expected_references"] == len(references)
            and counts["bound_references"] == sum(r["declaration_id"] is not None for r in references.values()),
            "MODEL_REFERENCE_COVERAGE")
    if model["status"] == "diagnostic":
        require(all(model[k] is None for k in ("evaluations", "segments", "locations", "instructions", "data", "emissions", "binary")),
                "MODEL_DIAGNOSTIC_STATE")
        require(bool(diagnostics), "MODEL_DIAGNOSTIC_STATE")
        return saved
    require(model["status"] == "certified" and not diagnostics, "MODEL_DIAGNOSTIC_STATE")
    roots = [q for q in scopes.values() if q["kind"] == "root"]
    require(len(roots) == 1 and roots[0]["parent_id"] is None and roots[0]["anchor_declaration_id"] is None,
            "MODEL_BINDING_TARGET")
    scope_by_owner = {q["anchor_declaration_id"]: q["id"] for q in scopes.values()}
    globals_ = {d["id"] for d in declarations.values() if d["kind"] == "global"}
    require(len(scope_by_owner) == len(scopes) and set(scope_by_owner) == globals_ | {None}, "MODEL_BINDING_TARGET")
    for decl in declarations.values():
        expected_owner = owner(decl["name_span"])
        require(decl["scope_id"] == scope_by_owner[expected_owner]
                and decl["lexical_owner_id"] == expected_owner, "MODEL_BINDING_TARGET")
    for ref in references.values():
        require(ref["scope_id"] == scope_by_owner[owner(ref["span"])] and ref["diagnostic_id"] is None,
                "MODEL_BINDING_TARGET")
    require(model["exit_status"] in (0, 4, 5), "MODEL_DIAGNOSTIC_STATE")
    if model["analysis_outcomes"]["comparison"] == "not_run":
        require(model["exit_status"] == 4 and model["analysis_outcomes"]["audit"] == "findings", "MODEL_DIAGNOSTIC_STATE")
    locations = index(model["locations"])
    segments = index(model["segments"])
    evaluations = index(model["evaluations"])
    by_expr = {v["expression_id"]: v for v in evaluations.values()}
    require(len(by_expr) == len(evaluations) and set(by_expr) == set(expressions), "MODEL_VALUE")
    by_syntax = {l["syntax_id"]: l for l in locations.values()}
    expected_locations = {s["id"] for s in syntax.values() if s["kind"] in
                          ("LABEL_NODE", "LOCAL_LABEL_NODE", "FORWARD_BRANCH_DECL_NODE", "BACKWARD_BRANCH_DECL_NODE",
                           "INSTRUCTION_NODE", "DATA_NODE", "ORG_NODE")}
    require(len(by_syntax) == len(locations) and set(by_syntax) == expected_locations, "MODEL_SEGMENT")
    require(counts["expected_locations"] == len(expected_locations) and counts["located_statements"] == len(locations)
            and counts["evaluated_expressions"] == len(expressions)
            and counts["instruction_statements"] == len(model["instructions"])
            and counts["data_statements"] == len(model["data"]), "MODEL_REFERENCE_COVERAGE")
    cache, active = {}, set()
    def evaluate(eid):
        if eid in cache:
            return cache[eid]
        require(eid not in active, "MODEL_VALUE")
        active.add(eid)
        e = expressions[eid]
        if e["kind"] == "integer": val = e["value"]
        elif e["kind"] == "string": val = decode(e["bytes_base64"])
        elif e["kind"] == "pc": val = by_expr[eid]["pc_context"]["cpu_address"]
        elif e["kind"] in ("unary", "binary"): val = integer_operator(e["operator"], [evaluate(c) for c in e["children"]])
        elif e["kind"] in ("symbol", "local", "anonymous_forward", "anonymous_backward"):
            decl = declarations[references[e["reference_id"]]["declaration_id"]]
            val = evaluate(decl["expression_id"]) if decl["kind"] == "equ" else locations[decl["location_id"]]["cpu_address"]
        else: raise ModelError("MODEL_VALUE")
        active.remove(eid); cache[eid] = val
        return val

    for eid, row in by_expr.items():
        expected = evaluate(eid)
        observed = row["value"]
        observed = decode(observed["bytes_base64"]) if observed["kind"] == "bytes" else observed["value"]
        require(expected == observed, "MODEL_VALUE")
        todo, expected_refs = [eid], []
        while todo:
            e = expressions[todo.pop()]
            if "reference_id" in e:
                r = references[e["reference_id"]]
                expected_refs.append({"reference_id": r["id"], "declaration_id": r["declaration_id"],
                                      "value": by_expr[e["id"]]["value"]})
            todo.extend(reversed(e["children"]))
        require(row["reference_values"] == expected_refs, "MODEL_VALUE")
    binary = decode(model["binary"]["bytes_base64"])
    require(len(binary) == model["binary"]["byte_length"] and sha(binary) == model["binary"]["sha256"], "MODEL_EMISSION_RANGE")
    emissions = index(model["emissions"], "MODEL_EMISSION_RANGE")
    emissions_by_syntax = defaultdict(list)
    offset = 0
    for emission in emissions.values():
        emissions_by_syntax[emission["syntax_id"]].append(emission)
        require(emission["output_start"] == offset and emission["output_end_exclusive"] > offset, "MODEL_EMISSION_RANGE")
        offset = emission["output_end_exclusive"]
        require(offset <= len(binary) and emission["syntax_id"] in syntax, "MODEL_EMISSION_RANGE")
        require(emission["segment_id"] == by_syntax[emission["syntax_id"]]["segment_id"], "MODEL_SEGMENT")
    require(offset == len(binary), "MODEL_EMISSION_RANGE")
    segment_list = list(segments.values())
    require(segment_list[0] == {"id": "g1", "start_syntax_id": None, "address_space": "cpu", "origin": 0, "output_offset": 0}, "MODEL_SEGMENT")
    segment_at, pc, output = 0, 0, 0
    for s in syntax.values():
        if s["id"] not in by_syntax:
            continue
        loc = by_syntax[s["id"]]
        require(loc["segment_id"] == segment_list[segment_at]["id"] and loc["cpu_address"] == pc
                and loc["output_offset"] == output, "MODEL_SEGMENT")
        if s["kind"] == "ORG_NODE":
            segment_at += 1
            require(segment_at < len(segment_list), "MODEL_SEGMENT")
            seg = segment_list[segment_at]
            expr = expr_by_syntax[roots_by_context[s["id"]][0]]
            pc = evaluate(expr["id"])
            require(seg["start_syntax_id"] == s["id"] and seg["origin"] == pc and seg["output_offset"] == output, "MODEL_SEGMENT")
        else:
            pc += loc["byte_length"]; output += loc["byte_length"]
    require(output == len(binary) and segment_at + 1 == len(segment_list), "MODEL_SEGMENT")
    for table, kind in (("data", "DATA_NODE"), ("instructions", "INSTRUCTION_NODE")):
        rows = index(model[table])
        require(len({r["syntax_id"] for r in rows.values()}) == len(rows) and
                {r["syntax_id"] for r in rows.values()} ==
                {sid for sid, node in syntax.items() if node["kind"] == kind}, "MODEL_REFERENCE_COVERAGE")
        for row in rows.values():
            require(row["emission_ids"] == [e["id"] for e in emissions_by_syntax[row["syntax_id"]]], "MODEL_EMISSION_RANGE")
            require([expressions[o["expression_id"]]["syntax_id"] for o in row["operands"]] ==
                    roots_by_context[row["syntax_id"]], "MODEL_REFERENCE_COVERAGE")
            for operand in row["operands"]:
                require(evaluations[operand["evaluation_id"]]["expression_id"] == operand["expression_id"], "MODEL_VALUE")
                loc = locations[row["location_id"]]
                start = operand.get("output_offset", loc["output_offset"] + 1)
                end = start + operand.get("byte_length", loc["byte_length"] - 1)
                require(operand["emission_ranges"] == [{"output_start": start, "output_end_exclusive": end}], "MODEL_EMISSION_RANGE")
                actual = [e for e in emissions_by_syntax[row["syntax_id"]] if e["operand_id"] == operand["id"]]
                require([(e["output_start"], e["output_end_exclusive"]) for e in actual] ==
                        ([] if end == start else [(start, end)]), "MODEL_EMISSION_RANGE")
    for row in model["data"]:
        loc = locations[row["location_id"]]
        offset = loc["output_offset"]
        for operand in row["operands"]:
            val = evaluate(operand["expression_id"])
            values = list(val) if isinstance(val, bytes) else [val]
            width = row["element_width"]
            expected = b"".join((v & ((1 << (width * 8)) - 1)).to_bytes(width, "little") for v in values)
            require(operand["element_count"] == len(values) and operand["output_offset"] == offset
                    and operand["byte_length"] == len(expected), "MODEL_EMISSION_RANGE")
            require(binary[offset:offset + len(expected)] == expected, "MODEL_VALUE")
            offset += len(expected)
        require(offset == loc["output_offset"] + loc["byte_length"], "MODEL_EMISSION_RANGE")
    for row in model["instructions"]:
        loc = locations[row["location_id"]]
        code = binary[loc["output_offset"]:loc["output_offset"] + loc["byte_length"]]
        require(code and code[0] == row["opcode"], "MODEL_VALUE")
        require(row["operand_ids"] == [o["id"] for o in row["operands"]], "MODEL_REFERENCE_COVERAGE")
        require(row["opcode"] in OPCODES, "MODEL_VALUE")
        mnemonic, mode = OPCODES[row["opcode"]]
        require(mnemonic == row["mnemonic"] and row["final_mode"] == mode.upper()+"_MODE", "MODEL_VALUE")
        native_operand = int.from_bytes(code[1:], "little") if len(code) > 1 else 0
        require(row["memory_access"] == expected_memory_access({"opcode": row["opcode"], "operand_value": native_operand}), "MODEL_VALUE")
        if row["operand_ids"]:
            val = evaluate(row["operands"][0]["expression_id"])
            require(type(val) is int, "MODEL_VALUE")
            width = len(code) - 1
            if row["final_mode"] == "RELATIVE_MODE":
                val -= loc["cpu_address"] + 2
                require(-128 <= val <= 127, "MODEL_VALUE")
                require(row["control_flow"]["encoded_displacement"] == val, "MODEL_VALUE")
            require(code[1:] == (val & ((1 << (8 * width)) - 1)).to_bytes(width, "little"), "MODEL_VALUE")
        else:
            require(len(code) == 1, "MODEL_VALUE")
    for table in ("instructions", "data"):
        for row in model[table]:
            loc = locations[row["location_id"]]
            for operand in row["operands"]:
                pc = loc["cpu_address"]
                if table == "data":
                    pc += operand["output_offset"] - loc["output_offset"]
                todo = [operand["expression_id"]]
                while todo:
                    eid = todo.pop()
                    require(by_expr[eid]["pc_context"] == {"cpu_address": pc, "segment_id": loc["segment_id"]}, "MODEL_VALUE")
                    todo.extend(expressions[eid]["children"])

    return saved


if __name__ == "__main__":
    import argparse
    import pathlib
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("model", type=pathlib.Path)
    parser.add_argument("--reconstruct", type=pathlib.Path)
    args = parser.parse_args()
    result = validate(json.loads(args.model.read_bytes()))
    if args.reconstruct:
        require(len(result) == 1, "MODEL_SOURCE_PARTITION")
        args.reconstruct.write_bytes(next(iter(result.values())))
    print("source model: valid")
