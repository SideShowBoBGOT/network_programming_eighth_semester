import subprocess
import pathlib
import os
import time

SCRIPT_DIR = pathlib.Path(os.path.dirname(os.path.abspath(__file__)))
BUILD_DIR = SCRIPT_DIR / 'build'
CLIENT_EXECUTABLE = BUILD_DIR / 'client.o'
SERVER_EXECUTABLE = BUILD_DIR / 'multiplex_server.o'
ADDRESS  = '0.0.0.0'
PORT = '55001'
MAX_FILE_SIZE = '1000000000'
MAX_CLIENTS = '100'

BOOKS_DIR = pathlib.Path('/home/sideshowbobgot/university/test_book')

server = subprocess.Popen([SERVER_EXECUTABLE, ADDRESS, PORT, BOOKS_DIR, MAX_CLIENTS])

clients: list[subprocess.Popen[bytes]] = []
for dirpath, dirnames, filenames in os.walk(BOOKS_DIR):
    for file in filenames:
        if file.endswith('.pdf'):
            clients.append(subprocess.Popen([CLIENT_EXECUTABLE, ADDRESS, PORT, file, MAX_FILE_SIZE]))

for client in clients:
    client.wait()

time.sleep(1)

print('[CLIENTS FINISHED]')

server.wait()