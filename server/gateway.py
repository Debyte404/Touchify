from fastapi import FastAPI, HTTPException, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
import uvicorn
from datetime import datetime
import json
from typing import List

app = FastAPI(title="Touchify Gateway Server")

# Add CORS Middleware
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],  # Allow all origins for the dashboard
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# --- Database Configuration ---
DB_FILE = "database.json"

def load_db():
    try:
        with open(DB_FILE, "r") as f:
            return json.load(f)
    except Exception as e:
        print(f"Error loading database: {e}")
        return []

def save_db(data):
    try:
        with open(DB_FILE, "w") as f:
            json.dump(data, f, indent=2)
    except Exception as e:
        print(f"Error saving database: {e}")

# --- WebSocket Connection Manager ---
class ConnectionManager:
    def __init__(self):
        self.active_connections: List[WebSocket] = []

    async def connect(self, websocket: WebSocket):
        await websocket.accept()
        self.active_connections.append(websocket)
        print(f"New WebSocket client connected. Total: {len(self.active_connections)}")

    def disconnect(self, websocket: WebSocket):
        self.active_connections.remove(websocket)
        print(f"WebSocket client disconnected. Total: {len(self.active_connections)}")

    async def broadcast(self, message: dict):
        for connection in self.active_connections:
            try:
                await connection.send_json(message)
            except Exception as e:
                print(f"Error broadcasting to a client: {e}")

manager = ConnectionManager()

# --- Data Models ---
class BiometricData(BaseModel):
    year: int
    section_id: str
    finger_id: str  # Student ID
    confidence: int

# --- Endpoints ---
@app.get("/")
async def root():
    return {"message": "Touchify Gateway Server is running"}

@app.get("/students")
async def get_students():
    return load_db()

@app.websocket("/ws/attendance")
async def websocket_endpoint(websocket: WebSocket):
    await manager.connect(websocket)
    try:
        while True:
            # Keep connection alive
            await websocket.receive_text()
    except WebSocketDisconnect:
        manager.disconnect(websocket)

@app.post("/ingest/biometric")
async def ingest_biometric(data: BiometricData):
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    time_only = timestamp.split(' ')[1]
    
    # Update Database
    students = load_db()
    updated = False
    for student in students:
        if student["id"] == data.finger_id:
            student["status"] = "Present"
            student["time"] = time_only
            updated = True
            break
    
    if updated:
        save_db(students)
    
    event = {
        "type": "biometric_event",
        "timestamp": timestamp,
        "data": {
            "year": data.year,
            "section_id": data.section_id,
            "finger_id": data.finger_id,
            "confidence": data.confidence
        }
    }
    
    # terminal log
    print(f"[{timestamp}] PERSISTED & BROADCASTING: Student ID {data.finger_id}")
    
    # Broadcast
    await manager.broadcast(event)
    
    return {
        "status": "success",
        "timestamp": timestamp,
        "received": data
    }

if __name__ == "__main__":
    uvicorn.run("gateway:app", host="0.0.0.0", port=8000, reload=False)
