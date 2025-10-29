from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse, parse_qs

class SimpleGETHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        # Parse URL and query
        parsed_url = urlparse(self.path)
        query_components = parse_qs(parsed_url.query)

        print("\n===== New GET Request =====")
        print(f"Path: {parsed_url.path}")
        print(f"Query: {query_components}")
        print(f"Client IP: {self.client_address[0]}")
        print(f"Headers:\n{self.headers}")

        # Respond to client
        self.send_response(200)
        self.send_header("Content-type", "text/plain")
        self.end_headers()
        self.wfile.write(b"GET request received successfully!\n")

    # Disable logging to console by BaseHTTPRequestHandler
    def log_message(self, format, *args):
        return

def run_server(host="0.0.0.0", port=80):
    server_address = (host, port)
    httpd = HTTPServer(server_address, SimpleGETHandler)
    print(f"✅ HTTP GET Server running on http://{host}:{port}")
    print("Press Ctrl+C to stop.\n")
    httpd.serve_forever()

if __name__ == "__main__":
    run_server()
