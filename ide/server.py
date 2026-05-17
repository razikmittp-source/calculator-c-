#!/usr/bin/env python3
"""CoreC IDE Backend — serves the web IDE and compiles/runs CoreC code."""

import http.server
import json
import subprocess
import tempfile
import os
import sys
import time

# Add compiler to path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'compiler'))

PORT = 8080
IDE_DIR = os.path.dirname(os.path.abspath(__file__))
COMPILER = os.path.join(os.path.dirname(IDE_DIR), 'compiler', 'corec.py')


class IDEHandler(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=IDE_DIR, **kwargs)

    def do_POST(self):
        if self.path == '/api/run':
            content_length = int(self.headers['Content-Length'])
            body = json.loads(self.rfile.read(content_length))
            code = body.get('code', '')
            action = body.get('action', 'run')

            result = self.compile_and_run(code, action)
            
            self.send_response(200)
            self.send_header('Content-Type', 'application/json')
            self.send_header('Access-Control-Allow-Origin', '*')
            self.end_headers()
            self.wfile.write(json.dumps(result).encode())
        else:
            self.send_error(404)

    def do_OPTIONS(self):
        self.send_response(200)
        self.send_header('Access-Control-Allow-Origin', '*')
        self.send_header('Access-Control-Allow-Methods', 'POST, OPTIONS')
        self.send_header('Access-Control-Allow-Headers', 'Content-Type')
        self.end_headers()

    def compile_and_run(self, code: str, action: str) -> dict:
        with tempfile.TemporaryDirectory() as tmpdir:
            src = os.path.join(tmpdir, 'program.crc')
            binary = os.path.join(tmpdir, 'program')
            
            with open(src, 'w') as f:
                f.write(code)

            if action == 'emit':
                try:
                    result = subprocess.run(
                        [sys.executable, COMPILER, 'emit', src],
                        capture_output=True, text=True, timeout=10
                    )
                    if result.returncode == 0:
                        return {'success': True, 'output': result.stdout}
                    else:
                        return {'success': False, 'error': result.stderr or result.stdout}
                except Exception as e:
                    return {'success': False, 'error': str(e)}

            # Build
            try:
                result = subprocess.run(
                    [sys.executable, COMPILER, 'build', src, '-o', binary],
                    capture_output=True, text=True, timeout=15
                )
                if result.returncode != 0:
                    error_msg = result.stderr or result.stdout
                    return {'success': False, 'error': f'Compilation error:\n{error_msg}'}
            except subprocess.TimeoutExpired:
                return {'success': False, 'error': 'Compilation timed out (15s)'}
            except Exception as e:
                return {'success': False, 'error': f'Compiler error: {e}'}

            # Run
            try:
                start = time.time()
                result = subprocess.run(
                    [binary],
                    capture_output=True, text=True, timeout=10
                )
                elapsed = time.time() - start

                output = result.stdout
                if result.stderr:
                    output += '\n' + result.stderr
                
                return {
                    'success': True,
                    'output': output.strip(),
                    'time': f'{elapsed*1000:.1f}ms',
                    'exit_code': result.returncode
                }
            except subprocess.TimeoutExpired:
                return {'success': False, 'error': 'Program timed out (10s)'}
            except Exception as e:
                return {'success': False, 'error': f'Runtime error: {e}'}

    def log_message(self, format, *args):
        print(f"[IDE] {args[0]}")


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else PORT
    server = http.server.HTTPServer(('0.0.0.0', port), IDEHandler)
    print(f"🚀 CoreC IDE running at http://localhost:{port}")
    print(f"   Press Ctrl+C to stop")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\n👋 IDE stopped")
        server.server_close()


if __name__ == '__main__':
    main()
