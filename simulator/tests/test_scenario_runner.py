import unittest

from sim.runner import run_builtin_scenario, run_process_scenario
from sim.supervisor import InProcessSupervisor
from sim.supervisor_process import SupervisorEvent


class ScenarioRunnerTests(unittest.TestCase):
    def test_basic_floor_request_reaches_requested_floor(self):
        result = run_builtin_scenario("basic_floor_request", InProcessSupervisor())

        self.assertTrue(result.passed)
        self.assertEqual(result.final_floor, 3)
        self.assertIn("floor_request:3", result.events)
        self.assertIn("arrived:3", result.events)

    def test_process_scenario_uses_supervisor_process_for_decisions(self):
        process = FakeSupervisorProcess()

        result = run_process_scenario("basic_floor_request", process)

        self.assertTrue(result.passed)
        self.assertEqual(process.received_ids[0], 0x203)


class FakeSupervisorProcess:
    def __init__(self):
        self.received_ids = []

    def start(self):
        pass

    def stop(self):
        pass

    def send_can_rx(self, frame):
        self.received_ids.append(frame.can_id)
        if frame.can_id == 0x203:
            return SupervisorEvent(event_type="can_tx", can_id=0x100, data=[0x07])
        return None


if __name__ == "__main__":
    unittest.main()
