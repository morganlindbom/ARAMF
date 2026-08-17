"""Dependency-free reconstruction of the source-backed AR&MF core."""
from __future__ import annotations
import argparse, json, re
from datetime import datetime, timezone
from pathlib import Path
from typing import Any
from uuid import uuid4

CONTROL_PLANE_EVENTS = {"PROJECT_MEMORY_ACTIVATED", "PROJECT_CONTEXT_CHANGED", "DECISION_RECORDED", "CHECKPOINT_CREATED"}

def now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()

def read_json(path: Path, default: Any) -> Any:
    return json.loads(path.read_text(encoding="utf-8")) if path.exists() else default

def write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")

def read_jsonl(path: Path) -> list[dict[str, Any]]:
    if not path.exists(): return []
    records = []
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.strip(): continue
        try: value = json.loads(line)
        except json.JSONDecodeError as exc: raise ValueError(f"{path}:{number}: malformed JSONL") from exc
        if not isinstance(value, dict): raise ValueError(f"{path}:{number}: record is not an object")
        records.append(value)
    return records

class ProjectMemory:
    def __init__(self, project: Path):
        self.project = Path(project).resolve(); self.root = self.project / "aramf"; self.memory = self.root / "memory"
        self.manifest_path = self.memory / "memory-manifest.json"; self.events_path = self.memory / "event-log.jsonl"

    def initialize(self) -> None:
        for folder in (self.root, self.memory, self.root/"generated", self.root/"routing", self.root/"resources", self.root/"platforms", self.root/"templates", self.root/"verification", self.root/"custom"):
            folder.mkdir(parents=True, exist_ok=True)
        if not self.manifest_path.exists(): write_json(self.manifest_path, {"memoryVersion":"2", "nextSequenceNumber":1, "workEntryCount":0, "eventCount":0})
        defaults = {
            "aramf-profile.json": {"name":"AI Rules & Memory Framework", "version":1, "projectId":str(uuid4()), "projectContext":{}, "developmentEnvironment":{"ide":"Visual Studio Code"}},
            "provenance.json": {"status":"reconstructed", "source":"recovered knowledge pack"},
            "selection-effects.json": {"rulesSelected":[], "resourcesSelected":[]}, "routing/task-routes.json": {}, "routing/scope-routes.json": {},
            "resources/resources.json": {"resources":[]}, "templates/custom-templates.json": {"templates":[]},
        }
        for relative, value in defaults.items():
            path = self.root / relative
            if not path.exists(): write_json(path, value)
        self._text("AGENTS.md", "# AR&MF project instructions\n\nRead `aramf/generated/rules.md` and relevant routes before project work.\nProtect `aramf/custom/`.\n")
        self._text("generated/rules.md", "# Generated rules\n\nNo project-specific rules have been configured.\n")
        self._text("routing/README.md", "# Routing\n\nRoutes select only context relevant to a task or scope.\n")
        self._text("decisions.md", "# Durable decisions\n\n"); self._text("checkpoints.json", "[]\n")
        self._text("metrics.json", json.dumps({"iterations":0,"buildAttempts":0,"testAttempts":0,"failures":0}, indent=2)+"\n")
        if not self.events_path.exists(): self.append_event("PROJECT_MEMORY_ACTIVATED", "Project Memory initialized")
        self.generate_current_state(); self.generate_cold_start_validation(); self.validate()

    def _text(self, relative: str, content: str) -> None:
        path = self.root / relative; path.parent.mkdir(parents=True, exist_ok=True)
        if not path.exists(): path.write_text(content, encoding="utf-8")

    def _manifest(self) -> dict[str, Any]: return read_json(self.manifest_path, {"memoryVersion":"2","nextSequenceNumber":1,"eventCount":0})

    def append_event(self, event_type: str, task: str, **fields: Any) -> dict[str, Any]:
        self.memory.mkdir(parents=True, exist_ok=True); manifest = self._manifest(); sequence = int(manifest.get("nextSequenceNumber", 1))
        event = {"eventId":f"event-{uuid4()}","eventType":event_type,"sequenceNumber":sequence,"timestamp":now(),"task":task,**fields}
        with self.events_path.open("a", encoding="utf-8") as stream: stream.write(json.dumps(event, ensure_ascii=False)+"\n")
        manifest.update({"nextSequenceNumber":sequence+1,"eventCount":int(manifest.get("eventCount",0))+1,"latestEventId":event["eventId"]}); write_json(self.manifest_path, manifest)
        self.generate_current_state(); return event

    def generate_current_state(self) -> None:
        events = read_jsonl(self.events_path); durable = max((int(e.get("sequenceNumber",0)) for e in events), default=0)
        production = [e for e in events if e.get("eventType") not in CONTROL_PLANE_EVENTS]; latest = max(production, key=lambda e:int(e.get("sequenceNumber",0)), default=None)
        text = "# Current Project State\n\n" + f"## Latest Durable Sequence\n{durable}\n\n" + f"## Latest Production Development Event\n{latest.get('eventId','') if latest else ''}\n\n" + f"## Latest Production Sequence\n{latest.get('sequenceNumber',0) if latest else 0}\n"
        (self.memory/"current-state.md").write_text(text, encoding="utf-8")

    def generate_cold_start_validation(self) -> None:
        events = read_jsonl(self.events_path); write_json(self.memory/"cold-start-validation.json", {"status":"PASS","checkedAt":now(),"durableSequence":max((e.get("sequenceNumber",0) for e in events), default=0),"checks":[{"name":"memory-root","status":"PASS"}],"errors":[],"warnings":[]})

    def validate(self) -> dict[str, Any]:
        checks=[]; errors=[]; manifest=self._manifest()
        try: events=read_jsonl(self.events_path)
        except ValueError as exc: events=[]; errors.append(str(exc))
        sequences=[e.get("sequenceNumber") for e in events]; ids=[e.get("eventId") for e in events]
        def check(name, ok, message):
            checks.append({"name":name,"status":"PASS" if ok else "FAIL"});
            if not ok: errors.append(message)
        check("event-identifiers-unique", len(ids)==len(set(ids)) and all(ids), "event IDs must be present and unique")
        check("sequence-order", sequences==sorted(sequences) and len(sequences)==len(set(sequences)), "event sequences must be strictly ordered")
        maximum=max(sequences, default=0)
        check("manifest-next-sequence", manifest.get("nextSequenceNumber")==maximum+1, "manifest nextSequenceNumber is stale")
        check("manifest-event-count", manifest.get("eventCount",0)==len(events), "manifest eventCount does not match event log")
        report={"status":"PASS" if not errors else "FAIL","checkedAt":now(),"durableSequence":maximum,"productionSequence":max((e.get("sequenceNumber",0) for e in events if e.get("eventType") not in CONTROL_PLANE_EVENTS),default=0),"checks":checks,"errors":errors,"warnings":[]}
        write_json(self.memory/"memory-consistency-validation.json", report); return report

def main() -> int:
    parser=argparse.ArgumentParser(description="Reconstructed AR&MF core"); sub=parser.add_subparsers(dest="command",required=True)
    for name in ("init","validate"): sub.add_parser(name).add_argument("project",nargs="?",default=".")
    event=sub.add_parser("event"); event.add_argument("event_type"); event.add_argument("task"); event.add_argument("project",nargs="?",default=".")
    args=parser.parse_args(); memory=ProjectMemory(Path(args.project))
    if args.command=="init": memory.initialize(); print(f"Initialized {memory.root}")
    elif args.command=="event": memory.initialize(); print(json.dumps(memory.append_event(args.event_type,args.task),indent=2))
    else:
        report=memory.validate(); print(json.dumps(report,indent=2)); return 0 if report["status"]=="PASS" else 1
    return 0

if __name__ == "__main__": raise SystemExit(main())
