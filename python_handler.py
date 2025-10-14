from http.server import BaseHTTPRequestHandler, HTTPServer
import logging
import time

class RequestLoggerHandler(BaseHTTPRequestHandler):
    def _read_body(self):
        length = self.headers.get('Content-Length')
        if length:
            try:
                length = int(length)
                time.sleep(0.1)  # give client time to finish sending
                return self.rfile.read(length).decode('utf-8', errors='replace')
            except Exception as e:
                return f"[Error reading body: {e}]"
        return ""

    def _log_request(self):
        client_ip, _ = self.client_address
        logging.info(f"\n===== New {self.command} Request from {client_ip} =====")
        logging.info(f"Path: {self.path}")
        logging.info(f"Headers:\n{self.headers}")

        body = self._read_body()
        if body.strip():
            logging.info(f"Body:\n{body}")
        else:
            logging.info("Body: [empty or not received]")

        # Send response
        self.send_response(200)
        self.send_header('Content-Type', 'text/plain')
        self.send_header('Connection', 'close')  # force close after response
        self.end_headers()
        self.wfile.write(b"OK\n")

    def do_POST(self):
        self._log_request()

    def do_GET(self):
        self._log_request()

    # silence default console spam
    def log_message(self, format, *args):
        return

if __name__ == "__main__":
    logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(message)s')
    server = HTTPServer(('0.0.0.0', 80), RequestLoggerHandler)
    print("✅ HTTP Logger Server running on port 80...")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nServer stopped manually.")
