"""
Unreal Engine MCP Server

A simple MCP server for interacting with Unreal Engine.
"""

import logging
import socket
import sys
import json
from mcp.server.fastmcp import FastMCP
from unreal_mcp.bridge import unreal_bridge as ub
from contextlib import asynccontextmanager
from typing import AsyncIterator, Dict, Any, Optional

# Configure logging with more detailed format
logging.basicConfig(
    level=logging.DEBUG,  # Change to DEBUG level for more details
    format='%(asctime)s - %(name)s - %(levelname)s - [%(filename)s:%(lineno)d] - %(message)s',
    handlers=[
        logging.FileHandler('unreal_mcp.log'),
        # logging.StreamHandler(sys.stdout) # Remove this handler to avoid JSON corruption
    ]
)
logger = logging.getLogger("UnrealMCP")

# Configuration
UNREAL_HOST = "127.0.0.1"
UNREAL_PORT = 55557

class UnrealConnection:
    """Connection to an Unreal Engine instance."""
    
    def __init__(self):
        """Initialize the connection."""
        self.socket = None
        self.connected = False
    
    def connect(self) -> bool:
        """Connect to the Unreal Engine instance."""
        try:
            if self.socket:
                try:
                    self.socket.close()
                except:
                    pass
                self.socket = None
            logger.info(f"Connecting to Unreal at {UNREAL_HOST}:{UNREAL_PORT}...")
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(5)
            self.socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 65536)
            self.socket.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 65536)
            self.socket.connect((UNREAL_HOST, UNREAL_PORT))
            self.connected = True
            logger.info("Connected to Unreal Engine")
            return True
        except Exception as e:
            logger.error(f"Failed to connect to Unreal: {e}")
            self.connected = False
            return False
    
    def disconnect(self):
        """Disconnect from the Unreal Engine instance."""
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
        self.socket = None
        self.connected = False

    def receive_full_response(self, sock, buffer_size=4096) -> bytes:
        """Receive a complete response from Unreal, handling chunked data."""
        chunks = []
        sock.settimeout(5)
        try:
            while True:
                chunk = sock.recv(buffer_size)
                if not chunk:
                    if not chunks:
                        raise Exception("Connection closed before receiving data")
                    break
                chunks.append(chunk)
                data = b''.join(chunks)
                try:
                    json.loads(data.decode('utf-8'))
                    logger.info(f"Received complete response ({len(data)} bytes)")
                    return data
                except json.JSONDecodeError:
                    logger.debug("Received partial response, waiting for more data...")
                    continue
        except socket.timeout:
            logger.warning("Socket timeout during receive")
            if chunks:
                data = b''.join(chunks)
                try:
                    json.loads(data.decode('utf-8'))
                    logger.info(f"Using partial response after timeout ({len(data)} bytes)")
                    return data
                except:
                    pass
            raise Exception("Timeout receiving Unreal response")
        except Exception as e:
            logger.error(f"Error during receive: {str(e)}")
            raise
    
    def send_command(self, command: str, params: Dict[str, Any] = None) -> Optional[Dict[str, Any]]:
        """Send a command to Unreal Engine and get the response."""
        if self.socket:
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            self.connected = False
        if not self.connect():
            logger.error("Failed to connect to Unreal Engine for command")
            return None
        try:
            command_obj = {"type": command, "params": params or {}}
            command_json = json.dumps(command_obj)
            logger.info(f"Sending command: {command_json}")
            self.socket.sendall(command_json.encode('utf-8'))
            response_data = self.receive_full_response(self.socket)
            response = json.loads(response_data.decode('utf-8'))
            logger.info(f"Complete response from Unreal: {response}")
            if response.get("status") == "error":
                error_message = response.get("error") or response.get("message", "Unknown Unreal error")
                logger.error(f"Unreal error (status=error): {error_message}")
                response["error"] = error_message
            elif response.get("success") is False:
                error_message = response.get("error") or response.get("message", "Unknown Unreal error")
                logger.error(f"Unreal error (success=false): {error_message}")
                response = {"status": "error", "error": error_message}
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            self.connected = False
            return response
        except Exception as e:
            logger.error(f"Error sending command: {e}")
            self.connected = False
            try:
                self.socket.close()
            except:
                pass
            self.socket = None
            return {"status": "error", "error": str(e)}

def get_unreal_connection():
    """Adapter to Unreal bridge using unrealbridge module."""
    try:
        ub.initialize(UNREAL_HOST, UNREAL_PORT)
        class BridgeConnection:
            def send_command(self, command: str, params=None):
                cmd = {"type": command, "params": params or {}}
                return ub.send_command(cmd)
            def disconnect(self):
                ub.close_connection()
        return BridgeConnection()
    except Exception as e:
        logger.error(f"Error initializing Unreal bridge: {e}")
        return None

@asynccontextmanager
async def server_lifespan(server: FastMCP) -> AsyncIterator[Dict[str, Any]]:
    """Handle server startup and shutdown."""
    logger.info("UnrealMCP server starting up")
    connection = get_unreal_connection()
    if connection:
        logger.info("Connected to Unreal Engine on startup")
    else:
        logger.warning("Could not connect to Unreal Engine on startup")
    try:
        yield {}
    finally:
        if connection:
            connection.disconnect()
        logger.info("Unreal MCP server shut down")

# Initialize server
mcp = FastMCP(
    "UnrealMCP",
    description="Unreal Engine integration via Model Context Protocol",
    lifespan=server_lifespan
)

# Import and register tools
from unreal_mcp.tools.editor_tools import register_editor_tools
from unreal_mcp.tools.blueprint_tools import register_blueprint_tools
from unreal_mcp.tools.node_tools import register_blueprint_node_tools
from unreal_mcp.tools.project_tools import register_project_tools
from unreal_mcp.tools.umg_tools import register_umg_tools

# Register tools
register_editor_tools(mcp)
register_blueprint_tools(mcp)
register_blueprint_node_tools(mcp)
register_project_tools(mcp)
register_umg_tools(mcp)  

@mcp.prompt()
def info():
    """Information about available Unreal MCP tools and best practices."""
    return """
    # Unreal MCP Server Tools and Best Practices
    ...
"""

# Run the server
if __name__ == "__main__":
    logger.info("Starting MCP server with stdio transport")
    mcp.run(transport='stdio')