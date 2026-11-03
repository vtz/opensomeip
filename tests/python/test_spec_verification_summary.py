"""Unit tests for spec-to-test class counts in the traceability matrix."""

import importlib.util
from pathlib import Path

_SCRIPT = Path(__file__).resolve().parents[2] / "scripts" / "generate_traceability_matrix.py"
_SPEC = importlib.util.spec_from_file_location("generate_traceability_matrix", _SCRIPT)
_MATRIX = importlib.util.module_from_spec(_SPEC)
assert _SPEC.loader is not None
_SPEC.loader.exec_module(_MATRIX)

Requirement = _MATRIX.Requirement
_TestCase = _MATRIX.TestCase
summarize_spec_verification = _MATRIX.summarize_spec_verification


def _req(req_id: str, satisfies: list[str], tested_by: list[str]) -> Requirement:
    return Requirement(id=req_id, satisfies=satisfies, tested_by=tested_by)


def _case(case_id: str, location: str) -> _TestCase:
    return _TestCase(id=case_id, name=case_id, location=location, tests=[])


def test_counts_union_of_linked_software_requirements() -> None:
    requirements = {
        "REQ_A": _req("REQ_A", ["feat_req_someip_1", "REQ_INTERNAL"], ["TC_UNIT"]),
        "REQ_B": _req("REQ_B", ["feat_req_someip_1", "feat_req_someip_2"], ["TC_INT"]),
        "REQ_C": _req("REQ_C", ["feat_req_someip_3"], ["TC_SYS", "TC_MISSING"]),
        "REQ_D": _req("REQ_D", ["feat_req_someip_4"], []),
        "REQ_E": _req("REQ_E", ["REQ_ONLY"], ["TC_UNIT"]),
    }
    test_cases = {
        "TC_UNIT": _case("TC_UNIT", "tests/test_message.cpp"),
        "TC_INT": _case("TC_INT", "tests/integration/test_rpc_spec_integration.py"),
        "TC_SYS": _case("TC_SYS", "tests/system/test_full_stack_integration.py"),
    }

    summary = summarize_spec_verification(requirements, test_cases)

    assert summary == {
        "linked": 4,
        "integration": 2,
        "system": 1,
        "integration_or_system": 3,
        "unit_only": 0,
        "no_test": 1,
    }


def test_unit_only_when_no_integration_or_system_test_reaches_the_spec() -> None:
    requirements = {
        "REQ_A": _req("REQ_A", ["Feat_Req_Someip_9"], ["TC_UNIT"]),
    }
    test_cases = {"TC_UNIT": _case("TC_UNIT", "tests/test_message.cpp")}

    summary = summarize_spec_verification(requirements, test_cases)

    assert summary["linked"] == 1
    assert summary["unit_only"] == 1
    assert summary["integration_or_system"] == 0
    assert summary["no_test"] == 0
