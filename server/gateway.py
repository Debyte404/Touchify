from fastapi import FastAPI, HTTPException
from pydantic import BaseModel
import uvicorn
from datetime import datetime

app = FastAPI(title="Touchify Gateway Server")

class BiometricData(BaseModel):
    year: int
    section_id: str
    finger_id: str  # Now a string to support 10+ digit IDs
    confidence: int

@app.get("/")
async def root():
    return {"message": "Touchify Gateway Server is running"}

@app.post("/ingest/biometric")
async def ingest_biometric(data: BiometricData):
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    print(f"[{timestamp}] RECEIVED BIOMETRIC DATA:")
    print(f"  - Year: {data.year}")
    print(f"  - Section: {data.section_id}")
    print(f"  - Student ID: {data.finger_id}")
    print(f"  - Confidence: {data.confidence}")
    
    return {
        "status": "success",
        "timestamp": timestamp,
        "received": {
            "year": data.year,
            "section_id": data.section_id,
            "finger_id": data.finger_id,
            "confidence": data.confidence
        }
    }

if __name__ == "__main__":
    uvicorn.run("gateway:app", host="0.0.0.0", port=8000, reload=True)
