import json
import tempfile
import unittest
from pathlib import Path
from aramf import ProjectMemory

class AramfCoreTests(unittest.TestCase):
    def test_init_and_control_plane_divergence(self):
        with tempfile.TemporaryDirectory() as folder:
            memory=ProjectMemory(Path(folder)); memory.initialize(); memory.append_event("BUILD_VERIFIED","Build project"); memory.append_event("DECISION_RECORDED","Record decision")
            report=memory.validate(); self.assertEqual(report["status"],"PASS"); self.assertEqual(report["durableSequence"],3); self.assertEqual(report["productionSequence"],2)
            self.assertIn("Latest Durable Sequence\n3", (Path(folder)/"aramf/memory/current-state.md").read_text())

    def test_stale_manifest_is_detected(self):
        with tempfile.TemporaryDirectory() as folder:
            memory=ProjectMemory(Path(folder)); memory.initialize(); path=Path(folder)/"aramf/memory/memory-manifest.json"; manifest=json.loads(path.read_text()); manifest["nextSequenceNumber"]=99; path.write_text(json.dumps(manifest)); self.assertEqual(memory.validate()["status"],"FAIL")

if __name__ == "__main__": unittest.main()
