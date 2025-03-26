# Blueprint Test Scripts

This directory contains test scripts for the blueprint tools in the UnrealMCP plugin.

## Available Tests

1. **test_basic_blueprint.py** - Creates a basic blueprint with a static mesh component
2. **test_physics_blueprint.py** - Creates a blueprint with physics enabled on a component
3. **test_component_properties.py** - Tests setting various properties on blueprint components

## Running the Tests

To run these tests, you need:

1. Unreal Engine running with the UnrealMCP plugin loaded
2. The MCP server running (port 55557 by default)

Run any test from the command line:

```bash
# From the UNREAL/Python directory:
cd UNREAL/Python
python scripts/blueprints/test_basic_blueprint.py

# Or run all tests:
for test in scripts/blueprints/test_*.py; do python $test; done
```

## Expected Results

Each test creates specific blueprints in the current Unreal project:

- **test_basic_blueprint.py** creates a "TestBP" blueprint with a cube mesh
- **test_physics_blueprint.py** creates a "PhysicsBP" blueprint with physics simulation
- **test_component_properties.py** creates a "PropertiesBP" blueprint with a custom lit mesh

All blueprints are spawned in the current level at Z=100 or higher so they're visible.

## Troubleshooting

If a test fails, check:

1. Is the Unreal MCP server running? (Check Unreal log)
2. Is the port correct? (Default: 55557)
3. Are there any error messages in the Unreal log?

## Adding New Tests

To add a new test:

1. Create a new Python file in this directory
2. Follow the pattern of existing tests
3. Make sure to reestablish a new connection for each command sent

## Understanding the Connection Pattern

Each command requires a fresh socket connection due to how the Unreal MCP plugin handles connections. This is why each test:

1. Creates a new socket connection
2. Sends a single command
3. Receives and processes the response  
4. Closes the connection
5. Creates a new connection for the next command 