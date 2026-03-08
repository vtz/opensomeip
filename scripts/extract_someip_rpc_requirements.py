#!/usr/bin/env python3
"""Extract all spec requirements from someip-rpc.rst with ID, type, and one-line summary."""

import re
from pathlib import Path

# Manual overrides for items where auto-extraction fails (compact lists, etc.)
SUMMARY_OVERRIDES = {
    "feat_req_someip_4": "Basic motivation for SOME/IP: embedded resource consumption, compatibility, AUTOSAR, automotive features, scalability.",
    "feat_req_someip_15": "Definition of terms: Method, Parameters, RPC, Request, Response, Event, Field, Service, etc.",
    "feat_req_someip_49": "Response and error message IP addresses and port shall match the request (source/dest swapped).",
    "feat_req_someip_329": "Client must construct request: payload, Message ID, Length, Request ID, Protocol/Interface Version, Message Type 0x00, Return Code 0x00.",
    "feat_req_someip_338": "Server builds response header from client header: payload, length, Message Type 0x80/0x81, Return Code.",
    "feat_req_someip_345": "Fire&Forget: no response message, Message Type set to REQUEST_NO_RETURN (0x01).",
    "feat_req_someip_356": "Notification strategies: cyclic update, update on change, epsilon change.",
    "feat_req_someip_423": "Recommended exception layout: union of specific exceptions plus dynamic length string for description.",
    "feat_req_someip_430": "Reliability semantics: Maybe, At least once, Exactly once.",
    "feat_req_someip_661": "ECU shall use IETF/IANA rules for dynamic ports: ephemeral ports 49152-65535.",
    "feat_req_someip_720": "Error handling flow chart: based on message type, errors checked in defined order.",
    "feat_req_someip_808": "Design for selective event sending compatibility when sender does not support the feature.",
    "feat_req_someip_642": "UTF-16 termination and length rules (feat_req_someip_639, 640, 641) apply to dynamic length strings.",
    "feat_req_someip_696": "Different length columns and rows in same dimension supported (see feat_req_someip_258).",
    "feat_req_someip_609": "Magic Cookie layout: Service ID 0xFFFF, Method ID 0x0000/0x8000, Length 8, Client ID 0xDEAD, Session ID 0xBEEF.",
    "feat_req_someip_813": "UDP Binding shall support dynamic switching of eventgroups between unicast and multicast based on Multicast-Threshold.",
}


def main():
    spec_path = Path(__file__).resolve().parent.parent / "open-someip-spec/src/someip-rpc.rst"
    if not spec_path.exists():
        print(f"Error: {spec_path} not found")
        return 1

    with open(spec_path) as f:
        lines = f.readlines()

    current_section = "Unknown"
    items = []

    def get_summary(block_text, rid):
        if rid in SUMMARY_OVERRIDES:
            return SUMMARY_OVERRIDES[rid]
        content_start = block_text.find(":collapse:")
        content_start = block_text.find("\n", content_start) + 1 if content_start >= 0 else 0
        rest = block_text[content_start:]
        for ln in rest.split("\n"):
            s = ln.strip()
            if not s:
                continue
            if s.startswith(".. ") or s.startswith(":"):
                continue
            if "list-table" in s or "bitfield_directive" in s or "drawsvg_directive" in s:
                continue
            if s.startswith("* "):
                s = s[2:].strip()
            s = re.sub(r":need:`[^`]+`", "ref", s)
            if len(s) > 15:
                return (s[:120] + "...") if len(s) > 120 else s
        if "Figure" in block_text:
            m = re.search(r"(Figure[^.]+)", block_text)
            return m.group(1).strip() if m else "Figure/diagram"
        if "bitfield_directive" in block_text:
            return "Bitfield diagram"
        return ""

    i = 0
    while i < len(lines):
        line = lines[i]

        hm = re.match(r"^\.\. heading::\s*(.+)$", line)
        if hm:
            section = hm.group(1).strip()
            current_section = section
            for j in range(i + 1, min(i + 6, len(lines))):
                im = re.search(r":id:\s*(feat_req_someip_\d+)", lines[j])
                if im:
                    items.append((current_section, im.group(1), "heading", section))
                    break
            i += 1
            continue

        if ".. feat_req::" in line:
            block = []
            j = i
            while j < len(lines):
                block.append(lines[j])
                j += 1
                if (
                    j < len(lines)
                    and lines[j].strip()
                    and not lines[j].startswith(" ")
                    and not lines[j].startswith("\t")
                    and ".. " in lines[j]
                ):
                    break
                if j < len(lines) and re.match(r"^\.\.\s+\w+", lines[j]) and j > i + 2:
                    break
                if j - i > 30:
                    break
            block_text = "".join(block)
            id_m = re.search(r":id:\s*(feat_req_someip_\d+)", block_text)
            type_m = re.search(r":reqtype:\s*(\w+)", block_text)
            if id_m and type_m:
                rid, rtype = id_m.group(1), type_m.group(1)
                summary = get_summary(block_text, rid)
                if not summary:
                    for ln in block_text.split("\n"):
                        s = ln.strip()
                        if 20 < len(s) < 200 and not s.startswith("..") and not s.startswith(":"):
                            summary = (s[:120] + "...") if len(s) > 120 else s
                            break
                items.append((current_section, rid, rtype, summary or "(no summary)"))
            i = j
            continue
        i += 1

    out_path = spec_path.parent.parent.parent / "someip-rpc-requirements-extracted.txt"
    with open(out_path, "w") as out:
        last_section = None
        for section, rid, rtype, summary in items:
            if section != last_section:
                out.write(f"\nSECTION: [{section}]\n")
                last_section = section
            out.write(f"- {rid} ({rtype}): {summary}\n")

    print(f"Extracted {len(items)} requirements to someip-rpc-requirements-extracted.txt")
    return 0


if __name__ == "__main__":
    exit(main())
