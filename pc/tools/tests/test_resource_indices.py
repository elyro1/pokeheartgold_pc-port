"""Check resource generation on hosts whose default text encoding is CP1252."""
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import resource_indices
from message_headers import write_if_changed


def cp1252_text_encoding(encoding, stacklevel=2):
    return "cp1252" if encoding is None else encoding


class ResourceEncodingTests(unittest.TestCase):
    def test_requested_headers_with_utf8_source_and_cp1252_default(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "src").mkdir()
            (root / "include").mkdir()
            # U+3041 contains byte 0x81 in UTF-8, undefined in CP1252.
            (root / "src/example.c").write_text(
                '/* \u3041 */\n#include "graphic/example.naix"\n', encoding="utf-8")
            (root / "include/example.h").write_text(
                '/* \u3041 */\n#include <graphic/other.naix>\n'
                '#include "msgdata/msg.naix"\n', encoding="utf-8")
            with patch.object(resource_indices, "ROOT", root), \
                    patch("io.text_encoding", cp1252_text_encoding):
                self.assertEqual(resource_indices.requested_headers(),
                                 ["graphic/example.naix", "graphic/other.naix"])

    def test_generated_text_round_trips_utf8_with_cp1252_default(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "generated/header.h"
            text = "/* \u3041 */\n"
            with patch("io.text_encoding", cp1252_text_encoding):
                write_if_changed(path, text)
                timestamp = path.stat().st_mtime_ns
                write_if_changed(path, text)
            # Text writes use native line endings; decoding must still be UTF-8.
            self.assertEqual(path.read_text(encoding="utf-8"), text)
            self.assertEqual(path.stat().st_mtime_ns, timestamp)


if __name__ == "__main__":
    unittest.main()
