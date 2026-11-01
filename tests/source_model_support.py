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
import re


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
    require(bool(files), "MODEL_SOURCE_PARTITION")
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


def check_units(model, syntax, raw):
    """Check lexical labels against embedded bytes, not a second source parse."""
    punctuation = dict(zip(
        [b",", b":", b"#", b"(", b")", b"[", b"]", b"+", b"-", b"*", b"/", b"%", b"&", b"|", b"^", b"<", b">", b"~", b"!", b"$",
         b"<<", b">>", b"<=", b">=", b"==", b"!=", b"A", b"X", b"Y"],
        ["COMMA", "COLON", "IMMEDIATE", "LPAREN", "RPAREN", "LBRACKET", "RBRACKET", "PLUS", "MINUS", "STAR", "SLASH", "PERCENT", "AMPERSAND", "PIPE", "CARET", "LESS", "GREATER", "TILDE", "BANG", "CURRENT_PC",
         "SHL_OP", "SHR_OP", "LE_OP", "GE_OP", "EQ_OP", "NE_OP", "ACCUMULATOR", "REGISTER_X", "REGISTER_Y"]))
    if model["semantic_options"]["swap_parens"]:
        for a, b in ((b"(", b"["), (b")", b"]")): punctuation[a], punctuation[b] = punctuation[b], punctuation[a]
    directives = {b"DB": "BYTE", b"BYTE": "BYTE", b"DW": "WORD", b"WORD": "WORD",
                  b"ORG": "ORG", b"EQU": "EQU", b"END": "END"}
    mnemonics = {m.encode() for m, _ in OPCODES.values()} | {b"BLT", b"BGE"}
    counts, by_file = defaultdict(int), defaultdict(list)
    for u in model["units"]:
        b = raw[u["file_id"]][u["byte_start"]:u["byte_end_exclusive"]]
        kind = "newline" if b == b"\n" else "comment" if b.startswith(b";") else "whitespace" if not b.strip(b" \t") else "token"
        require(u["kind"] == kind, "MODEL_SOURCE_PARTITION")
        if kind != "token":
            require(u["syntax_id"] is None and u["token_kind"] == ("NEWLINE" if kind == "newline" else None), "MODEL_SOURCE_PARTITION")
            continue
        text = b.strip(b" \t").upper()
        if text != b":" and text.endswith(b":"): expected = "LOCAL_LABEL" if text.startswith(b"@@") else "LABEL"
        elif text in punctuation: expected = punctuation[text]
        elif text.lstrip(b".") in directives: expected = directives[text.lstrip(b".")]
        elif text.removesuffix(b".W") in mnemonics: expected = "MNEMONIC"
        elif text.startswith(b'"') and text.endswith(b'"'): expected = "STRING_LITERAL"
        elif re.fullmatch(rb"(?:%[01]+|[01]+B|0[0-7]*|[1-9][0-9]*|0X[0-9A-F]+|\$[0-9A-F]+|[0-9][0-9A-F]*H|'.')", text): expected = "INTEGER_LITERAL"
        elif text.startswith(b"@@"): expected = "LOCAL_ID"
        elif set(text) == {43}: expected = "FORWARD_BRANCH"
        elif set(text) == {45}: expected = "BACKWARD_BRANCH"
        else: expected = "IDENTIFIER"
        require(u["token_kind"] == expected, "MODEL_SOURCE_PARTITION")
        counts[expected] += 1
        by_file[u["file_id"]].append(u)
    # Independently derive the smallest containing syntax span for every token.
    starts = {fid: [u["byte_start"] for u in us] for fid, us in by_file.items()}
    owners = {}
    for node in syntax.values():
        sp = node["span"]
        if not sp or sp["file_id"] not in by_file: continue
        us = by_file[sp["file_id"]]
        a = bisect_left(starts[sp["file_id"]], sp["byte_start"])
        b = bisect_left(starts[sp["file_id"]], sp["byte_end_exclusive"])
        for at in range(a, b):
            u = us[at]
            if u["byte_end_exclusive"] <= sp["byte_end_exclusive"]: owners[u["id"]] = node["id"]
    for us in by_file.values():
        for u in us: require(u["syntax_id"] == owners.get(u["id"]) and u["syntax_id"] in syntax, "MODEL_SOURCE_PARTITION")
    for tokens, kinds in ((["END"], ["END_NODE"]), (["EQU"], ["EQU_NODE"]), (["ORG"], ["ORG_NODE"]),
                         (["MNEMONIC"], ["INSTRUCTION_NODE"]), (["BYTE", "WORD"], ["DATA_NODE"]),
                         (["LABEL"], ["LABEL_NODE"]), (["LOCAL_LABEL"], ["LOCAL_LABEL_NODE"])):
        require(sum(counts[t] for t in tokens) == sum(n["kind"] in kinds for n in syntax.values()), "MODEL_REFERENCE_COVERAGE")


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
    roots_by_context, context_by_syntax, operand_by_syntax = {}, {}, {}
    for sid, node in syntax.items():
        children = node["children"]
        roots = (children[1:] if node["kind"] in ("EQU_NODE", "DATA_NODE") else
                 children if node["kind"] in ("ORG_NODE", "INSTRUCTION_NODE") else [])
        roots_by_context[sid] = roots
        context = sid
        while syntax[context]["parent_id"] and syntax[syntax[context]["parent_id"]]["kind"] != "LIST_NODE":
            context = syntax[context]["parent_id"]
        todo = [(child, pos) for pos, child in enumerate(roots)]
        while todo:
            child, pos = todo.pop()
            context_by_syntax[child] = context
            operand_by_syntax[child] = pos if node["kind"] in ("DATA_NODE", "INSTRUCTION_NODE", "ORG_NODE") else None
            if syntax[child]["kind"] in expression_kinds:
                require(child not in expected_expressions, "MODEL_REFERENCE_COVERAGE")
                expected_expressions.add(child)
                if syntax[child]["kind"] in reference_kinds:
                    expected_reference_syntax.add(child)
            todo.extend((c, pos) for c in syntax[child]["children"])
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
        require(e["span"] == node["span"] and e["context_syntax_id"] == context_by_syntax[e["syntax_id"]], "MODEL_REFERENCE_COVERAGE")
        span_bytes(e["span"])
    for d in declarations.values():
        require(d["syntax_id"] in syntax, "MODEL_BINDING_TARGET")
        node = syntax[d["syntax_id"]]
        expected_kind = {"EQU_NODE": "equ", "LABEL_NODE": "global", "LOCAL_LABEL_NODE": "local",
                         "FORWARD_BRANCH_DECL_NODE": "anonymous", "BACKWARD_BRANCH_DECL_NODE": "anonymous"}[node["kind"]]
        require(d["kind"] == expected_kind and d["symbol_id"] == d["id"], "MODEL_BINDING_TARGET")
        if d["kind"] == "equ":
            require(d["name_span"] == syntax[node["children"][0]]["span"] and
                    d["expression_id"] in expressions and
                    expressions[d["expression_id"]]["syntax_id"] == node["children"][1] and
                    d["location_id"] is None, "MODEL_BINDING_TARGET")
        else:
            raw = span_bytes(node["span"])
            start = node["span"]["byte_start"] + len(raw) - len(raw.lstrip(b" \t"))
            require(d["name_span"] == {"file_id": node["span"]["file_id"], "byte_start": start,
                                       "byte_end_exclusive": start + len(d["name"])} and
                    d["expression_id"] is None, "MODEL_BINDING_TARGET")
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
        expression = expressions[ref["expression_id"]]
        require(ref["span"] == expression["span"] and ref["context_syntax_id"] == expression["context_syntax_id"],
                "MODEL_REFERENCE_COVERAGE")
        spelling = span_bytes(ref["span"]).decode("ascii")
        require(ref["spelling"] == spelling, "MODEL_BINDING_TARGET")
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
    root_scope = roots[0]["id"]
    require(len(files) == 1 and roots[0]["extent"] == {
        "file_id": next(iter(files)), "byte_start": 0,
        "byte_end_exclusive": len(next(iter(source_bytes.values())))}, "MODEL_BINDING_TARGET")
    scope_anchors = defaultdict(list)
    for did in globals_:
        sp = syntax[declarations[did]["syntax_id"]]["span"]
        # A label's parsed span may begin before its name, at indentation.
        scope_anchors[sp["file_id"]].append((sp["byte_start"], did))
    for fid, chain in scope_anchors.items():
        chain.sort()
        for at, (start, did) in enumerate(chain):
            q = scopes[scope_by_owner[did]]
            end = chain[at + 1][0] if at + 1 < len(chain) else len(source_bytes[fid])
            require(q["kind"] == "global" and q["parent_id"] == root_scope and q["extent"] == {
                "file_id": fid, "byte_start": start, "byte_end_exclusive": end}, "MODEL_BINDING_TARGET")
    for node in syntax.values():
        expected_scope = root_scope
        if node["parent_id"] is not None:
            sp = node["span"]
            chain = scope_anchors[sp["file_id"]]
            at = bisect_right(chain, (sp["byte_start"], "\uffff")) - 1
            if at >= 0: expected_scope = scope_by_owner[chain[at][1]]
        require(node["scope_id"] == expected_scope, "MODEL_BINDING_TARGET")
    for decl in declarations.values():
        expected_owner = owner(decl["name_span"])
        require(decl["scope_id"] == scope_by_owner[expected_owner]
                and decl["lexical_owner_id"] == expected_owner, "MODEL_BINDING_TARGET")
    for ref in references.values():
        require(ref["scope_id"] == scope_by_owner[owner(ref["span"])] and ref["diagnostic_id"] is None,
                "MODEL_BINDING_TARGET")
    check_units(model, syntax, source_bytes)
    require(model["exit_status"] in (0, 4, 5), "MODEL_DIAGNOSTIC_STATE")
    comparison, audit = model["analysis_outcomes"]["comparison"], model["analysis_outcomes"]["audit"]
    require(comparison in ("not_requested", "not_run", "equal", "mismatch") and
            audit in ("not_requested", "not_run", "clear", "findings"), "MODEL_DIAGNOSTIC_STATE")
    compared = [d for d in model["dependencies"] if "comparison" in d["roles"]]
    requested_compare = any(a == "--compare" or a.startswith("--compare=") for a in model["invocation"]["argv"])
    require(requested_compare == (comparison != "not_requested"), "MODEL_DIAGNOSTIC_STATE")
    require(comparison not in ("equal", "mismatch") or any(not d["missing"] for d in compared), "MODEL_DIAGNOSTIC_STATE")
    require((model["exit_status"] == 5) == (comparison == "mismatch"), "MODEL_DIAGNOSTIC_STATE")
    require(model["exit_status"] != 4 or audit == "findings", "MODEL_DIAGNOSTIC_STATE")
    for f in files.values():
        require(any("source" in d["roles"] and d["sha256"] == f["sha256"] and not d["missing"]
                    for d in model["dependencies"]), "MODEL_IDENTITY")
    require(any("producer" in d["roles"] and d["sha256"] == identity["producer_sha256"]
                for d in model["dependencies"]), "MODEL_IDENTITY")
    # Reachability, not incidental first-pass folding, determines EQU origins.
    equ_by_syntax = {d["syntax_id"]: d for d in declarations.values() if d["kind"] == "equ"}
    refs_by_context = defaultdict(list)
    for r in references.values(): refs_by_context[r["context_syntax_id"]].append(r)
    pending = [r["declaration_id"] for r in references.values() if r["context_syntax_id"] not in equ_by_syntax]
    reached = set()
    while pending:
        did = pending.pop()
        if did in reached: continue
        reached.add(did)
        decl = declarations[did]
        if decl["kind"] == "equ": pending.extend(r["declaration_id"] for r in refs_by_context[decl["syntax_id"]])
    def origin(context):
        return "assembly" if context not in equ_by_syntax or equ_by_syntax[context]["id"] in reached else "analysis"
    for ref in references.values():
        require(ref["resolution_origin"] == origin(ref["context_syntax_id"]), "MODEL_VALUE")
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
    for d in declarations.values():
        if d["kind"] != "equ":
            require(d["location_id"] in locations and locations[d["location_id"]]["syntax_id"] == d["syntax_id"], "MODEL_SEGMENT")
    for row in evaluations.values():
        e = expressions[row["expression_id"]]
        require(row["context_syntax_id"] == e["context_syntax_id"] and
                row["operand_index"] == operand_by_syntax[e["syntax_id"]] and
                row["resolution_origin"] == origin(e["context_syntax_id"]), "MODEL_VALUE")
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
        loc = by_syntax[emission["syntax_id"]]
        require(emission["segment_id"] == loc["segment_id"], "MODEL_SEGMENT")
        require(emission["cpu_address"] == loc["cpu_address"] + emission["output_start"] - loc["output_offset"], "MODEL_EMISSION_RANGE")
    require(offset == len(binary), "MODEL_EMISSION_RANGE")
    segment_list = list(segments.values())
    require(segment_list[0] == {"id": "g1", "start_syntax_id": None, "address_space": "cpu", "origin": 0, "output_offset": 0}, "MODEL_SEGMENT")
    segment_at, pc, output = 0, 0, 0
    lexical_pc = {}
    for s in syntax.values():
        if s["kind"] in ("EQU_NODE", "ORG_NODE"):
            lexical_pc[s["id"]] = {"cpu_address": pc, "segment_id": segment_list[segment_at]["id"]}
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
    pc_contexts = {e["context_syntax_id"] for e in expressions.values() if e["kind"] == "pc"}
    for v in evaluations.values():
        context = v["context_syntax_id"]
        if context in lexical_pc:
            expected_pc = dict(lexical_pc[context])
            if syntax[context]["kind"] == "EQU_NODE" and context not in pc_contexts:
                expected_pc["cpu_address"] = None
            require(v["pc_context"] == expected_pc, "MODEL_VALUE")
    for table, kind in (("data", "DATA_NODE"), ("instructions", "INSTRUCTION_NODE")):
        rows = index(model[table])
        require(len({r["syntax_id"] for r in rows.values()}) == len(rows) and
                {r["syntax_id"] for r in rows.values()} ==
                {sid for sid, node in syntax.items() if node["kind"] == kind}, "MODEL_REFERENCE_COVERAGE")
        for row in rows.values():
            require(row["location_id"] in locations and locations[row["location_id"]]["syntax_id"] == row["syntax_id"], "MODEL_SEGMENT")
            require(row["emission_ids"] == [e["id"] for e in emissions_by_syntax[row["syntax_id"]]], "MODEL_EMISSION_RANGE")
            require([expressions[o["expression_id"]]["syntax_id"] for o in row["operands"]] ==
                    roots_by_context[row["syntax_id"]], "MODEL_REFERENCE_COVERAGE")
            for pos, operand in enumerate(row["operands"]):
                require(operand["index"] == pos, "MODEL_EMISSION_RANGE")
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
        kind = ("branch" if mode == "relative" else "jump" if mnemonic == "JMP" else "call" if mnemonic == "JSR"
                else "return" if mnemonic in ("RTS", "RTI") else "trap" if mnemonic == "BRK" else "none")
        displacement = int.from_bytes(code[1:], "little", signed=True) if kind == "branch" else None
        target = ((loc["cpu_address"] + 2 + displacement) & 0xffff if kind == "branch" else
                  native_operand if kind in ("call", "jump") and mode != "indirect" else None)
        require(row["control_flow"] == {"kind": kind, "target": target, "encoded_displacement": displacement}, "MODEL_VALUE")
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
