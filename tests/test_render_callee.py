import importlib.util
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
SPEC=importlib.util.spec_from_file_location("render_compare",ROOT/"tools/compare_render_callee.py")
MODULE=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(MODULE)

class RenderCalleeTests(unittest.TestCase):
    def test_retained_two_bank_replay_and_frontiers(self):
        self.assertEqual(MODULE.main(),0)

if __name__=="__main__":unittest.main()
