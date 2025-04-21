#!/usr/bin/env python3

"""Bridge to Unreal Engine."""

import socket
import json
import logging
from typing import Dict, Any, Optional

# Global connection settings
_host = "127.0.0.1"
_port = 55557
_connection: Optional[socket.socket] = None

def initialize(host: str = "127.0.0.1", port: int = 55557) -> None:
    """Initialize the Unreal bridge connection details."""
    global _host, _port
    _host = host
    _port = port
    logging.info(f"Unreal bridge configured for host={host}, port={port}")

def _get_connection() -> socket.socket:
    """Establish or return the existing socket connection."""
    global _connection
    if _connection is None:
        try:
            logging.info(f"Attempting to connect to Unreal bridge at {_host}:{_port}...")
            _connection = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            _connection.settimeout(5.0) # Add a timeout
            _connection.connect((_host, _port))
            logging.info("Successfully connected to Unreal bridge.")
        except socket.timeout:
            logging.error(f"Connection timed out connecting to Unreal bridge at {_host}:{_port}")
            _connection = None # Reset on timeout
            raise ConnectionRefusedError(f"Timeout connecting to {_host}:{_port}")
        except ConnectionRefusedError:
            logging.error(f"Connection refused by Unreal bridge at {_host}:{_port}. Is Unreal Editor with the plugin running?")
            _connection = None # Reset on failure
            raise
        except Exception as e:
            logging.error(f"Unexpected error connecting to Unreal bridge: {e}")
            _connection = None # Reset on failure
            raise
    return _connection

def close_connection() -> None:
    """Close the socket connection if it exists."""
    global _connection
    if _connection is not None:
        try:
            _connection.close()
            logging.info("Closed connection to Unreal bridge.")
        except Exception as e:
            logging.error(f"Error closing connection: {e}")
        finally:
            _connection = None

def send_command(command: Dict[str, Any]) -> Optional[Dict[str, Any]]:
    """Send a command to the Unreal bridge and return the response."""
    response_str = ""
    try:
        sock = _get_connection()
        message = json.dumps(command) + "\n" # Ensure newline terminator
        logging.debug(f"Sending command to Unreal: {message.strip()}")
        sock.sendall(message.encode('utf-8'))

        # Receive response
        sock.settimeout(10.0) # Timeout for response
        buffer = b""
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                # Connection closed unexpectedly
                logging.error("Connection closed by Unreal bridge while waiting for response.")
                close_connection()
                return None 
            buffer += chunk
            # Check if the buffer ends with a newline, indicating complete JSON message
            if buffer.endswith(b'\n'):
                response_str = buffer.decode('utf-8').strip()
                logging.debug(f"Received response from Unreal: {response_str}")
                break
            # Add a safeguard against infinitely growing buffer if no newline is found
            if len(buffer) > 1024 * 1024: # e.g., 1MB limit
                logging.error("Response buffer exceeded limit without finding newline terminator.")
                close_connection()
                return None

        if response_str:
            return json.loads(response_str)
        return None # Should not happen if newline is detected

    except (ConnectionRefusedError, ConnectionResetError, BrokenPipeError, socket.timeout) as e:
        logging.error(f"Network error communicating with Unreal bridge: {e}")
        close_connection() # Close broken connection
        return {"error": f"Network error: {e}"} # Return error dict
    except json.JSONDecodeError as e:
        logging.error(f"Error decoding JSON response from Unreal: {e}. Response: '{response_str}'")
        # Don't close connection just for decode error, maybe next command works
        return {"error": f"JSON decode error: {e}"}
    except Exception as e:
        logging.error(f"Unexpected error sending command to Unreal bridge: {e}")
        close_connection() # Close connection on unknown error
        return {"error": f"Unexpected error: {e}"}

