import unittest

from sim.can import CanBus, CanFrame
from sim.clock import SimClock
from sim.nodes import ElevatorControllerNode


class CanBusTests(unittest.TestCase):
    def test_bus_delivers_frames_to_matching_subscribers(self):
        bus = CanBus()
        received = []

        bus.subscribe(0x101, received.append)
        bus.send(CanFrame(can_id=0x101, data=[1, 2], source="test"))

        self.assertEqual(received, [CanFrame(can_id=0x101, data=[1, 2], source="test")])


class ElevatorNodeTests(unittest.TestCase):
    def test_elevator_moves_to_requested_floor_and_reports_position(self):
        clock = SimClock()
        bus = CanBus()
        reports = []
        bus.subscribe(0x101, reports.append)
        elevator = ElevatorControllerNode(bus, clock, travel_ms_per_floor=1000)

        bus.send(CanFrame(can_id=0x100, data=[0x07], source="supervisor"))
        clock.advance(3000)
        elevator.update()

        self.assertEqual(elevator.current_floor, 3)
        self.assertEqual(elevator.target_floor, 3)
        self.assertEqual(reports[-1].can_id, 0x101)
        self.assertEqual(reports[-1].data, [0x03])


if __name__ == "__main__":
    unittest.main()
