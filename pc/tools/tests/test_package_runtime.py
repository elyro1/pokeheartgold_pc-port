"""Verify redistributable staging and startup from outside the package folder."""
from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from package_runtime import linux_libraries, package, verify_linux_runtime


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
                with self.subTest(game=game), \
                        patch("package_runtime.linux_libraries", return_value={"libSDL2.so": library}), \
                        patch("package_runtime.relocate_linux_libraries"), \
                        patch("package_runtime.verify_linux_runtime"):
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

    def test_runtime_verification_rejects_host_fallback(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary)
            (output / "lib").mkdir()
            with patch("package_runtime.linux_libraries", return_value={
                    "libpulsecommon.so": Path("/usr/lib/libpulsecommon.so")}):
                with self.assertRaisesRegex(ValueError, "outside package"):
                    verify_linux_runtime(output, ["hg-game"])

    @unittest.skipUnless(sys.platform == "linux" and shutil.which("cc") and shutil.which("patchelf"),
                         "Linux ELF integration test needs cc and patchelf")
    def test_relocated_runtime_resolves_library_with_absolute_vendor_runpath(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            build, vendor = root / "build", root / "vendor"
            (build / "lib").mkdir(parents=True)
            vendor.mkdir()
            (root / "private.c").write_text("int private_value(void) { return 42; }\n", encoding="utf-8")
            (root / "parent.c").write_text(
                "int private_value(void); int parent_value(void) { return private_value(); }\n", encoding="utf-8")
            (root / "main.c").write_text(
                "int parent_value(void); int main(void) { return parent_value() != 42; }\n", encoding="utf-8")
            commands = [
                ["cc", "-shared", "-fPIC", str(root / "private.c"),
                 "-Wl,-soname,libhgci_private.so", "-o", str(vendor / "libhgci_private.so")],
                ["cc", "-shared", "-fPIC", str(root / "parent.c"), "-L" + str(vendor),
                 "-lhgci_private", "-Wl,-soname,libhgci_parent.so", "-Wl,--enable-new-dtags",
                 "-Wl,-rpath," + str(vendor), "-o", str(build / "lib/libhgci_parent.so")],
                ["cc", str(root / "main.c"), "-L" + str(build / "lib"), "-lhgci_parent",
                 "-Wl,-rpath-link," + str(vendor), "-Wl,--disable-new-dtags,-rpath,$ORIGIN/lib",
                 "-o", str(build / "hg-game")],
            ]
            for command in commands:
                subprocess.run(command, check=True, capture_output=True)
            shutil.copy2(build / "hg-game", build / "hg-extract")
            output = root / "package"
            package(build, output, "heartgold", "linux")
            # Neither the original build nor its vendor directory is available
            # on a user's computer. Move the package and hide both source paths.
            relocated = root / "relocated package with spaces"
            output.rename(relocated)
            build.rename(root / "hidden build")
            vendor.rename(root / "hidden vendor")
            verify_linux_runtime(relocated, ["hg-game", "hg-extract"])
            environment = {key: value for key, value in os.environ.items()
                           if not key.startswith("LD_")}
            for name in ("hg-game", "hg-extract"):
                subprocess.run([str(relocated / name)], cwd=root, env=environment, check=True)



if __name__ == "__main__":
    unittest.main()
