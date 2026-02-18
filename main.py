import os
import sys
import subprocess
import shutil

def is_admin():
    try:
        return os.getuid() == 0
    except AttributeError:
        return False

def main():
    print("--- MicroClaw Launcher ---")
    
    # Check if running as root/admin
    if not is_admin():
        print("Root privileges are required to access serial ports.")
        print("Relaunching with sudo...")
        
        # Get the path to the current python interpreter
        python_executable = sys.executable
        
        # Re-run the script with sudo
        # Preserve environment variables if needed, or at least PATH
        cmd = ["sudo", python_executable] + sys.argv
        
        try:
            subprocess.check_call(cmd)
        except subprocess.CalledProcessError as e:
            print(f"Error relaunching with sudo: {e}")
            sys.exit(e.returncode)
        except KeyboardInterrupt:
            sys.exit(0)
        return

    # If we are here, we are root.
    # Launch uvicorn for the Web UI
    print("Starting Web UI...")
    
    # We need to run uvicorn. We typically install dependencies in a venv or user site.
    # If running with sudo, we might lose access to user packages unless we use full path or appropriate env.
    # Assuming 'uv run' handles environment, but 'sudo uv' might fail if uv is not in root's path.
    # A safer bet since we are already in python: run uvicorn as a module.
    
    try:
        # We import uvicorn here to ensure it's available in this python environment
        import uvicorn
        
        # Run the app
        uvicorn.run("tools.web_ui:app", host="0.0.0.0", port=8000, reload=False)
        
    except ImportError:
        print("Error: 'uvicorn' not found. Please run 'pip install -r tools/requirements.txt'")
    except KeyboardInterrupt:
        print("\nStopping...")

if __name__ == "__main__":
    main()
