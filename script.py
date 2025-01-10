import socket
import time

def test_request(request, description):
    print(f"\nTesting: {description}")
    print(f"Sending: {request}")
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.connect(('localhost', 8080))
        s.send(request.encode())
        response = s.recv(4096)
        s.close()
        print("Response received:")
        print(response.decode())
    except Exception as e:
        print(f"Error: {e}")
    print("-" * 50)

# Test cases
print("Starting server tests...")

tests = [
    ('GET / HTTP/1.0\r\n\r\n', "Valid GET request (should return 200)"),
    ('POST / HTTP/1.0\r\n\r\n', "POST request (should return 501)"),
    ('MALFORMED', "Malformed request (should return 400)"),
    ('PUT / HTTP/1.0\r\n\r\n', "PUT request (should return 501)"),
    ('get / HTTP/1.0\r\n\r\n', "Lowercase GET (should return 400)"),
]

for request, description in tests:
    test_request(request, description)
    time.sleep(0.1)  # Small delay between tests

print("Tests completed!")