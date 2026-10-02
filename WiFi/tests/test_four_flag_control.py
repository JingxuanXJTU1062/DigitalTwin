import importlib.util
import pathlib
import struct
import unittest


MODULE_PATH = pathlib.Path(__file__).resolve().parents[1] / "mcu_wifi_listener.py"
SPEC = importlib.util.spec_from_file_location("mcu_wifi_listener", MODULE_PATH)
listener = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(listener)


class FourFlagControlTests(unittest.TestCase):
    def test_legacy_packet_contains_four_flags_and_valid_checksum(self):
        packet = listener.build_cmd_packet(1, 0, 1, 0)
        self.assertEqual(packet, b"$CMD,1,0,1,0*4A\r\n")

    def test_modbus_coil_read_defaults_to_four_flags(self):
        client = listener.ModbusTcpClient()
        calls = []

        def fake_exchange(function, payload):
            calls.append((function, payload))
            return bytes((0x01, 0x01, 0b00001001))

        client.exchange = fake_exchange
        self.assertEqual(client.read_coils(), [True, False, False, True])
        self.assertEqual(calls, [(0x01, struct.pack(">HH", 0, 4))])

    def test_four_host_flags_are_defined(self):
        self.assertEqual(
            [listener.FLAG1, listener.FLAG2, listener.FLAG3, listener.FLAG4],
            [1, 1, 1, 1],
        )


if __name__ == "__main__":
    unittest.main()
