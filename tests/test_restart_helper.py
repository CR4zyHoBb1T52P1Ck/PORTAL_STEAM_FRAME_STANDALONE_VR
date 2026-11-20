import sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools/frame'))
import unittest
from portal_restart_helper import RestartState,valid_request

class RestartTests(unittest.TestCase):
    def test_normal_exit_never_relaunches(self):
        state=RestartState(100)
        self.assertEqual(state.tick(101,True,False),'running')
        self.assertEqual(state.tick(110,False,False),'wait')
        self.assertEqual(state.tick(147,False,False),'stop')
    def test_restart_waits_for_exit_and_cleanup_and_launches_once(self):
        state=RestartState(100)
        self.assertEqual(state.tick(101,True,False,True),'wait')
        self.assertEqual(state.tick(130,True,False),'wait')
        self.assertEqual(state.tick(131,False,False),'wait')
        self.assertEqual(state.tick(138,False,False),'wait')
        self.assertEqual(state.tick(139,False,False),'launch')
        self.assertEqual(state.tick(140,False,False),'wait')
        self.assertEqual(state.tick(145,True,False),'running')
        self.assertEqual(state.tick(146,True,False),'running')
    def test_other_game_blocks_restart_and_resets_cleanup_timer(self):
        state=RestartState(100);state.tick(101,True,False,True)
        self.assertEqual(state.tick(102,False,False),'wait')
        self.assertEqual(state.tick(109,False,True),'wait')
        self.assertEqual(state.tick(110,False,False),'wait')
        self.assertEqual(state.tick(117,False,False),'wait')
        self.assertEqual(state.tick(118,False,False),'launch')
    def test_game_that_does_not_quit_is_never_forced_closed(self):
        state=RestartState(100);state.tick(101,True,False,True)
        self.assertEqual(state.tick(177,True,False),'restart-timeout')
        self.assertEqual(state.tick(178,True,False),'running')
    def test_failed_relaunch_observer_detaches_without_an_app(self):
        state=RestartState(100);state.tick(101,True,False,True)
        state.tick(102,False,False);self.assertEqual(state.tick(110,False,False),'launch')
        self.assertEqual(state.tick(201,False,False),'stop')
    def test_active_game_has_no_observer_or_session_lifetime(self):
        state=RestartState(100)
        for elapsed in (1,600,10800,86400,604800):
            self.assertEqual(state.tick(100+elapsed,True,False),'running')
    def test_requests_are_fresh_scoped_and_not_replayed(self):
        request=dict(version=1,operation='restart_portal',created_at=110,nonce='33-440')
        self.assertTrue(valid_request(request,111,100,set()))
        self.assertFalse(valid_request(request,111,100,{'33-440'}))
        self.assertFalse(valid_request(request,160,100,set()))
        self.assertFalse(valid_request(request,111,120,set()))
        self.assertFalse(valid_request({**request,'operation':'run_command'},111,100,set()))
        self.assertFalse(valid_request({**request,'nonce':'arbitrary shell text'},111,100,set()))

if __name__=='__main__':unittest.main()
