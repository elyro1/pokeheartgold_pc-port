"""Verify redistributable staging and startup from outside the package folder."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from package_runtime import linux_libraries, package


class PackageRuntimeTests(unittest.TestCase):
    def test_linux_dependency_closure_keeps_host_loader_and_glibc(self):
        output = """
    linux-vdso.so.1 (0x00007fff)
    libSDL2-2.0.so.0 => /usr/lib/libSDL2-2.0.so.0 (0x00007fff)
    libstdc++.so.6 => /usr/lib/libstdc++.so.6 (0x00007fff)
    libpulsecommon-16.1.so => /usr/lib/pulseaudio/libpulsecommon-16.1.so (0x00007fff)
    libc.so.6 => /usr/lib/libc.so.6 (0x00007fff)
    libm.so.6 => /usr/lib/libm.so.6 (0x00007fff)
    libGLX.so.0 => /usr/lib/libGLX.so.0 (0x00007fff)
    /lib64/ld-linux-x86-64.so.2 (0x00007fff)
    /lib64/ld-linux-x86-64.so.2 => /usr/lib64/ld-linux-x86-64.so.2 (0x00007fff)
"""
        with patch("package_runtime.subprocess.run", return_value=subprocess.CompletedProcess([], 0, output)):
            result = linux_libraries([Path("hg-game"), Path("hg-extract")])
        self.assertEqual(set(result), {"libSDL2-2.0.so.0", "libstdc++.so.6", "libpulsecommon-16.1.so"})

    def test_unresolved_dependency_fails_packaging(self):
        with patch("package_runtime.subprocess.run", return_value=subprocess.CompletedProcess(
                [], 0, "libSDL2-2.0.so.0 => not found\n")):
            with self.assertRaisesRegex(ValueError, "Unresolved runtime dependency"):
                linux_libraries([Path("hg-game")])

    def test_packages_each_game_with_its_header_and_blank_settings(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            build = root / "build"
            build.mkdir()
            for name in ("hg-game", "hg-extract", "save1.bin", "private.nds"):
                (build / name).write_bytes(b"test")
            library = root / "libSDL2.so"
            library.write_bytes(b"library")
            for game, code in (("heartgold", b"IPKE"), ("soulsilver", b"IPGE")):
                with self.subTest(game=game), patch("package_runtime.linux_libraries", return_value={"libSDL2.so": library}):
                    output = root / game
                    package(build, output, game, "linux")
                    header = (output / "header.bin").read_bytes()
                    self.assertEqual(len(header), 0x160)
                    self.assertEqual(header[0xc:0x10], code)
                    self.assertEqual((output / "firmware.bin").read_bytes(), b"\xff" * 0x40000)
                    self.assertEqual((output / "lib/libSDL2.so").read_bytes(), b"library")
                    self.assertFalse((output / "save1.bin").exists())
                    self.assertFalse((output / "private.nds").exists())
                    self.assertFalse(list(output.glob("*.sh")))
                    self.assertFalse(list(output.glob("*.cmd")))
                    with self.assertRaisesRegex(ValueError, "must be empty"):
                        package(build, output, game, "linux")



if __name__ == "__main__":
    unittest.main()
