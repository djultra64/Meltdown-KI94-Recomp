import importlib.util
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
SPEC=importlib.util.spec_from_file_location("contact_compare",ROOT/"tools/compare_contact_transaction.py")
MODULE=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(MODULE)

class ContactTransactionTests(unittest.TestCase):
    def test_retained_original_replay_and_platform_negatives(self):
        self.assertEqual(MODULE.main(),0)

if __name__=="__main__":unittest.main()
