import importlib.util
import tempfile
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,ROOT/path);value=importlib.util.module_from_spec(spec);spec.loader.exec_module(value);return value
GEN=module("frame_generator","tools/frame_selection_oracle.py")
CMP=module("frame_compare","tools/compare_frame_selection_oracle.py")

class FrameOracleTests(unittest.TestCase):
    def test_render_has_seven_complete_finite_cases(self):
        text=GEN.render(Path("work/mame/example"))
        self.assertEqual(text.count("# case "),7)
        self.assertEqual(text.count("gtime 0x2"),7)
        self.assertEqual(text.count("do pc=0xffffffff880016c4"),7)
        self.assertIn("bp 0x88001898",text)

    def test_context_rejects_changed_entry_register(self):
        values={r:0x5a5a000000000000+i for i,r in enumerate(GEN.REGS,1)}
        values.update(v1=0x12,a1=0x1a,gp=4,sp=0xffffffff88087300,
                      fp=0xffffffff8808c000,ra=0xffffffff88001898,
                      hi=0x1111222233334444,lo=0xaaaabbbbccccdddd,pc=0)
        fields=" ".join(f"{r}={values[r]:016X}" for r in GEN.REGS+("hi","lo","pc"))
        log="\n".join(f"KI_FRAME phase=entry case={n:X} {fields}" for n in range(1,8))
        parsed=CMP.contexts(log,"entry")
        with self.assertRaisesRegex(ValueError,"changed entry v1"):
            CMP.validate_entry_context(parsed)

    def test_context_rejects_extra_or_malformed_cases(self):
        fields=" ".join(f"{r}=0000000000000000" for r in GEN.REGS+("hi","lo","pc"))
        with self.assertRaises(ValueError):CMP.contexts("\n".join(f"KI_FRAME phase=entry case={n:X} {fields}" for n in range(1,9)),"entry")
        with self.assertRaises(ValueError):CMP.contexts("KI_FRAME phase=entry case=1 trailing", "entry")

    def test_script_binding_rejects_intervening_execution(self):
        with tempfile.TemporaryDirectory(dir=ROOT/"work") as temporary:
            directory=Path(temporary);script=GEN.render(directory.relative_to(ROOT))
            (directory/"oracle.cmd").write_text(script.replace("do pc=0xffffffff880016c4","gtime 0x1\ndo pc=0xffffffff880016c4",1))
            (directory/"run.log").write_text("")
            with self.assertRaisesRegex(ValueError,"script identity"):CMP.compare(directory)

if __name__=="__main__":unittest.main()
