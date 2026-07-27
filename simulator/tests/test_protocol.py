import unittest

from sim.can import CanFrame
from sim.protocol import (
    CAN_BITRATE_BITS_PER_SECOND,
    CC_CAN_ID,
    SC_CAN_ID,
    encode_supervisor_command,
    floor_from_payload,
    payload_has_status_or_enable,
)


class ProtocolTests(unittest.TestCase):
    def test_protocol_records_physical_can_bitrate(self):
        self.assertEqual(CAN_BITRATE_BITS_PER_SECOND, 250_000)

    def test_supervisor_command_encoding_uses_protocol_layout(self):
        frame = encode_supervisor_command(3, enable=True)

        self.assertEqual(frame, CanFrame(can_id=SC_CAN_ID, data=[0x07], source="supervisor"))

    def test_payload_helpers_accept_status_bit_without_changing_floor(self):
        self.assertEqual(floor_from_payload(0x06), 2)
        self.assertTrue(payload_has_status_or_enable(0x06))

    def test_car_controller_id_is_defined_in_protocol_module(self):
        self.assertEqual(CC_CAN_ID, 0x200)


if __name__ == "__main__":
    unittest.main()
