import json
import unittest

from sim.supervisor_process import encode_can_rx, encode_tick, parse_supervisor_line


class ProcessAdapterTests(unittest.TestCase):
    def test_encode_can_rx_uses_json_line_protocol(self):
        line = encode_can_rx(can_id=0x201, data=[3], timestamp_ms=250)

        self.assertEqual(
            json.loads(line),
            {"type": "can_rx", "id": 0x201, "data": [3], "timestamp_ms": 250},
        )
        self.assertTrue(line.endswith("\n"))

    def test_parse_supervisor_can_tx_line(self):
        event = parse_supervisor_line('{"type":"can_tx","id":256,"data":[1,3]}\n')

        self.assertEqual(event.event_type, "can_tx")
        self.assertEqual(event.can_id, 0x100)
        self.assertEqual(event.data, [1, 3])

    def test_encode_tick_uses_elapsed_milliseconds(self):
        line = encode_tick(1000)

        self.assertEqual(json.loads(line), {"type": "tick", "ms": 1000})


if __name__ == "__main__":
    unittest.main()
