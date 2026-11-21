"""Real WAV + local HTTP integration. The API service is simulated, not the audio model."""
import base64
import contextlib
import io
import json
from pathlib import Path
import tempfile
import threading
import unittest
from unittest.mock import patch
from http.server import BaseHTTPRequestHandler, HTTPServer
import listen

class ListenerIntegration(unittest.TestCase):
    def setUp(self):
        self.seen = []
        self.status = 200
        self.response = {'id': 'test-only', 'model': 'simulated-service', 'usage': {},
                         'choices': [{'finish_reason': 'stop', 'message': {'content': 'SIMULATED assessment for transport testing only.'}}]}
        case = self
        class Handler(BaseHTTPRequestHandler):
            def do_POST(self):
                case.seen.append((self.path, self.headers.get('Authorization'),
                    json.loads(self.rfile.read(int(self.headers['Content-Length'])))))
                self.send_response(case.status)
                self.send_header('Content-Type', 'application/json')
                self.end_headers()
                self.wfile.write(json.dumps(case.response).encode())
            def log_message(self, *args): pass
        self.server = HTTPServer(('127.0.0.1', 0), Handler)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        self.url = 'http://127.0.0.1:%d/v1/chat/completions' % self.server.server_port

    def tearDown(self):
        self.server.shutdown()
        self.server.server_close()
        self.thread.join()

    def test_real_wavs_through_http_and_saved_report(self):
        original_send = listen.send_request
        with tempfile.TemporaryDirectory() as folder:
            argv = ['listen.py', str(listen.ROOT/'test-note.wav'), '--reference',
                    str(listen.ROOT/'test-held.wav'), '--output-dir', folder]
            with patch('sys.argv', argv), patch.object(listen, 'get_key', return_value='fake-test-key'), \
                 patch.object(listen, 'send_request', side_effect=lambda p,k: original_send(p,k,self.url)), \
                 contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(listen.main(), 0)
            path, auth, payload = self.seen[0]
            self.assertEqual(path, '/v1/chat/completions')
            self.assertEqual(auth, 'Bearer fake-test-key')
            self.assertEqual(payload['modalities'], ['text'])
            self.assertFalse(payload['store'])
            content = payload['messages'][1]['content']
            for i, name in [(1,'test-note.wav'), (3,'test-held.wav')]:
                self.assertEqual(base64.b64decode(content[i]['input_audio']['data']), (listen.ROOT/name).read_bytes())
            records = list(Path(folder).glob('*.json'))
            self.assertEqual(len(records), 1)
            text = records[0].read_text()
            record = json.loads(text)
            self.assertEqual(len(record['clips']), 2)
            self.assertIn('SIMULATED', record['assessment'])
            self.assertNotIn('fake-test-key', text)
            self.assertNotIn('input_audio', text)

    def test_http_error_is_actionable_and_redacted(self):
        self.status=401
        self.response={'error': {'message':'Must not expose fake-test-key'}}
        with self.assertRaisesRegex(RuntimeError, 'API key rejected') as error:
            listen.send_request({}, 'fake-test-key', self.url)
        self.assertNotIn('fake-test-key', str(error.exception))

    def test_incomplete_response_rejected(self):
        self.response['choices'][0]['finish_reason']='length'
        with self.assertRaisesRegex(ValueError, 'incomplete'):
            listen.send_request({}, 'fake-test-key', self.url)

    def test_bad_wav_and_missing_key_send_nothing(self):
        with tempfile.TemporaryDirectory() as folder:
            path=Path(folder)/'bad.wav'
            path.write_bytes(b'not a wave file')
            with self.assertRaises(Exception): listen.read_clip(path)
        with patch('sys.argv', ['listen.py']), patch.object(listen,'get_key',return_value=''), \
             contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(listen.main(), 2)
        self.assertEqual(self.seen, [])

if __name__ == '__main__': unittest.main(verbosity=2)
