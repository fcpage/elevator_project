import unittest

from sim.can import CanBus, CanFrame
from sim.supervisor_process import SupervisorEvent
from sim.supervisor_node import ProcessSupervisorNode


class FakeSupervisorProcess:
    def __init__(self):
        self.started = False
        self.received = []

    def start(self):
        self.started = True

    def stop(self):
        self.started = False

    def send_can_rx(self, frame):
        self.received.append(frame)
        if frame.can_id == 0x201:
            return SupervisorEvent(event_type="can_tx", can_id=0x100, data=[0x05])
        return None


class ProcessSupervisorNodeTests(unittest.TestCase):
    def test_process_supervisor_converts_process_can_tx_to_bus_frame(self):
        bus = CanBus()
        process = FakeSupervisorProcess()
        emitted = []
        bus.subscribe(0x100, emitted.append)
        node = ProcessSupervisorNode(bus, process)

        node.start()
        bus.send(CanFrame(can_id=0x201, data=[1], source="floor_1"))

        self.assertTrue(process.started)
        self.assertEqual(emitted[-1], CanFrame(can_id=0x100, data=[0x05], source="supervisor_process"))


if __name__ == "__main__":
    unittest.main()
